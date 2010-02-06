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
        void (*_write)(disko_t *d, const void *buf, unsigned int len);
        void (*_seek)(disko_t *d, long pos, int whence);
        void (*_close)(disko_t *d);
        void (*_seterror)(disko_t *d);

        // these could be unionized
        FILE *file;
        uint8_t *data;

        /* untouched by diskwriter; driver may use for anything */
        void *userdata;

        /* used by readers, etc */
        size_t pos, length;
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

/* kindler, gentler, (and most importantly) simpler version (can't call sync)
NOTE: used by orderlist and pattern editor */
int disko_writeout(const char *file, disko_t *f);

/* fopen/fclose-ish writeout/finish wrapper that allocates a structure */
disko_t *disko_open(const char *filename);
int disko_close(disko_t *f);

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

#ifdef __cplusplus
};
#endif

#endif

