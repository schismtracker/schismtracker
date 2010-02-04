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
#ifndef __disko_h
#define __disko_h

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct disko disko_t;
struct disko {
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
        */
        void (*header)(disko_t *x);
        void (*writeaudio)(disko_t *x, const void *buf, unsigned int len);
        void (*writemidi)(disko_t *x, const void *buf, unsigned int len, unsigned int delay);
        void (*finish)(disko_t *x);

        /* don't touch these in the diskwriter drivers -- use disko_write etc. */
        void (*_write)(disko_t *x, const void *buf, unsigned int len);
        void (*_seek)(disko_t *x, off_t pos, int whence);
        void (*_seterror)(disko_t *x);

        /* supplied by driver
                if "s" is supplied, schism will call it and expect IT
                to call disko_start(0,0) again when its actually ready.

                this routine is supplied to allow drivers to write dialogs for
                accepting some configuration
        */
        void (*configure)(disko_t *x);

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
int disko_start(const char *file, disko_t *f);

/* multiout */
int disko_multiout(const char *dir, disko_t *f);

/* kindler, gentler, (and most importantly) simpler version (can't call sync) */
int disko_writeout(const char *file, disko_t *f);

/* copy a pattern into a sample */
int disko_writeout_sample(int sampno, int patno, int bindme);

/* this synchronizes with the diskwriter.
return: DW_SYNC_*, self explanatory */
int disko_sync(void);

/* Terminate the diskwriter.
        If called BEFORE disko_sync() returns DW_OK, this will delete any
        temporary files created; otherwise, it will commit them.
return: DW_OK or DW_ERROR */
int disko_finish(void);



/* For use by the diskwriter drivers: */

/* Write data to the file, as in fwrite() */
void disko_write(disko_t *ds, const void *buf, unsigned int len);

/* Change file position. This CAN be used to seek past the end,
but be cognizant that random data might exist in the "gap". */
void disko_seek(disko_t *ds, off_t pos, int whence);

/* Call this to signal a nonrecoverable error condition. */
void disko_seterror(disko_t *ds);

/* ------------------------------------------------------------------------- */

extern disko_t *disko_formats[];

/* this call is used by audio/loadsave to send midi data */
int _disko_writemidi(const void *data, unsigned int len, unsigned int delay);

/* these are used inbetween diskwriter interfaces */
void disko_dialog_progress(unsigned int perc);
void disko_dialog_finished(void);


extern unsigned int disko_output_rate, disko_output_bits,
                        disko_output_channels;


extern disko_t wavewriter;
extern disko_t itwriter;
extern disko_t s3mwriter;
extern disko_t xmwriter;
extern disko_t modwriter;
extern disko_t mtmwriter;
extern disko_t midiwriter;

#ifdef __cplusplus
};
#endif

#endif

