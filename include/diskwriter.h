/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
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
	void (*p)(diskwriter_driver_t *x);
	void (*m)(diskwriter_driver_t *x, unsigned char *buf, unsigned int len);
	void (*g)(diskwriter_driver_t *x, unsigned char *buf, unsigned int len, unsigned int delay);
	void (*x)(diskwriter_driver_t *x);

	/* supplied by diskwriter (write function) */
	void (*o)(diskwriter_driver_t *x, const unsigned char *buf,
							unsigned int len);
	void (*l)(diskwriter_driver_t *x, off_t pos);
	/* error condition */
	void (*e)(diskwriter_driver_t *x); 

	/* supplied by driver
		if "s" is supplied, schism will call it and expect IT
		to call diskwriter_start(0,0) again when its actually ready.

		this routine is supplied to allow drivers to write dialogs for
		accepting some configuration
	*/
	void (*s)(diskwriter_driver_t *x);

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
int _diskwriter_writemidi(unsigned char *data, unsigned int len,
						unsigned int delay);

/* these are used inbetween diskwriter interfaces */
void diskwriter_dialog_progress(unsigned int perc);
void diskwriter_dialog_finished(void);


extern unsigned int diskwriter_output_rate, diskwriter_output_bits,
			diskwriter_output_channels;

#ifdef __cplusplus
};
#endif

#endif
