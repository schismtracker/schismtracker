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

// An implementation of the C99 vsnprintf() function that only uses
// functions required by C89. It's kind of bloat-y, but I don't see
// any other way of doing this besides using tmpfile()
int vsnprintf(char *buffer, size_t count, const char *fmt, va_list ap)
{
	FILE *f;
	int x;

	// Add a NUL terminator to the buffer before anything else,
	// as to prevent cases where this function fails and the
	// surrounding code always expects the result to be a valid
	// C string.
	if (count > 0)
		buffer[0] = '\0';

	f = tmpfile();
	if (!f)
		return -1;

	x = vfprintf(f, fmt, ap);
	if (x < 0) {
		// that's a wrap!
		fclose(f);
		return -1;
	}

	if (count > 0) {
		count = CLAMP(x, 0, count - 1);

		// Now read back the file into our buffer
		if (fseek(f, 0, SEEK_SET)) {
			fclose(f);
			return -1;
		}

		if (fread(buffer, 1, count, f) != count) {
			fclose(f);
			buffer[count] = '\0'; // WUH?
			return -1;
		}

		// Force NUL terminate
		buffer[count] = '\0';
	}

	fclose(f);

	return x;
}