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
#ifndef SCHISM_CLIPPY_H_
#define SCHISM_CLIPPY_H_

#include "it.h"
#include "page.h"

#define CLIPPY_SELECT   0       /* reflects the current selection */
#define CLIPPY_BUFFER   1       /* reflects the yank/cut buffer */

/* called when schism needs a paste operation; cb is CLIPPY_SELECT if the middle button is
used to paste, otherwise if the "paste key" is pressed, this uses CLIPPY_BUFFER */
void clippy_paste(int cb);

/* updates the clipboard selection; called by various widgets that perform a copy operation;
stops at the first null, so setting len to <0 means we get the next utf8 or asciiz string */
void clippy_select(struct widget *w, char *addr, int len);
struct widget *clippy_owner(int cb);

/* copies the selection to the yank buffer (0 -> 1) */
void clippy_yank(void);

// initializes the backend
int clippy_init(void);
void clippy_quit(void);

#endif /* SCHISM_CLIPPY_H_ */
