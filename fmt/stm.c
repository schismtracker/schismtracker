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

#include "sndfile.h"

/* --------------------------------------------------------------------- */

/* TODO: get more stm's and test this... one file's not good enough */

int fmt_stm_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        /* data[29] is the type: 1 = song, 2 = module (with samples) */
        if (!(length > 28 && data[28] == 0x1a && (data[29] == 1 || data[29] == 2)
              && (memcmp(data + 14, "!Scream!", 8) || memcmp(data + 14, "BMOD2STM", 8))))
                return false;

        /* I used to check whether it was a 'song' or 'module' and set the description
        accordingly, but it's fairly pointless information :) */
        file->description = "Scream Tracker 2";
        /*file->extension = str_dup("stm");*/
        file->type = TYPE_MODULE_MOD;
        file->title = calloc(21, sizeof(char));
        memcpy(file->title, data, 20);
        file->title[20] = 0;
        return true;
}

/* --------------------------------------------------------------------- */

#pragma pack(push, 1)
struct stm_sample {
        char name[12];
        uint8_t zero;
        uint8_t inst_disk; // lol disks
        uint16_t reserved;
        uint16_t length, loop_start, loop_end;
        uint8_t volume;
        uint8_t reserved2;
        uint16_t c5speed;
        uint32_t morejunk;
        uint16_t paragraphs; // what?
};
#pragma pack(pop)

static uint8_t stm_effects[16] = {
        CMD_NONE,               // .
        CMD_SPEED,              // A
        CMD_POSITIONJUMP,       // B
        CMD_PATTERNBREAK,       // C
        CMD_VOLUMESLIDE,        // D
        CMD_PORTAMENTODOWN,     // E
        CMD_PORTAMENTOUP,       // F
        CMD_TONEPORTAMENTO,     // G
        CMD_VIBRATO,            // H
        CMD_TREMOR,             // I
        CMD_ARPEGGIO,           // J
        // KLMNO can be entered in the editor but don't do anything
};

/* ST2 says at startup:
"Remark: the user ID is encoded in over ten places around the file!"
I wonder if this is interesting at all. */


static void load_stm_pattern(MODCOMMAND *note, slurp_t *fp)
{
        int row, chan;
        uint8_t v[4];

        for (row = 0; row < 64; row++, note += 64 - 4) {
                for (chan = 0; chan < 4; chan++, note++) {
                        slurp_read(fp, v, 4);
                        
                        // mostly copied from modplug...
                        if (v[0] < 251)
                                note->note = (v[0] >> 4) * 12 + (v[0] & 0xf) + 37;
                        note->instr = v[1] >> 3;
                        if (note->instr > 31)
                                note->instr = 0; // oops never mind, that was crap
                        note->vol = (v[1] & 0x7) + (v[2] >> 1); // I don't understand this line
                        if (note->vol <= 64)
                                note->volcmd = VOLCMD_VOLUME;
                        else
                                note->vol = 0;
                        note->param = v[3]; // easy!
                        
                        note->command = stm_effects[v[2] & 0xf];
                        // patch a couple effects up
                        switch (note->command) {
                        case CMD_SPEED:
                                // I don't know how Axx really works, but I do know that this
                                // isn't it. It does all sorts of mindbogglingly screwy things:
                                //      01 - very fast,
                                //      0F - very slow.
                                //      10 - fast again!
                                // I don't get it.
                                note->param >>= 4;
                                break;
                        case CMD_PATTERNBREAK:
                                note->param = (note->param & 0xf0) * 10 + (note->param & 0xf);
                                break;
                        case CMD_POSITIONJUMP:
                                // This effect is also very weird.
                                // Bxx doesn't appear to cause an immediate break -- it merely
                                // sets the next order for when the pattern ends (either by
                                // playing it all the way through, or via Cxx effect)
                                // I guess I'll "fix" it later...
                                break;
                        case CMD_TREMOR:
                                // this actually does something with zero values, and has no
                                // effect memory. which makes SENSE for old-effects tremor,
                                // but ST3 went and screwed it all up by adding an effect
                                // memory and IT followed that, and those are much more popular
                                // than STM so we kind of have to live with this effect being
                                // broken... oh well. not a big loss.
                                break;
                        default:
                                // Anything not listed above is a no-op if there's no value.
                                // (ST2 doesn't have effect memory)
                                if (!note->param)
                                        note->command = CMD_NONE;
                                break;
                        }
                }
        }
}

int fmt_stm_load_song(CSoundFile *song, slurp_t *fp, unsigned int lflags)
{
        char id[8];
        uint8_t tmp[4];
        int npat, n;

        slurp_seek(fp, 20, SEEK_SET);
        slurp_read(fp, id, 8);
        slurp_read(fp, tmp, 4);

        if (!(
                // this byte is guaranteed to be 0x1a, always...
                tmp[0] == 0x1a
                // from the doc:
                //      1 - song (contains no samples)
                //      2 - module (contains samples)
                // I'm not going to care about "songs".
                && tmp[1] == 2
                // and check the file tag -- but god knows why, it's case insensitive
                && (strncasecmp(id, "!Scream!", 8) == 0 || strncasecmp(id, "BMOD2STM", 8) == 0)
        )) {
                return LOAD_UNSUPPORTED;
        }
        // and the next two bytes are the tracker version.
        // (XXX should this care about BMOD2STM? what is that anyway?)
        sprintf(song->tracker_id, "Scream Tracker %d.%02x", tmp[2], tmp[3]);

        slurp_seek(fp, 0, SEEK_SET);
        slurp_read(fp, song->song_title, 20);
        song->song_title[20] = '\0';
        slurp_seek(fp, 12, SEEK_CUR); // skip the tag and stuff

        song->m_nDefaultSpeed = (slurp_getc(fp) >> 4) ?: 1;
        npat = slurp_getc(fp);
        song->m_nDefaultGlobalVolume = 2 * slurp_getc(fp);
        slurp_seek(fp, 13, SEEK_CUR); // junk

        if (npat > 64)
                return LOAD_FORMAT_ERROR;

        for (n = 1; n <= 31; n++) {
                struct stm_sample stmsmp;
                uint16_t blen;
                SONGSAMPLE *sample = song->Samples + n;

                slurp_read(fp, &stmsmp, sizeof(stmsmp));
                // the strncpy here is intentional -- ST2 doesn't show the '3' after the \0 bytes in the first
                // sample of pm_fract.stm, for example
                strncpy(sample->filename, stmsmp.name, 12);
                memcpy(sample->name, sample->filename, 12);
                blen = sample->nLength = bswapLE16(stmsmp.length);
                sample->nLoopStart = bswapLE16(stmsmp.loop_start);
                sample->nLoopEnd = bswapLE16(stmsmp.loop_end);
                sample->nC5Speed = bswapLE16(stmsmp.c5speed);
                sample->nVolume = stmsmp.volume * 4; //mphack
                if (sample->nLoopStart < blen
                    && sample->nLoopEnd <= blen
                    && sample->nLoopStart < sample->nLoopEnd) {
                        sample->uFlags |= CHN_LOOP;
                }
        }
        
        slurp_read(fp, song->Orderlist, 128);
        for (n = 0; n < 128; n++) {
                if (song->Orderlist[n] >= 64)
                        song->Orderlist[n] = ORDER_LAST;
        }
        
        if (lflags & LOAD_NOPATTERNS) {
                slurp_seek(fp, npat * 64 * 4 * 4, SEEK_CUR);
        } else {
                for (n = 0; n < npat; n++) {
                        song->Patterns[n] = csf_allocate_pattern(64, 64);
                        song->PatternSize[n] = song->PatternAllocSize[n] = 64;
                        load_stm_pattern(song->Patterns[n], fp);
                }
        }
        
        if (!(lflags & LOAD_NOSAMPLES)) {
                for (n = 1; n <= 31; n++) {
                        SONGSAMPLE *sample = song->Samples + n;
                        int align = (sample->nLength + 15) & ~15;

                        if (sample->nLength < 3) {
                                // Garbage?
                                sample->nLength = 0;
                        } else {
                                csf_read_sample(sample, SF_LE | SF_PCMS | SF_8 | SF_M,
                                        (const char *) (fp->data + fp->pos), sample->nLength);
                        }
                        slurp_seek(fp, align, SEEK_CUR);
                }
        }
        
        for (n = 0; n < 4; n++)
                song->Channels[n].nPan = ((n & 1) ? 64 : 0) * 4; //mphack
        for (; n < 64; n++)
                song->Channels[n].dwFlags |= CHN_MUTE;
        song->m_nStereoSeparation = 64;
        song->m_dwSongFlags = SONG_ITOLDEFFECTS | SONG_COMPATGXX;

        return LOAD_SUCCESS;
}

