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

#include "auto/helptext.h" /* help_text is defined here */

/* --------------------------------------------------------------------- */

char *help_text_pointers[HELP_NUM_ITEMS] = { NULL };

void setup_help_text_pointers(void)
{
        int n;
        char *ptr = (char*)help_text;
	
        for (n = 0; n < HELP_NUM_ITEMS; n++) {
                help_text_pointers[n] = ptr;
                ptr = strchr(ptr, 0) + 1;
        }
}
