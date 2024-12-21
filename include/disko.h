/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
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
#ifndef SCHISM_DISKO_H_
#define SCHISM_DISKO_H_

#include "headers.h"

typedef struct disko disko_t;
struct disko {
	// Functions whose implementation depends on the backend in use
	// Use disko_write et al. instead of these.
	void (*_write)(disko_t *ds, const void *buf, size_t len);
	void (*_seek)(disko_t *ds, int64_t offset, int whence);
	int64_t (*_tell)(disko_t *ds);

	// Temporary filename that's being written to
	char *tempname;

	// Name to change it to on close (if successful)
	char *filename;

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

/* fopen/fclose-ish writeout/finish wrapper that shoves data into the
 * user-allocated structure */
int disko_open(disko_t *ds, const char *filename);
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
int disko_memopen(disko_t *f);
int disko_memclose(disko_t *f, int free_buffer);


/* copy a pattern into a sample */
int disko_writeout_sample(int smpnum, int pattern, int bind);

/* copy a pattern into multiple samples (split by channel,
and writing into free slots starting at smpnum) */
int disko_multiwrite_samples(int firstsmp, int pattern);

/* Wrapper for the above that writes to the current sample,
and with a confirmation dialog if the sample already has data */
void song_pattern_to_sample(int pattern, int split, int bind);

/* export the song to a file */
struct save_format;
int disko_export_song(const char *filename, const struct save_format *format);

/* call periodically if (status.flags & DISKWRITER_ACTIVE) to write more stuff.
return: DW_SYNC_*, self explanatory */
int disko_sync(void);



/* For use by the diskwriter drivers: */

/* Write data to the file, as in fwrite() */
void disko_write(disko_t *ds, const void *buf, size_t len);

/* Write one character */
void disko_putc(disko_t *ds, unsigned char c);

/* Change file position. This CAN be used to seek past the end,
but be cognizant that random data might exist in the "gap". */
void disko_seek(disko_t *ds, int64_t pos, int whence);

/* Get the position, as set by seek */
int64_t disko_tell(disko_t *ds);

/* Call this to signal a nonrecoverable error condition. */
void disko_seterror(disko_t *ds, int err);

/* Call this to seek to an aligned byte boundary */
void disko_align(disko_t *ds, uint32_t bytes);

/* ------------------------------------------------------------------------- */

/* this call is used by audio/loadsave to send midi data */
int _disko_writemidi(const void *data, unsigned int len, unsigned int delay);

#endif /* SCHISM_DISKO_H_ */

