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

#include "headers.h"

/* UCS4 shouldn't ever be used externally; the output depends on endianness.
 * It should only be used as sort of an in-between from UTF-8 to CP437 for use
 * where the strings can be edited, e.g. in the file selector */
typedef enum {
	/* Unicode */
	CHARSET_UCS4,
	CHARSET_UTF8,
	CHARSET_UTF16LE,
	CHARSET_UTF16BE,
	CHARSET_UCS2LE,
	CHARSET_UCS2BE,
#ifdef WORDS_BIGENDIAN
# define CHARSET_UTF16 CHARSET_UTF16BE
# define CHARSET_UCS2  CHARSET_UCS2BE
#else
# define CHARSET_UTF16 CHARSET_UTF16LE
# define CHARSET_UCS2  CHARSET_UCS2LE
#endif

	/* European languages */
	CHARSET_CP437,
	CHARSET_WINDOWS1252, /* thanks modplug! */

	/* Windows cludge */
#ifdef SCHISM_WIN32
	CHARSET_ANSI,
#endif

	/* Uses the system's HFS encoding */
#ifdef SCHISM_MACOS
	CHARSET_HFS,
#endif

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

enum {
	DECODER_STATE_INVALID_CHAR = -4, /* character unavailable in destination */
	DECODER_STATE_ILL_FORMED = -3,   /* input buffer is ill-formed */
	DECODER_STATE_OVERFLOWED = -2,   /* reached past input buffer size */
	DECODER_STATE_ERROR = -1,        /* unknown generic decoding error */
	DECODER_STATE_NEED_MORE = 0,     /* needs more bytes */
	DECODER_STATE_DONE = 1,          /* decoding done! */
};

typedef struct {
	/* -- input, set by the caller */
	const unsigned char *in;  /* input buffer */
	size_t size;              /* size of the buffer, can be SIZE_MAX if unknown */
	size_t offset;            /* current decoding offset, should always be set to zero */

	/* -- output, decoder initializes these */
	uint32_t codepoint; /* decoded codepoint if successful, undefined if not */
	int state;          /* one of DECODER_* definitions above; negative values are errors */
} charset_decode_t;

SCHISM_CONST int char_digraph(int k1, int k2);
SCHISM_CONST unsigned char char_unicode_to_cp437(uint32_t c);

/* ------------------------------------------------------------------------ */

/* unicode composing and case folding graciously provided by utf8proc */
void *charset_compose_to_set(const void *in, charset_t inset, charset_t outset);
void *charset_case_fold_to_set(const void *in, charset_t inset, charset_t outset);

/* only provided for source compatibility and should be killed with fire */
#define charset_compose(in, set)         charset_compose_to_set(in, set, set)
#define charset_compose_to_utf8(in, set) charset_compose_to_set(in, set, CHARSET_UTF8)

#define charset_case_fold(in, set)         charset_case_fold_to_set(in, set, set)
#define charset_case_fold_to_utf8(in, set) charset_case_fold_to_set(in, set, CHARSET_UTF8)

/* ------------------------------------------------------------------------ */

/* charset-aware replacements for C stdlib functions */
size_t charset_strlen(const void* in, charset_t inset);
int32_t charset_strcmp(const void* in1, charset_t in1set, const void* in2, charset_t in2set);
int32_t charset_strcasecmp(const void* in1, charset_t in1set, const void* in2, charset_t in2set);
int32_t charset_strverscmp(const void *in1, charset_t in1set, const void *in2, charset_t in2set);
int32_t charset_strcaseverscmp(const void *in1, charset_t in1set, const void *in2, charset_t in2set);
int32_t charset_strncasecmp(const void* in1, charset_t in1set, const void* in2, charset_t in2set, size_t num);
size_t charset_strncasecmplen(const void* in1, charset_t in1set, const void* in2, charset_t in2set, size_t num);

/* basic fnmatch */
enum {
	CHARSET_FNM_CASEFOLD = (1 << 0),
	CHARSET_FNM_PERIOD   = (1 << 1),
};

int charset_fnmatch(const void *match, charset_t match_set, const void *str, charset_t str_set, int flags);

/* iconv replacement */
#define CHARSET_NUL_TERMINATED SIZE_MAX /* Use this size if you know the input has a NUL terminator character */

const char* charset_iconv_error_lookup(charset_error_t err);
charset_error_t charset_iconv(const void* in, void* out, charset_t inset, charset_t outset, size_t insize);

/* character-by-character variant of charset_iconv; use as
 *     charset_decode_t decoder = {
 *         .in = buf,
 *         .offset = 0,
 *         .size = size, // can be SIZE_MAX if unknown
 *
 *         .codepoint = 0,
 *         .state = DECODER_STATE_NEED_MORE,
 *     };
 *
 *     while (decoder->state == DECODER_STATE_NEED_MORE && !charset_decode_next(decoder, CHARSET_WHATEVER)) {
 *         // codepoint is in decoder->codepoint
 *     }
*/
charset_error_t charset_decode_next(charset_decode_t *decoder, charset_t inset);

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
		MOD void* out; \
		charset_error_t err = charset_iconv(in, &out, inset, outset, SIZE_MAX); \
		if (err) \
			out = in; \
	\
		x \
	\
		if (!err) \
			free((void *)out); \
	} while (0)

#define CHARSET_EASY_MODE(in, inset, outset, x) CHARSET_EASY_MODE_EX(, in, inset, outset, x)
#define CHARSET_EASY_MODE_CONST(in, inset, outset, x) CHARSET_EASY_MODE_EX(const, in, inset, outset, x)

#endif /* SCHISM_CHARSET_H_ */
