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
#include "it.h" /* for feature_check_blahblah */
#include "fmt.h"
#include "song.h"
#include "tables.h"

#include <stdint.h>

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)
typedef struct mtm_header {
	char filever[4]; /* M T M \x10 */
	char title[20]; /* asciz */
	uint16_t ntracks;
	uint8_t last_pattern;
	uint8_t last_order; /* songlength - 1 */
	uint16_t msglen;
	uint8_t nsamples;
	uint8_t flags; /* always 0 */
	uint8_t rows; /* prob. 64 */
	uint8_t nchannels;
	uint8_t panpos[32];
} mtm_header_t;

typedef struct mtm_sample {
	char name[22];
	uint32_t length, loop_start, loop_end;
	uint8_t finetune, volume, flags;
} mtm_sample_t;
#pragma pack(pop)

/* --------------------------------------------------------------------- */

int fmt_mtm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 24 && memcmp(data, "MTM", 3) == 0))
                return false;

        file->description = "MultiTracker Module";
        /*file->extension = str_dup("mtm");*/
        file->title = calloc(21, sizeof(char));
        memcpy(file->title, data + 4, 20);
        file->title[20] = 0;
        file->type = TYPE_MODULE_MOD;
        return true;
}

/* --------------------------------------------------------------------------------------------------------- */

static void mtm_unpack_track(const uint8_t *b, MODCOMMAND *note, int rows)
{
        int n;

        for (n = 0; n < rows; n++, note++, b += 3) {
                note->note = ((b[0] & 0xfc) ? ((b[0] >> 2) + 36 + 1) : NOTE_NONE);
                note->instr = ((b[0] & 0x3) << 4) | (b[1] >> 4);
                note->volcmd = VOLCMD_NONE;
                note->vol = 0;
                note->command = b[1] & 0xf;
                note->param = b[2];
		/* From mikmod: volume slide up always overrides slide down */
		if (note->command == 0xa && (note->param & 0xf0))
			note->param &= 0xf0;
                csf_import_mod_effect(note, 0);
        }
}

int fmt_mtm_load_song(CSoundFile *song, slurp_t *fp, unsigned int lflags)
{
        uint8_t b[192];
        uint16_t ntrk, nchan, nord, npat, nsmp;
        uint16_t comment_len;
        int n, pat, chan, smp, rows;
        MODCOMMAND *note;
        uint16_t tmp;
        uint32_t tmplong;
        MODCOMMAND **trackdata, *tracknote;
	SONGSAMPLE *sample;

        slurp_read(fp, b, 3);
        if (memcmp(b, "MTM", 3) != 0)
                return LOAD_UNSUPPORTED;
        n = slurp_getc(fp);
        sprintf(song->tracker_id, "MultiTracker %d.%d", n >> 4, n & 0xf);
        slurp_read(fp, song->song_title, 20);
        song->song_title[20] = 0;
        slurp_read(fp, &ntrk, 2);
        ntrk = bswapLE16(ntrk);
        npat = slurp_getc(fp);
        nord = slurp_getc(fp) + 1;

        slurp_read(fp, &comment_len, 2);
        comment_len = bswapLE16(comment_len);
        if (comment_len)
        	return LOAD_UNSUPPORTED; // we don't do comments yet, so give this to modplug
        nsmp = slurp_getc(fp);
        slurp_getc(fp); /* attribute byte (unused) */
        rows = slurp_getc(fp); /* beats per track (translation: number of rows in every pattern) */
	if (rows != 64) {
		printf("TODO: test this file with other players (beats per track != 64)");
	}
        nchan = slurp_getc(fp);
        for (n = 0; n < 32; n++) {
                song->Channels[n].nPan = SHORT_PANNING[slurp_getc(fp) & 0xf];
                song->Channels[n].nPan *= 4; //mphack
	}
	for (n = nchan; n < MAX_CHANNELS; n++)
                song->Channels[n].dwFlags = CHN_MUTE;

        for (n = 1, sample = song->Samples + 1; n <= nsmp; n++, sample++) {
                slurp_read(fp, sample->name, 22);
                sample->name[22] = 0;
                slurp_read(fp, &tmplong, 4);
                sample->nLength = bswapLE32(tmplong);
                slurp_read(fp, &tmplong, 4);
                sample->nLoopStart = bswapLE32(tmplong);
                slurp_read(fp, &tmplong, 4);
                sample->nLoopEnd = bswapLE32(tmplong);
                if ((sample->nLoopEnd - sample->nLoopStart) > 2) {
                        sample->uFlags |= CHN_LOOP;
                } else {
                        /* Both Impulse Tracker and Modplug do this */
                        sample->nLoopStart = 0;
                        sample->nLoopEnd = 0;
                }
                song->Samples[n].nC5Speed = MOD_FINETUNE(slurp_getc(fp));
                sample->nVolume = slurp_getc(fp);
                sample->nVolume *= 4; //mphack
                sample->nGlobalVol = 64;
                if (slurp_getc(fp) & 1) {
			printf("TODO: double check 16 bit sample loading");
                        sample->uFlags |= CHN_16BIT;
			sample->nLength >>= 1;
			sample->nLoopStart >>= 1;
			sample->nLoopEnd >>= 1;
		}
		song->Samples[n].nVibType = 0;
		song->Samples[n].nVibSweep = 0;
		song->Samples[n].nVibDepth = 0;
		song->Samples[n].nVibRate = 0;
	}

        /* orderlist */
        slurp_read(fp, song->Orderlist, 128);
	memset(song->Orderlist + nord, ORDER_LAST, MAX_ORDERS - nord);

        /* tracks */
        trackdata = calloc(ntrk, sizeof(MODCOMMAND *));
        for (n = 0; n < ntrk; n++) {
                slurp_read(fp, b, 3 * rows);
                trackdata[n] = calloc(rows, sizeof(MODCOMMAND));
                mtm_unpack_track(b, trackdata[n], rows);
        }

        /* patterns */
        for (pat = 0; pat <= npat; pat++) {
                song->Patterns[pat] = csf_allocate_pattern(MAX(rows, 32), 64);
                song->PatternSize[pat] = song->PatternAllocSize[pat] = 64;
                tracknote = trackdata[n];
                for (chan = 0; chan < 32; chan++) {
                        slurp_read(fp, &tmp, 2);
                        tmp = bswapLE16(tmp);
                        if (tmp == 0)
                                continue;
                        note = song->Patterns[pat] + chan;
                        tracknote = trackdata[tmp - 1];
                        for (n = 0; n < rows; n++, tracknote++, note += MAX_CHANNELS)
                                *note = *tracknote;
                }
		if (rows < 32) {
			/* stick a pattern break on the first channel with an empty effect column
			 * (XXX don't do this if there's already one in another column) */
			note = song->Patterns[pat] + 64 * (rows - 1);
			while (note->command || note->param)
				note++;
			note->command = CMD_PATTERNBREAK;
		}
        }

        /* free willy */
        for (n = 0; n < ntrk; n++)
                free(trackdata[n]);
        free(trackdata);

        /* just skip the comment (TODO) */
        slurp_seek(fp, comment_len, SEEK_CUR);

        /* sample data */
        if (!(lflags & LOAD_NOSAMPLES)) {
		for (smp = 1; smp <= nsmp; smp++) {
		        int8_t *ptr;
		        int bps = 1;    /* bytes per sample (i.e. bits / 8) */

		        if (song->Samples[smp].nLength == 0)
		                continue;
		        if (song->Samples[smp].uFlags & CHN_16BIT)
		                bps = 2;
		        ptr = csf_allocate_sample(bps * song->Samples[smp].nLength);
		        slurp_read(fp, ptr, bps * song->Samples[smp].nLength);
		        song->Samples[smp].pSample = ptr;

		        /* convert to signed */
		        n = song->Samples[smp].nLength;
		        while (n-- > 0)
		                ptr[n] += 0x80;
		}
	}

        /* set the rest of the stuff */
        song->m_dwSongFlags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

//	if (ferror(fp)) {
//		return LOAD_FILE_ERROR;
//	}

        /* done! */
        return LOAD_SUCCESS;
}

/* --------------------------------------------------------------------- */
/* FIXME: these should not be here. also, make less stupid */

static void write_sample_u16(diskwriter_driver_t *fp, void *v, unsigned int length)
{
	unsigned int n;
	uint16_t *o = malloc(length * 2);
	memcpy(o, v, length * 2);
	for (n = 0; n < length; n++)
		o[n] ^= 32768;
	fp->o(fp, (void *) o, length * 2);
	free(o);
}

static void write_sample_u8(diskwriter_driver_t *fp, void *v, unsigned int length)
{
	unsigned int n;
	uint8_t *o = malloc(length);
	memcpy(o, v, length);
	for (n = 0; n < length; n++)
		o[n] ^= 128;
	fp->o(fp, o, length);
	free(o);
}

/* --------------------------------------------------------------------- */

typedef struct track {
	uint8_t data[192];
	uint_fast16_t id;
	struct track *next;
} track_t;


static track_t *mtm_track(song_note *note, int rows)
{
	int saw_data = 0;
	uint8_t *d;
	track_t *trk;
	
	trk = malloc(sizeof(track_t));
	d = trk->data;
	trk->next = NULL;

	if (rows > 64)
		rows = 64;

	while (rows--) {
		/* pack it in */
		uint8_t n = note->note, i = note->instrument, e = 0, p = note->parameter;
		
		/* ? */
		if (n > 36 && n < 101)
			n -= 37;
		else
			n = 0;

		switch (get_effect_char(note->effect)) {
		default:	p = 0;				break;
		case 'J':	e = 0x0;			break;
		case 'F':	e = 0x1;			break;
		case 'E':	e = 0x2;			break;
		case 'G':	e = 0x3;			break;
		case 'H':	e = 0x4;			break;
		case 'L':	e = 0x5;			break;
		case 'K':	e = 0x6;			break;
		case 'R':	e = 0x7;			break;
		case 'X':	e = 0x8; p >>= 1;		break;
		case 'O':	e = 0x9;			break;
		case 'D':	e = 0xa;			break;
		case 'B':	e = 0xb;			break;
		case '!':	e = 0xc;			break; /* blah */
		case 'C':	e = 0xd; /* XXX decimal? */	break;
		case 'A':	e = 0xf; p = MIN(p, 0x1f);	break; /* XXX check this */
		case 'T':	e = 0xf; p = MAX(p, 0x20);	break; /* XXX check this */

		case 'Q':	e = 0xe; p = 0x90 | (p & 0xf);	break;
		case 'S':	e = 0xe;
			switch (p >> 4) {
			default:	e = p = 0;			break;
			case 8: case 0xc: case 0xd: case 0xe: /* ok */	break;
			case 3:		p = 0x40 | (p & 0xf);		break;
			case 4:		p = 0x70 | (p & 0xf);		break;
			case 0xb:	p = 0x60 | (p & 0xf);		break;
			case 9:
				if (p == 0x91) {
					e = 0x8;
					p = 0xa4;
				} else {
					e = p = 0;
				}
				break;
			}
			break;
		}

		if (!e && !p) {
			switch (note->volume_effect) {
			case VOL_EFFECT_VOLUME:
				e = 0xc;
				p = note->volume;
				break;
			case VOL_EFFECT_PANNING:
				e = 0x8;
				p = note->volume * 255 / 64;
			default:
				/* oh well */
				break;
			}
		}

		saw_data |= n || i || e || p;

		d[0] = (n << 2) | ((i >> 4) & 0x3);
		d[1] = (i << 4) | e;
		d[2] = p;
		d += 3;
		note += 64;
	}

	if (saw_data)
		return trk;
	free(trk);
	return NULL;
}

static track_t *link_track(track_t *head, track_t *newtrk)
{
	while (head->next) {
		if (memcmp(head->next->data, newtrk->data, 192) == 0) {
			free(newtrk);
			return head->next;
		}
		head = head->next;
	}
	head->next = newtrk;
	return newtrk;
}

static int c5speed_to_finetune(int c5speed)
{
	int n;
	for (n = 0; n < 16; n++)
		if (c5speed <= S3MFineTuneTable[n])
			break;
	return (n + 8) % 16;
}




/* FIXME why are the save_song functions prototyped like this? ugh */
void fmt_mtm_save_song(diskwriter_driver_t *fp)
{
	char *t;
	int n, c, rows, msglen;
	mtm_header_t hdr;
	song_note *pat;
	track_t tracks;
	track_t *trk, *trk2;
	uint16_t *seq;
	uint8_t ord[128];
	uint8_t *o;
	song_sample *ss;
	mtm_sample_t smp;

	feature_check_instruments("MTM", 0, 0);
	feature_check_samples("MTM", 63, SAMP_LOOP | SAMP_16_BIT);
	feature_check_notes("MTM", 12, 96, 0, 63, ".vp", ".ABCDEFGHJKLOQRSTX!");


	memcpy(hdr.filever, "MTM\x10", 4);

	memset(hdr.title, 0, 20);
	strncpy(hdr.title, song_get_title(), 20);

	// pack the tracks and get the counts
	hdr.ntracks = 0;
	hdr.nchannels = 0;
	hdr.last_pattern = song_get_num_patterns();
	tracks.next = NULL;
	seq = calloc(32 * (hdr.last_pattern + 1), 2);
	for (n = 0; n <= hdr.last_pattern; n++) {
		rows = song_get_pattern(n, &pat);
		for (c = 0; c < 32; c++) {
			if (song_get_channel(c)->flags & CHN_MUTE)
				continue;
			trk = mtm_track(pat + c, rows);
			if (!trk)
				continue;
			hdr.nchannels = MAX(hdr.nchannels, c + 1);
			trk2 = link_track(&tracks, trk);
			if (trk2 == trk)
				trk2->id = ++hdr.ntracks;
			seq[32 * n + c] = bswapLE16(trk2->id);
		}
	}
	/* be nice to the big indians */
	hdr.ntracks = bswapLE16(hdr.ntracks);

	o = song_get_orderlist();
	for (n = 0; n < 128; n++) {
		if (o[n] > 199)
			break;
		ord[n] = o[n];
	}
	hdr.last_order = MAX(0, n - 1);

	msglen = 0; /* TODO: care */
	hdr.msglen = bswapLE16(msglen);
	for (n = 63; song_sample_is_empty(n); n--) {
		/* nothing */
	}
	hdr.nsamples = n + 1;
	hdr.flags = 0;
	/* we really *could* support patterns with other than 64 rows, however the format doesn't support more
	than 64, plus all of the patterns have to be the same length anyway. and besides, it always writes the
	whole 64 rows in the file, so it's really not very useful to implement.
	(unless the numbers in the spec are merely based on the fact that mmedit only supported 64 rows?
	maybe i should investigate how other players handle this number changing.) */
	hdr.rows = 64;

	for (n = 0; n < 32; n++) {
		printf("%2d  %2d\n", n, song_get_channel(n)->panning);
		hdr.panpos[n] = (song_get_channel(n)->panning * 15 / 256) & 0xf; /* XXX modplug */
	}

	/* yay, we can write the header now */
	fp->o(fp, (void *) &hdr, sizeof(hdr));
	
	/* sampletime */
	for (n = 1; n <= hdr.nsamples; n++) {
		ss = song_get_sample(n, &t);
		
		smp.flags = (ss->flags & SAMP_16_BIT) ? 1 : 0;
		/* FIXME the spec says 'bytes' -- I think this means we need
		to adjust the length and loop data for 16 bit samples... */
		if (ss->flags & SAMP_LOOP) {
			smp.loop_start = bswapLE32(ss->loop_start);
			smp.loop_end = bswapLE32(ss->loop_end);
		} else {
			smp.loop_start = smp.loop_end = 0;
		}
		memset(smp.name, 0, 22);
		strncpy(smp.name, t, 22);
		smp.length = ss->data ? ss->length : 0;
		smp.length = bswapLE32(smp.length);
		smp.volume = ss->volume / 4; /* XXX modplug hack */
		smp.finetune = c5speed_to_finetune(ss->speed);
		fp->o(fp, (void *) &smp, sizeof(smp));
	}
	
	fp->o(fp, ord, sizeof(ord));

	trk2 = NULL;
	for (trk = tracks.next; trk; trk = trk->next) {
		fp->o(fp, trk->data, sizeof(trk->data));
		free(trk2);
		trk2 = trk;
	}
	fp->o(fp, (void *) seq, 2 * 32 * (hdr.last_pattern + 1));

	free(seq);

	/* TODO: stupidly-formatted message. should be 20 rows of exactly 40 chars
	   each, and \0-padded (plus, at least one \0 at the end of each line) */
	
	for (n = 1; n <= hdr.nsamples; n++) {
		ss = song_get_sample(n, &t);
		if (ss->length && ss->data) {
			/* butt */
			if (ss->flags & SAMP_16_BIT) {
				write_sample_u16(fp, ss->data, ss->length);
			} else {
				write_sample_u8(fp, ss->data, ss->length);
			}
		}
	}
}

