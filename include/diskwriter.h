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
#ifndef __diskwriter_h
#define __diskwriter_h

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct diskwriter_driver diskwriter_driver_t;
struct diskwriter_driver {
        const char *name; /* this REALLY needs to be short (3 characters) */
        const char *extension; /* no dot */
        int export_only;

        /* supplied by driver
                p is called before anything else (optional)

                m does some mutation/work
                g receives midi events

                x is called at the end (optional)

                these functions definitely need better names /storlek
        */
        /* (preceding comment left for posterity)
        now that I've spent about 2 hours unobfuscating all of this code, here's what
        these functions REALLY DO:
                * first thing, when a .wav (or whatever export-format) write is started,
                  the disk writer calls 'header', so that the driver can do whatever setup
                  is necessary; namely, writing out the file header (or more realistically,
                  seeking past it, since the file length and other stuff that's likely to
                  go in a header isn't known yet)
                * then, 'writeaudio' and 'writemidi' are called as necessary while the file
                  is played through, and are given various chunks of data
                * at the end of the song, the 'finish' function is called; this is the time
                  to seek back to that incomplete header and fill in the fields (or do
                  anything else that has to happen at the end)
                * 'configure' is called FIRST, but seeing as it's not ever used, its true
                  purpose is evidently to make the struct slightly bigger -- sort of like
                  the button on the Alt-T dialog in the sample editor :P
                * write/seek/error are completely unrelated to the driver, and handle the
                  actual data being output to the file.
        */
        void (*header)(diskwriter_driver_t *x);
        void (*writeaudio)(diskwriter_driver_t *x, const void *buf, unsigned int len);
        void (*writemidi)(diskwriter_driver_t *x, const void *buf, unsigned int len, unsigned int delay);
        void (*finish)(diskwriter_driver_t *x);

        /* supplied by diskwriter (write function) */
        void (*write)(diskwriter_driver_t *x, const void *buf, unsigned int len);
        void (*seek)(diskwriter_driver_t *x, off_t pos);
        /* error condition */
        void (*error)(diskwriter_driver_t *x);

        /* supplied by driver
                if "s" is supplied, schism will call it and expect IT
                to call diskwriter_start(0,0) again when its actually ready.

                this routine is supplied to allow drivers to write dialogs for
                accepting some configuration
        */
        void (*configure)(diskwriter_driver_t *x);

        /* untouched by diskwriter; driver may use for anything */
        void *userdata;

        /* sound config */
        unsigned int rate, bits, channels;
        int output_le;

        /* used by readers, etc */
        off_t pos;
};

enum {
        DW_OK = 1,
        DW_ERROR = 0,
        DW_NOT_RUNNING = -1,

        DW_SYNC_DONE = 0,
        DW_SYNC_ERROR = -1,
        DW_SYNC_MORE = 1,
};

/* starts up the diskwriter.
return values: DW_OK, DW_ERROR */
int diskwriter_start(const char *file, diskwriter_driver_t *f);

/* multiout */
int diskwriter_multiout(const char *dir, diskwriter_driver_t *f);

/* kindler, gentler, (and most importantly) simpler version (can't call sync) */
int diskwriter_writeout(const char *file, diskwriter_driver_t *f);

/* copy a pattern into a sample */
int diskwriter_writeout_sample(int sampno, int patno, int bindme);

/* this synchronizes with the diskwriter.
return: DW_SYNC_*, self explanatory */
int diskwriter_sync(void);

/* Terminate the diskwriter.
        If called BEFORE diskwriter_sync() returns DW_OK, this will delete any
        temporary files created; otherwise, it will commit them.
return: DW_OK or DW_ERROR */
int diskwriter_finish(void);

/* ------------------------------------------------------------------------- */

extern diskwriter_driver_t *diskwriter_drivers[];

/* this call is used by audio/loadsave to send midi data */
int _diskwriter_writemidi(const void *data, unsigned int len, unsigned int delay);

/* these are used inbetween diskwriter interfaces */
void diskwriter_dialog_progress(unsigned int perc);
void diskwriter_dialog_finished(void);


extern unsigned int diskwriter_output_rate, diskwriter_output_bits,
                        diskwriter_output_channels;


extern diskwriter_driver_t wavewriter;
extern diskwriter_driver_t itwriter;
extern diskwriter_driver_t s3mwriter;
extern diskwriter_driver_t xmwriter;
extern diskwriter_driver_t modwriter;
extern diskwriter_driver_t mtmwriter;
extern diskwriter_driver_t midiwriter;

#ifdef __cplusplus
};
#endif

#endif

