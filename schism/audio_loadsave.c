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

#include "it.h"
#include "sndfile.h"
#include "song.h"
#include "slurp.h"
#include "page.h"
#include "version.h"

#include "fmt.h"
#include "dmoz.h"

#include "it_defs.h"

#include "snd_gm.h"
#include "midi.h"
#include "disko.h"

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <assert.h>

// ------------------------------------------------------------------------

char song_filename[PATH_MAX + 1];
char song_basename[NAME_MAX + 1];

uint8_t row_highlight_major = 16, row_highlight_minor = 4;

// ------------------------------------------------------------------------
// quiet a sample when loading

void song_stop_sample(song_sample_t *ssmp)
{
        int i;
        song_sample_t *smp = (song_sample_t *) ssmp; // bleargh

        for (i = 0; i < MAX_VOICES; i++) {
                if (current_song->voices[i].ptr_sample == smp
                    || current_song->voices[i].current_sample_data == smp->data
                    || current_song->voices[i].data == smp->data) {
                        current_song->voices[i].note = current_song->voices[i].new_note = current_song->voices[i].new_instrument = 0;
                        current_song->voices[i].fadeout_volume = 0;
                        current_song->voices[i].flags |= CHN_KEYOFF|CHN_NOTEFADE;
                        current_song->voices[i].period = 0;
                        current_song->voices[i].position = current_song->voices[i].length = 0;
                        current_song->voices[i].loop_start = 0;
                        current_song->voices[i].loop_end = 0;
                        current_song->voices[i].rofs = current_song->voices[i].lofs = 0;
                        current_song->voices[i].data = NULL;
                        current_song->voices[i].ptr_sample = NULL;
                        current_song->voices[i].ptr_instrument = NULL;
                        current_song->voices[i].left_volume = current_song->voices[i].right_volume = 0;
                        current_song->voices[i].left_volume_new = current_song->voices[i].right_volume_new = 0;
                        current_song->voices[i].left_ramp = current_song->voices[i].right_ramp = 0;
                }
        }
}

static void _squelch_sample(int n)
{
        song_stop_sample(current_song->samples + n); // sdkflhaghljfsdh
}


// functions to "fix" the song for editing.
// these are all called by fix_song after a file is loaded.



// replace any '\0' chars with spaces, mostly to make the string handling
// much easier.
// TODO | Maybe this should be done with the filenames and the song title
// TODO | as well? (though I've never come across any cases of either of
// TODO | these having null characters in them...)
static void _fix_names(song_t *qq)
{
        int c, n;

        for (n = 1; n < MAX_INSTRUMENTS; n++) {
                for (c = 0; c < 25; c++)
                        if (qq->samples[n].name[c] == 0)
                                qq->samples[n].name[c] = 32;
                qq->samples[n].name[25] = 0;

                if (!qq->instruments[n])
                        continue;
                for (c = 0; c < 25; c++)
                        if (qq->instruments[n]->name[c] == 0)
                                qq->instruments[n]->name[c] = 32;
                qq->instruments[n]->name[25] = 0;
        }
}

static void fix_song(void)
{
        /* poop */
        current_song->locked_order = MAX_ORDERS;

        _fix_names(current_song);
}

// ------------------------------------------------------------------------
// file stuff

static void song_set_filename(const char *file)
{
        if (file && file[0]) {
                strncpy(song_filename, file, PATH_MAX);
                strncpy(song_basename, get_basename(file), NAME_MAX);
                song_filename[PATH_MAX] = '\0';
                song_basename[NAME_MAX] = '\0';
        } else {
                song_filename[0] = '\0';
                song_basename[0] = '\0';
        }
}

// clear patterns => clear filename and save flag
// clear orderlist => clear title, message, and channel settings
void song_new(int flags)
{
        int i;

        song_lock_audio();

        song_stop_unlocked(0);

        if ((flags & KEEP_PATTERNS) == 0) {
                song_set_filename(NULL);
                status.flags &= ~SONG_NEEDS_SAVE;

                for (i = 0; i < MAX_PATTERNS; i++) {
                        if (current_song->patterns[i]) {
                                csf_free_pattern(current_song->patterns[i]);
                                current_song->patterns[i] = NULL;
                        }
                        current_song->pattern_size[i] = 64;
                        current_song->pattern_alloc_size[i] = 64;
                }
        }
        if ((flags & KEEP_SAMPLES) == 0) {
                for (i = 1; i < MAX_SAMPLES; i++) {
                        if (current_song->samples[i].data) {
                                csf_free_sample(current_song->samples[i].data);
                        }
                }
                memset(current_song->samples, 0, sizeof(current_song->samples));
                for (i = 1; i < MAX_SAMPLES; i++) {
                        current_song->samples[i].c5speed = 8363;
                        current_song->samples[i].volume = 64 * 4;
                        current_song->samples[i].global_volume = 64;
                }
        }
        if ((flags & KEEP_INSTRUMENTS) == 0) {
                for (i = 0; i < MAX_INSTRUMENTS; i++) {
                        if (current_song->instruments[i]) {
                                csf_free_instrument(current_song->instruments[i]);
                                current_song->instruments[i] = NULL;
                        }
                }
        }
        if ((flags & KEEP_ORDERLIST) == 0) {
                current_song->locked_order = MAX_ORDERS;
                memset(current_song->orderlist, ORDER_LAST, sizeof(current_song->orderlist));

                memset(current_song->title, 0, sizeof(current_song->title));

                memset(current_song->message, 0, MAX_MESSAGE);

                for (i = 0; i < 64; i++) {
                        current_song->channels[i].volume = 64;
                        current_song->channels[i].panning = 128;
                        current_song->channels[i].flags = 0;
                        current_song->voices[i].volume = 256;
                        current_song->voices[i].global_volume = current_song->channels[i].volume;
                        current_song->voices[i].panning = current_song->channels[i].panning;
                        current_song->voices[i].flags = current_song->channels[i].flags;
                        current_song->voices[i].cutoff = 0x7F;
                }
        }

        current_song->repeat_count = -1;
        //song_stop();

        csf_reset_midi_cfg(current_song);

        song_unlock_audio();

        // ugly #1
        current_song->row_highlight_major = row_highlight_major;
        current_song->row_highlight_minor = row_highlight_minor;

        main_song_changed_cb();
}

// ------------------------------------------------------------------------------------------------------------

#define LOAD_SONG(x) fmt_##x##_load_song,
static fmt_load_song_func load_song_funcs[] = {
#include "fmt-types.h"
        NULL,
};


static const char *fmt_strerror(int n)
{
        switch (n) {
        case -LOAD_UNSUPPORTED:
                return "Unrecognised file type";
        case -LOAD_FORMAT_ERROR:
                return "File format error (corrupt?)";
        default:
                return strerror(errno);
        }
}

static song_t *song_create_load(const char *file)
{
        fmt_load_song_func *func;
        int ok = 0, err = 0;

        slurp_t *s = slurp(file, NULL, 0);
        if (!s)
                return NULL;

        song_t *newsong = csf_allocate();

        for (func = load_song_funcs; *func && !ok; func++) {
                slurp_rewind(s);
                switch ((*func)(newsong, s, 0)) {
                case LOAD_SUCCESS:
                        err = 0;
                        ok = 1;
                        break;
                case LOAD_UNSUPPORTED:
                        err = -LOAD_UNSUPPORTED;
                        continue;
                case LOAD_FORMAT_ERROR:
                        err = -LOAD_FORMAT_ERROR;
                        break;
                case LOAD_FILE_ERROR:
                        err = errno;
                        break;
                }
                if (err) {
                        csf_free(newsong);
                        unslurp(s);
                        errno = err;
                        return NULL;
                }
        }

        unslurp(s);

        if (err) {
                // awwww, nerts!
                csf_free(newsong);
                errno = err;
                return NULL;
        }
        return newsong;
}

int song_load_unchecked(const char *file)
{
        const char *base = get_basename(file);
        int was_playing;
        song_t *newsong;

        // IT stops the song even if the new song can't be loaded
        if (status.flags & PLAY_AFTER_LOAD) {
                was_playing = (song_get_mode() == MODE_PLAYING);
        } else {
                was_playing = 0;
                song_stop();
        }

        log_nl();
        log_nl();
        log_appendf(2, "Loading %s", base);
        log_underline(strlen(base) + 8);

        newsong = song_create_load(file);
        if (!newsong) {
                log_appendf(4, " %s", fmt_strerror(errno));
                return 0;
        }


        song_set_filename(file);

        song_lock_audio();
        csf_free(current_song);
        current_song = newsong;
        current_song->repeat_count = -1;
        max_channels_used = 0;
        fix_song();
        song_stop_unlocked(0);
        song_unlock_audio();

        if (was_playing && (status.flags & PLAY_AFTER_LOAD))
                song_start();

        // ugly #2
        row_highlight_major = current_song->row_highlight_major;
        row_highlight_minor = current_song->row_highlight_minor;

        main_song_changed_cb();

        status.flags &= ~SONG_NEEDS_SAVE;

        // print out some stuff
        const char *tid = current_song->tracker_id;

        char fmt[] = " %d patterns, %d samples, %d instruments";
        int n, nsmp, nins;
        song_sample_t *smp;
        song_instrument_t **ins;

        for (n = 0, smp = current_song->samples + 1, nsmp = 0; n < MAX_SAMPLES; n++, smp++)
                if (smp->data)
                        nsmp++;
        for (n = 0, ins = current_song->instruments + 1, nins = 0; n < MAX_INSTRUMENTS; n++, ins++)
                if (*ins != NULL)
                        nins++;

        if (tid[0])
                log_appendf(5, " %s", tid);
        if (!nins)
                *strrchr(fmt, ',') = 0; // cut off 'instruments'
        log_appendf(5, fmt, song_get_num_patterns(), nsmp, nins);


        return 1;
}

// ------------------------------------------------------------------------------------------------------------

int song_instrument_is_empty(int n)
{
        if (current_song->instruments[n] == NULL)
                return 1;
        if (current_song->instruments[n]->filename[0] != '\0')
                return 0;
        for (int i = 0; i < 25; i++) {
                if (current_song->instruments[n]->name[i] != '\0' && current_song->instruments[n]->name[i] != ' ')
                        return 0;
        }
        for (int i = 0; i < 119; i++) {
                if (current_song->instruments[n]->sample_map[i] != 0)
                        return 0;
        }
        // this used to check midi program and bank, but if those are set to -1 the instrument should still
        // be considered empty. not to mention, isn't midi program 0 meaningful?
        if (current_song->instruments[n]->midi_channel_mask != 0)
                return 0;
        return 1;
}

int song_sample_is_empty(int n)
{
        n++;

        if (current_song->samples[n].length)
                return 0;
        if (current_song->samples[n].filename[0] != '\0')
                return 0;
        for (int i = 0; i < 25; i++) {
                if (current_song->samples[n].name[i] != '\0' && current_song->samples[n].name[i] != ' ')
                        return 0;
        }

        return 1;
}

// ------------------------------------------------------------------------------------------------------------
// generic sample data saving

void save_sample_data_LE(disko_t *fp, song_sample_t *smp, int noe)
{
        unsigned char buffer[4096];
        unsigned int bufcount;
        unsigned int len;

        len = smp->length;
        if (smp->flags & CHN_STEREO) len *= 2;

        if (smp->flags & CHN_16BIT) {
                if (noe && smp->flags & CHN_STEREO) {
                        bufcount = 0;
                        for (unsigned int n = 0; n < len; n += 2) {

                                signed short s = ((signed short *) smp->data)[n];
                                s = bswapLE16(s);
                                memcpy(buffer+bufcount, &s, 2);
                                bufcount += 2;
                                if (bufcount >= sizeof(buffer)) {
                                        disko_write(fp, buffer, bufcount);
                                        bufcount = 0;
                                }
                        }
                        for (unsigned int n = 1; n < len; n += 2) {
                                signed short s = ((signed short *) smp->data)[n];
                                s = bswapLE16(s);
                                memcpy(buffer+bufcount, &s, 2);
                                bufcount += 2;
                                if (bufcount >= sizeof(buffer)) {
                                        disko_write(fp, buffer, bufcount);
                                        bufcount = 0;
                                }
                        }
                        if (bufcount > 0) {
                                disko_write(fp, buffer, bufcount);
                        }
                } else {
#if WORDS_BIGENDIAN
                        bufcount = 0;
                        for (unsigned int n = 0; n < len; n++) {
                                signed short s = ((signed short *) smp->data)[n];
                                s = bswapLE16(s);
                                memcpy(buffer+bufcount, &s, 2);
                                bufcount += 2;
                                if (bufcount >= sizeof(buffer)) {
                                        disko_write(fp, buffer, bufcount);
                                        bufcount = 0;
                                }
                        }
                        if (bufcount > 0) {
                                disko_write(fp, buffer, bufcount);
                        }
#else
                        disko_write(fp, smp->data, 2*len);
#endif
                }
        } else if (smp->flags & CHN_STEREO) {
                bufcount = 0;
                for (unsigned int n = 0; n < len; n += 2) {
                        buffer[bufcount++] = (smp->data)[n];
                        if (bufcount >= sizeof(buffer)) {
                                disko_write(fp, buffer, bufcount);
                                bufcount = 0;
                        }
                }
                for (unsigned int n = 1; n < len; n += 2) {
                        buffer[bufcount++] = (smp->data)[n];
                        if (bufcount >= sizeof(buffer)) {
                                disko_write(fp, buffer, bufcount);
                                bufcount = 0;
                        }
                }
                if (bufcount > 0) {
                        disko_write(fp, buffer, bufcount);
                }

        } else {
                disko_write(fp, smp->data, len);
        }
}

/* same as above, except the other way around */
/* the one thing cheese did that was pretty nifty was the disk read/write operations had separate big-endian
   and little-endian operations...
   we could probably do something like that, say fp->write and fp->O for bigendian
   but 'O' looks too much like zero, so maybe rename 'em to w/W ... and have r/R to read whatever endianness
   anyway, just a thought. /storlek */
void save_sample_data_BE(disko_t *fp, song_sample_t *smp, int noe)
{
        unsigned int len;
        len = smp->length;
        if (smp->flags & CHN_STEREO) len *= 2;

        if (smp->flags & CHN_16BIT) {
                if (noe && smp->flags & CHN_STEREO) {
                        for (unsigned int n = 0; n < len; n += 2) {
                                signed short s = ((signed short *) smp->data)[n];
                                s = bswapBE16(s);
                                disko_write(fp, &s, 2);
                        }
                        for (unsigned int n = 1; n < len; n += 2) {
                                signed short s = ((signed short *) smp->data)[n];
                                s = bswapBE16(s);
                                disko_write(fp, &s, 2);
                        }
                } else {
#if WORDS_BIGENDIAN
                        disko_write(fp, smp->data, 2*len);
#else
                        for (unsigned int n = 0; n < len; n++) {
                                signed short s = ((signed short *) smp->data)[n];
                                s = bswapBE16(s);
                                disko_write(fp, &s, 2);
                        }
#endif
                }
        } else if (smp->flags & CHN_STEREO) {
                for (unsigned int n = 0; n < len; n += 2) {
                        disko_write(fp, (smp->data)+n, 1);
                }
                for (unsigned int n = 1; n < len; n += 2) {
                        disko_write(fp, (smp->data)+n, 1);
                }

        } else {
                disko_write(fp, smp->data, len);
        }
}

// ------------------------------------------------------------------------------------------------------------

static song_instrument_t blank_instrument; // should be zero, it's coming from bss

// set iti_file if saving an instrument to disk by itself
static void _save_it_instrument(int n, disko_t *fp, int iti_file)
{
        n++; // FIXME: this is dumb; really all the numbering should be one-based to make it simple

        struct it_instrument iti;
        song_instrument_t *i = current_song->instruments[n];

        if (!i)
                i = &blank_instrument;

        memset(&iti, 0, sizeof(iti));

        // envelope: flags num lpb lpe slb sle data[25*3] reserved

        iti.id = bswapLE32(0x49504D49); // IMPI
        strncpy((char *) iti.filename, (char *) i->filename, 12);
        iti.zero = 0;
        iti.nna = i->nna;
        iti.dct = i->dct;
        iti.dca = i->dca;
        iti.fadeout = bswapLE16(i->fadeout >> 5);
        iti.pps = i->pitch_pan_separation;
        iti.ppc = i->pitch_pan_center;
        iti.gbv = i->global_volume;
        iti.dfp = i->panning / 4;
        if (!(i->flags & ENV_SETPANNING))
                iti.dfp |= 0x80;
        iti.rv = i->vol_swing;
        iti.rp = i->pan_swing;
        if (iti_file) {
                iti.trkvers = bswapLE16(0x0214);
                //iti.nos = ???;
        }
        // reserved1
        strncpy((char *) iti.name, (char *) i->name, 25);
        iti.name[25] = 0;
        iti.ifc = i->ifc;
        iti.ifr = i->ifr;
        iti.mch = 0;
        if(i->midi_channel_mask >= 0x10000)
        {
            iti.mch = i->midi_channel_mask - 0x10000;
            if(iti.mch <= 16) iti.mch = 16;
        }
        else if(i->midi_channel_mask & 0xFFFF)
        {
            iti.mch = 1;
            while(!(i->midi_channel_mask & (1 << (iti.mch-1)))) ++iti.mch;
        }
        iti.mpr = i->midi_program;
        iti.mbank = bswapLE16(i->midi_bank);

        static int iti_map[255];
        static int iti_invmap[255];
        static int iti_nalloc = 0;

        iti_nalloc = 0;
        for (int j = 0; j < 255; j++) {
                iti_map[j] = -1;
        }
        for (int j = 0; j < 120; j++) {
                if (iti_file) {
                        int o = i->sample_map[j];
                        if (o > 0 && o < 255 && iti_map[o] == -1) {
                                iti_map[o] = iti_nalloc;
                                iti_invmap[iti_nalloc] = o;
                                iti_nalloc++;
                        }
                        iti.keyboard[2 * j + 1] = iti_map[o]+1;
                } else {
                        iti.keyboard[2 * j + 1] = i->sample_map[j];
                }
                iti.keyboard[2 * j] = i->note_map[j] - 1;
        }
        // envelope stuff from modplug
        iti.volenv.flags = 0;
        iti.panenv.flags = 0;
        iti.pitchenv.flags = 0;
        if (i->flags & ENV_VOLUME) iti.volenv.flags |= 0x01;
        if (i->flags & ENV_VOLLOOP) iti.volenv.flags |= 0x02;
        if (i->flags & ENV_VOLSUSTAIN) iti.volenv.flags |= 0x04;
        if (i->flags & ENV_VOLCARRY) iti.volenv.flags |= 0x08;
        iti.volenv.num = i->vol_env.nodes;
        iti.volenv.lpb = i->vol_env.loop_start;
        iti.volenv.lpe = i->vol_env.loop_end;
        iti.volenv.slb = i->vol_env.sustain_start;
        iti.volenv.sle = i->vol_env.sustain_end;
        if (i->flags & ENV_PANNING) iti.panenv.flags |= 0x01;
        if (i->flags & ENV_PANLOOP) iti.panenv.flags |= 0x02;
        if (i->flags & ENV_PANSUSTAIN) iti.panenv.flags |= 0x04;
        if (i->flags & ENV_PANCARRY) iti.panenv.flags |= 0x08;
        iti.panenv.num = i->pan_env.nodes;
        iti.panenv.lpb = i->pan_env.loop_start;
        iti.panenv.lpe = i->pan_env.loop_end;
        iti.panenv.slb = i->pan_env.sustain_start;
        iti.panenv.sle = i->pan_env.sustain_end;
        if (i->flags & ENV_PITCH) iti.pitchenv.flags |= 0x01;
        if (i->flags & ENV_PITCHLOOP) iti.pitchenv.flags |= 0x02;
        if (i->flags & ENV_PITCHSUSTAIN) iti.pitchenv.flags |= 0x04;
        if (i->flags & ENV_PITCHCARRY) iti.pitchenv.flags |= 0x08;
        if (i->flags & ENV_FILTER) iti.pitchenv.flags |= 0x80;
        iti.pitchenv.num = i->pitch_env.nodes;
        iti.pitchenv.lpb = i->pitch_env.loop_start;
        iti.pitchenv.lpe = i->pitch_env.loop_end;
        iti.pitchenv.slb = i->pitch_env.sustain_start;
        iti.pitchenv.sle = i->pitch_env.sustain_end;
        for (int j = 0; j < 25; j++) {
                iti.volenv.data[3 * j] = i->vol_env.values[j];
                iti.volenv.data[3 * j + 1] = i->vol_env.ticks[j] & 0xFF;
                iti.volenv.data[3 * j + 2] = i->vol_env.ticks[j] >> 8;
                iti.panenv.data[3 * j] = i->pan_env.values[j] - 32;
                iti.panenv.data[3 * j + 1] = i->pan_env.ticks[j] & 0xFF;
                iti.panenv.data[3 * j + 2] = i->pan_env.ticks[j] >> 8;
                iti.pitchenv.data[3 * j] = i->pitch_env.values[j] - 32;
                iti.pitchenv.data[3 * j + 1] = i->pitch_env.ticks[j] & 0xFF;
                iti.pitchenv.data[3 * j + 2] = i->pitch_env.ticks[j] >> 8;
        }

        // ITI files *need* to write 554 bytes due to alignment, but in a song it doesn't matter
        disko_write(fp, &iti, sizeof(iti));
        if (iti_file) {
                if (sizeof(iti) < 554) {
                        for (int j = sizeof(iti); j < 554; j++) {
                                disko_write(fp, "\x0", 1);
                        }
                }
                assert(sizeof(iti) <= 554);

                unsigned int qp = 554;
                /* okay, now go through samples */
                for (int j = 0; j < iti_nalloc; j++) {
                        int o = iti_invmap[ j ];

                        iti_map[o] = qp;
                        qp += 80; /* header is 80 bytes */
                        save_its_header(fp, current_song->samples + o);
                }
                for (int j = 0; j < iti_nalloc; j++) {
                        unsigned int op, tmp;

                        int o = iti_invmap[ j ];

                        song_sample_t *smp = current_song->samples + o;

                        op = disko_tell(fp);
                        tmp = bswapLE32(op);
                        disko_seek(fp, iti_map[o]+0x48, SEEK_SET);
                        disko_write(fp, &tmp, 4);
                        disko_seek(fp, op, SEEK_SET);
                        save_sample_data_LE(fp, smp, 1);

                }
        }
}

// NOBODY expects the Spanish Inquisition!
static void _save_it_pattern(disko_t *fp, song_note_t *pat, int patsize)
{
        song_note_t *noteptr = pat;
        song_note_t lastnote[64];
        uint8_t initmask[64];
        uint8_t lastmask[64];
        unsigned short pos = 0;
        uint8_t data[65536];

        memset(lastnote, 0, sizeof(lastnote));
        memset(initmask, 0, 64);
        memset(lastmask, 0xff, 64);

        for (int row = 0; row < patsize; row++) {
                for (int chan = 0; chan < 64; chan++, noteptr++) {
                        uint8_t m = 0;  // current mask
                        int vol = -1;
                        unsigned int note = noteptr->note;
                        unsigned int effect = noteptr->effect, param = noteptr->param;

                        if (note) {
                                m |= 1;
                                if (note < 0x80)
                                        note--;
                        }
                        if (noteptr->instrument) m |= 2;
                        switch (noteptr->voleffect) {
                        default:                                                       break;
                        case VOLFX_VOLUME:         vol = MIN(noteptr->volparam, 64);       break;
                        case VOLFX_FINEVOLUP:      vol = MIN(noteptr->volparam,  9) +  65; break;
                        case VOLFX_FINEVOLDOWN:    vol = MIN(noteptr->volparam,  9) +  75; break;
                        case VOLFX_VOLSLIDEUP:     vol = MIN(noteptr->volparam,  9) +  85; break;
                        case VOLFX_VOLSLIDEDOWN:   vol = MIN(noteptr->volparam,  9) +  95; break;
                        case VOLFX_PORTADOWN:      vol = MIN(noteptr->volparam,  9) + 105; break;
                        case VOLFX_PORTAUP:        vol = MIN(noteptr->volparam,  9) + 115; break;
                        case VOLFX_PANNING:        vol = MIN(noteptr->volparam, 64) + 128; break;
                        case VOLFX_VIBRATODEPTH:   vol = MIN(noteptr->volparam,  9) + 203; break;
                        case VOLFX_VIBRATOSPEED:   vol = 203;                         break;
                        case VOLFX_TONEPORTAMENTO: vol = MIN(noteptr->volparam,  9) + 193; break;
                        }
                        if (vol != -1) m |= 4;
                        csf_export_s3m_effect(&effect, &param, 1);
                        if (effect || param) m |= 8;
                        if (!m) continue;

                        if (m & 1) {
                                if ((note == lastnote[chan].note) && (initmask[chan] & 1)) {
                                        m &= ~1;
                                        m |= 0x10;
                                } else {
                                        lastnote[chan].note = note;
                                        initmask[chan] |= 1;
                                }
                        }
                        if (m & 2) {
                                if ((noteptr->instrument == lastnote[chan].instrument) && (initmask[chan] & 2)) {
                                        m &= ~2;
                                        m |= 0x20;
                                } else {
                                        lastnote[chan].instrument = noteptr->instrument;
                                        initmask[chan] |= 2;
                                }
                        }
                        if (m & 4) {
                                if ((vol == lastnote[chan].volparam) && (initmask[chan] & 4)) {
                                        m &= ~4;
                                        m |= 0x40;
                                } else {
                                        lastnote[chan].volparam = vol;
                                        initmask[chan] |= 4;
                                }
                        }
                        if (m & 8) {
                                if ((effect == lastnote[chan].effect) && (param == lastnote[chan].param)
                                    && (initmask[chan] & 8)) {
                                        m &= ~8;
                                        m |= 0x80;
                                } else {
                                        lastnote[chan].effect = effect;
                                        lastnote[chan].param = param;
                                        initmask[chan] |= 8;
                                }
                        }
                        if (m == lastmask[chan]) {
                                data[pos++] = chan + 1;
                        } else {
                                lastmask[chan] = m;
                                data[pos++] = (chan + 1) | 0x80;
                                data[pos++] = m;
                        }
                        if (m & 1) data[pos++] = note;
                        if (m & 2) data[pos++] = noteptr->instrument;
                        if (m & 4) data[pos++] = vol;
                        if (m & 8) {
                                data[pos++] = effect;
                                data[pos++] = param;
                        }
                }                       // end channel
                data[pos++] = 0;
        }                               // end row

        // write the data to the file (finally!)
        unsigned short h[4] = {0};
        h[0] = bswapLE16(pos);
        h[1] = bswapLE16(patsize);
        // h[2] and h[3] are meaningless
        disko_write(fp, &h, 8);
        disko_write(fp, data, pos);
}

static int _save_it(disko_t *fp, UNUSED song_t *song)
{
        struct it_file hdr;
        int n;
        int nord, nins, nsmp, npat;
        int msglen = strlen(current_song->message);
        unsigned int para_ins[256], para_smp[256], para_pat[256];
        unsigned int extra;
        unsigned short zero;

        memset(&hdr, 0, sizeof(hdr));

        // TODO complain about nonstandard stuff? or just stop saving it to begin with

        extra = 2;

        // IT always saves at least two orders.
        nord = 255;
        while (nord >= 1 && current_song->orderlist[nord] == 0xff)
                nord--;
        nord += 2;

        nins = 198;
        while (nins >= 0 && song_instrument_is_empty(nins))
                nins--;
        if (nins < 0) nins = 0; // stupid workaround

        nsmp = 198;
        while (nsmp >= 0 && song_sample_is_empty(nsmp))
                nsmp--;
        nsmp++;
        if (nsmp > 200) nsmp = 200; /* is this okay? */

        // IT always saves at least one pattern.
        //npat = 199;
        //while (npat >= 0 && song_pattern_is_empty(npat))
        //      npat--;
        //npat++;
        npat = song_get_num_patterns() + 1;

        hdr.id = bswapLE32(0x4D504D49); // IMPM
        strncpy((char *) hdr.songname, current_song->title, 25);
        hdr.songname[25] = 0;
        hdr.hilight_major = current_song->row_highlight_major;
        hdr.hilight_minor = current_song->row_highlight_minor;
        hdr.ordnum = bswapLE16(nord);
        hdr.insnum = bswapLE16(nins);
        hdr.smpnum = bswapLE16(nsmp);
        hdr.patnum = bswapLE16(npat);
        // No one else seems to be using the cwtv's tracker id number, so I'm gonna take 1. :)
        hdr.cwtv = bswapLE16(0x1000 | ver_cwtv); // cwtv 0xtxyy = tracker id t, version x.yy
        // compat:
        //     really simple IT files = 1.00 (when?)
        //     "normal" = 2.00
        //     vol col effects = 2.08
        //     pitch wheel depth = 2.13
        //     embedded midi config = 2.13
        //     row highlight = 2.13 (doesn't necessarily affect cmwt)
        //     compressed samples = 2.14
        //     instrument filters = 2.17
        hdr.cmwt = bswapLE16(0x0214);   // compatible with IT 2.14
        for (n = 1; n < nins; n++) {
                song_instrument_t *i = current_song->instruments[n];
                if (!i) continue;
                if (i->flags & ENV_FILTER) {
                        hdr.cmwt = bswapLE16(0x0217);
                        break;
                }
        }

        hdr.flags = 0;
        hdr.special = 2 | 4;            // reserved (always on?)

        if (song_is_stereo())               hdr.flags |= 1;
        if (song_is_instrument_mode())      hdr.flags |= 4;
        if (song_has_linear_pitch_slides()) hdr.flags |= 8;
        if (song_has_old_effects())         hdr.flags |= 16;
        if (song_has_compatible_gxx())      hdr.flags |= 32;
        if (midi_flags & MIDI_PITCHBEND) {
                hdr.flags |= 64;
                hdr.pwd = midi_pitch_depth;
        }
        if (midi_flags & MIDI_EMBED_DATA) {
                hdr.flags |= 128;
                hdr.special |= 8;
                extra += sizeof(midi_config_t);
        }
        hdr.flags = bswapLE16(hdr.flags);
        if (msglen) {
                hdr.special |= 1;
                msglen++;
        }
        hdr.special = bswapLE16(hdr.special);

        // 16+ = reserved (always off?)
        hdr.globalvol = current_song->initial_global_volume;
        hdr.mv = current_song->mixing_volume;
        hdr.speed = current_song->initial_speed;
        hdr.tempo = current_song->initial_tempo;
        hdr.sep = current_song->pan_separation;
        if (msglen) {
                hdr.msgoffset = bswapLE32(extra + 0xc0 + nord + 4 * (nins + nsmp + npat));
                hdr.msglength = bswapLE16(msglen);
        }
        // hdr.reserved2

        for (n = 0; n < 64; n++) {
                hdr.chnpan[n] = ((current_song->channels[n].flags & CHN_SURROUND)
                                 ? 100 : (current_song->channels[n].panning / 4));
                hdr.chnvol[n] = current_song->channels[n].volume;
                if (current_song->channels[n].flags & CHN_MUTE)
                        hdr.chnpan[n] += 128;
        }

        disko_write(fp, &hdr, sizeof(hdr));
        disko_write(fp, current_song->orderlist, nord);

        // we'll get back to these later
        disko_write(fp, para_ins, 4*nins);
        disko_write(fp, para_smp, 4*nsmp);
        disko_write(fp, para_pat, 4*npat);


        // here is the IT "extra" info (IT doesn't seem to use it)
        // TODO: check to see if any "registered" IT save formats (217?)
        zero = 0; disko_write(fp, &zero, 2);

        // here comes MIDI configuration
        // here comes MIDI configuration
        // right down MIDI configuration lane
        if (midi_flags & MIDI_EMBED_DATA) {
                //printf("attempting to embed %d bytes\n", sizeof(current_song->midi_config));
                disko_write(fp, &current_song->midi_config, sizeof(current_song->midi_config));
        }

        // IT puts something else here (timestamp?)
        // (need to change hdr.msgoffset above if adding other stuff here)
        disko_write(fp, current_song->message, msglen);

        // instruments, samples, and patterns
        for (n = 0; n < nins; n++) {
                para_ins[n] = disko_tell(fp);
                para_ins[n] = bswapLE32(para_ins[n]);
                _save_it_instrument(n, fp, 0);
        }
        for (n = 0; n < nsmp; n++) {
                // the sample parapointers are byte-swapped later
                para_smp[n] = disko_tell(fp);
                save_its_header(fp, current_song->samples + n + 1);
        }
        for (n = 0; n < npat; n++) {
                if (song_pattern_is_empty(n)) {
                        para_pat[n] = 0;
                } else {
                        para_pat[n] = disko_tell(fp);
                        para_pat[n] = bswapLE32(para_pat[n]);
                        _save_it_pattern(fp, current_song->patterns[n], current_song->pattern_size[n]);
                }
        }

        // sample data
        for (n = 0; n < nsmp; n++) {
                unsigned int tmp, op;
                song_sample_t *smp = current_song->samples + (n + 1);

                // Always save the data pointer, even if there's not actually any data being pointed to
                op = disko_tell(fp);
                tmp = bswapLE32(op);
                disko_seek(fp, para_smp[n]+0x48, SEEK_SET);
                disko_write(fp, &tmp, 4);
                disko_seek(fp, op, SEEK_SET);
                if (smp->data)
                        save_sample_data_LE(fp, smp, 1);
                // done using the pointer internally, so *now* swap it
                para_smp[n] = bswapLE32(para_smp[n]);
        }

        // rewrite the parapointers
        disko_seek(fp, 0xc0 + nord, SEEK_SET);
        disko_write(fp, para_ins, 4*nins);
        disko_write(fp, para_smp, 4*nsmp);
        disko_write(fp, para_pat, 4*npat);

        return SAVE_SUCCESS;
}

static int _save_stub(UNUSED disko_t *fp, UNUSED song_t *song)
{
        log_appendf(4, "File format temporarily removed");
        return SAVE_INTERNAL_ERROR;
}

/* blah... */
static int _export_head_stub(UNUSED disko_t *fp, UNUSED int bits, UNUSED int channels, UNUSED int rate)
{
        log_appendf(4, "Export not ready yet");
        return SAVE_INTERNAL_ERROR;
}

static int _export_body_stub(UNUSED disko_t *fp, UNUSED const uint8_t *data, UNUSED size_t length)
{
        log_appendf(4, "Export not ready yet");
        return SAVE_INTERNAL_ERROR;
}

static int _export_tail_stub(UNUSED disko_t *fp)
{
        log_appendf(4, "Export not ready yet");
        return SAVE_INTERNAL_ERROR;
}

/* ------------------------------------------------------------------------- */

struct save_format song_save_formats[] = {
        {"IT", "Impulse Tracker", "it", {.save_song = _save_it}},
        {"S3M", "Scream Tracker 3", "s3m", {.save_song = _save_stub}},
        {.label = NULL}
};

struct save_format song_export_formats[] = {
        {"WAV", "WAV", "wav", {.export = {_export_head_stub, _export_body_stub, _export_tail_stub}}},
        {.label = NULL}
};

struct save_format sample_save_formats[] = {
        {"ITS", "Impulse Tracker", "its", {.save_sample = fmt_its_save_sample}},
        //{"S3I", "Scream Tracker", "s3i", {.save_sample = fmt_s3i_save_sample}},
        {"AIFF", "Audio IFF", "aiff", {.save_sample = fmt_aiff_save_sample}},
        {"AU", "Sun/NeXT", "au", {.save_sample = fmt_au_save_sample}},
        {"WAV", "WAV", "wav", {.save_sample = fmt_wav_save_sample}},
        {"RAW", "Raw", "raw", {.save_sample = fmt_raw_save_sample}},
        {.label = NULL}
};

static struct save_format *get_save_format(struct save_format *formats, const char *label)
{
        int n;

        if (!label) {
                // why would this happen, ever?
                log_appendf(4, "No file type given, very weird! (Try a different filename?)");
                return NULL;
        }

        for (n = 0; formats[n].label; n++)
                if (strcmp(formats[n].label, label) == 0)
                        return formats + n;

        log_appendf(4, "Unknown save format %s", label);
        return NULL;
}


int song_export(UNUSED const char *filename, UNUSED const char *type)
{
        struct save_format *format = get_save_format(song_export_formats, type);

        if (!format)
                return SAVE_INTERNAL_ERROR;

        return SAVE_INTERNAL_ERROR;
}


int song_save(const char *filename, const char *type)
{
        int ret, backup;
        struct save_format *format = get_save_format(song_save_formats, type);

        if (!format)
                return SAVE_INTERNAL_ERROR;

        log_nl();
        log_nl();
        log_appendf(2, "Saving %s module", format->name);
        log_underline(strlen(format->name) + 14);

/* TODO: add or replace file extension as appropriate

From IT 2.10 update:
  - Automatic filename extension replacement on Ctrl-S, so that if you press
    Ctrl-S after loading a .MOD, .669, .MTM or .XM, the filename will be
    automatically modified to have a .IT extension.

In IT, if the filename given has no extension ("basename"), then the extension for the proper file type
(IT/S3M) will be appended to the name.
A filename with an extension is not modified, even if the extension is blank. (as in "basename.")

This brings up a rather odd bit of behavior: what happens when saving a file with the deliberately wrong
extension? Selecting the IT type and saving as "test.s3m" works just as one would expect: an IT file is
written, with the name "test.s3m". No surprises yet, but as soon as Ctrl-S is used, the filename is "fixed",
producing a second file called "test.it". The reverse happens when trying to save an S3M named "test.it" --
it's rewritten to "test.s3m".
Note that in these scenarios, Impulse Tracker *DOES NOT* check if an existing file by that name exists; it
will GLADLY overwrite the old "test.s3m" (or .it) with the renamed file that's being saved. Presumably this
is NOT intentional behavior.

Another note: if the file could not be saved for some reason or another, Impulse Tracker pops up a dialog
saying "Could not save file". This can be seen rather easily by trying to save a file with a malformed name,
such as "abc|def.it". This dialog is presented both when saving from F10 and Ctrl-S.
*/

        // ugly
        current_song->row_highlight_major = row_highlight_major;
        current_song->row_highlight_minor = row_highlight_minor;

        disko_t *fp = disko_open(filename);
        if (!fp) {
                log_perror(filename);
                return SAVE_FILE_ERROR;
        }

        ret = format->f.save_song(fp, current_song);
        if (ret != SAVE_SUCCESS)
                disko_seterror(fp, EINVAL);
        backup = ((status.flags & MAKE_BACKUPS)
                  ? (status.flags & NUMBERED_BACKUPS)
                  ? 65536 : 1 : 0);
        if (disko_close(fp, backup) == DW_ERROR && ret == SAVE_SUCCESS) {
                // this was not as successful as originally claimed!
                ret = SAVE_FILE_ERROR;
        }

        switch (ret) {
        case SAVE_SUCCESS:
                status.flags &= ~SONG_NEEDS_SAVE;
                if (strcasecmp(song_filename, filename))
                        song_set_filename(filename);
                log_appendf(5, " Done");
                break;
        case SAVE_FILE_ERROR:
                log_perror(filename);
                break;
        case SAVE_INTERNAL_ERROR:
        default: // ???
                log_appendf(4, " Internal error saving song");
                break;
        }

        return ret;
}

int song_save_sample(const char *filename, const char *type, song_sample_t *smp, int num)
{
        int ret;
        struct save_format *format = get_save_format(sample_save_formats, type);

        if (!format)
                return SAVE_INTERNAL_ERROR;

        if (!filename || !filename[0]) {
                status_text_flash("Error: Sample %d NOT saved! (%s)", num, "No Filename?");
                return SAVE_INTERNAL_ERROR; // ?
        }

        disko_t *fp = disko_open(filename);
        if (fp) {
                ret = format->f.save_sample(fp, smp);
                if (ret != SAVE_SUCCESS)
                        disko_seterror(fp, EINVAL);
                if (disko_close(fp, 0) == DW_ERROR && ret == SAVE_SUCCESS) {
                        // this was not as successful as originally claimed!
                        ret = SAVE_FILE_ERROR;
                }
        } else {
                ret = SAVE_FILE_ERROR;
        }

        switch (ret) {
        case SAVE_SUCCESS:
                status_text_flash("%s sample saved (sample %d)", format->name, num);
                break;
        case SAVE_FILE_ERROR:
                status_text_flash("Error: Sample %d NOT saved! (%s)", num, "File Error");
                log_perror(get_basename(filename));
                break;
        case SAVE_INTERNAL_ERROR:
        default: // ???
                status_text_flash("Error: Sample %d NOT saved! (%s)", num, "Internal error");
                log_appendf(4, "Internal error saving sample");
                break;
        }

        return ret;
}

// ------------------------------------------------------------------------

// All of the sample's fields are initially zeroed except the filename (which is set to the sample's
// basename and shouldn't be changed). A sample loader should not change anything in the sample until
// it is sure that it can accept the file.
// The title points to a buffer of 26 characters.

#define LOAD_SAMPLE(x) fmt_##x##_load_sample,
static fmt_load_sample_func load_sample_funcs[] = {
#include "fmt-types.h"
        NULL,
};

#define LOAD_INSTRUMENT(x) fmt_##x##_load_instrument,
static fmt_load_instrument_func load_instrument_funcs[] = {
#include "fmt-types.h"
        NULL,
};


void song_clear_sample(int n)
{
        song_lock_audio();
        csf_destroy_sample(current_song, n);
        memset(current_song->samples + n, 0, sizeof(song_sample_t));
        current_song->samples[n].c5speed = 8363;
        current_song->samples[n].volume = 64 * 4;
        current_song->samples[n].global_volume = 64;
        song_unlock_audio();
}

void song_copy_sample(int n, song_sample_t *src)
{
        memcpy(current_song->samples + n, src, sizeof(song_sample_t));

        if (src->data) {
                unsigned long bytelength = src->length;
                if (src->flags & CHN_16BIT)
                        bytelength *= 2;
                if (src->flags & CHN_STEREO)
                        bytelength *= 2;

                current_song->samples[n].data = csf_allocate_sample(bytelength);
                memcpy(current_song->samples[n].data, src->data, bytelength);
        }
}

int song_load_instrument_ex(int target, const char *file, const char *libf, int n)
{
        slurp_t *s;
        int sampmap[MAX_SAMPLES];
        int r, x;

        song_lock_audio();

        /* 0. delete old samples */
        memset(sampmap, 0, sizeof(sampmap));
        if (current_song->instruments[target]) {
                /* init... */
                for (unsigned int j = 0; j < 128; j++) {
                        x = current_song->instruments[target]->sample_map[j];
                        sampmap[x] = 1;
                }
                /* mark... */
                for (unsigned int q = 0; q < MAX_INSTRUMENTS; q++) {
                        if ((int) q == target) continue;
                        if (!current_song->instruments[q]) continue;
                        for (unsigned int j = 0; j < 128; j++) {
                                x = current_song->instruments[q]->sample_map[j];
                                sampmap[x] = 0;
                        }
                }
                /* sweep! */
                for (int j = 1; j < MAX_SAMPLES; j++) {
                        if (!sampmap[j]) continue;

                        csf_destroy_sample(current_song, j);
                        memset(current_song->samples + j, 0, sizeof(current_song->samples[j]));
                }
                /* now clear everything "empty" so we have extra slots */
                for (int j = 1; j < MAX_SAMPLES; j++) {
                        if (song_sample_is_empty(j)) sampmap[j] = 0;
                }
        }

        if (libf) { /* file is ignored */
                song_t *xl = song_create_load(libf);
                if (!xl) {
                        log_appendf(4, "%s: %s", libf, fmt_strerror(errno));
                        song_unlock_audio();
                        return 0;
                }

                /* 1. find a place for all the samples */
                memset(sampmap, 0, sizeof(sampmap));
                for (unsigned int j = 0; j < 128; j++) {
                        x = xl->instruments[n]->sample_map[j];
                        if (!sampmap[x]) {
                                if (x > 0 && x < MAX_INSTRUMENTS) {
                                        for (int k = 1; k < MAX_SAMPLES; k++) {
                                                if (current_song->samples[k].length) continue;
                                                sampmap[x] = k;
                                                //song_sample *smp = (song_sample *)song_get_sample(k);

                                                for (int c = 0; c < 25; c++) {
                                                        if (xl->samples[x].name[c] == 0)
                                                                xl->samples[x].name[c] = 32;
                                                }
                                                xl->samples[x].name[25] = 0;

                                                song_copy_sample(k, &xl->samples[x]);
                                                break;
                                        }
                                }
                        }
                }

                /* transfer the instrument */
                current_song->instruments[target] = xl->instruments[n];
                xl->instruments[n] = NULL; /* dangle */

                /* and rewrite! */
                for (unsigned int k = 0; k < 128; k++) {
                        current_song->instruments[target]->sample_map[k] = sampmap[
                                        current_song->instruments[target]->sample_map[k]
                        ];
                }

                song_unlock_audio();
                return 1;
        }

        /* okay, load an ITI file */
        s = slurp(file, NULL, 0);
        if (!s) {
                log_perror(file);
                song_unlock_audio();
                return 0;
        }

        r = 0;
        for (x = 0; load_instrument_funcs[x]; x++) {
                r = load_instrument_funcs[x](s->data, s->length, target);
                if (r) break;
        }

        unslurp(s);
        song_unlock_audio();

        return r;
}

int song_load_instrument(int n, const char *file)
{
        return song_load_instrument_ex(n,file,NULL,-1);
}

int song_preload_sample(dmoz_file_t *file)
{
        // 0 is our "hidden sample"
#define FAKE_SLOT 0
        //_squelch_sample(FAKE_SLOT);
        if (file->sample) {
                song_sample_t *smp = song_get_sample(FAKE_SLOT);

                song_lock_audio();
                csf_destroy_sample(current_song, FAKE_SLOT);
                song_copy_sample(FAKE_SLOT, file->sample);
                strncpy(smp->name, file->title, 25);
                smp->name[25] = 0;
                strncpy(smp->filename, file->base, 12);
                smp->filename[12] = 0;
                song_unlock_audio();
                return FAKE_SLOT;
        }
        return song_load_sample(FAKE_SLOT, file->path) ? FAKE_SLOT : KEYJAZZ_NOINST;
#undef FAKE_SLOT
}

int song_load_sample(int n, const char *file)
{
        fmt_load_sample_func *load;
        song_sample_t smp;

        const char *base = get_basename(file);
        slurp_t *s = slurp(file, NULL, 0);

        if (s == NULL) {
                log_perror(base);
                return 0;
        }

        // set some default stuff
        song_lock_audio();
        _squelch_sample(n);
        memset(&smp, 0, sizeof(smp));
        strncpy(smp.name, base, 25);

        for (load = load_sample_funcs; *load; load++) {
                if ((*load)(s->data, s->length, &smp)) {
                        break;
                }
        }

        if (!load) {
                unslurp(s);
                log_perror(base);
                song_unlock_audio();
                return 0;
        }

        // this is after the loaders because i don't trust them, even though i wrote them ;)
        strncpy(smp.filename, base, 12);
        smp.filename[12] = 0;
        smp.name[25] = 0;

        csf_destroy_sample(current_song, n);
        if (((unsigned char)smp.name[23]) == 0xFF) {
                // don't load embedded samples
                // (huhwhat?!)
                smp.name[23] = ' ';
        }
        memcpy(&(current_song->samples[n]), &smp, sizeof(song_sample_t));
        song_unlock_audio();

        unslurp(s);

        return 1;
}

void song_create_host_instrument(int smp)
{
        int ins = instrument_get_current();

        if (song_instrument_is_empty(smp))
                ins = smp;
        else if ((status.flags & CLASSIC_MODE) || !song_instrument_is_empty(ins))
                ins = song_first_unused_instrument();

        if (ins) {
                song_init_instrument_from_sample(ins, smp);
                status_text_flash("Sample assigned to Instrument %d", ins);
        } else {
                status_text_flash("Error: No available Instruments!");
        }
}

// ------------------------------------------------------------------------

int song_save_instrument(int n, const char *file)
{
        song_instrument_t *ins = current_song->instruments[n];

        log_appendf(2, "Saving instrument %s", file);
        if (!ins) {
                /* this should never happen */
                log_appendf(4, "Instrument %d: there is no spoon", n);
                return 0;
        }

        if (file[0] == '\0') {
                log_appendf(4, "Instrument %d: no filename", n);
                return 0;
        }
        disko_t *fp = disko_open(file);
        if (!fp) {
                log_perror(get_basename(file));
                return 0;
        }
        _save_it_instrument(n-1 /* grr.... */, fp, 1);
        if (disko_close(fp, 0) == DW_ERROR) {
                log_perror(get_basename(file));
                return 0;
        }
        return 1;
}

// ------------------------------------------------------------------------
// song information

const char *song_get_filename(void)
{
        return song_filename;
}

const char *song_get_basename(void)
{
        return song_basename;
}

// ------------------------------------------------------------------------
// sample library browsing

// FIXME: unload the module when leaving the library 'directory'
static song_t *library = NULL;


// TODO: stat the file?
int dmoz_read_instrument_library(const char *path, dmoz_filelist_t *flist, UNUSED dmoz_dirlist_t *dlist)
{
        unsigned int j;
        int x;

        _squelch_sample(0);
        csf_free(library);

        const char *base = get_basename(path);
        library = song_create_load(path);
        if (!library) {
                log_appendf(4, "%s: %s", base, fmt_strerror(errno));
                return -1;
        }

        for (int n = 1; n < MAX_INSTRUMENTS; n++) {
                if (!library->instruments[n])
                        continue;

                dmoz_file_t *file = dmoz_add_file(flist,
                        str_dup(path), str_dup(base), NULL, n);
                file->title = str_dup(library->instruments[n]->name);

                int count[128];
                memset(count, 0, sizeof(count));

                file->sampsize = 0;
                file->filesize = 0;
                file->instnum = n;
                for (j = 0; j < 128; j++) {
                        x = library->instruments[n]->sample_map[j];
                        if (!count[x]) {
                                if (x > 0 && x < MAX_INSTRUMENTS) {
                                        file->filesize += library->samples[x].length;
                                        file->sampsize++;
                                }
                        }
                        count[x]++;
                }

                file->type = TYPE_INST_ITI;
                file->description = "Fishcakes";
                // IT doesn't support this, despite it being useful.
                // Simply "unrecognized"
        }

        return 0;
}


int dmoz_read_sample_library(const char *path, dmoz_filelist_t *flist, UNUSED dmoz_dirlist_t *dlist)
{
        _squelch_sample(0);
        csf_free(library);

        const char *base = get_basename(path);
        library = song_create_load(path);
        if (!library) {
                /* FIXME: try loading as an instrument before giving up */
                log_appendf(4, "%s: %s", base, fmt_strerror(errno));
                errno = ENOTDIR;
                return -1;
        }

        for (int n = 1; n < MAX_SAMPLES; n++) {
                if (library->samples[n].length) {
                        for (int c = 0; c < 25; c++) {
                                if (library->samples[n].name[c] == 0)
                                        library->samples[n].name[c] = 32;
                                library->samples[n].name[25] = 0;
                        }
                        dmoz_file_t *file = dmoz_add_file(flist, str_dup(path), str_dup(base), NULL, n);
                        file->type = TYPE_SAMPLE_EXTD;
                        file->description = "Fishcakes"; // FIXME - what does IT say?
                        file->smp_speed = library->samples[n].c5speed;
                        file->smp_loop_start = library->samples[n].loop_start;
                        file->smp_loop_end = library->samples[n].loop_end;
                        file->smp_sustain_start = library->samples[n].sustain_start;
                        file->smp_sustain_end = library->samples[n].sustain_end;
                        file->smp_length = library->samples[n].length;
                        file->smp_flags = library->samples[n].flags;
                        file->smp_defvol = library->samples[n].volume>>2;
                        file->smp_gblvol = library->samples[n].global_volume;
                        file->smp_vibrato_speed = library->samples[n].vib_speed;
                        file->smp_vibrato_depth = library->samples[n].vib_depth;
                        file->smp_vibrato_rate = library->samples[n].vib_rate;
                        // don't screw this up...
                        if (((unsigned char)library->samples[n].name[23]) == 0xFF) {
                                library->samples[n].name[23] = ' ';
                        }
                        file->title = str_dup(library->samples[n].name);
                        file->sample = (song_sample_t *) library->samples + n;
                }
        }

        return 0;
}

// ------------------------------------------------------------------------
// instrument loader

song_instrument_t *instrument_loader_init(struct instrumentloader *ii, int slot)
{
        ii->expect_samples = 0;
        ii->inst = song_get_instrument(slot);
        ii->slot = slot;
        ii->basex = 1;
        memset(ii->sample_map, 0, sizeof(ii->sample_map));
        return ii->inst;
}

int instrument_loader_abort(struct instrumentloader *ii)
{
        int n;
        song_wipe_instrument(ii->slot);
        for (n = 0; n < MAX_SAMPLES; n++) {
                if (ii->sample_map[n]) {
                        song_delete_sample(ii->sample_map[n]-1);
                        ii->sample_map[n] = 0;
                }
        }
        return 0;
}

int instrument_loader_sample(struct instrumentloader *ii, int slot)
{
        int x;
        if (!slot) return 0;
        if (ii->sample_map[slot]) return ii->sample_map[slot];
        for (x = ii->basex; x < MAX_SAMPLES; x++) {
                if (!song_sample_is_empty(x-1)) continue;

                ii->expect_samples++;
                ii->sample_map[slot] = x;
                ii->basex = x+1;
                return ii->sample_map[slot];
        }
        status_text_flash("Too many samples");
        return 0;
}

