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
#include "version.h"

#include "player/sndfile.h"

#include "disko.h"
#include "log.h"

#include <inttypes.h>

/* --------------------------------------------------------------------- */

int fmt_s3m_read_info(dmoz_file_t *file, slurp_t *fp)
{
	unsigned char magic[4], title[27];
	
	slurp_seek(fp, 44, SEEK_SET);
	if (slurp_read(fp, magic, sizeof(magic)) != sizeof(magic)
		|| memcmp(magic, "SCRM", sizeof(magic)))
		return 0;

	slurp_rewind(fp);
	if (slurp_read(fp, title, sizeof(title)) != sizeof(title))
		return 0;

	file->description = "Scream Tracker 3";
	/*file->extension = str_dup("s3m");*/
	file->title = strn_dup((const char *)title, sizeof(title));
	file->type = TYPE_MODULE_S3M;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

enum {
	S3I_TYPE_NONE = 0,
	S3I_TYPE_PCM = 1,
	S3I_TYPE_ADMEL = 2,
	S3I_TYPE_CONTROL = 0xff, // only internally used for saving
};


/* misc flags for loader (internal) */
#define S3M_UNSIGNED 1
#define S3M_CHANPAN 2 // the FC byte

static int s3m_import_edittime(song_t *song, uint16_t trkvers, uint32_t reserved32)
{
	if (song->histlen)
		return 0; // ?

	song->histlen = 1;
	song->history = mem_calloc(1, sizeof(*song->history));

	uint32_t runtime = it_decode_edit_timer(trkvers, reserved32);
	song->history[0].runtime = dos_time_to_ms(runtime);

	return 1;
}

int fmt_s3m_load_song(song_t *song, slurp_t *fp, unsigned int lflags)
{
	uint16_t nsmp, nord, npat;
	int misc = S3M_UNSIGNED | S3M_CHANPAN; // temporary flags, these are both generally true
	int n;
	song_note_t *note;
	/* junk variables for reading stuff into */
	uint16_t tmp;
	uint8_t c;
	uint32_t tmplong;
	uint8_t b[4];
	uint8_t channel_types[32];
	/* parapointers */
	uint16_t para_smp[MAX_SAMPLES];
	uint16_t para_pat[MAX_PATTERNS];
	uint32_t para_sdata[MAX_SAMPLES] = { 0 };
	uint32_t smp_flags[MAX_SAMPLES] = { 0 };
	song_sample_t *sample;
	uint16_t trkvers;
	uint16_t flags;
	uint16_t special;
	uint8_t reserved[8];
	uint16_t reserved16low; // low 16 bits of version info
	uint16_t reserved16high; // high 16 bits of version info
	uint32_t reserved32; // Impulse Tracker edit time
	uint32_t adlib = 0; // bitset
	uint16_t gus_addresses = 0;
	uint8_t mix_volume; /* detect very old modplug tracker */
	char any_samples = 0;
	int uc;
	const char *tid = NULL;

	/* check the tag */
	slurp_seek(fp, 44, SEEK_SET);
	slurp_read(fp, b, 4);
	if (memcmp(b, "SCRM", 4) != 0)
		return LOAD_UNSUPPORTED;

	/* read the title */
	slurp_rewind(fp);
	slurp_read(fp, song->title, 25);
	song->title[25] = 0;

	/* skip the last three bytes of the title, the supposed-to-be-0x1a byte,
	the tracker ID, and the two useless reserved bytes */
	slurp_seek(fp, 7, SEEK_CUR);

	slurp_read(fp, &nord, 2);
	slurp_read(fp, &nsmp, 2);
	slurp_read(fp, &npat, 2);
	nord = bswapLE16(nord);
	nsmp = bswapLE16(nsmp);
	npat = bswapLE16(npat);

	if (nord > MAX_ORDERS || nsmp > MAX_SAMPLES || npat > MAX_PATTERNS)
		return LOAD_FORMAT_ERROR;

	song->flags = SONG_ITOLDEFFECTS;
	slurp_read(fp, &flags, 2);  /* flags (don't really care) */
	flags = bswapLE16(flags);
	slurp_read(fp, &trkvers, 2);
	trkvers = bswapLE16(trkvers);
	slurp_read(fp, &tmp, 2);  /* file format info */
	if (tmp == bswapLE16(1))
		misc &= ~S3M_UNSIGNED;     /* signed samples (ancient s3m) */

	slurp_seek(fp, 4, SEEK_CUR); /* skip the tag */

	song->initial_global_volume = slurp_getc(fp) << 1;
	// In the case of invalid data, ST3 uses the speed/tempo value that's set in the player prior to
	// loading the song, but that's just crazy.
	song->initial_speed = slurp_getc(fp);
	if (!song->initial_speed)
		song->initial_speed = 6;

	song->initial_tempo = slurp_getc(fp);
	if (song->initial_tempo <= 32) {
		// (Yes, 32 is ignored by Scream Tracker.)
		song->initial_tempo = 125;
	}
	mix_volume = song->mixing_volume = slurp_getc(fp);
	if (song->mixing_volume & 0x80) {
		song->mixing_volume ^= 0x80;
	} else {
		song->flags |= SONG_NOSTEREO;
	}
	uc = slurp_getc(fp); /* ultraclick removal (useless) */

	if (slurp_getc(fp) != 0xfc)
		misc &= ~S3M_CHANPAN;     /* stored pan values */

	slurp_read(fp, &reserved, 8);
	memcpy(&reserved16low, reserved, 2);
	memcpy(&reserved32, reserved + 2, 4);
	memcpy(&reserved16high, reserved + 6, 2);
	reserved16low = bswapLE16(reserved16low); // schism & openmpt version info
	reserved32 = bswapLE32(reserved32); // impulse tracker edit timer
	reserved16high = bswapLE16(reserved16high); // high bits of schism version info
	slurp_read(fp, &special, 2); // field not used by st3
	special = bswapLE16(special);

	/* channel settings */
	slurp_read(fp, channel_types, 32);
	for (n = 0; n < 32; n++) {
		/* Channel 'type': 0xFF is a disabled channel, which shows up as (--) in ST3.
		Any channel with the high bit set is muted.
		00-07 are L1-L8, 08-0F are R1-R8, 10-18 are adlib channels A1-A9.
		Hacking at a file with a hex editor shows some perhaps partially-implemented stuff:
		types 19-1D show up in ST3 as AB, AS, AT, AC, and AH; 20-2D are the same as 10-1D
		except with 'B' insted of 'A'. None of these appear to produce any sound output,
		apart from 19 which plays adlib instruments briefly before cutting them. (Weird!)
		Also, 1E/1F and 2E/2F display as "??"; and pressing 'A' on a disabled (--) channel
		will change its type to 1F.
		Values past 2F seem to display bits of the UI like the copyright and help, strange!
		These out-of-range channel types will almost certainly hang or crash ST3 or
		produce other strange behavior. Simply put, don't do it. :) */
		c = channel_types[n];
		if (c & 0x80) {
			song->channels[n].flags |= CHN_MUTE;
			// ST3 doesn't even play effects in muted channels -- throw them out?
			c &= ~0x80;
		}
		if (c < 0x08) {
			// L1-L8 (panned to 3 in ST3)
			song->channels[n].panning = 14;
		} else if (c < 0x10) {
			// R1-R8 (panned to C in ST3)
			song->channels[n].panning = 50;
		} else if (c < 0x19) {
			// A1-A9
			song->channels[n].panning = 32;
			adlib |= 1 << n;
		} else {
			// Disabled 0xff/0x7f, or broken
			song->channels[n].panning = 32;
			song->channels[n].flags |= CHN_MUTE;
		}
		song->channels[n].volume = 64;
	}
	for (; n < 64; n++) {
		song->channels[n].panning = 32;
		song->channels[n].volume = 64;
		song->channels[n].flags = CHN_MUTE;
	}

	// Schism Tracker before 2018-11-12 played AdLib instruments louder than ST3. Compensate by lowering the sample mixing volume.
	if (adlib && trkvers >= 0x4000 && trkvers < 0x4D33) {
		song->mixing_volume = song->mixing_volume * 2274 / 4096;
	}

	/* orderlist */
	slurp_read(fp, song->orderlist, nord);
	memset(song->orderlist + nord, ORDER_LAST, MAX_ORDERS - nord);

	/* load the parapointers */
	slurp_read(fp, para_smp, 2 * nsmp);
	slurp_read(fp, para_pat, 2 * npat);

	/* default pannings */
	if (misc & S3M_CHANPAN) {
		for (n = 0; n < 32; n++) {
			int pan = slurp_getc(fp);
			if ((pan & 0x20) && (!(adlib & (1 << n)) || trkvers > 0x1320))
				song->channels[n].panning = ((pan & 0xf) * 64) / 15;
		}
	}

	//mphack - fix the pannings
	for (n = 0; n < 64; n++)
		song->channels[n].panning *= 4;

	/* samples */
	for (n = 0, sample = song->samples + 1; n < nsmp; n++, sample++) {
		uint8_t type;

		slurp_seek(fp, bswapLE16(para_smp[n]) << 4, SEEK_SET);

		type = slurp_getc(fp);
		slurp_read(fp, sample->filename, 12);
		sample->filename[12] = 0;

		slurp_read(fp, b, 3); // data pointer for pcm, irrelevant otherwise
		switch (type) {
		case S3I_TYPE_PCM:
			para_sdata[n] = b[1] | (b[2] << 8) | (b[0] << 16);
			slurp_read(fp, &tmplong, 4);
			sample->length = bswapLE32(tmplong);
			slurp_read(fp, &tmplong, 4);
			sample->loop_start = bswapLE32(tmplong);
			slurp_read(fp, &tmplong, 4);
			sample->loop_end = bswapLE32(tmplong);
			sample->volume = slurp_getc(fp) * 4; //mphack
			slurp_getc(fp);      /* unused byte */
			slurp_getc(fp);      /* packing info (never used) */
			c = slurp_getc(fp);  /* flags */
			if (c & 1)
				sample->flags |= CHN_LOOP;
			smp_flags[n] = (SF_LE
				| ((misc & S3M_UNSIGNED) ? SF_PCMU : SF_PCMS)
				| ((c & 4) ? SF_16 : SF_8)
				| ((c & 2) ? SF_SS : SF_M));
			if (sample->length)
				any_samples = 1;
			break;

		default:
			//printf("s3m: mystery-meat sample type %d\n", type);
		case S3I_TYPE_NONE:
			slurp_seek(fp, 12, SEEK_CUR);
			sample->volume = slurp_getc(fp) * 4; //mphack
			slurp_seek(fp, 3, SEEK_CUR);
			break;

		case S3I_TYPE_ADMEL:
			slurp_read(fp, sample->adlib_bytes, 12);
			sample->volume = slurp_getc(fp) * 4; //mphack
			// next byte is "dsk", what is that?
			slurp_seek(fp, 3, SEEK_CUR);
			sample->flags |= CHN_ADLIB;
			// dumb hackaround that ought to some day be fixed:
			sample->length = 1;
			sample->data = csf_allocate_sample(1);
			break;
		}

		slurp_read(fp, &tmplong, 4);
		sample->c5speed = bswapLE32(tmplong);
		if (type == S3I_TYPE_ADMEL) {
			if (sample->c5speed < 1000 || sample->c5speed > 0xFFFF) {
				sample->c5speed = 8363;
			}
		}
		slurp_seek(fp, 4, SEEK_CUR);        /* unused space */
		int16_t gus_address;
		slurp_read(fp, &gus_address, 2);
		gus_addresses |= bswapLE16(gus_address);
		slurp_seek(fp, 6, SEEK_CUR);
		slurp_read(fp, sample->name, 25);
		sample->name[25] = 0;
		sample->vib_type = 0;
		sample->vib_rate = 0;
		sample->vib_depth = 0;
		sample->vib_speed = 0;
		sample->global_volume = 64;
	}

	/* sample data */
	if (!(lflags & LOAD_NOSAMPLES)) {
		for (n = 0, sample = song->samples + 1; n < nsmp; n++, sample++) {
			if (!sample->length || (sample->flags & CHN_ADLIB))
				continue;
			slurp_seek(fp, para_sdata[n] << 4, SEEK_SET);
			csf_read_sample(sample, smp_flags[n], fp);
		}
	}

	// Mixing volume is not used with the GUS driver; relevant for PCM + OPL tracks
	if (gus_addresses > 1)
		song->mixing_volume = 48;

	if (!(lflags & LOAD_NOPATTERNS)) {
		for (n = 0; n < npat; n++) {
			int row = 0;
			long end;

			para_pat[n] = bswapLE16(para_pat[n]);
			if (!para_pat[n])
				continue;

			slurp_seek(fp, para_pat[n] << 4, SEEK_SET);
			slurp_read(fp, &tmp, 2);
			end = (para_pat[n] << 4) + bswapLE16(tmp) + 2;

			song->patterns[n] = csf_allocate_pattern(64);

			while (row < 64 && slurp_tell(fp) < end) {
				int mask = slurp_getc(fp);
				uint8_t chn = (mask & 31);

				if (mask == EOF) {
					log_appendf(4, " Warning: Pattern %d: file truncated", n);
					break;
				}
				if (!mask) {
					/* done with the row */
					row++;
					continue;
				}
				note = song->patterns[n] + 64 * row + chn;
				if (mask & 32) {
					/* note/instrument */
					note->note = slurp_getc(fp);
					note->instrument = slurp_getc(fp);
					//if (note->instrument > 99)
					//      note->instrument = 0;
					switch (note->note) {
					default:
						// Note; hi=oct, lo=note
						note->note = (note->note >> 4) * 12 + (note->note & 0xf) + 13;
						break;
					case 255:
						note->note = NOTE_NONE;
						break;
					case 254:
						note->note = (adlib & (1 << chn)) ? NOTE_OFF : NOTE_CUT;
						break;
					}
				}
				if (mask & 64) {
					/* volume */
					note->voleffect = VOLFX_VOLUME;
					note->volparam = slurp_getc(fp);
					if (note->volparam == 255) {
						note->voleffect = VOLFX_NONE;
						note->volparam = 0;
					} else if (note->volparam >= 128 && note->volparam <= 192) {
						// ModPlug (or was there any earlier tracker using this command?)
						note->voleffect = VOLFX_PANNING;
						note->volparam -= 128;
					} else if (note->volparam > 64) {
						// some weirdly saved s3m?
						note->volparam = 64;
					}
				}
				if (mask & 128) {
					note->effect = slurp_getc(fp);
					note->param = slurp_getc(fp);
					csf_import_s3m_effect(note, 0);
					if (note->effect == FX_SPECIAL) {
						// mimic ST3's SD0/SC0 behavior
						if (note->param == 0xd0) {
							note->note = NOTE_NONE;
							note->instrument = 0;
							note->voleffect = VOLFX_NONE;
							note->volparam = 0;
							note->effect = FX_NONE;
							note->param = 0;
						} else if (note->param == 0xc0) {
							note->effect = FX_NONE;
							note->param = 0;
						} else if ((note->param & 0xf0) == 0xa0) {
							// Convert the old messy SoundBlaster stereo control command (or an approximation of it, anyway)
							uint8_t ctype = channel_types[chn] & 0x7f;
							if (gus_addresses > 1 || ctype >= 0x10)
								note->effect = FX_NONE;
							else if (note->param == 0xa0 || note->param == 0xa2)  // Normal panning
								note->param = (ctype & 8) ? 0x8c : 0x83;
							else if (note->param == 0xa1 || note->param == 0xa3)  // Swap left / right channel
								note->param = (ctype & 8) ? 0x83 : 0x8c;
							else if (note->param <= 0xa7)  // Center
								note->param = 0x88;
							else
								note->effect = FX_NONE;
						}
					}
				}
				/* ... next note, same row */
			}
		}
	}

	/* MPT identifies as ST3.20 in the trkvers field, but it puts zeroes for the 'special' field, only ever
	 * sets flags 0x10 and 0x40, writes multiples of 16 orders, always saves channel pannings, and writes
	 * zero into the ultraclick removal field. (ST3.2x always puts either 16, 24, or 32 there, older versions put 0).
	 * Velvet Studio also pretends to be ST3, but writes zeroes for 'special'. ultraclick, and flags, and
	 * does NOT save channel pannings. Also, it writes a fairly recognizable LRRL pattern for the channels,
	 * but I'm not checking that. (yet?) */
	if (trkvers == 0x1320) {
		if (!memcmp(reserved, "SCLUB2.0", 8)) {
			tid = "Sound Club 2";
		} else if (special == 0 && uc == 0 && (flags & ~0x50) == 0
		    && misc == (S3M_UNSIGNED | S3M_CHANPAN) && (nord % 16) == 0) {
			/* from OpenMPT:
			 * MPT 1.0 alpha5 doesn't set the stereo flag, but MPT 1.0 alpha6 does. */

			tid = ((mix_volume & 0x80) != 0)
				? "ModPlug Tracker / OpenMPT 1.17"
				: "ModPlug Tracker 1.0 alpha";
		} else if (special == 0 && uc == 0 && flags == 0 && misc == S3M_UNSIGNED) {
			if (song->initial_global_volume == 128 && mix_volume == 48)
				tid = "PlayerPRO";
			else  // Always stereo
				tid = "Velvet Studio";
		} else if(special == 0 && uc == 0 && flags == 8 && misc == S3M_UNSIGNED) {
			tid = "Impulse Tracker < 1.03";  // Not sure if 1.02 saves like this as I don't have it
		} else if (uc != 16 && uc != 24 && uc != 32) {
			// sure isn't scream tracker
			tid = "Unknown tracker";
		}
	}

	if (!tid) {
		switch (trkvers >> 12) {
		case 0:
			if (trkvers == 0x0208)
				strcpy(song->tracker_id, "Akord");
			break;
		case 1:
			if (gus_addresses > 1)
				tid = "Scream Tracker %" PRIu8 ".%02" PRIx8 " (GUS)";
			else if (gus_addresses == 1 || !any_samples || trkvers == 0x1300)
				tid = "Scream Tracker %" PRIu8 ".%02" PRIx8 " (SB)"; // could also be a GUS file with a single sample
			else {
				strcpy(song->tracker_id, "Unknown tracker");
				if (trkvers == 0x1301 && uc == 0) {
					if (!(flags & ~0x50) && (mix_volume & 0x80) && (misc & S3M_CHANPAN))
						strcpy(song->tracker_id, "UNMO3");
					else if (!flags && song->initial_global_volume == 96 && mix_volume == 176 && song->initial_tempo == 150 && !(misc & S3M_CHANPAN))
						strcpy(song->tracker_id, "deMODifier");  // SoundSmith to S3M converter
					else if (!flags && song->initial_global_volume == 128 && song->initial_speed == 6 && song->initial_tempo == 125 && !(misc & S3M_CHANPAN))
						strcpy(song->tracker_id, "Kosmic To-S3M");  // MTM to S3M converter by Zab/Kosmic
				}
			}
			break;
		case 2:
			if (trkvers == 0x2013) // PlayerPRO on Intel forgets to byte-swap the tracker ID bytes 
				strcpy(song->tracker_id, "PlayerPRO");
			else
				tid = "Imago Orpheus %" PRIu8 ".%02" PRIx8;
			break;
		case 3:
			if (trkvers <= 0x3214) {
				tid = "Impulse Tracker %" PRIu8 ".%02" PRIx8;
			} else if (trkvers == 0x3320) {
				tid = "Impulse Tracker 1.03";  // Could also be 1.02, maybe? I don't have that one
			} else if(trkvers >= 0x3215 && trkvers <= 0x3217) {
				tid = NULL;
				const char *versions[] = { "1-2", "3", "4-5" };
				sprintf(song->tracker_id, "Impulse Tracker 2.14p%s", versions[trkvers - 0x3215]);
			}

			if (trkvers >= 0x3207 && trkvers <= 0x3217 && reserved32)
				s3m_import_edittime(song, trkvers, reserved32);

			break;
		case 4:
			if (trkvers == 0x4100) {
				strcpy(song->tracker_id, "BeRoTracker");
			} else {
				uint32_t full_version = (((uint32_t)reserved16high) << 16) | (reserved16low);
				strcpy(song->tracker_id, "Schism Tracker ");
				ver_decode_cwtv(trkvers, full_version, song->tracker_id + strlen(song->tracker_id));
				if (trkvers == 0x4fff && full_version >= 0x1560)
					s3m_import_edittime(song, 0x0000, reserved32);
			}
			break;
		case 5:
			/* from OpenMPT src:
			 *
			 * Liquid Tracker's ID clashes with OpenMPT's.
			 * OpenMPT started writing full version information with OpenMPT 1.29 and later changed the ultraClicks value from 8 to 16.
			 * Liquid Tracker writes an ultraClicks value of 16.
			 * So we assume that a file was saved with Liquid Tracker if the reserved fields are 0 and ultraClicks is 16. */
			if ((trkvers >> 8) == 0x57) {
				tid = "NESMusa %" PRIu8 ".%" PRIX8; /* tool by Bisquit */
			} else if (!reserved16low && uc == 16 && channel_types[1] != 1) {
				tid = "Liquid Tracker %" PRIu8 ".%" PRIX8;
			} else if (trkvers == 0x5447) {
				strcpy(song->tracker_id, "Graoumf Tracker");
			} else if (trkvers >= 0x5129 && reserved16low) {
				/* e.x. 1.29.01.12 <-> 0x1290112 */
				const uint32_t ver = (((trkvers & 0xfff) << 16) | reserved16low);
				sprintf(song->tracker_id, "OpenMPT %" PRIu32 ".%02" PRIX32 ".%02" PRIX32 ".%02" PRIX32, ver >> 24, (ver >> 16) & 0xFF, (ver >> 8) & 0xFF, (ver) & 0xFF);
				if (ver >= UINT32_C(0x01320031))
					s3m_import_edittime(song, 0x0000, reserved32);
			} else {
				tid = "OpenMPT %" PRIu8 ".%02" PRIX8;
			}
			break;
		case 6:
			strcpy(song->tracker_id, "BeRoTracker");
			break;
		case 7:
			strcpy(song->tracker_id, "CreamTracker");
			break;
		case 12:
			if (trkvers == 0xCA00)
				strcpy(song->tracker_id, "Camoto");
			break;
		default:
			break;
		}
	}
	if (tid)
		sprintf(song->tracker_id, tid, (uint8_t)((trkvers & 0xf00) >> 8), (uint8_t)(trkvers & 0xff));

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }
	/* done! */
	return LOAD_SUCCESS;
}

/* --------------------------------------------------------------------------------------------------------- */

/* IT displays some of these slightly differently
most notably "Only 100 patterns supported" which doesn't follow the general pattern,
and the channel limits (IT entirely refuses to save data in channels > 16 at all).
Also, the Adlib and sample count warnings of course do not exist in IT at all. */

enum {
	WARN_MAXPATTERNS,
	WARN_CHANNELVOL,
	WARN_LINEARSLIDES,
	WARN_SAMPLEVOL,
	WARN_LOOPS,
	WARN_SAMPLEVIB,
	WARN_INSTRUMENTS,
	WARN_PATTERNLEN,
	WARN_MAXCHANNELS,
	WARN_MAXPCM,
	WARN_MAXADLIB,
	WARN_PCMADLIBMIX,
	WARN_MUTED,
	WARN_NOTERANGE,
	WARN_VOLEFFECTS,
	WARN_MAXSAMPLES,

	MAX_WARN
};

static const char *s3m_warnings[] = {
	[WARN_MAXPATTERNS]  = "Over 100 patterns",
	[WARN_CHANNELVOL]   = "Channel volumes",
	[WARN_LINEARSLIDES] = "Linear slides",
	[WARN_SAMPLEVOL]    = "Sample volumes",
	[WARN_LOOPS]        = "Sustain and Ping Pong loops",
	[WARN_SAMPLEVIB]    = "Sample vibrato",
	[WARN_INSTRUMENTS]  = "Instrument functions",
	[WARN_PATTERNLEN]   = "Pattern lengths other than 64 rows",
	[WARN_MAXCHANNELS]  = "Data outside 32 channels",
	[WARN_MAXPCM]       = "Over 16 PCM channels",
	[WARN_MAXADLIB]     = "Over 9 Adlib channels",
	[WARN_PCMADLIBMIX]  = "Adlib and PCM in the same channel",
	[WARN_MUTED]        = "Data in muted channels",
	[WARN_NOTERANGE]    = "Notes outside the range C-1 to B-8",
	[WARN_VOLEFFECTS]   = "Extended volume column effects",
	[WARN_MAXSAMPLES]   = "Over 99 samples",

	[MAX_WARN]          = NULL
};


struct s3m_header {
	char title[28];
	char eof; // 0x1a
	char type; // 16
	uint16_t ordnum, smpnum, patnum; // ordnum should be even
	uint16_t flags, cwtv, ffi; // 0, 0x4nnn, 2 for unsigned
	char scrm[4]; // "SCRM"
	uint8_t gv, is, it, mv, uc, dp; // gv is half range of IT, uc should be 8/12/16, dp is 252
	uint16_t reserved; // extended version information is stored here
	uint32_t reserved2; // Impulse Tracker hides its edit timer here
	uint16_t reserved3; // high bits of extended version information
};

static int write_s3m_header(const struct s3m_header *hdr, disko_t *fp)
{
#define WRITE_VALUE(x) do { disko_write(fp, &hdr->x, sizeof(hdr->x)); } while (0)

	WRITE_VALUE(title);
	WRITE_VALUE(eof);
	WRITE_VALUE(type);
	disko_seek(fp, 2, SEEK_CUR);
	WRITE_VALUE(ordnum);
	WRITE_VALUE(smpnum);
	WRITE_VALUE(patnum);
	WRITE_VALUE(flags);
	WRITE_VALUE(cwtv);
	WRITE_VALUE(ffi);
	WRITE_VALUE(scrm);
	WRITE_VALUE(gv);
	WRITE_VALUE(is);
	WRITE_VALUE(it);
	WRITE_VALUE(mv);
	WRITE_VALUE(uc);
	WRITE_VALUE(dp);
	WRITE_VALUE(reserved);
	WRITE_VALUE(reserved2);
	WRITE_VALUE(reserved3);
	disko_seek(fp, 2, SEEK_CUR);

#undef WRITE_VALUE

	return 1;
}

static int write_s3m_pattern(disko_t *fp, song_t *song, int pat, uint8_t *chantypes, uint16_t *para_pat)
{
	int64_t start, end;
	uint8_t b, type;
	uint16_t w;
	int row, rows, chan;
	song_note_t out, *note;
	uint32_t warn = 0;

	if (csf_pattern_is_empty(song, pat)) {
		// easy!
		para_pat[pat] = 0;
		return 0;
	}

	if (song->pattern_size[pat] != 64) {
		warn |= 1 << WARN_PATTERNLEN;
	}
	rows = MIN(64, song->pattern_size[pat]);

	disko_align(fp, 16);

	start = disko_tell(fp);
	para_pat[pat] = bswapLE16(start >> 4);

	// write a bogus length for now...
	disko_putc(fp, 0);
	disko_putc(fp, 0);

	note = song->patterns[pat];
	for (row = 0; row < rows; row++) {
		for (chan = 0; chan < 32; chan++, note++) {
			out = *note;
			b = 0;

			if (song->channels[chan].flags & CHN_MUTE) {
				if (out.instrument || out.effect) {
					/* most players do in fact play data on muted channels, but that's
					wrong since ST3 doesn't. to eschew the problem, we'll just drop the
					data when writing (and complain) */
					warn |= 1 << WARN_MUTED;
					continue;
				}
			} else if ((song->flags & SONG_INSTRUMENTMODE)
				   && out.instrument && NOTE_IS_NOTE(out.note)) {
				song_instrument_t *ins = song->instruments[out.instrument];
				if (ins) {
					out.instrument = ins->sample_map[out.note - 1];
					out.note = ins->note_map[out.note - 1];
				}
			}

			/* Translate notes */
			if ((out.note > 0 && out.note <= 12) || (out.note >= 109 && out.note <= 120)) {
				// Octave 0/9 (or higher?)
				warn |= 1 << WARN_NOTERANGE;
				out.note = 255;
			} else if (out.note > 12 && out.note < 109) {
				// C-1 through B-8
				out.note -= 13;
				out.note = (out.note % 12) + ((out.note / 12) << 4);
				b |= 32;
			} else if (out.note == NOTE_CUT || out.note == NOTE_OFF) {
				// IT translates === to ^^^ when writing S3M files
				// (and more importantly, we load ^^^ as === in adlib-channels)
				out.note = 254;
				b |= 32;
			} else {
				// Nothing (or garbage values)
				out.note = 255;
			}

			if (out.instrument != 0) {
				if (song->samples[out.instrument].flags & CHN_ADLIB)
					type = S3I_TYPE_ADMEL;
				else if (song->samples[out.instrument].data != NULL)
					type = S3I_TYPE_PCM;
				else
					type = S3I_TYPE_NONE;
				if (type != S3I_TYPE_NONE) {
					if (chantypes[chan] == S3I_TYPE_NONE
					    || chantypes[chan] == S3I_TYPE_CONTROL) {
						chantypes[chan] = type;
					} else if (chantypes[chan] != type) {
						warn |= 1 << WARN_PCMADLIBMIX;
					}
				}

				b |= 32;
			}

			switch (out.voleffect) {
			case VOLFX_NONE:
				break;
			case VOLFX_VOLUME:
				b |= 64;
				break;
			default:
				warn |= 1 << WARN_VOLEFFECTS;
				break;
			}

			csf_export_s3m_effect(&out.effect, &out.param, 0);
			if (out.effect || out.param) {
				b |= 128;
			}

			// If there's an effect, don't allow the channel to be muted in the S3M file.
			// S3I_TYPE_CONTROL is an internal value indicating that the channel should get a
			// "junk" value (such as B1) that doesn't actually play.
			if (chantypes[chan] == S3I_TYPE_NONE && out.effect) {
				chantypes[chan] = S3I_TYPE_CONTROL;
			}

			if (!b)
				continue;
			b |= chan;

			// write it!
			disko_putc(fp, b);
			if (b & 32) {
				disko_putc(fp, out.note);
				disko_putc(fp, out.instrument);
			}
			if (b & 64) {
				disko_putc(fp, out.volparam);
			}
			if (b & 128) {
				disko_putc(fp, out.effect);
				disko_putc(fp, out.param);
			}
		}

		if (!(warn & (1 << WARN_MAXCHANNELS))) {
			/* if the flag is already set, there's no point in continuing to search for stuff */
			for (; chan < MAX_CHANNELS; chan++, note++) {
				if (!csf_note_is_empty(note)) {
					warn |= 1 << WARN_MAXCHANNELS;
					break;
				}
			}
		}

		note += MAX_CHANNELS - chan;

		disko_putc(fp, 0); /* end of row */
	}

	/* if the pattern was < 64 rows, pad it */
	for (; row < 64; row++) {
		disko_putc(fp, 0);
	}

	/* hop back and write the real length */
	end = disko_tell(fp);
	disko_seek(fp, start, SEEK_SET);
	w = bswapLE16(end - start);
	disko_write(fp, &w, 2);
	disko_seek(fp, end, SEEK_SET);

	return warn;
}

static int fixup_chantypes(song_channel_t *channels, uint8_t *chantypes)
{
	int warn = 0;
	int npcm = 0, nadmel = 0, nctrl = 0;
	int pcm = 0, admel = 0x10, junk = 0x20;
	int n;

	/*
	Value   Label           Value   Label           (20-2F => 10-1F with B instead of A)
	00      L1              10      A1
	01      L2              11      A2
	02      L3              12      A3
	03      L4              13      A4
	04      L5              14      A5
	05      L6              15      A6
	06      L7              16      A7
	07      L8              17      A8
	08      R1              18      A9
	09      R2              19      AB
	0A      R3              1A      AS
	0B      R4              1B      AT
	0C      R5              1C      AC
	0D      R6              1D      AH
	0E      R7              1E      ??
	0F      R8              1F      ??

	For the L1 R1 L2 R2 pattern: ((n << 3) | (n >> 1)) & 0xf

	PCM  * 16 = 00-0F
	Adlib * 9 = 10-18
	Remaining = 20-2F (nothing will be played, but effects are still processed)

	Try to make as many of the "control" channels PCM as possible.
	*/

	for (n = 0; n < 32; n++) {
		switch (chantypes[n]) {
		case S3I_TYPE_PCM:
			npcm++;
			break;
		case S3I_TYPE_ADMEL:
			nadmel++;
			break;
		case S3I_TYPE_CONTROL:
			nctrl++;
			break;
		}
	}

	if (npcm > 16) {
		npcm = 16;
		warn |= 1 << WARN_MAXPCM;
	}
	if (nadmel > 9) {
		nadmel = 9;
		warn |= 1 << WARN_MAXADLIB;
	}

	for (n = 0; n < 32; n++) {
		switch (chantypes[n]) {
		case S3I_TYPE_PCM:
			if (pcm <= 0x0f)
				chantypes[n] = pcm++;
			else
				chantypes[n] = junk++;
			break;
		case S3I_TYPE_ADMEL:
			if (admel <= 0x18)
				chantypes[n] = admel++;
			else
				chantypes[n] = junk++;
			break;
		case S3I_TYPE_NONE:
			if (channels[n].flags & CHN_MUTE) {
				chantypes[n] = 255; // (--)
				break;
			}
			// else fall through - attempt to honor unmuted channels.
		default:
			if (npcm < 16) {
				chantypes[n] = ((pcm << 3) | (pcm >> 1)) & 0xf;
				pcm++;
				npcm++;
			} else if (nadmel < 9) {
				chantypes[n] = admel++;
				nadmel++;
			} else if (chantypes[n] == S3I_TYPE_NONE) {
				chantypes[n] = 255; // (--)
			} else {
				chantypes[n] = junk++; // give up
			}
			break;
		}
		if (junk > 0x2f)
			junk = 0x19; // "overflow" to the adlib drums
	}

	return warn;
}


int fmt_s3m_save_song(disko_t *fp, song_t *song)
{
	struct s3m_header hdr = {0};
	int nord, nsmp, npat;
	int n, i;
	song_sample_t *smp;
	int64_t smphead_pos; /* where to write the sample headers */
	int64_t patptr_pos; /* where to write pattern pointers */
	int64_t pos; /* temp */
	uint16_t w;
	uint16_t para_pat[MAX_PATTERNS];
	uint32_t para_sdata[MAX_SAMPLES];
	uint8_t chantypes[32];
	uint32_t warn = 0;


	if (song->flags & SONG_INSTRUMENTMODE)
		warn |= 1 << WARN_INSTRUMENTS;
	if (song->flags & SONG_LINEARSLIDES)
		warn |= 1 << WARN_LINEARSLIDES;


	nord = csf_get_num_orders(song) + 1;
	// TECH.DOC says orders should be even. In practice it doesn't appear to matter (in fact IT doesn't
	// make the number even), but if the spec says...
	if (nord & 1)
		nord++;
	// see note in IT writer -- shouldn't clamp here, but can't save more than we're willing to load
	nord = CLAMP(nord, 2, MAX_ORDERS);

	nsmp = csf_get_num_samples(song); // ST3 always saves one sample
	if (!nsmp)
		nsmp = 1;

	if (nsmp > 99) {
		nsmp = 99;
		warn |= 1 << WARN_MAXSAMPLES;
	}

	npat = csf_get_num_patterns(song); // ST3 always saves one pattern
	if (!npat)
		npat = 1;

	if (npat > 100) {
		npat = 100;
		warn |= 1 << WARN_MAXPATTERNS;
	}

	log_appendf(5, " %d orders, %d samples, %d patterns", nord, nsmp, npat);

	/* this is used to identify what kinds of samples (pcm or adlib)
	are used on which channels, since it actually matters to st3 */
	memset(chantypes, S3I_TYPE_NONE, 32);


	memcpy(hdr.title, song->title, 25);
	hdr.eof = 0x1a;
	hdr.type = 16; // ST3 module (what else is there?!)
	hdr.ordnum = bswapLE16(nord);
	hdr.smpnum = bswapLE16(nsmp);
	hdr.patnum = bswapLE16(npat);
	hdr.flags = 0;
	hdr.cwtv = bswapLE16(0x4000 | ver_cwtv);
	hdr.ffi = bswapLE16(2); // format version; 1 = signed samples, 2 = unsigned
	memcpy(hdr.scrm, "SCRM", 4);
	hdr.gv = song->initial_global_volume / 2;
	hdr.is = song->initial_speed;
	hdr.it = song->initial_tempo;

	/* .S3M "MasterVolume" only supports 0x10 .. 0x7f,
	 * if we save 0x80, the schism max volume, it becomes zero in S3M.
	 * I didn't test to see what ScreamTracker does if a value below 0x10
	 * is loaded into it, but its UI prevents setting below 0x10.
	 * Just enforce both bounds here.
	 */
	hdr.mv = MIN(MAX(0x10, song->mixing_volume), 0x7f);
	if (!(song->flags & SONG_NOSTEREO))
		hdr.mv |= 128;
	hdr.uc = 16; // ultraclick (the "Waste GUS channels" option)
	hdr.dp = 252;
	hdr.reserved = bswapLE16(ver_reserved);
	hdr.reserved3 = bswapLE16(ver_reserved >> 16);

	/* Save the edit time in the reserved header, where
	 * Impulse Tracker also conveniently stores it */
	hdr.reserved2 = 0;

	for (size_t i = 0; i < song->histlen; i++)
		hdr.reserved2 += ms_to_dos_time(song->history[i].runtime);

	// 32-bit DOS tick count (tick = 1/18.2 second; 54945 * 18.2 = 999999 which is Close Enough)
	hdr.reserved2 += it_get_song_elapsed_dos_time(song);

	hdr.reserved2 = bswapLE32(hdr.reserved2);

	/* The sample data parapointers are 24+4 bits, whereas pattern data and sample headers are only 16+4
	bits -- so while the sample data can be written up to 268 MB within the file (starting at 0xffffff0),
	the pattern data and sample headers are restricted to the first 1 MB (starting at 0xffff0). In effect,
	this practically requires the sample data to be written last in the file, as it is entirely possible
	(and quite easy, even) to write more than 1 MB of sample data in a file.
	The "practical standard order" listed in TECH.DOC is sample headers, patterns, then sample data.
	Thus:
	    File header
	    Channel settings
	    Orderlist
	    Sample header pointers
	    Pattern pointers
	    Default pannings
	    Sample headers
	    Pattern data
	    Sample data
	*/

	write_s3m_header(&hdr, fp); // header
	disko_seek(fp, 32, SEEK_CUR); // channel settings (skipped for now)
	disko_write(fp, song->orderlist, nord); // orderlist

	/* sample header pointers
	because the sample headers are fixed-size, it's possible to determine where they will be written
	right now: the first sample will be at the start of the next 16-byte block after all the header
	stuff, and each subsequent sample starts 0x50 bytes after the previous one. */
	pos = smphead_pos = (0x60 + nord + 2 * (nsmp + npat) + 32 + 15) & ~15;
	for (n = 0; n < nsmp; n++) {
		w = bswapLE16(pos >> 4);
		disko_write(fp, &w, 2);
		pos += 0x50;
	}

	/* pattern pointers
	can't figure these out ahead of time since the patterns are variable length,
	but do make a note of where to seek later in order to write the values... */
	patptr_pos = disko_tell(fp);
	disko_seek(fp, 2 * npat, SEEK_CUR);

	/* channel pannings ... also not yet! */
	disko_seek(fp, 32, SEEK_CUR);

	/* skip ahead past the sample headers as well (what a pain) */
	disko_seek(fp, 0x50 * nsmp, SEEK_CUR);

	/* patterns -- finally omg we can write some data */
	for (n = 0; n < npat; n++)
		warn |= write_s3m_pattern(fp, song, n, chantypes, para_pat);

	/* sample data */
	for (n = 0, smp = song->samples + 1; n < nsmp; n++, smp++) {
		if ((smp->flags & CHN_ADLIB) || smp->data == NULL) {
			para_sdata[n] = 0;
			continue;
		}
		disko_align(fp, 16);
		para_sdata[n] = disko_tell(fp);
		csf_write_sample(fp, smp, SF_LE | SF_PCMU
			| ((smp->flags & CHN_16BIT) ? SF_16 : SF_8)
			| ((smp->flags & CHN_STEREO) ? SF_SS : SF_M),
			UINT32_MAX);
	}

	/* now that we're done adding stuff to the end of the file,
	go back and rewrite everything we skipped earlier.... */

	// channel types
	warn |= fixup_chantypes(song->channels, chantypes);
	disko_seek(fp, 0x40, SEEK_SET);
	disko_write(fp, chantypes, 32);

	// pattern pointers
	disko_seek(fp, patptr_pos, SEEK_SET);
	disko_write(fp, para_pat, 2 * npat);

	/* channel panning settings come after the pattern pointers...
	This produces somewhat left-biased panning values, but this is what IT does, and more importantly
	it's stable across repeated load/saves. (Hopefully.)
	Technically it is possible to squeeze out two "extra" values for hard-left and hard-right panning by
	writing a "disabled" pan value (omit the 0x20 bit, so it's presented as a dot in ST3) -- but some
	trackers, including MPT and older Schism Tracker versions, load such values as 16/48 rather than 0/64,
	so this would result in potentially inconsistent behavior and is therefore undesirable. */
	for (n = 0; n < 32; n++) {
		song_channel_t *ch = song->channels + n;
		uint8_t b;

		if (ch->volume != 64)
			warn |= 1 << WARN_CHANNELVOL;

		//mphack: channel panning range
		b = ((chantypes[n] & 0x7f) < 0x20)
			? ((ch->panning * 15) / 64)
			: 0;
		disko_putc(fp, b);
	}

	/* sample headers */
	disko_seek(fp, smphead_pos, SEEK_SET);
	for (n = 0, smp = song->samples + 1; n < nsmp; n++, smp++) {
		if (smp->global_volume != 64) {
			warn |= 1 << WARN_SAMPLEVOL;
		}
		if ((smp->flags & (CHN_LOOP | CHN_PINGPONGLOOP)) == (CHN_LOOP | CHN_PINGPONGLOOP)
		    || (smp->flags & CHN_SUSTAINLOOP)) {
			warn |= 1 << WARN_LOOPS;
		}
		if (smp->vib_depth != 0) {
			warn |= 1 << WARN_SAMPLEVIB;
		}
		s3i_write_header(fp, smp, para_sdata[n]);
	}

	/* announce all the things we broke */
	for (n = 0; n < MAX_WARN; n++) {
		if (warn & (1 << n))
			log_appendf(4, " Warning: %s unsupported in S3M format", s3m_warnings[n]);
	}

	return SAVE_SUCCESS;
}

