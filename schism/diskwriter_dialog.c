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
#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"

#include "diskwriter.h"

/* -------------------------------------------------------------------------------- */

static struct widget _diskwriter_widgets[1];
static struct dialog *dg = NULL;
static int dg_init = 0;

static unsigned int dg_progress = 0;

static void _diskwriter_draw_const(void)
{
	int i, x;

	if (status.flags & DISKWRITER_ACTIVE_PATTERN) {
		draw_text((const unsigned char *)"Updating sample...", 30, 27, 0, 2);
		draw_text((const unsigned char *)"Please wait...", 34, 33, 0, 2); /* no cancel button */
	} else {
		draw_text((const unsigned char *)"Writing song to disk...", 28, 27, 0, 2);
	}
	draw_fill_chars(24,30,55,30,0);

	x = (int)((((float)dg_progress) / 100.0)*64.0);
	draw_vu_meter(24, 30, 32, x, 4, 4);
	draw_box(23, 29, 56, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}
static void _diskwriter_cancel(UNUSED void*ignored)
{
	if (status.flags & DISKWRITER_ACTIVE_PATTERN) return; /* err? */
	if (dg != NULL) {
		diskwriter_finish(); /* eek! */
		dg = NULL;
	}
}
void diskwriter_dialog_progress(unsigned int perc)
{
	if (dg_init == 0) {
		dg_init = 1;
		create_button(_diskwriter_widgets+0, 36, 33, 6,
				0,0,0,0,0, dialog_cancel_NULL, "Cancel", 1);
	}
	if (!dg) {
		dg = dialog_create_custom(22,25,36,11,
			_diskwriter_widgets,
			(status.flags & DISKWRITER_ACTIVE_PATTERN ? 0 : 1),
			0,
			_diskwriter_draw_const,
			NULL);
		if (!(status.flags & DISKWRITER_ACTIVE_PATTERN)) {
			dg->action_yes = _diskwriter_cancel;
			dg->action_cancel = _diskwriter_cancel;
		}
	}

	dg_progress = perc;
}
void diskwriter_dialog_finished(void)
{
	if (dg) {
		dg = NULL;
		if (status.dialog_type != DIALOG_NONE)
			dialog_cancel_NULL();
		dialog_destroy_all(); /* poop */
	}
	dg_progress = 100;
}
