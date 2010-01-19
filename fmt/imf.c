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
#include "log.h"
#include "fmt.h"

#include "sndfile.h"

/* --------------------------------------------------------------------- */

int fmt_imf_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 64 && memcmp(data + 60, "IM10", 4) == 0))
                return 0;

        file->description = "Imago Orpheus";
        /*file->extension = str_dup("imf");*/
        file->title = calloc(32, sizeof(char));
        memcpy(file->title, data, 32);
        file->title[32] = 0;
        file->type = TYPE_MODULE_IT;
        return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

#pragma pack(push, 1)
struct imf_channel {
        char name[12];          /* Channelname (ASCIIZ-String, max 11 chars) */
        uint8_t chorus;         /* Default chorus */
        uint8_t reverb;         /* Default reverb */
        uint8_t panning;        /* Pan positions 00-FF */
        uint8_t status;         /* Channel status: 0 = enabled, 1 = mute, 2 = disabled (ignore effects!) */
};


struct imf_header {
        char title[32];         /* Songname (ASCIIZ-String, max. 31 chars) */
        uint16_t ordnum;        /* Number of orders saved */
        uint16_t patnum;        /* Number of patterns saved */
        uint16_t insnum;        /* Number of instruments saved */
        uint16_t flags;         /* Module flags (&1 => linear) */
        uint8_t unused1[8];
        uint8_t tempo;          /* Default tempo (Axx, 1..255) */
        uint8_t bpm;            /* Default beats per minute (BPM) (Txx, 32..255) */
        uint8_t master;         /* Default mastervolume (Vxx, 0..64) */
        uint8_t amp;            /* Amplification factor (mixing volume, 4..127) */
        uint8_t unused2[8];
        char im10[4];           /* 'IM10' */
        struct imf_channel channels[32]; /* Channel settings */
        uint8_t orderlist[256]; /* Order list (0xff = +++; blank out anything beyond ordnum) */
};

enum {
        IMF_ENV_VOL = 0,
        IMF_ENV_PAN = 1,
        IMF_ENV_FILTER = 2,
};

struct imf_env {
        uint8_t points;         /* Number of envelope points */
        uint8_t sustain;        /* Envelope sustain point */
        uint8_t loop_start;     /* Envelope loop start point */
        uint8_t loop_end;       /* Envelope loop end point */
        uint8_t flags;          /* Envelope flags */
        uint8_t unused[3];
};

struct imf_envnodes {
        uint16_t tick;
        uint16_t value;
};

struct imf_instrument {
        char name[32];          /* Inst. name (ASCIIZ-String, max. 31 chars) */
        uint8_t map[120];       /* Multisample settings */
        uint8_t unused[8];
        struct imf_envnodes nodes[3][16];
        struct imf_env env[3];
        uint16_t fadeout;       /* Fadeout rate (0...0FFFH) */
        uint16_t smpnum;        /* Number of samples in instrument */
        char ii10[4];           /* 'II10' */
};

struct imf_sample {
        char name[13];          /* Sample filename (12345678.ABC) */
        uint8_t unused1[3];
        uint32_t length;        /* Length */
        uint32_t loop_start;    /* Loop start */
        uint32_t loop_end;      /* Loop end */
        uint32_t c5speed;       /* Samplerate */
        uint8_t volume;         /* Default volume (0..64) */
        uint8_t panning;        /* Default pan (00h = Left / 80h = Middle) */
        uint8_t unused2[14];
        uint8_t flags;          /* Sample flags */
        uint8_t unused3[5];
        uint16_t ems;           /* Reserved for internal usage */
        uint32_t dram;          /* Reserved for internal usage */
        char is10[4];           /* 'IS10' */
};
#pragma pack(pop)


static uint8_t imf_efftrans[] = {
        CMD_NONE,
        CMD_SPEED, // 0x01 1xx Set Tempo
        CMD_TEMPO, // 0x02 2xx Set BPM
        CMD_TONEPORTAMENTO, // 0x03 3xx Tone Portamento                  (*)
        CMD_TONEPORTAVOL, // 0x04 4xy Tone Portamento + Volume Slide   (*)
        CMD_VIBRATO, // 0x05 5xy Vibrato                          (*)
        CMD_VIBRATOVOL, // 0x06 6xy Vibrato + Volume Slide           (*)
        CMD_FINEVIBRATO, // 0x07 7xy Fine Vibrato                     (*)
        CMD_TREMOLO, // 0x08 8xy Tremolo                          (*)
        CMD_ARPEGGIO, // 0x09 9xy Arpeggio                         (*)
        CMD_PANNING, // 0x0A Axx Set Pan Position
        CMD_PANNINGSLIDE, // 0x0B Bxy Pan Slide                        (*)
        CMD_VOLUME, // 0x0C Cxx Set Volume
        CMD_VOLUMESLIDE, // 0x0D Dxy Volume Slide                     (*)
        CMD_VOLUMESLIDE, // 0x0E Exy Fine Volume Slide                (*)
        CMD_S3MCMDEX, // 0x0F Fxx Set Finetune
        CMD_NOTESLIDEUP, // 0x10 Gxy Note Slide Up                    (*)
        CMD_NOTESLIDEDOWN, // 0x11 Hxy Note Slide Down                  (*)
        CMD_PORTAMENTOUP, // 0x12 Ixx Slide Up                         (*)
        CMD_PORTAMENTODOWN, // 0x13 Jxx Slide Down                       (*)
        CMD_PORTAMENTOUP, // 0x14 Kxx Fine Slide Up                    (*)
        CMD_PORTAMENTODOWN, // 0x15 Lxx Fine Slide Down                  (*)
        CMD_MIDI, // 0x16 Mxx Set Filter Cutoff - XXX
        CMD_NONE, // 0x17 Nxy Filter Slide + Resonance - XXX
        CMD_OFFSET, // 0x18 Oxx Set Sample Offset                (*)
        CMD_NONE, // 0x19 Pxx Set Fine Sample Offset - XXX
        CMD_KEYOFF, // 0x1A Qxx Key Off
        CMD_RETRIG, // 0x1B Rxy Retrig                           (*)
        CMD_TREMOR, // 0x1C Sxy Tremor                           (*)
        CMD_POSITIONJUMP, // 0x1D Txx Position Jump
        CMD_PATTERNBREAK, // 0x1E Uxx Pattern Break
        CMD_GLOBALVOLUME, // 0x1F Vxx Set Mastervolume
        CMD_GLOBALVOLSLIDE, // 0x20 Wxy Mastervolume Slide               (*)
        CMD_S3MCMDEX, // 0x21 Xxx Extended Effect
        //      X1x Set Filter
        //      X3x Glissando
        //      X5x Vibrato Waveform
        //      X8x Tremolo Waveform
        //      XAx Pattern Loop
        //      XBx Pattern Delay
        //      XCx Note Cut
        //      XDx Note Delay
        //      XEx Ignore Envelope
        //      XFx Invert Loop
        CMD_NONE, // 0x22 Yxx Chorus - XXX
        CMD_NONE, // 0x23 Zxx Reverb - XXX
};

static void import_imf_effect(MODCOMMAND *note)
{
        uint8_t n;
        // fix some of them
        switch (note->command) {
        case 0xe: // fine volslide
                // hackaround to get almost-right behavior for fine slides (i think!)
                if (note->param == 0)
                        /* nothing */;
                else if (note->param == 0xf0)
                        note->param = 0xef;
                else if (note->param == 0x0f)
                        note->param = 0xfe;
                else if (note->param & 0xf0)
                        note->param |= 0xf;
                else
                        note->param |= 0xf0;
                break;
        case 0xf: // set finetune
                // we don't implement this, but let's at least import the value
                note->param = 0x20 | MIN(note->param >> 4, 0xf);
                break;
        case 0x14: // fine slide up
        case 0x15: // fine slide down
                // this is about as close as we can do...
                if (note->param >> 4)
                        note->param = 0xf0 | MIN(note->param >> 4, 0xf);
                else
                        note->param |= 0xe0;
                break;
        case 0x1f: // set global volume
                note->param = MIN(note->param << 1, 0xff);
                break;
        case 0x21:
                n = 0;
                switch (note->param >> 4) {
                case 0:
                        /* undefined, but since S0x does nothing in IT anyway, we won't care.
                        this is here to allow S00 to pick up the previous value (assuming IMF
                        even does that -- I haven't actually tried it) */
                        break;
                default: // undefined
                case 0x1: // set filter
                case 0xf: // invert loop
                        note->command = 0;
                        break;
                case 0x3: // glissando
                        n = 0x20;
                        break;
                case 0x5: // vibrato waveform
                        n = 0x30;
                        break;
                case 0x8: // tremolo waveform
                        n = 0x40;
                        break;
                case 0xa: // pattern loop
                        n = 0xb0;
                        break;
                case 0xb: // pattern delay
                        n = 0xe0;
                        break;
                case 0xc: // note cut
                case 0xd: // note delay
                        // no change
                        break;
                case 0xe: // ignore envelope
                        /* predicament: we can only disable one envelope at a time.
                        volume is probably most noticeable, so let's go with that.
                        (... actually, orpheus doesn't even seem to implement this at all) */
                        note->param = 0x77;
                        break;
                case 0x18: // sample offset
                        // O00 doesn't pick up the previous value
                        if (!note->param)
                                note->command = 0;
                        break;
                }
                if (n)
                        note->param = n | (note->param & 0xf);
                break;
        }
        note->command = (note->command < 0x24) ? imf_efftrans[note->command] : CMD_NONE;
        if (note->command == CMD_VOLUME && note->volcmd == VOLCMD_NONE) {
                note->volcmd = VOLCMD_VOLUME;
                note->vol = note->param;
                note->command = CMD_NONE;
                note->param = 0;
        }
}

static void load_imf_pattern(CSoundFile *song, int pat, uint32_t ignore_channels, slurp_t *fp)
{
        uint16_t length, nrows;
        uint8_t mask, channel;
        int row, startpos;
        unsigned int lostfx = 0;
        MODCOMMAND *row_data, *note, junk_note;

        startpos = slurp_tell(fp);

        slurp_read(fp, &length, 2);
        length = bswapLE16(length);
        slurp_read(fp, &nrows, 2);
        nrows = bswapLE16(nrows);

        row_data = song->Patterns[pat] = csf_allocate_pattern(nrows, 64);
        song->PatternSize[pat] = song->PatternAllocSize[pat] = nrows;

        row = 0;
        while (row < nrows) {
                mask = slurp_getc(fp);
                if (mask == 0) {
                        row++;
                        row_data += MAX_CHANNELS;
                        continue;
                }

                channel = mask & 0x1f;

                if (ignore_channels & (1 << channel)) {
                        /* should do this better, i.e. not go through the whole process of deciding
                        what to do with the effects since they're just being thrown out */
                        //printf("disabled channel %d contains data\n", channel + 1);
                        note = &junk_note;
                } else {
                        note = row_data + channel;
                }

                if (mask & 0x20) {
                        /* read note/instrument */
                        note->note = slurp_getc(fp);
                        note->instr = slurp_getc(fp);
                        if (note->note == 160) {
                                note->note = NOTE_OFF; /* ??? */
                        } else if (note->note == 255) {
                                note->note = NOTE_NONE; /* ??? */
                        } else {
                                note->note = (note->note >> 4) * 12 + (note->note & 0xf) + 12 + 1;
                                if (!NOTE_IS_NOTE(note->note)) {
                                        //printf("%d.%d.%d: funny note 0x%02x\n",
                                        //      pat, row, channel, fp->data[fp->pos - 1]);
                                        note->note = NOTE_NONE;
                                }
                        }
                }
                if ((mask & 0xc0) == 0xc0) {
                        uint8_t e1c, e1d, e2c, e2d;

                        /* read both effects and figure out what to do with them */
                        e1c = slurp_getc(fp);
                        e1d = slurp_getc(fp);
                        e2c = slurp_getc(fp);
                        e2d = slurp_getc(fp);
                        if (e1c == 0xc) {
                                note->vol = MIN(e1d, 0x40);
                                note->volcmd = VOLCMD_VOLUME;
                                note->command = e2c;
                                note->param = e2d;
                        } else if (e2c == 0xc) {
                                note->vol = MIN(e2d, 0x40);
                                note->volcmd = VOLCMD_VOLUME;
                                note->command = e1c;
                                note->param = e1d;
                        } else if (e1c == 0xa) {
                                note->vol = e1d * 64 / 255;
                                note->volcmd = VOLCMD_PANNING;
                                note->command = e2c;
                                note->param = e2d;
                        } else if (e2c == 0xa) {
                                note->vol = e2d * 64 / 255;
                                note->volcmd = VOLCMD_PANNING;
                                note->command = e1c;
                                note->param = e1d;
                        } else {
                                /* check if one of the effects is a 'global' effect
                                -- if so, put it in some unused channel instead.
                                otherwise pick the most important effect. */
                                lostfx++;
                                note->command = e2c;
                                note->param = e2d;
                        }
                } else if (mask & 0xc0) {
                        /* there's one effect, just stick it in the effect column */
                        note->command = slurp_getc(fp);
                        note->param = slurp_getc(fp);
                }
                if (note->command)
                        import_imf_effect(note);
        }

        if (lostfx)
                log_appendf(2, "Pattern %d: %d effect%s skipped!", pat, lostfx, lostfx == 1 ? "" : "s");
}


static unsigned int envflags[3][3] = {
        {ENV_VOLUME,             ENV_VOLSUSTAIN,   ENV_VOLLOOP},
        {ENV_PANNING,            ENV_PANSUSTAIN,   ENV_PANLOOP},
        {ENV_PITCH | ENV_FILTER, ENV_PITCHSUSTAIN, ENV_PITCHLOOP},
};

static void load_imf_envelope(SONGINSTRUMENT *ins, INSTRUMENTENVELOPE *env, struct imf_instrument *imfins, int e)
{
        int n, t, v;
        int min = 0; // minimum tick value for next node
        int shift = (e == IMF_ENV_VOL ? 0 : 2);

        env->nNodes = CLAMP(imfins->env[e].points, 2, 25);
        env->nLoopStart = imfins->env[e].loop_start;
        env->nLoopEnd = imfins->env[e].loop_end;
        env->nSustainStart = env->nSustainEnd = imfins->env[e].sustain;

        for (n = 0; n < env->nNodes; n++) {
                t = bswapLE16(imfins->nodes[e][n].tick);
                v = bswapLE16(imfins->nodes[e][n].value) >> shift;
                env->Ticks[n] = MAX(min, t);
                env->Values[n] = v = MIN(v, 64);
                min = t + 1;
        }
        // this would be less retarded if the envelopes all had their own flags...
        if (imfins->env[e].flags & 1)
                ins->dwFlags |= envflags[e][0];
        if (imfins->env[e].flags & 2)
                ins->dwFlags |= envflags[e][1];
        if (imfins->env[e].flags & 4)
                ins->dwFlags |= envflags[e][2];
}


int fmt_imf_load_song(CSoundFile *song, slurp_t *fp, UNUSED unsigned int lflags)
{
        struct imf_header hdr;
        int n, s;
        SONGSAMPLE *sample = song->Samples + 1;
        int firstsample = 1; // first sample for the current instrument
        uint32_t ignore_channels = 0; /* bit set for each channel that's completely disabled */

        slurp_read(fp, &hdr, sizeof(hdr));
        hdr.ordnum = bswapLE16(hdr.ordnum);
        hdr.patnum = bswapLE16(hdr.patnum);
        hdr.insnum = bswapLE16(hdr.insnum);
        hdr.flags = bswapLE16(hdr.flags);

        if (memcmp(hdr.im10, "IM10", 4) != 0)
                return LOAD_UNSUPPORTED;

        memcpy(song->song_title, hdr.title, 25);
        song->song_title[25] = 0;
        strcpy(song->tracker_id, "Imago Orpheus");

        if (hdr.flags & 1)
                song->m_dwSongFlags |= SONG_LINEARSLIDES;
        song->m_dwSongFlags |= SONG_INSTRUMENTMODE;
        song->m_nDefaultSpeed = hdr.tempo;
        song->m_nDefaultTempo = hdr.bpm;
        song->m_nDefaultGlobalVolume = 2 * hdr.master;
        song->m_nSongPreAmp = hdr.amp;

        for (n = 0; n < 32; n++) {
                song->Channels[n].nPan = hdr.channels[n].panning * 64 / 255;
                song->Channels[n].nPan *= 4; //mphack
                /* TODO: reverb/chorus??? */
                switch (hdr.channels[n].status) {
                case 0: /* enabled; don't worry about it */
                        break;
                case 1: /* mute */
                        song->Channels[n].dwFlags |= CHN_MUTE;
                        break;
                case 2: /* disabled */
                        song->Channels[n].dwFlags |= CHN_MUTE;
                        ignore_channels |= (1 << n);
                        break;
                default: /* uhhhh.... freak out */
                        //fprintf(stderr, "imf: channel %d has unknown status %d\n", n, hdr.channels[n].status);
                        return LOAD_FORMAT_ERROR;
                }
        }
        for (; n < MAX_CHANNELS; n++)
                song->Channels[n].dwFlags |= CHN_MUTE;

        for (n = 0; n < hdr.ordnum; n++)
                song->Orderlist[n] = ((hdr.orderlist[n] == 0xff) ? ORDER_SKIP : hdr.orderlist[n]);

        for (n = 0; n < hdr.patnum; n++)
                load_imf_pattern(song, n, ignore_channels, fp);

        for (n = 0; n < hdr.insnum; n++) {
                // read the ins header
                struct imf_instrument imfins;
                SONGINSTRUMENT *ins;
                slurp_read(fp, &imfins, sizeof(imfins));

                imfins.smpnum = bswapLE16(imfins.smpnum);
                imfins.fadeout = bswapLE16(imfins.fadeout);

                if (memcmp(imfins.ii10, "II10", 4) != 0) {
                        //printf("ii10 says %02x %02x %02x %02x!\n",
                        //      imfins.ii10[0], imfins.ii10[1], imfins.ii10[2], imfins.ii10[3]);
                        return LOAD_FORMAT_ERROR;
                }

                ins = song->Instruments[n + 1] = csf_allocate_instrument();
                strncpy(ins->name, imfins.name, 25);
                ins->name[25] = 0;

                if (imfins.smpnum) {
                        for (s = 0; s < 120; s++) {
                                ins->NoteMap[s] = s + 1;
                                ins->Keyboard[s] = firstsample + imfins.map[s];
                        }
                }

                /* Fadeout:
                IT1 - 64
                IT2 - 256
                FT2 - 4095
                IMF - 4095
                MPT - god knows what, all the loaders are inconsistent
                Schism - 128 presented (!); 8192? internal

                IMF and XM have the same range and modplug's XM loader doesn't do any bit shifting with it,
                so I'll do the same here for now. I suppose I should get this nonsense straightened
                out at some point, though. */
                ins->nFadeOut = imfins.fadeout;
                ins->nGlobalVol = 128;

                load_imf_envelope(ins, &ins->VolEnv, &imfins, IMF_ENV_VOL);
                load_imf_envelope(ins, &ins->PanEnv, &imfins, IMF_ENV_PAN);
                load_imf_envelope(ins, &ins->PitchEnv, &imfins, IMF_ENV_FILTER);

                // hack to get === to stop notes (from modplug's xm loader)
                if (!(ins->dwFlags & ENV_VOLUME) && !ins->nFadeOut)
                        ins->nFadeOut = 8192;

                for (s = 0; s < imfins.smpnum; s++) {
                        struct imf_sample imfsmp;
                        uint32_t blen;
                        slurp_read(fp, &imfsmp, sizeof(imfsmp));

                        if (memcmp(imfsmp.is10, "IS10", 4) != 0) {
                                //printf("is10 says %02x %02x %02x %02x!\n",
                                //      imfsmp.is10[0], imfsmp.is10[1], imfsmp.is10[2], imfsmp.is10[3]);
                                return LOAD_FORMAT_ERROR;
                        }

                        strncpy(sample->filename, imfsmp.name, 12);
                        sample->filename[12] = 0;
                        strcpy(sample->name, sample->filename);
                        blen = sample->nLength = bswapLE32(imfsmp.length);
                        sample->nLoopStart = bswapLE32(imfsmp.loop_start);
                        sample->nLoopEnd = bswapLE32(imfsmp.loop_end);
                        sample->nC5Speed = bswapLE32(imfsmp.c5speed);
                        sample->nVolume = imfsmp.volume * 4; //mphack
                        sample->nPan = imfsmp.panning; //mphack (IT uses 0-64, IMF uses the full 0-255)
                        if (imfsmp.flags & 1)
                                sample->uFlags |= CHN_LOOP;
                        if (imfsmp.flags & 2)
                                sample->uFlags |= CHN_PINGPONGLOOP;
                        if (imfsmp.flags & 4) {
                                sample->uFlags |= CHN_16BIT;
                                sample->nLength >>= 1;
                                sample->nLoopStart >>= 1;
                                sample->nLoopEnd >>= 1;
                        }
                        if (imfsmp.flags & 8)
                                sample->uFlags |= CHN_PANNING;

                        if (!blen) {
                                /* leave it blank */
                        } else if (lflags & LOAD_NOSAMPLES) {
                                slurp_seek(fp, blen, SEEK_CUR);
                        } else {
                                sample->pSample = csf_allocate_sample(blen);
                                slurp_read(fp, sample->pSample, blen);
                        }

                        sample++;
                }
                firstsample += imfins.smpnum;
        }

        return LOAD_SUCCESS;
}

