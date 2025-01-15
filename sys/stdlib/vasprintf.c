/* Like vsprintf but provides a pointer to malloc'd storage, which must
   be freed by the caller.
   Copyright (C) 1994 Free Software Foundation, Inc.

This file is part of the libiberty library.
Libiberty is free software; you can redistribute it and/or
modify it under the terms of the GNU Library General Public
License as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later version.

Libiberty is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Library General Public License for more details.

You should have received a copy of the GNU Library General Public
License along with libiberty; see the file COPYING.LIB.  If
not, write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
Boston, MA 02111-1307, USA.  */

#include "headers.h"

int vasprintf(char **result, const char *format, va_list args)
{
	va_list args2;

	va_copy(args2, args);

	int num = vsnprintf(NULL, 0, format, args);
	if (num < 0) {
		va_end(args2);
		return -1;
	}

	*result = malloc(num + 1);
	if (!*result) {
		va_end(args2);
		return -1;
	}

	vsnprintf(*result, num + 1, format, args2);

	va_end(args2);

	return num;
}
