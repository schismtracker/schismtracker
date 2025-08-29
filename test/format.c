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

static char string_format_buffer[10000];
static int string_format_buffer_next = 0;

void test_format_string_reset(void)
{
	memset(string_format_buffer, 0, sizeof(string_format_buffer));
	string_format_buffer_next = 0;
}

const char *test_format_string(const char *str)
{
	int chars;
	char *allocated_space;

	if (str == NULL)
		return "NULL";

	chars = strlen(str) + 3;

	if (string_format_buffer_next + chars >= ARRAY_SIZE(string_format_buffer)) {
		// Fallback
		return str;
	}

	allocated_space = string_format_buffer + string_format_buffer_next;

	string_format_buffer_next += chars;

	sprintf(allocated_space, "\"%s\"", str);

	return allocated_space;
}
