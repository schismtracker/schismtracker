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

#include "headers.h"

#include "config-parser.h"
#include "dialog.h"
#include "disko.h"
#include "dmoz.h"
#include "it.h"
#include "page.h"
#include "song.h"
#include "song.h"
#include "util.h"
#include "vgamem.h"

#include "player/sndfile.h"
#include "player/cmixer.h"

#include <sys/stat.h>

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

#define DW_BUFFER_SIZE 65536

// ---------------------------------------------------------------------------

static unsigned int disko_output_rate = 44100;
static unsigned int disko_output_bits = 16;
static unsigned int disko_output_channels = 2;

void cfg_load_disko(cfg_file_t *cfg)
{
	disko_output_rate = cfg_get_number(cfg, "Diskwriter", "rate", 44100);
	disko_output_bits = cfg_get_number(cfg, "Diskwriter", "bits", 16);
	disko_output_channels = cfg_get_number(cfg, "Diskwriter", "channels", 2);
}

void cfg_save_disko(cfg_file_t *cfg)
{
	cfg_set_number(cfg, "Diskwriter", "rate", disko_output_rate);
	cfg_set_number(cfg, "Diskwriter", "bits", disko_output_bits);
	cfg_set_number(cfg, "Diskwriter", "channels", disko_output_channels);
}

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
			disko_seterror(ds, errno);
			return 0;
		}
		memset(new + ds->allocated, 0, newsize - ds->allocated);
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

/* used by multi-write */
static void disko_seekcur(disko_t *ds, long pos)
{
	disko_seek(ds, pos, SEEK_CUR);
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
	ds->error = errno = ds->error ? ds->error : err ? err : EINVAL;
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

#ifndef SCHISM_WII /* FIXME - make a replacement access() */
	// Attempt to honor read-only (since we're writing them in such a roundabout way)
	if (access(filename, W_OK) != 0 && errno != ENOENT)
		return NULL;
#endif

	disko_t *ds = calloc(1, sizeof(disko_t));
	if (!ds)
		return NULL;

	memcpy(ds->filename, filename, len * sizeof(char));
	memcpy(ds->tempname, filename, len * sizeof(char));
	memcpy(ds->tempname + len, "XXXXXX", 6 * sizeof(char));

#ifdef SCHISM_WIN32
	{
		if (win32_mktemp(ds->tempname, sizeof(ds->tempname)/sizeof(ds->tempname[0]))) {
			free(ds);
			return NULL;
		}

		ds->file = win32_fopen(ds->tempname, "wb");
		if (!ds->file) {
			free(ds);
			return NULL;
		}
	}
#else
	fd = mkstemp(ds->tempname);
	if (fd == -1) {
		free(ds);
		return NULL;
	}
	ds->file = fdopen(fd, "wb");
	if (!ds->file) {
		err = errno;
		close(fd);
		unlink(ds->tempname);
		free(ds);
		errno = err;
		return NULL;
	}
#endif

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
#ifndef SCHISM_WII /* FIXME - autoconf check for this instead */
		struct stat st;
		if (os_stat(ds->filename, &st) < 0) {
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
#ifndef SCHISM_WII
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
	if (!keep_buffer || err)
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

static void _export_setup(song_t *dwsong, int *bps)
{
	song_lock_audio();

	/* install our own */
	memcpy(dwsong, current_song, sizeof(song_t)); /* shadow it */

	dwsong->multi_write = NULL; /* should be null already, but to be sure... */

	csf_set_current_order(dwsong, 0); /* rather indirect way of resetting playback variables */
	csf_set_wave_config(dwsong, disko_output_rate, disko_output_bits,
		(dwsong->flags & SONG_NOSTEREO) ? 1 : disko_output_channels);

	dwsong->mix_flags |= SNDMIX_DIRECTTODISK | SNDMIX_NOBACKWARDJUMPS;

	dwsong->repeat_count = -1; // FIXME do this right
	dwsong->buffer_count = 0;
	dwsong->flags &= ~(SONG_PAUSED | SONG_PATTERNLOOP | SONG_ENDREACHED);
	dwsong->stop_at_order = -1;
	dwsong->stop_at_row = -1;

	*bps = dwsong->mix_channels * ((dwsong->mix_bits_per_sample + 7) / 8);

	song_unlock_audio();
}

static void _export_teardown(void)
{
	global_vu_left = global_vu_right = 0;
}

// ---------------------------------------------------------------------------

static int close_and_bind(song_t *dwsong, disko_t *ds, song_sample_t *sample, int bps)
{
	disko_t dsshadow = *ds;
	int8_t *newdata;

	if (disko_memclose(ds, 1) == DW_ERROR) {
		return DW_ERROR;
	}

	newdata = csf_allocate_sample(dsshadow.length);
	if (!newdata)
		return DW_ERROR;
	csf_stop_sample(current_song, sample);
	if (sample->data)
		csf_free_sample(sample->data);
	sample->data = newdata;

	memcpy(newdata, dsshadow.data, dsshadow.length);
	sample->length = dsshadow.length / bps;
	sample->flags &= ~(CHN_16BIT | CHN_STEREO | CHN_ADLIB);
	if (dwsong->mix_channels > 1)
		sample->flags |= CHN_STEREO;
	if (dwsong->mix_bits_per_sample > 8)
		sample->flags |= CHN_16BIT;
	sample->c5speed = dwsong->mix_frequency;
	sample->name[0] = '\0';

	return DW_OK;
}

int disko_writeout_sample(int smpnum, int pattern, int dobind)
{
	int ret = DW_OK;
	song_t dwsong;
	song_sample_t *sample;
	uint8_t buf[DW_BUFFER_SIZE];
	disko_t *ds;
	int bps;

	if (smpnum < 1 || smpnum >= MAX_SAMPLES)
		return DW_ERROR;

	ds = disko_memopen();
	if (!ds)
		return DW_ERROR;

	_export_setup(&dwsong, &bps);
	dwsong.repeat_count = -1; // FIXME do this right
	csf_loop_pattern(&dwsong, pattern, 0);

	do {
		disko_write(ds, buf, csf_read(&dwsong, buf, sizeof(buf)) * bps);
		if (ds->length >= (size_t) (MAX_SAMPLE_LENGTH * bps)) {
			/* roughly 3 minutes at 44khz -- surely big enough (?) */
			ds->length = MAX_SAMPLE_LENGTH * bps;
			dwsong.flags |= SONG_ENDREACHED;
		}
	} while (!(dwsong.flags & SONG_ENDREACHED));

	sample = current_song->samples + smpnum;
	if (close_and_bind(&dwsong, ds, sample, bps) == DW_OK) {
		sprintf(sample->name, "Pattern %03d", pattern);
		if (dobind) {
			/* This is hideous */
			sample->name[23] = 0xff;
			sample->name[24] = pattern;
		}
	} else {
		/* Balls. Something died. */
		ret = DW_ERROR;
	}

	_export_teardown();

	return ret;
}

int disko_multiwrite_samples(int firstsmp, int pattern)
{
	int err = 0;
	song_t dwsong;
	song_sample_t *sample;
	uint8_t buf[DW_BUFFER_SIZE];
	disko_t *ds[MAX_CHANNELS] = {NULL};
	int bps;
	size_t smpsize = 0;
	int smpnum = CLAMP(firstsmp, 1, MAX_SAMPLES);
	int n;

	_export_setup(&dwsong, &bps);
	dwsong.repeat_count = -1; // FIXME do this right
	csf_loop_pattern(&dwsong, pattern, 0);
	dwsong.multi_write = calloc(MAX_CHANNELS, sizeof(struct multi_write));
	if (!dwsong.multi_write)
		err = errno ? errno : ENOMEM;

	if (!err) {
		for (n = 0; n < MAX_CHANNELS; n++) {
			ds[n] = disko_memopen();
			if (!ds[n]) {
				err = errno ? errno : EINVAL;
				break;
			}
		}
	}

	if (err) {
		/* you might think this code is insane, and you might be correct ;)
		but it's structured like this to keep all the early-termination handling HERE. */
		_export_teardown();
		err = err ? err : errno;
		free(dwsong.multi_write);
		for (n = 0; n < MAX_CHANNELS; n++)
			disko_memclose(ds[n], 0);
		errno = err;
		return DW_ERROR;
	}

	for (n = 0; n < MAX_CHANNELS; n++) {
		dwsong.multi_write[n].data = ds[n];
		/* Dumb casts. (written this way to make the definition of song_t independent of disko) */
		dwsong.multi_write[n].write = (void(*)(void*, const uint8_t*, size_t))disko_write;
		dwsong.multi_write[n].silence = (void(*)(void*, long))disko_seekcur;
	}

	do {
		/* buf is used as temp space for converting the individual channel buffers from 32-bit.
		the output is being handled well inside the mixer, so we don't have to do any actual writing
		here, but we DO need to make sure nothing died... */
		smpsize += csf_read(&dwsong, buf, sizeof(buf));

		if (smpsize >= MAX_SAMPLE_LENGTH) {
			/* roughly 3 minutes at 44khz -- surely big enough (?) */
			smpsize = MAX_SAMPLE_LENGTH;
			dwsong.flags |= SONG_ENDREACHED;
			break;
		}

		for (n = 0; n < MAX_CHANNELS; n++) {
			if (ds[n]->error) {
				// Kill the write, but leave the other files alone
				dwsong.flags |= SONG_ENDREACHED;
				break;
			}
		}
	} while (!(dwsong.flags & SONG_ENDREACHED));

	for (n = 0; n < MAX_CHANNELS; n++) {
		if (!dwsong.multi_write[n].used) {
			/* this channel was completely empty - don't bother with it */
			disko_memclose(ds[n], 0);
			continue;
		}

		ds[n]->length = MIN(ds[n]->length, smpsize * bps);
		smpnum = csf_first_blank_sample(current_song, smpnum);
		if (smpnum < 0)
			break;
		sample = current_song->samples + smpnum;
		if (close_and_bind(&dwsong, ds[n], sample, bps) == DW_OK) {
			sprintf(sample->name, "Pattern %03d, channel %02d", pattern, n + 1);
		} else {
			/* Balls. Something died. */
			err = errno;
		}
	}

	for (; n < MAX_CHANNELS; n++) {
		if (disko_memclose(ds[n], 0) != DW_OK) {
			err = errno;
		}
	}

	_export_teardown();
	free(dwsong.multi_write);

	if (err) {
		errno = err;
		return DW_ERROR;
	} else {
		return DW_OK;
	}
}

// ---------------------------------------------------------------------------

static song_t export_dwsong;
static int export_bps;
static disko_t *export_ds[MAX_CHANNELS + 1]; /* only [0] is used unless multichannel */
static const struct save_format *export_format = NULL; /* NULL == not running */
static struct widget diskodlg_widgets[1];
static size_t est_len;
static int prgh;
static struct timeval export_start_time;
static int canceled = 0; /* this sucks, but so do I */

static int disko_finish(void);

static void diskodlg_draw(void)
{
	int sec, pos;
	char buf[32];

	if (!export_ds[0]) {
		/* what are we doing here?! */
		dialog_destroy_all();
		log_appendf(4, "disk export dialog was eaten by a grue!");
		return;
	}

	sec = export_ds[0]->length / export_dwsong.mix_frequency;
	pos = export_ds[0]->length * 64 / est_len;
	snprintf(buf, 32, "Exporting song...%6d:%02d", sec / 60, sec % 60);
	buf[31] = '\0';
	draw_text(buf, 27, 27, 0, 2);
	draw_fill_chars(24, 30, 55, 30, DEFAULT_FG, 0);
	draw_vu_meter(24, 30, 32, pos, prgh, prgh);
	draw_box(23, 29, 56, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void diskodlg_cancel(UNUSED void *ignored)
{
	canceled = 1;
	export_dwsong.flags |= SONG_ENDREACHED;
	if (!export_ds[0]) {
		log_appendf(4, "export was already dead on the inside");
		return;
	}
	for (int n = 0; export_ds[n]; n++)
		disko_seterror(export_ds[n], EINTR);

	/* The next disko_sync will notice the (artifical) error status and call disko_finish,
	which will clean up all the files.
	'canceled' prevents disko_finish from making a second call to dialog_destroy (since
	this function is already being called in response to the dialog being canceled) and
	also affects the message it prints at the end. */
}

static void disko_dialog_setup(size_t len);

// this needs to be done to work around stupid inconsistent key-up code
static void diskodlg_reset(UNUSED void *ignored)
{
	disko_dialog_setup(est_len);
}

static void disko_dialog_setup(size_t len)
{
	struct dialog *d = dialog_create_custom(22, 25, 36, 8, diskodlg_widgets, 0, 0, diskodlg_draw, NULL);
	d->action_yes = diskodlg_reset;
	d->action_no = diskodlg_reset;
	d->action_cancel = diskodlg_cancel;

	canceled = 0; /* stupid */

	est_len = len;
	uint32_t r = (rand() >> 8) & 63;
	if (r <= 7) {
		prgh = 6;
	} else if (r <= 18) {
		prgh = 3;
	} else if (r <= 31) {
		prgh = 5;
	} else {
		prgh = 4;
	}
}

// ---------------------------------------------------------------------------

static char *get_filename(const char *template, int n)
{
	char *s, *sub, buf[4];

	s = strdup(template);
	if (!s)
		return NULL;
	sub = strcasestr(s, "%c");
	if (!sub) {
		errno = EINVAL;
		free(s);
		return NULL;
	}
	num99tostr(n, buf);
	sub[0] = buf[0];
	sub[1] = buf[1];
	return s;
}

int disko_export_song(const char *filename, const struct save_format *format)
{
	int err = 0;
	int numfiles, n;

	if (export_format) {
		log_appendf(4, "Another export is already active");
		errno = EAGAIN;
		return DW_ERROR;
	}

	gettimeofday(&export_start_time, NULL);

	numfiles = format->f.export.multi ? MAX_CHANNELS : 1;

	_export_setup(&export_dwsong, &export_bps);
	if (numfiles > 1) {
		export_dwsong.multi_write = calloc(numfiles, sizeof(struct multi_write));
		if (!export_dwsong.multi_write)
			err = errno ? errno : ENOMEM;
	}

	memset(export_ds, 0, sizeof(export_ds));
	for (n = 0; n < numfiles; n++) {
		if (numfiles > 1) {
			char *tmp = get_filename(filename, n + 1);
			if (tmp) {
				export_ds[n] = disko_open(tmp);
				free(tmp);
			}
		} else {
			export_ds[n] = disko_open(filename);
		}
		if (!(export_ds[n] && format->f.export.head(export_ds[n], export_dwsong.mix_bits_per_sample,
				export_dwsong.mix_channels, export_dwsong.mix_frequency) == DW_OK)) {
			err = errno ? errno : EINVAL;
			break;
		}
	}

	if (err) {
		_export_teardown();
		free(export_dwsong.multi_write);
		for (n = 0; export_ds[n]; n++) {
			disko_seterror(export_ds[n], err); /* keep from writing a bunch of useless files */
			disko_close(export_ds[n], 0);
		}
		errno = err ? err : EINVAL;
		log_perror(filename);
		return DW_ERROR;
	}

	if (numfiles > 1) {
		for (n = 0; n < numfiles; n++) {
			export_dwsong.multi_write[n].data = export_ds[n];
			/* Dumb casts, again */
			export_dwsong.multi_write[n].write = (void(*)(void*, const uint8_t*, size_t))format->f.export.body;
			export_dwsong.multi_write[n].silence = (void(*)(void*, long))format->f.export.silence;
		}
	}

	log_appendf(5, " %" PRIu32 " Hz, %" PRIu32 " bit, %s",
		export_dwsong.mix_frequency, export_dwsong.mix_bits_per_sample,
		export_dwsong.mix_channels == 1 ? "mono" : "stereo");
	export_format = format;
	status.flags |= DISKWRITER_ACTIVE; /* tell main to care about us */

	uint32_t s = (csf_get_length(&export_dwsong) * export_dwsong.mix_frequency);
	disko_dialog_setup(s ? s : 1);

	return DW_OK;
}


/* main calls this periodically when the .wav exporter is busy */
int disko_sync(void)
{
	uint8_t buf[DW_BUFFER_SIZE];
	size_t frames;
	int n;

	if (!export_format) {
		log_appendf(4, "disko_sync: unexplained bacon");
		return DW_SYNC_ERROR; /* no writer running (why are we here?) */
	}

	frames = csf_read(&export_dwsong, buf, sizeof(buf));

	if (!export_dwsong.multi_write)
		export_format->f.export.body(export_ds[0], buf, frames * export_bps);
	/* always check if something died, multi-write or not */
	for (n = 0; export_ds[n]; n++) {
		if (export_ds[n]->error) {
			disko_finish();
			return DW_SYNC_ERROR;
		}
	}

	/* update the progress bar (kind of messy, yes...) */
	export_ds[0]->length += frames;
	status.flags |= NEED_UPDATE;

	if (export_dwsong.flags & SONG_ENDREACHED) {
		disko_finish();
		return DW_SYNC_DONE;
	} else {
		return DW_SYNC_MORE;
	}
}

static int disko_finish(void)
{
	int ret = DW_OK, n, tmp;
	struct timeval export_end_time;
	double elapsed;
	int num_files = 0;
	size_t total_size = 0; // in bytes
	size_t samples_0;

	if (!export_format) {
		log_appendf(4, "disko_finish: unexplained eggs");
		return DW_ERROR; /* no writer running (why are we here?) */
	}

	if (!canceled)
		dialog_destroy();

	samples_0 = export_ds[0]->length;
	for (n = 0; export_ds[n]; n++) {
		if (export_dwsong.multi_write && !export_dwsong.multi_write[n].used) {
			/* this channel was completely empty - don't bother with it */
			disko_seterror(export_ds[n], EINVAL); /* kludge */
			disko_close(export_ds[n], 0);
		} else {
			/* there was noise on this channel */
			num_files++;
			if (export_format->f.export.tail(export_ds[n]) != DW_OK) {
				disko_seterror(export_ds[n], errno);
			} else {
				disko_seek(export_ds[n], 0, SEEK_END);
				total_size += disko_tell(export_ds[n]);
			}
			tmp = disko_close(export_ds[n], 0);
			if (ret == DW_OK)
				ret = tmp;
		}
	}
	memset(export_ds, 0, sizeof(export_ds));

	_export_teardown();
	free(export_dwsong.multi_write);
	export_format = NULL;

	status.flags &= ~DISKWRITER_ACTIVE; /* please unsubscribe me from your mailing list */

	switch (ret) {
	case DW_OK:
		gettimeofday(&export_end_time, NULL);
		elapsed = (export_end_time.tv_sec - export_start_time.tv_sec)
			+ ((export_end_time.tv_usec - export_start_time.tv_usec) / 1000000.0);

		/* it would be more useful if this actually got the real size of the files */
		log_appendf(5, " %.2f MiB (%zu:%zu) written in %.2lf sec",
			total_size / 1048576.0,
			samples_0 / disko_output_rate / 60, (samples_0 / disko_output_rate) % 60,
			elapsed);
		break;
	case DW_ERROR:
		/* hey, what was the filename? oops */
		if (canceled)
			log_appendf(5, " Canceled");
		else
			log_perror(" Write error");
		break;
	default:
		log_appendf(5, " Internal error exporting song");
		break;
	}

	return ret;
}

// ---------------------------------------------------------------------------

struct pat2smp {
	int pattern, sample, bind;
};

static void pat2smp_single(void *data)
{
	struct pat2smp *ps = data;

	if (disko_writeout_sample(ps->sample, ps->pattern, ps->bind) == DW_OK) {
		set_page(PAGE_SAMPLE_LIST);
	} else {
		log_perror("Sample write");
		status_text_flash("Error writing to sample");
	}

	free(ps);
}

static void pat2smp_multi(void *data)
{
	struct pat2smp *ps = data;
	if (disko_multiwrite_samples(ps->sample, ps->pattern) == DW_OK) {
		set_page(PAGE_SAMPLE_LIST);
	} else {
		log_perror("Sample multi-write");
		status_text_flash("Error writing to samples");
	}

	free(ps);
}

void song_pattern_to_sample(int pattern, int split, int bind)
{
	struct pat2smp *ps;
	int n;

	if (split && bind) {
		log_appendf(4, "song_pattern_to_sample: internal error!");
		return;
	}

	if (pattern < 0 || pattern >= MAX_PATTERNS) {
		return;
	}

	/* this is horrid */
	for (n = 1; n < MAX_SAMPLES; n++) {
		song_sample_t *samp = song_get_sample(n);
		if (!samp) continue;
		if (((unsigned char) samp->name[23]) != 0xFF) continue;
		if (((unsigned char) samp->name[24]) != pattern) continue;
		status_text_flash("Pattern %d already linked to sample %d", pattern, n);
		return;
	}

	ps = mem_alloc(sizeof(struct pat2smp));
	ps->pattern = pattern;

	int samp = sample_get_current();
	ps->sample = samp ? samp : 1;

	ps->bind = bind;

	if (split) {
		/* Nothing to confirm, as this never overwrites samples */
		pat2smp_multi(ps);
	} else {
		if (current_song->samples[ps->sample].data == NULL) {
			pat2smp_single(ps);
		} else {
			dialog_create(DIALOG_OK_CANCEL, "This will replace the current sample.",
				pat2smp_single, free, 1, ps);
		}
	}
}

// ---------------------------------------------------------------------------

/* called from audio_playback.c _schism_midi_out_raw() */
int _disko_writemidi(UNUSED const void *data, UNUSED unsigned int len, UNUSED unsigned int delay)
{
	return DW_ERROR;
}
