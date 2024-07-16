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

#include "charset.h"
#include "log.h"

#include <utf8proc.h>

/* despite the name, this just returns utf8. */
uint8_t *charset_compose_to_utf8(const uint8_t *in, charset_t set)
{
	int allocated = 0;
	const uint8_t *utf8;
	uint8_t *alloc_ptr; /* need this to actually be const */

	if (set == CHARSET_UTF8) {
		utf8 = in;
	} else {
		if (charset_iconv(in, &alloc_ptr, set, CHARSET_UTF8))
			return NULL;
		utf8 = alloc_ptr;
		allocated = 1;
	}

	uint8_t *composed;
	if (utf8proc_map(utf8, 0, &composed, UTF8PROC_NULLTERM | UTF8PROC_COMPOSE) < 0) {
		if (allocated)
			free(alloc_ptr);

		return NULL;
	}

	if (allocated)
		free(alloc_ptr);

	return composed;
}

/* returns data in the same set as `set`. to compose to UTF-8, use
 * charset_compose_to_utf8() */
uint8_t *charset_compose(const uint8_t *in, charset_t set)
{
	uint8_t *composed = charset_compose_to_utf8(in, set);
	if (set == CHARSET_UTF8)
		return composed;

	uint8_t *composed_in_set;
	if (charset_iconv(composed, &composed_in_set, CHARSET_UTF8, set)) {
		free(composed);
		return NULL;
	}

	free(composed);
	return composed_in_set;
}

/* case folds and decomposes the characters in `in` */
uint8_t *charset_case_fold_to_utf8(const uint8_t *in, charset_t set)
{
	int allocated = 0;
	const uint8_t *utf8;
	uint8_t *alloc_ptr;

	if (set == CHARSET_UTF8) {
		utf8 = in;
	} else {
		if (charset_iconv(in, &alloc_ptr, set, CHARSET_UTF8))
			return NULL;

		utf8 = alloc_ptr;
		allocated = 1;
	}

	uint8_t *folded;
	if (utf8proc_map(utf8, 0, &folded, UTF8PROC_NULLTERM | UTF8PROC_CASEFOLD | UTF8PROC_DECOMPOSE) < 0) {
		if (allocated)
			free(alloc_ptr);

		return NULL;
	}

	if (allocated)
		free(alloc_ptr);

	return folded;
}

uint8_t *charset_case_fold(const uint8_t *in, charset_t set)
{
	uint8_t *folded = charset_compose_to_utf8(in, set);
	if (set == CHARSET_UTF8)
		return folded;

	uint8_t *folded_in_set;
	if (charset_iconv(folded, &folded_in_set, CHARSET_UTF8, set)) {
		free(folded);
		return NULL;
	}

	free(folded);
	return folded_in_set;
}
