/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include "headers.h"
#include "bswap.h"
#include "slurp.h"
#include "fmt.h"
#include "log.h"

#include "player/sndfile.h"

struct mus_header {
	char id[4]; // MUS\x1a
	uint16_t scorelen;
	uint16_t scorestart;
	//uint16_t channels;
	//uint16_t sec_channels;
	//uint16_t instrcnt;
	//uint16_t dummy;
};

static int read_mus_header(struct mus_header *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { return 0; } } while (0)

	READ_VALUE(id);
	READ_VALUE(scorelen);
	READ_VALUE(scorestart);
	//READ_VALUE(channels);
	//READ_VALUE(sec_channels);
	//READ_VALUE(instrcnt);
	//READ_VALUE(dummy);

#undef READ_VALUE

	if (memcmp(hdr->id, "MUS\x1a", 4))
		return 0;

	hdr->scorelen   = bswapLE16(hdr->scorelen);
	hdr->scorestart = bswapLE16(hdr->scorestart);

	if (((size_t)hdr->scorestart + hdr->scorelen) > slurp_length(fp))
		return 0;

	slurp_seek(fp, 8, SEEK_CUR); // skip

	return 1;
}

/* --------------------------------------------------------------------- */

int fmt_mus_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct mus_header hdr;

	if (!read_mus_header(&hdr, fp))
		return 0;

	file->description = "Doom Music File";
	file->title = strdup("");
	file->type = TYPE_MODULE_MOD;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */
/* I really don't know what I'm doing here -- I don't know much about either midi or adlib at all, and I've
never even *played* Doom. Frankly, I'm surprised that this produces something that's actually listenable.

Some things yet to tackle:
- Pitch wheel support is nonexistent. Shouldn't be TOO difficult; keep track of the target pitch value and how
	much of a slide has already been done, insert EFx/FFx effects, adjust notes when inserting them if the pitch
	wheel is more than a semitone off, and keep the speed at 1 if there's more sliding to do.
- Percussion channel isn't handled. Get a few adlib patches from some adlib S3Ms?
- Volumes for a couple of files are pretty screwy -- don't know whether I'm doing something wrong here, or if
	adlib's doing something funny with the volume, or maybe it's with the patches I'm using...
- awesomus/d_doom.mus has some very strange timing issues: I'm getting note events with thousands of ticks.
- Probably ought to clean up the warnings so messages only show once... */


#define MUS_ROWS_PER_PATTERN 200
#define MUS_SPEED_CHANNEL 15 // where the speed adjustments go (counted from 0 -- 15 is the drum channel)
#define MUS_BREAK_CHANNEL (MUS_SPEED_CHANNEL + 1)
#define MUS_TICKADJ_CHANNEL (MUS_BREAK_CHANNEL + 1) // S6x tick adjustments go here *and subsequent channels!!*

// Tick calculations are done in fixed point for better accuracy
#define FRACBITS 12
#define FRACMASK ((1 << FRACBITS) - 1)


int fmt_mus_load_song(song_t *song, slurp_t *fp, SCHISM_UNUSED unsigned int lflags)
{
	struct mus_header hdr;
	int n;
	song_note_t *note;
	int pat, row;
	int finished = 0;
	int tickfrac = 0; // fixed point
	struct {
		uint8_t note; // the last note played in this channel
		uint8_t instrument; // 1 -> 128
		uint8_t volume; // 0 -> 64
	} chanstate[16] = {0};
	uint8_t prevspeed = 1;
	uint8_t patch_samples[128] = {0};
	uint8_t patch_percussion[128] = {0};
	uint8_t nsmp = 1; // Next free sample
	size_t len;

	if (!read_mus_header(&hdr, fp))
		return LOAD_UNSUPPORTED;

	for (n = 16; n < 64; n++)
		song->channels[n].flags |= CHN_MUTE;

	slurp_seek(fp, hdr.scorestart, SEEK_SET);

	// Narrow the data buffer to simplify reading
	len = slurp_length(fp);
	len = MIN(len, hdr.scorestart + hdr.scorelen);

	/* start the first pattern */
	pat = 0;
	row = 0;
	song->pattern_size[pat] = song->pattern_alloc_size[pat] = MUS_ROWS_PER_PATTERN;
	song->patterns[pat] = csf_allocate_pattern(MUS_ROWS_PER_PATTERN);
	note = song->patterns[pat];
	song->orderlist[pat] = pat;

	while (!finished && slurp_tell(fp) < len) {
		uint8_t event, b1, b2, type, ch;

		event = slurp_getc(fp);
		type = (event >> 4) & 7;
		ch = event & 15;

		switch (type) {
		case 0: // Note off - figure out what channel the note was playing in and stick a === there.
			b1 = slurp_getc(fp) & 127; // & 127 => note number
			b1 = MIN((b1 & 127) + 1, NOTE_LAST);
			if (chanstate[ch].note == b1) {
				// Ok, we're actually playing that note
				if (!NOTE_IS_NOTE(note[ch].note))
					note[ch].note = NOTE_OFF;
			}
			break;
		case 1: // Play note
			b1 = slurp_getc(fp); // & 128 => volume follows, & 127 => note number
			if (b1 & 128) {
				chanstate[ch].volume = ((slurp_getc(fp) & 127) + 1) >> 1;
				b1 &= 127;
			}
			chanstate[ch].note = MIN(b1 + 1, NOTE_LAST);
			if (ch == 15) {
				// Percussion
				b1 = CLAMP(b1, 24, 84); // ?
				if (!patch_percussion[b1]) {
					if (nsmp < MAX_SAMPLES) {
						// New sample!
						patch_percussion[b1] = nsmp;
						strncpy(song->samples[nsmp].name,
							midi_percussion_names[b1 - 24], 25);
						song->samples[nsmp].name[25] = '\0';
						nsmp++;
					} else {
						// Phooey.
						log_appendf(4, " Warning: Too many samples");
						note[ch].note = NOTE_OFF;
					}
				}
#if 0
				note[ch].note = NOTE_MIDC;
				note[ch].instrument = patch_percussion[b1];
#else
				/* adlib is broken currently: it kind of "folds" every 9th channel, but only
				for SOME events ... what this amounts to is attempting to play notes from
				both of any two "folded" channels will cause everything to go haywire.
				for the moment, ignore the drums. even if we could load them, the playback
				would be completely awful.
				also reset the channel state, so that random note-off events don't stick ===
				into the channel, that's even enough to screw it up */
				chanstate[ch].note = NOTE_NONE;
#endif
			} else {
				if (chanstate[ch].instrument) {
					note[ch].note = chanstate[ch].note;
					note[ch].instrument = chanstate[ch].instrument;
				}
			}
			note[ch].voleffect = VOLFX_VOLUME;
			note[ch].volparam = chanstate[ch].volume;
			break;
		case 2: // Pitch wheel (TODO)
			b1 = slurp_getc(fp);
			break;
		case 3: // System event
			b1 = slurp_getc(fp) & 127;
			switch (b1) {
			case 10: // All sounds off
				for (n = 0; n < 16; n++) {
					note[ch].note = chanstate[ch].note = NOTE_CUT;
					note[ch].instrument = 0;
				}
				break;
			case 11: // All notes off
				for (n = 0; n < 16; n++) {
					note[ch].note = chanstate[ch].note = NOTE_OFF;
					note[ch].instrument = 0;
				}
				break;
			case 14: // Reset all controllers
				// ?
				memset(chanstate, 0, sizeof(chanstate));
				break;
			case 12: // Mono
			case 13: // Poly
				break;
			}
			break;
		case 4: // Change controller
			b1 = slurp_getc(fp) & 127; // controller
			b2 = slurp_getc(fp) & 127; // new value
			switch (b1) {
			case 0: // Instrument number
				if (ch == 15) {
					// don't fall for this nasty trick, this is the percussion channel
					break;
				}
				if (!patch_samples[b2]) {
					if (nsmp < MAX_SAMPLES) {
						// New sample!
						patch_samples[b2] = nsmp;
						adlib_patch_apply(song->samples + nsmp, b2);
						nsmp++;
					} else {
						// Don't have a sample number for this patch, and never will.
						log_appendf(4, " Warning: Too many samples");
						note[ch].note = NOTE_OFF;
					}
				}
				chanstate[ch].instrument = patch_samples[b2];
				break;
			case 3: // Volume
				b2 = (b2 + 1) >> 1;
				chanstate[ch].volume = b2;
				note[ch].voleffect = VOLFX_VOLUME;
				note[ch].volparam = chanstate[ch].volume;
				break;
			case 1: // Bank select
			case 2: // Modulation pot
			case 4: // Pan
			case 5: // Expression pot
			case 6: // Reverb depth
			case 7: // Chorus depth
			case 8: // Sustain pedal (hold)
			case 9: // Soft pedal
				// I have no idea
				break;
			}
			break;
		case 6: // Score end
			finished = 1;
			break;
		default: // Unknown (5 or 7)
			// Hope it doesn't take any parameters, otherwise things are going to end up broken
			log_appendf(4, " Warning: Unknown event type %d", type);
			break;
		}

		if (finished) {
			int leftover = (tickfrac + (1 << FRACBITS)) >> FRACBITS;
			note[MUS_BREAK_CHANNEL].effect = FX_PATTERNBREAK;
			note[MUS_BREAK_CHANNEL].param = 0;
			if (leftover && leftover != prevspeed) {
				note[MUS_SPEED_CHANNEL].effect = FX_SPEED;
				note[MUS_SPEED_CHANNEL].param = leftover;
			}
		} else if (event & 0x80) {
			// Read timing information and advance the row
			int ticks = 0;

			do {
				b1 = slurp_getc(fp);
				ticks = 128 * ticks + (b1 & 127);
				if (ticks > 0xffff)
					ticks = 0xffff;
			} while (b1 & 128);
			ticks = MIN(ticks, (0x7fffffff / 255) >> 12); // protect against overflow

			ticks <<= FRACBITS; // convert to fixed point
			ticks = ticks * 255 / 350; // 140 ticks/sec * 125/50hz => tempo of 350 (scaled)
			ticks += tickfrac; // plus whatever was leftover from the last row
			tickfrac = ticks & FRACMASK; // save the fractional part
			ticks >>= FRACBITS; // and back to a normal integer

			if (ticks < 1) {
#if 0
				// There's only part of a tick - compensate by skipping one tick later
				tickfrac -= 1 << FRACBITS;
				ticks = 1;
#else
				/* Don't advance the row: if there's another note right after one of the ones
				inserted already, the existing note will be rendered more or less irrelevant
				anyway, so just allow any following events to overwrite the data.
				Also, there's no need to write the speed, because it'd just be trampled over
				later anyway.
				The only thing that would necessitate advancing the row is if there's a pitch
				adjustment that's at least 15/16 of a semitone; in that case, "steal" a tick
				(see above). */
				continue;
#endif
			} else if (ticks > 255) {
				/* Too many ticks for a single row with Axx.
				We can increment multiple rows easily, but that only allows for exact multiples
				of some number of ticks, so adding in some "padding" is necessary. Since there
				is no guarantee that rows after the current one even exist, any adjusting has
				to happen on *this* row. */

				int adjust = ticks % 255;
				int s6xch = MUS_TICKADJ_CHANNEL;
				while (adjust) {
					int s6x = MIN(adjust, 0xf);
					note[s6xch].effect = FX_SPECIAL;
					note[s6xch].param = 0x60 | s6x;
					adjust -= s6x;
					s6xch++;
				}
			}
			if (prevspeed != MIN(ticks, 255)) {
				prevspeed = MIN(ticks, 255);
				note[MUS_SPEED_CHANNEL].effect = FX_SPEED;
				note[MUS_SPEED_CHANNEL].param = prevspeed;
			}
			ticks = ticks / 255 + 1;
			row += ticks;
			note += 64 * ticks;
		}

		while (row >= MUS_ROWS_PER_PATTERN) {
			/* Make a new pattern. */
			pat++;
			row -= MUS_ROWS_PER_PATTERN;
			if (pat >= MAX_PATTERNS) {
				log_appendf(4, " Warning: Too much note data");
				finished = 1;
				break;
			}
			song->pattern_size[pat] = song->pattern_alloc_size[pat] = MUS_ROWS_PER_PATTERN;
			song->patterns[pat] = csf_allocate_pattern(MUS_ROWS_PER_PATTERN);
			note = song->patterns[pat];
			song->orderlist[pat] = pat;

			note[MUS_SPEED_CHANNEL].effect = FX_SPEED;
			note[MUS_SPEED_CHANNEL].param = prevspeed;

			note += 64 * row;
		}
	}

	song->flags |= SONG_NOSTEREO;
	song->initial_speed = 1;
	song->initial_tempo = 255;

	strcpy(song->tracker_id, "Doom Music File"); // ?

	return LOAD_SUCCESS;
}

