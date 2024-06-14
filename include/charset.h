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
#ifndef SCHISM_CHARSET_H_
#define SCHISM_CHARSET_H_

#include <stdint.h>

/* UCS4 shouldn't ever be used externally; the output depends on endianness.
 * It should only be used as sort of an in-between from UTF-8 to CP437 for use
 * where the strings can be edited, e.g. in the file selector */
typedef enum {
	/* Unicode */
	CHARSET_UCS4,
	CHARSET_UTF8,
	CHARSET_UTF16LE,
	CHARSET_UTF16BE,

	/* European languages */
	CHARSET_CP437,
	CHARSET_WINDOWS1252, /* thanks modplug! */

	/* CHARSET_CHAR is special; it first tries UTF-8
	 * in our internal decoder, then we hand it off
	 * to SDL, which may or may not actually handle
	 * it correctly, depending on whether it uses
	 * the system iconv or its own. */
	CHARSET_CHAR,
	CHARSET_WCHAR_T
} charset_t;

typedef enum {
	CHARSET_ERROR_SUCCESS = 0,
	CHARSET_ERROR_UNIMPLEMENTED = -1,
	CHARSET_ERROR_INPUTISOUTPUT = -2,
	CHARSET_ERROR_NULLINPUT = -3,
	CHARSET_ERROR_NULLOUTPUT = -4,
	CHARSET_ERROR_DECODE = -5,
	CHARSET_ERROR_ENCODE = -6,
	CHARSET_ERROR_NOMEM = -7,
} charset_error_t;

int char_digraph(int k1, int k2);
uint8_t char_unicode_to_cp437(unsigned int c);
uint32_t charset_simple_case_fold(uint32_t codepoint);

/* charset-aware replacements for C stdlib functions */
int charset_strcmp(const uint8_t* in1, charset_t in1set, const uint8_t* in2, charset_t in2set);
int charset_strcasecmp(const uint8_t* in1, charset_t in1set, const uint8_t* in2, charset_t in2set);

const char* charset_iconv_error_lookup(charset_error_t err);
charset_error_t charset_iconv(const uint8_t* in, uint8_t** out, charset_t inset, charset_t outset);

/* This is a simple macro for easy charset conversion.
 *
 * Sample usage:
 *    CHARSET_EASY_MODE(in, CHARSET_CHAR, CHARSET_CP437, {
 *        do_something_with(out);
 *    });
 *
 * The macro will handle freeing the result. It cannot
 * be used outside of the block you feed to it.
 * If you need that, call charset_iconv() directly.
 */
#define CHARSET_EASY_MODE_EX(MOD, in, inset, outset, x) \
	do { \
		MOD uint8_t* out; \
		charset_error_t err = charset_iconv(in, (uint8_t**)&out, inset, outset); \
		if (err) \
			out = in; \
	\
		x \
	\
		if (!err) \
			free((uint8_t*)out); \
	} while (0)

#define CHARSET_EASY_MODE(in, inset, outset, x) CHARSET_EASY_MODE_EX(, in, inset, outset, x)
#define CHARSET_EASY_MODE_CONST(in, inset, outset, x) CHARSET_EASY_MODE_EX(const, in, inset, outset, x)

#endif /* SCHISM_CHARSET_H_ */
