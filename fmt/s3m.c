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
#include "version.h"

#include "sndfile.h"

/* --------------------------------------------------------------------- */

int fmt_s3m_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 48 && memcmp(data + 44, "SCRM", 4) == 0))
                return 0;

        file->description = "Scream Tracker 3";
        /*file->extension = str_dup("s3m");*/
        file->title = calloc(28, sizeof(char));
        memcpy(file->title, data, 27);
        file->title[27] = 0;
        file->type = TYPE_MODULE_S3M;
        return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

#define S3M_UNSIGNED 1
#define S3M_CHANPAN 2 // the FC byte

enum {
        S3I_TYPE_NONE = 0,
        S3I_TYPE_PCM = 1,
        S3I_TYPE_ADMEL = 2,
};

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
        /* parapointers */
        uint16_t para_smp[256];
        uint16_t para_pat[256];
        uint32_t para_sdata[256] = { 0 };
        uint32_t smp_flags[256] = { 0 };
        song_sample_t *sample;
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
        song->initial_speed = slurp_getc(fp);
        song->initial_tempo = slurp_getc(fp);
        song->mixing_volume = slurp_getc(fp);
        if (song->mixing_volume & 0x80) {
                song->mixing_volume ^= 0x80;
        } else {
                song->flags |= SONG_NOSTEREO;
        }
        uc = slurp_getc(fp); /* ultraclick removal (useless) */

        if (slurp_getc(fp) != 0xfc)
                misc &= ~S3M_CHANPAN;     /* stored pan values */

        slurp_seek(fp, 8, SEEK_CUR); // 8 unused bytes (XXX what do programs actually write for these?)
        slurp_read(fp, &special, 2); // field not used by st3
        special = bswapLE16(special);

        /* channel settings */
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
                c = slurp_getc(fp);
                if (c & 0x80) {
                        song->channels[n].flags |= CHN_MUTE;
                        c &= ~0x80;
                }
                if (c < 0x08) {
                        // L1-L8
                        song->channels[n].panning = 16;
                } else if (c < 0x10) {
                        // R1-R8
                        song->channels[n].panning = 48;
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

        /* orderlist */
        slurp_read(fp, song->orderlist, nord);
        memset(song->orderlist + nord, 255, 256 - nord);

        /* load the parapointers */
        slurp_read(fp, para_smp, 2 * nsmp);
        slurp_read(fp, para_pat, 2 * npat);
#ifdef WORDS_BIGENDIAN
        swab(para_smp, para_smp, 2 * nsmp);
        swab(para_pat, para_pat, 2 * npat);
#endif

        /* default pannings */
        if (misc & S3M_CHANPAN) {
                for (n = 0; n < 32; n++) {
                        c = slurp_getc(fp);
                        if (c & 0x20)
                                song->channels[n].panning = ((c & 0xf) << 2) + 2;
                }
        }

        //mphack - fix the pannings
        for (n = 0; n < 64; n++)
                song->channels[n].panning *= 4;

        /* samples */
        for (n = 0, sample = song->samples + 1; n < nsmp; n++, sample++) {
                uint8_t type;

                slurp_seek(fp, para_smp[n] << 4, SEEK_SET);

                type = slurp_getc(fp);
                slurp_read(fp, sample->filename, 12);
                sample->filename[12] = 0;

                slurp_read(fp, b, 3); // data pointer for pcm, irrelevant otherwise
                switch (type) {
                case S3I_TYPE_PCM:
                        para_sdata[n] = b[1] | (b[2] << 8) | (b[0] << 16);
                        if (para_sdata[n]) {
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
                                break;
                        }
                        // else fall through -- it's a blank sample

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
                slurp_seek(fp, 12, SEEK_CUR);        /* wasted space */
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
                        uint32_t len;

                        if (!sample->length || !para_sdata[n])
                                continue;

                        slurp_seek(fp, para_sdata[n] << 4, SEEK_SET);
                        len = csf_read_sample(sample, smp_flags[n], fp->data + fp->pos, fp->length - fp->pos);
                        slurp_seek(fp, len, SEEK_CUR);
                }
        }

        if (!(lflags & LOAD_NOPATTERNS)) {
                for (n = 0; n < npat; n++) {
                        int row = 0;

                        /* The +2 is because the first two bytes are the length of the packed
                        data, which is superfluous for the way I'm reading the patterns. */
                        slurp_seek(fp, (para_pat[n] << 4) + 2, SEEK_SET);

                        song->patterns[n] = csf_allocate_pattern(64);

                        while (row < 64) {
                                uint8_t mask = slurp_getc(fp);
                                uint8_t chn = (mask & 31);

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
                                        } else if (note->volparam > 64) {
                                                // some weirdly saved s3m?
                                                note->volparam = 64;
                                        }
                                }
                                if (mask & 128) {
                                        note->effect = slurp_getc(fp);
                                        note->param = slurp_getc(fp);
                                        csf_import_s3m_effect(note, 0);
                                        if (note->effect == FX_S3MCMDEX) {
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
                                                }
                                        }
                                }
                                /* ... next note, same row */
                        }
                }
        }

        /* MPT identifies as ST3.20 in the trkvers field, but it puts zeroes for the 'special' field, only ever
         * sets flags 0x10 and 0x40, writes multiples of 16 orders, always saves channel pannings, and writes
         * zero into the ultraclick removal field. (ST3 always puts either 8, 12, or 16 there).
         * Velvet Studio also pretends to be ST3, but writes zeroes for 'special'. ultraclick, and flags, and
         * does NOT save channel pannings. Also, it writes a fairly recognizable LRRL pattern for the channels,
         * but I'm not checking that. (yet?) */
        if (trkvers == 0x1320) {
                if (special == 0 && uc == 0 && (flags & ~0x50) == 0
                    && misc == (S3M_UNSIGNED | S3M_CHANPAN) && (nord % 16) == 0) {
                        tid = "Modplug Tracker";
                } else if (special == 0 && uc == 0 && flags == 0 && misc == (S3M_UNSIGNED)) {
                        tid = "Velvet Studio";
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
                        if (trkvers <= 0x3214) {
                                tid = "Impulse Tracker %d.%02x";
                        } else {
                                tid = NULL;
                                sprintf(song->tracker_id, "Impulse Tracker 2.14p%d", trkvers - 0x3214);
                        }
                        break;
                case 4:
                        tid = NULL;
                        strcpy(song->tracker_id, "Schism Tracker ");
                        ver_decode_cwtv(trkvers, song->tracker_id + strlen(song->tracker_id));
                        break;
                case 5:
                        tid = "OpenMPT %d.%02x";
                        break;
                }
        }
        if (tid)
                sprintf(song->tracker_id, tid, (trkvers & 0xf00) >> 8, trkvers & 0xff);

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }
        /* done! */
        return LOAD_SUCCESS;
}

