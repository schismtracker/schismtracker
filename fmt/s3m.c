/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include "sndfile.h"

/* --------------------------------------------------------------------- */

int fmt_s3m_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 48 && memcmp(data + 44, "SCRM", 4) == 0))
                return false;

        file->description = "Scream Tracker 3";
        /*file->extension = str_dup("s3m");*/
        file->title = calloc(28, sizeof(char));
        memcpy(file->title, data, 27);
        file->title[27] = 0;
        file->type = TYPE_MODULE_S3M;
        return true;
}

/* --------------------------------------------------------------------------------------------------------- */

int fmt_s3m_load_song(CSoundFile *song, slurp_t *fp, unsigned int lflags)
{
	uint16_t nsmp, nord, npat;
	/* 'bleh' is just some temporary flags:
	 *     if (bleh & 1) samples stored in unsigned format
	 *     if (bleh & 2) load channel pannings
	 * (these are both generally true) */
	int bleh = 3;
	int n;
	MODCOMMAND *note;
	/* junk variables for reading stuff into */
	uint16_t tmp;
	uint8_t c;
	uint32_t tmplong;
	uint8_t b[4];
	/* parapointers */
	uint16_t para_smp[256];
	uint16_t para_pat[256];
	uint32_t para_sdata[256] = { 0 };
	SONGSAMPLE *sample;
	uint16_t trkvers;
	uint16_t flags;
	uint16_t special;
	uint32_t adlib = 0; // bitset
	int uc;
        const char *tid = NULL;

	/* check the tag */
	slurp_seek(fp, 44, SEEK_SET);
	slurp_read(fp, b, 4);
	if (memcmp(b, "SCRM", 4) != 0)
		return LOAD_UNSUPPORTED;

	/* read the title */
	slurp_rewind(fp);
	slurp_read(fp, song->song_title, 25);
	song->song_title[25] = 0;

	/* skip the last three bytes of the title, the supposed-to-be-0x1a byte,
	the tracker ID, and the two useless reserved bytes */
	slurp_seek(fp, 7, SEEK_CUR);

	slurp_read(fp, &nord, 2);
	slurp_read(fp, &nsmp, 2);
	slurp_read(fp, &npat, 2);
	nord = bswapLE16(nord);
	nsmp = bswapLE16(nsmp);
	npat = bswapLE16(npat);
	
	nord = MIN(nord, MAX_ORDERS);
	nsmp = MIN(nsmp, MAX_SAMPLES);
	npat = MIN(npat, MAX_PATTERNS);

	song->m_dwSongFlags = SONG_ITOLDEFFECTS;
	slurp_read(fp, &flags, 2);  /* flags (don't really care) */
	flags = bswapLE16(flags);
	slurp_read(fp, &trkvers, 2);
	trkvers = bswapLE16(trkvers);
	slurp_read(fp, &tmp, 2);  /* file format info */
	if (tmp == bswapLE16(1))
		bleh &= ~1;     /* signed samples (ancient s3m) */

	slurp_seek(fp, 4, SEEK_CUR); /* skip the tag */
	
	song->m_nDefaultGlobalVolume = slurp_getc(fp) << 1;
	song->m_nDefaultSpeed = slurp_getc(fp);
	song->m_nDefaultTempo = slurp_getc(fp);
	song->m_nSongPreAmp = slurp_getc(fp);
	if (song->m_nSongPreAmp & 0x80) {
		song->m_nSongPreAmp ^= 0x80;
	} else {
		song->m_dwSongFlags |= SONG_NOSTEREO;
	}
	uc = slurp_getc(fp); /* ultraclick removal (useless) */

	if (slurp_getc(fp) != 0xfc)
		bleh &= ~2;     /* stored pan values */

	slurp_seek(fp, 8, SEEK_CUR); // 8 unused bytes (XXX what do programs actually write for these?)
	slurp_read(fp, &special, 2); // field not used by st3
	special = bswapLE16(special);

	/* channel settings */
	for (n = 0; n < 32; n++) {
		c = slurp_getc(fp);
		if (c == 255) {
			song->Channels[n].nPan = 32;
			song->Channels[n].dwFlags = CHN_MUTE;
		} else {
			song->Channels[n].nPan = (c & 8) ? 48 : 16;
			if (c & 0x80) {
				c ^= 0x80;
				song->Channels[n].dwFlags = CHN_MUTE;
			}
			if (c >= 16 && c < 32)
				adlib |= 1 << n;
		}
		song->Channels[n].nVolume = 64;
	}
	for (; n < 64; n++) {
		song->Channels[n].nPan = 32;
		song->Channels[n].nVolume = 64;
		song->Channels[n].dwFlags = CHN_MUTE;
	}

	/* orderlist */
	slurp_read(fp, song->Orderlist, nord);
	memset(song->Orderlist + nord, 255, 256 - nord);

	/* load the parapointers */
	slurp_read(fp, para_smp, 2 * nsmp);
	slurp_read(fp, para_pat, 2 * npat);
#ifdef WORDS_BIGENDIAN
	swab(para_smp, para_smp, 2 * nsmp);
	swab(para_pat, para_pat, 2 * npat);
#endif

	/* default pannings */
	if (bleh & 2) {
		for (n = 0; n < 32; n++) {
			c = slurp_getc(fp);
			if (c & 0x20)
				song->Channels[n].nPan = ((c & 0xf) << 2) + 2;
		}
	}
	
	//mphack - fix the pannings
	for (n = 0; n < 64; n++)
		song->Channels[n].nPan *= 4;

	/* samples */
	for (n = 0, sample = song->Samples + 1; n < nsmp; n++, sample++) {
		uint8_t type;

		slurp_seek(fp, para_smp[n] << 4, SEEK_SET);

		type = slurp_getc(fp);
		slurp_read(fp, sample->filename, 12);
		sample->filename[12] = 0;

		slurp_read(fp, b, 3);
		slurp_read(fp, &tmplong, 4);
		if (type == 1) {
			// pcm sample
			para_sdata[n] = b[1] | (b[2] << 8) | (b[0] << 16);
			sample->nLength = bswapLE32(tmplong);
		} else if (type == 2) {
			// adlib
			return LOAD_UNSUPPORTED; // it IS supported, but just not with *this* loader... yet :)
		}
		slurp_read(fp, &tmplong, 4);
		sample->nLoopStart = bswapLE32(tmplong);
		slurp_read(fp, &tmplong, 4);
		sample->nLoopEnd = bswapLE32(tmplong);
		sample->nVolume = slurp_getc(fp) * 4; //mphack
		sample->nGlobalVol = 64;
		slurp_getc(fp);      /* unused byte */
		slurp_getc(fp);      /* packing info (never used) */
		c = slurp_getc(fp);  /* flags */
		if (c & 1)
			sample->uFlags |= CHN_LOOP;
		if (c & 4)
			sample->uFlags |= CHN_16BIT;
		// TODO stereo
		slurp_read(fp, &tmplong, 4);
		sample->nC5Speed = bswapLE32(tmplong);
		slurp_seek(fp, 12, SEEK_CUR);        /* wasted space */
		slurp_read(fp, sample->name, 25);
		sample->name[25] = 0;
		sample->nVibType = 0;
		sample->nVibSweep = 0;
		sample->nVibDepth = 0;
		sample->nVibRate = 0;
	}
	
	/* sample data */
	if (!(lflags & LOAD_NOSAMPLES)) {
		for (n = 0, sample = song->Samples + 1; n < nsmp; n++, sample++) {
			int8_t *ptr;
			uint32_t len = sample->nLength;
			int bps = 1;    /* bytes per sample (i.e. bits / 8) */

			if (!para_sdata[n] || !len)
				continue;

			slurp_seek(fp, para_sdata[n] << 4, SEEK_SET);
			if (sample->uFlags & CHN_16BIT)
				bps = 2;
			ptr = csf_allocate_sample(bps * len);
			slurp_read(fp, ptr, bps * len);
			sample->pSample = ptr;

			if (bleh & 1) {
				/* convert to signed */
				uint32_t pos = len;
				if (bps == 2)
					while (pos-- > 0)
						ptr[2 * pos + 1] ^= 0x80;
				else
					while (pos-- > 0)
						ptr[pos] ^= 0x80;
			}
#ifdef WORDS_BIGENDIAN
			if (bps == 2)
				swab(ptr, ptr, 2 * len);
#endif
		}
	}

	if (!(lflags & LOAD_NOPATTERNS)) {
		for (n = 0; n < npat; n++) {
			int row = 0;

			/* The +2 is because the first two bytes are the length of the packed
			data, which is superfluous for the way I'm reading the patterns. */
			slurp_seek(fp, (para_pat[n] << 4) + 2, SEEK_SET);

			song->Patterns[n] = csf_allocate_pattern(64, 64);

			while (row < 64) {
				uint8_t mask = slurp_getc(fp);
				uint8_t chn = (mask & 31);

				if (!mask) {
					/* done with the row */
					row++;
					continue;
				}
				note = song->Patterns[n] + 64 * row + chn;
				if (mask & 32) {
					/* note/instrument */
					note->note = slurp_getc(fp);
					note->instr = slurp_getc(fp);
					//if (note->instr > 99)
					//	note->instr = 0;
					switch (note->note) {
					default:
						// Note; hi=oct, lo=note
						note->note = (note->note >> 4) * 12 + (note->note & 0xf) + 12 + 1;
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
					note->volcmd = VOLCMD_VOLUME;
					note->vol = slurp_getc(fp);
					if (note->vol == 255) {
						note->volcmd = VOLCMD_NONE;
						note->vol = 0;
					} else if (note->vol > 64) {
						// some weirdly saved s3m?
						note->vol = 64;
					}
				}
				if (mask & 128) {
					note->command = slurp_getc(fp);
					note->param = slurp_getc(fp);
					csf_import_s3m_effect(note, 0);
				}
				/* ... next note, same row */
			}
		}
	}

	/* MPT identifies as ST3.20 in the trkvers field, but it puts zeroes for the 'special' field, only ever
	 * sets flags 0x10 and 0x40, writes multiples of 16 orders, and writes zero into the ultraclick removal
	 * field. (ST3 always puts either 8, 12, or 16 there) */
	if (trkvers == 0x1320) {
		if (special == 0 && uc == 0 && (flags & ~0x50) == 0 && (nord % 16) == 0) {
			tid = "Modplug Tracker";
		} else if (uc != 8 && uc != 12 && uc != 16) {
			// sure isn't scream tracker
			tid = "Unknown tracker";
		}
	}
	if (!tid) {
		switch (trkvers >> 12) {
		case 1:
			tid = "Scream Tracker %d.%02x";
			break;
		case 2:
			tid = "Imago Orpheus %d.%02x";
			break;
		case 3:
			if (trkvers == 0x3216)
				tid = "Impulse Tracker 2.14v3";
			else if (trkvers == 0x3217)
				tid = "Impulse Tracker 2.14v5";
			else
				tid = "Impulse Tracker %d.%02x";
			break;
		case 4:
			// we don't really bump the version properly, but let's show it anyway
			tid = "Schism Tracker %d.%02x";
			break;
		case 5:
			tid = "OpenMPT %d.%02x";
			break;
		}
	}
	if (tid)
		sprintf(song->tracker_id, tid, (trkvers & 0xf00) >> 8, trkvers & 0xff);

//	if (ferror(fp)) {
//		return LOAD_FILE_ERROR;
//	}
	/* done! */
	return LOAD_SUCCESS;
}

