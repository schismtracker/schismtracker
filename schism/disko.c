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

// 0 => memory error, abandon ship
static int _dw_bufcheck(disko_t *ds, size_t extend)
{
        if (ds->pos + extend <= ds->length)
                return 1;

        ds->length = MAX(ds->length, ds->pos + extend);

        if (ds->length >= ds->allocated) {
                size_t newsize = MAX(ds->allocated + DW_BUFFER_SIZE, ds->length);
                uint8_t *new = realloc(ds->data, newsize);
                if (!new) {
                        // Eek
                        free(ds->data);
                        ds->data = NULL;
                        disko_seterror(ds, ENOMEM);
                        return 0;
                }
                ds->data = new;
                ds->allocated = newsize;
        }
        return 1;
}

static void _dw_mem_write(disko_t *ds, const void *buf, size_t len)
{
        if (_dw_bufcheck(ds, len)) {
                memcpy(ds->data + ds->pos, buf, len);
                ds->pos += len;
        }
}

static void _dw_mem_putc(disko_t *ds, int c)
{
        if (_dw_bufcheck(ds, 1))
                ds->data[ds->pos++] = c;
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
        /* note: seeking doesn't cause a buffer resize. This is consistent with the behavior of stdio streams.
        Consider:
                FILE *f = fopen("f", "w");
                fseek(f, 1000, SEEK_SET);
                fclose(f);
        This will produce a zero-byte file; the size is not extended until data is written. */
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

#ifndef GEKKO /* FIXME - make a replacement access() */
        // Attempt to honor read-only (since we're writing them in such a roundabout way)
        if (access(filename, W_OK) != 0 && errno != ENOENT)
                return NULL;
#endif

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
#ifndef GEKKO /* FIXME - autoconf check for this instead */
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
#endif
                if (backup) {
                        // back up the old file
                        make_backup_file(ds->filename, (backup != 1));
                }
                if (rename_file(ds->tempname, ds->filename, 1) != 0) {
                        err = errno;
                } else {
#ifndef GEKKO
                        // Fix the permissions on the file
                        chmod(ds->filename, st.st_mode);
#endif
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
        int err = ds->error;
        if (!keep_buffer)
                free(ds->data);
        free(ds);
        if (err) {
                errno = err;
                return DW_ERROR;
        } else {
                return DW_OK;
        }
}

// ---------------------------------------------------------------------------

struct dw_settings {
        uint32_t prev_freq, prev_bits, prev_channels, prev_flags;
        int bps;
};

static void _export_setup(song_t *dwsong, struct dw_settings *set)
{
        song_lock_audio();

        /* save the real settings */
        set->prev_freq = mix_frequency;
        set->prev_bits = mix_bits_per_sample;
        set->prev_channels = mix_channels;
        set->prev_flags = mix_flags;

        /* install our own */
        memcpy(dwsong, current_song, sizeof(song_t)); /* shadow it */
        csf_set_wave_config(dwsong, 44100, 16, (dwsong->flags & SONG_NOSTEREO) ? 1 : 2);
        csf_initialize_dsp(dwsong, 1);
        mix_flags |= SNDMIX_DIRECTTODISK | SNDMIX_NOBACKWARDJUMPS;

        set->bps = mix_channels * ((mix_bits_per_sample + 7) / 8);

        dwsong->stop_at_order = -1;
        dwsong->stop_at_row = -1;
        csf_set_current_order(dwsong, 0); /* rather indirect way of resetting playback variables */
        dwsong->flags &= ~(SONG_PAUSED | SONG_ENDREACHED);
}

static void _export_teardown(struct dw_settings *set)
{
        /* put everything back */
        csf_set_wave_config(current_song, set->prev_freq, set->prev_bits, set->prev_channels);
        mix_flags = set->prev_flags;

        song_unlock_audio();
}

// ---------------------------------------------------------------------------

int disko_writeout_sample(int smpnum, int pattern, int dobind)
{
        int ret = DW_OK;
        song_t dwsong;
        song_sample_t *sample;
        uint8_t buf[DW_BUFFER_SIZE];
        disko_t *ds;
        uint8_t *dsdata;
        struct dw_settings set;

        if (smpnum < 1 || smpnum >= MAX_SAMPLES)
                return DW_ERROR;

        ds = disko_memopen();
        if (!ds)
                return DW_ERROR;

        _export_setup(&dwsong, &set);
        dwsong.repeat_count = 2; /* THIS MAKES SENSE! */
        csf_loop_pattern(&dwsong, pattern, 0);

        do {
                disko_write(ds, buf, csf_read(&dwsong, buf, sizeof(buf)) * set.bps);
                if (ds->length >= MAX_SAMPLE_LENGTH * set.bps) {
                        /* roughly 3 minutes at 44khz -- surely big enough (?) */
                        ds->length = MAX_SAMPLE_LENGTH * set.bps;
                        dwsong.flags |= SONG_ENDREACHED;
                }
        } while (!(dwsong.flags & SONG_ENDREACHED));

        dsdata = ds->data;
        if (disko_memclose(ds, 1) == DW_OK) {
                sample = current_song->samples + smpnum;
                if (sample->data)
                        csf_free_sample(sample->data);
                sample->data = csf_allocate_sample(ds->length);
                if (sample->data) {
                        memcpy(sample->data, dsdata, ds->length);
                        sample->length = ds->length / set.bps;
                        sample->flags &= ~(CHN_16BIT | CHN_STEREO | CHN_ADLIB);
                        if (mix_channels > 1)
                                sample->flags |= CHN_STEREO;
                        if (mix_bits_per_sample > 8)
                                sample->flags |= CHN_16BIT;
                        sample->c5speed = mix_frequency;
                        sprintf(sample->name, "Pattern %03d", pattern);
                        if (dobind) {
                                /* This is hideous */
                                sample->name[23] = 0xff;
                                sample->name[24] = pattern;
                        }
                }
        } else {
                /* Balls. Something died. */
                ret = DW_ERROR;
        }
        free(dsdata);

        _export_teardown(&set);

        return ret;
}

// ---------------------------------------------------------------------------

int disko_export_song(const char *filename, struct save_format *format)
{
        int ret = DW_OK;
        song_t dwsong;
        uint8_t buf[DW_BUFFER_SIZE];
        disko_t *ds;
        struct dw_settings set;

        ds = disko_open(filename);
        if (!ds)
                return DW_ERROR;

        _export_setup(&dwsong, &set);
        dwsong.repeat_count = 1; /* THIS MAKES SENSE! */

        ret = format->f.export.head(ds, mix_bits_per_sample, mix_channels, mix_frequency);
        if (ret == DW_OK) {
                do {
                        format->f.export.body(ds, buf, csf_read(&dwsong, buf, sizeof(buf)) * set.bps);
                } while (!(dwsong.flags & SONG_ENDREACHED));
                if (format->f.export.tail(ds) != DW_OK)
                        disko_seterror(ds, errno);
        } else {
                disko_seterror(ds, errno);
        }
        ret = disko_close(ds, 0);

        _export_teardown(&set);

        return ret;
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

