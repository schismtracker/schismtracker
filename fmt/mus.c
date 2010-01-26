/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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

#define NEED_BYTESWAP
#include "headers.h"
#include "slurp.h"
#include "fmt.h"
#include "log.h"

#include "sndfile.h"


#pragma pack(push,1)
struct mus_header {
        char id[4]; // MUS\x1a
        uint16_t scorelen;
        uint16_t scorestart;
        uint16_t channels;
        uint16_t sec_channels;
        uint16_t instrcnt;
        uint16_t dummy;
};
#pragma pack(pop)

/* --------------------------------------------------------------------- */

int fmt_mus_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        struct mus_header *hdr = (struct mus_header *) data;

        if (!(length > sizeof(*hdr) && memcmp(hdr->id, "MUS\x1a", 4) == 0
              && bswapLE16(hdr->scorestart) + bswapLE16(hdr->scorelen) <= length))
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

// Tick calculations are done in fixed point for better accuracy
#define FRACBITS 12
#define FRACMASK ((1 << FRACBITS) - 1)


int fmt_mus_load_song(CSoundFile *song, slurp_t *fp, UNUSED unsigned int lflags)
{
        struct mus_header hdr;
        int n;
        MODCOMMAND *note = NULL;
        int pat = -1, row = MUS_ROWS_PER_PATTERN;
        int finished = 0;
        size_t reallen;
        int tickfrac = 0; // fixed point
        struct {
                uint8_t note; // the last note played in this channel
                uint8_t instr; // 1 -> 128
                uint8_t vol; // 0 -> 64
        } chanstate[16];
        uint8_t prevspeed = 1;
        uint8_t patch_samples[128] = {0};
        uint8_t nsmp = 1; // Next free sample

        slurp_read(fp, &hdr, sizeof(hdr));
        hdr.scorelen = bswapLE16(hdr.scorelen);
        hdr.scorestart = bswapLE16(hdr.scorestart);

        if (memcmp(hdr.id, "MUS\x1a", 4) != 0)
                return LOAD_UNSUPPORTED;
        else if (hdr.scorestart + hdr.scorelen > fp->length)
                return LOAD_FORMAT_ERROR;


        // Drums are on channel 16 -- until they're implemented properly, mute it to silence the random notes
        for (n = 15; n < 64; n++)
                song->Channels[n].dwFlags |= CHN_MUTE;

        slurp_seek(fp, hdr.scorestart, SEEK_SET);

        // Narrow the data buffer to simplify reading
        reallen = fp->length;
        fp->length = MIN(fp->length, hdr.scorestart + hdr.scorelen);

        memset(chanstate, 0, sizeof(chanstate));

        while (!finished && !slurp_eof(fp)) {
                uint8_t event, b1, b2, type, ch;

                while (row >= MUS_ROWS_PER_PATTERN) {
                        /* Make a new pattern. */
                        pat++;
                        row -= MUS_ROWS_PER_PATTERN;
                        if (pat >= MAX_PATTERNS) {
                                log_appendf(4, " Warning: Too much note data");
                                break;
                        }
                        song->PatternSize[pat] = song->PatternAllocSize[pat] = MUS_ROWS_PER_PATTERN;
                        song->Patterns[pat] = csf_allocate_pattern(MUS_ROWS_PER_PATTERN, 64);
                        note = song->Patterns[pat];
                        song->Orderlist[pat] = pat;

                        note[MUS_SPEED_CHANNEL].command = CMD_SPEED;
                        note[MUS_SPEED_CHANNEL].param = prevspeed;

                        note += 64 * row;
                }

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
                                chanstate[ch].vol = ((slurp_getc(fp) & 127) + 1) >> 1;
                        }
                        chanstate[ch].note = MIN((b1 & 127) + 1, NOTE_LAST);
                        if (chanstate[ch].instr) {
                                note[ch].note = chanstate[ch].note;
                                note[ch].instr = chanstate[ch].instr;
                        }
                        note[ch].volcmd = VOLCMD_VOLUME;
                        note[ch].vol = chanstate[ch].vol;
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
                                        note[ch].instr = 0;
                                }
                                break;
                        case 11: // All notes off
                                for (n = 0; n < 16; n++) {
                                        note[ch].note = chanstate[ch].note = NOTE_OFF;
                                        note[ch].instr = 0;
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
                                if (!patch_samples[b2]) {
                                        if (nsmp < MAX_SAMPLES) {
                                                // New sample!
                                                if (b2 < 128) {
                                                        // Not already allocated a sample
                                                        patch_samples[b2] = nsmp;
                                                        adlib_patch_apply(song->Samples + nsmp, b2);
                                                        nsmp++;
                                                } else {
                                                        // Percussion (TODO)
                                                }
                                        } else {
                                                // Don't have a sample number for this patch, and never will.
                                                log_appendf(4, " Warning: Too many samples");
                                                note[ch].note = NOTE_OFF;
                                        }
                                }
                                chanstate[ch].instr = patch_samples[b2];
                                break;
                        case 3: // Volume
                                b2 = (b2 + 1) >> 1;
                                chanstate[ch].vol = b2;
                                note[ch].volcmd = VOLCMD_VOLUME;
                                note[ch].vol = chanstate[ch].vol;
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
                        note[MUS_BREAK_CHANNEL].command = CMD_PATTERNBREAK;
                        note[MUS_BREAK_CHANNEL].param = 0;
                        note[MUS_SPEED_CHANNEL].command = CMD_SPEED;
                        note[MUS_SPEED_CHANNEL].param = (tickfrac + (1 << FRACBITS)) >> FRACBITS;
                } else if (event & 0x80) {
                        // Read timing information and advance the row
                        int ticks = 0;

                        do {
                                b1 = slurp_getc(fp);
                                ticks = 128 * ticks + (b1 & 127);
                        } while (b1 & 128);
                        ticks = MIN(ticks, (0x7fffffff / 255) >> 12); // protect against overflow

                        ticks <<= FRACBITS; // convert to fixed point
                        ticks = ticks * 255 / 350; // 140 ticks/sec * 125/50hz => tempo of 350 (scaled)
                        ticks += tickfrac; // plus whatever was leftover from the last row
                        tickfrac = ticks & FRACMASK; // save the fractional part
                        ticks >>= FRACBITS; // and back to a normal integer

                        if (!ticks) {
                                // There's only part of a tick - compensate by skipping one tick later
                                tickfrac -= 1 << FRACBITS;
                                ticks = 1;
                        }

                        if (prevspeed != MIN(ticks, 255)) {
                                prevspeed = MIN(ticks, 255);
                                note[MUS_SPEED_CHANNEL].command = CMD_SPEED;
                                note[MUS_SPEED_CHANNEL].param = prevspeed;
                        }
                        ticks = ticks / 255 + 1;
                        row += ticks;
                        note += 64 * ticks;
                }
        }

        // Widen the buffer again.
        fp->length = reallen;

        song->m_dwSongFlags |= SONG_NOSTEREO;
        song->m_nDefaultSpeed = 1;
        song->m_nDefaultTempo = 255;

        return LOAD_SUCCESS;
}

