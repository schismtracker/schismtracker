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

/*
mrsbrisby:

thanks for obfuscating this code, it was really fun untangling
all of those nondescript function names like "p" and "wl".
especially the ones that weren't actually used anywhere, those
were a lot of fun to figure out.
*/

#define NEED_BYTESWAP
#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"
#include "util.h"
#include "song.h"
#include "sndfile.h"
#include "dmoz.h"


#include "cmixer.h"
#include "disko.h"

#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define DW_BUFFER_SIZE 65536

// ---------------------------------------------------------------------------
// stdio backend

static void _dw_stdio_write(disko_t *ds, const void *buf, size_t len)
{
        if (fwrite(buf, len, 1, ds->file) != 1)
                disko_seterror(ds, errno);
}

static void _dw_stdio_putc(disko_t *ds, int c)
{
        if (fputc(c, ds->file) == EOF)
                disko_seterror(ds, errno);
}

static void _dw_stdio_seek(disko_t *ds, long pos, int whence)
{
        if (fseek(ds->file, pos, whence) < 0)
                disko_seterror(ds, errno);
}

static long _dw_stdio_tell(disko_t *ds)
{
        long pos = ftell(ds->file);
        if (pos < 0)
                disko_seterror(ds, errno);
        return pos;
}

// ---------------------------------------------------------------------------
// memory backend

// TODO: actually allocate memory, and adjust sizes

static void _dw_mem_write(disko_t *ds, UNUSED const void *buf, UNUSED size_t len)
{
        // TODO
        disko_seterror(ds, ENOSYS);
}

static void _dw_mem_putc(disko_t *ds, UNUSED int c)
{
        // TODO
        disko_seterror(ds, ENOSYS);
}

static void _dw_mem_seek(disko_t *ds, long offset, int whence)
{
        // mostly from slurp_seek
        switch (whence) {
        default:
        case SEEK_SET:
                break;
        case SEEK_CUR:
                offset += ds->pos;
                break;
        case SEEK_END:
                offset += ds->length;
                break;
        }
        if (offset < 0) {
                disko_seterror(ds, EINVAL);
                return;
        }
        if (offset >= ds->allocated) {
                // TODO reallocate here? later?
        }
        ds->pos = offset;
}

static long _dw_mem_tell(disko_t *ds)
{
        return (long) ds->pos;
}

// ---------------------------------------------------------------------------

void disko_write(disko_t *ds, const void *buf, size_t len)
{
        if (len != 0 && !ds->error)
                ds->_write(ds, buf, len);
}

void disko_putc(disko_t *ds, int c)
{
        if (!ds->error)
                ds->_putc(ds, c);
}

void disko_seek(disko_t *ds, long pos, int whence)
{
        if (!ds->error)
                ds->_seek(ds, pos, whence);
}

long disko_tell(disko_t *ds)
{
        if (!ds->error)
                return ds->_tell(ds);
        return -1;
}


void disko_seterror(disko_t *ds, int err)
{
        // Don't set an error if one already exists, and don't allow clearing an error value
        ds->error = errno = ds->error ?: err ?: EINVAL;
}

// ---------------------------------------------------------------------------

disko_t *disko_open(const char *filename)
{
        size_t len;
        int fd;
        int err;

        if (!filename)
                return NULL;

        len = strlen(filename);
        if (len + 6 >= PATH_MAX) {
                errno = ENAMETOOLONG;
                return NULL;
        }

        // Attempt to honor read-only (since we're writing them in such a roundabout way)
        if (access(filename, W_OK) != 0 && errno != ENOENT)
                return NULL;

        disko_t *ds = calloc(1, sizeof(disko_t));
        if (!ds)
                return NULL;

        // This might seem a bit redundant, but allows for flexibility later
        // (e.g. putting temp files elsewhere, or mangling the filename in some other way)
        strcpy(ds->filename, filename);
        strcpy(ds->tempname, filename);
        strcat(ds->tempname, "XXXXXX");

        fd = mkstemp(ds->tempname);
        if (fd == -1) {
                free(ds);
                return NULL;
        }
        ds->file = fdopen(fd, "wb");
        if (ds->file == NULL) {
                err = errno;
                close(fd);
                unlink(ds->tempname);
                free(ds);
                errno = err;
                return NULL;
        }

        setvbuf(ds->file, NULL, _IOFBF, DW_BUFFER_SIZE);

        ds->_write = _dw_stdio_write;
        ds->_seek = _dw_stdio_seek;
        ds->_tell = _dw_stdio_tell;
        ds->_putc = _dw_stdio_putc;

        return ds;
}

int disko_close(disko_t *ds, int backup)
{
        int err = ds->error;

        // try to preserve the *first* error set, because it's most likely to be interesting
        if (fclose(ds->file) == EOF && !err) {
                err = errno;
        } else if (!err) {
                // preserve file mode, or set it sanely -- mkstemp() sets file mode to 0600
                struct stat st;
                if (stat(ds->filename, &st) < 0) {
                        /* Probably didn't exist already, let's make something up.
                        0777 is "safer" than 0, so we don't end up throwing around world-writable
                        files in case something weird happens.
                        See also: man 3 getumask */
                        mode_t m = umask(0777);
                        umask(m);
                        st.st_mode = 0666 & ~m;
                }
                if (backup) {
                        // back up the old file
                        make_backup_file(ds->filename, (backup != 1));
                }
                if (rename_file(ds->tempname, ds->filename, 1) != 0) {
                        err = errno;
                } else {
                        // Fix the permissions on the file
                        chmod(ds->filename, st.st_mode);
                }
        }
        // If anything failed so far, kill off the temp file
        if (err) {
                unlink(ds->tempname);
        }
        free(ds);
        if (err) {
                errno = err;
                return DW_ERROR;
        } else {
                return DW_OK;
        }
}


disko_t *disko_memopen(void)
{
        disko_t *ds = calloc(1, sizeof(disko_t));
        if (!ds)
                return NULL;

        ds->data = calloc(DW_BUFFER_SIZE, sizeof(uint8_t));
        if (!ds->data) {
                free(ds);
                return NULL;
        }
        ds->allocated = DW_BUFFER_SIZE;

        ds->_write = _dw_mem_write;
        ds->_seek = _dw_mem_seek;
        ds->_tell = _dw_mem_tell;
        ds->_putc = _dw_mem_putc;

        return ds;
}

int disko_memclose(disko_t *ds, int keep_buffer)
{
        if (!keep_buffer)
                free(ds->data);
        free(ds);
        return DW_OK;
}

// ---------------------------------------------------------------------------

/* these are used for pattern->sample linking crap */

int disko_writeout_sample(UNUSED int sampno, UNUSED int patno, UNUSED int dobind)
{
        return DW_ERROR;
}

int disko_writeout(UNUSED const char *file, UNUSED disko_t *f)
{
        return DW_ERROR;
}


/* main calls this periodically when the .wav exporter is busy */
int disko_sync(void)
{
        return DW_SYNC_DONE;
}

/* called from main and disko_dialog */
int disko_finish(void)
{
        return DW_NOT_RUNNING; /* no writer running */
}



/* called from audio_playback.c _schism_midi_out_raw() */
int _disko_writemidi(UNUSED const void *data, UNUSED unsigned int len, UNUSED unsigned int delay)
{
        return DW_ERROR;
}


unsigned int disko_output_rate = 44100;
unsigned int disko_output_bits = 16;
unsigned int disko_output_channels = 2;

