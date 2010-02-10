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


typedef struct disko disko_t;
struct disko {
        // Functions whose implementation depends on the backend in use
        // Use disko_write et al. instead of these.
        void (*_write)(disko_t *ds, const void *buf, size_t len);
        void (*_putc)(disko_t *ds, int c);
        void (*_seek)(disko_t *ds, long offset, int whence);
        long (*_tell)(disko_t *ds);

        // Temporary filename that's being written to
        char tempname[PATH_MAX];

        // Name to change it to on close (if successful)
        char filename[PATH_MAX];

        // these could be unionized
        // file pointer (only exists for disk files)
        FILE *file;
        // data for memory buffers (no filename/handle)
        uint8_t *data;

        // First errno value recorded after something went wrong.
        int error;

        /* untouched by diskwriter; driver may use for anything */
        void *userdata;

        // for memory buffers
        size_t pos, length, allocated;
};

enum {
        DW_OK = 1,
        DW_ERROR = 0,
        DW_NOT_RUNNING = -1,
};
enum {
        DW_SYNC_DONE = 0,
        DW_SYNC_ERROR = -1,
        DW_SYNC_MORE = 1,
};

/* fopen/fclose-ish writeout/finish wrapper that allocates a structure */
disko_t *disko_open(const char *filename);
/* Close the file. If there was no error writing the file, it is renamed
to the name specified in disko_open; otherwise, the original file is left
intact and the temporary file is deleted. Returns DW_OK on success,
DW_ERROR (and sets errno) if there was a file error.
'backup' parameter:
        <1  don't backup
        =1  backup to file~
        >1  keep numbered backups to file.n~
(the semantics of this might change later to allow finer control) */
int disko_close(disko_t *f, int backup);

/* alloc/free a memory buffer
if free_buffer is 0, the internal buffer is left alone when deallocating,
so that it can continue to be used later */
disko_t *disko_memopen(void);
int disko_memclose(disko_t *f, int free_buffer);


/* copy a pattern into a sample */
int disko_writeout_sample(int smpnum, int pattern, int bind);

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
void disko_write(disko_t *ds, const void *buf, size_t len);

/* Write one character (unsigned char, cast to int) */
void disko_putc(disko_t *ds, int c);

/* Change file position. This CAN be used to seek past the end,
but be cognizant that random data might exist in the "gap". */
void disko_seek(disko_t *ds, long pos, int whence);

/* Get the position, as set by seek */
long disko_tell(disko_t *ds);

/* Call this to signal a nonrecoverable error condition. */
void disko_seterror(disko_t *ds, int err);

/* ------------------------------------------------------------------------- */

extern disko_t *disko_formats[];

/* this call is used by audio/loadsave to send midi data */
int _disko_writemidi(const void *data, unsigned int len, unsigned int delay);

/* these are used inbetween diskwriter interfaces */
void disko_dialog_progress(unsigned int perc);
void disko_dialog_finished(void);


extern unsigned int disko_output_rate, disko_output_bits,
                        disko_output_channels;


#endif

