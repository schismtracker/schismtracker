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
#include "charset.h"

#include <ctype.h>

size_t charset_strlen(const void* in, charset_t inset)
{
	charset_decode_t decoder = {
		.in = (unsigned char *)in,
		.offset = 0,
		.size = SIZE_MAX, /* this is ok here */
	};

	size_t count = 0;

	for (;;) {
		charset_decode_next(&decoder, inset);

		if (decoder.state < 0)
			return 0;

        if (decoder.state == DECODER_STATE_DONE)
            break;
        
        count++;
	}

	return count;
}

int charset_strcmp(const void* in1, charset_t in1set, const void* in2, charset_t in2set)
{
	int result = 0;

	charset_decode_t decoder1 = {
		.in = (const unsigned char *)in1,
		.offset = 0,
		.size = SIZE_MAX,
	};

	charset_decode_t decoder2 = {
		.in = (const unsigned char *)in2,
		.offset = 0,
		.size = SIZE_MAX,
	};

	for (;;) {
		charset_decode_next(&decoder1, in1set);
		charset_decode_next(&decoder2, in2set);

		if (decoder1.state == DECODER_STATE_ERROR || decoder2.state == DECODER_STATE_ERROR)
			goto charsetfail;

		result = decoder1.codepoint - decoder2.codepoint;

		if (decoder1.state == DECODER_STATE_DONE || decoder2.state == DECODER_STATE_DONE || result)
			break;
	}

	return result;

charsetfail:
	return strcmp((const char *)in1, (const char *)in2);
}

/* this IS necessary to actually sort properly. */
int charset_strcasecmp(const void* in1, charset_t in1set, const void* in2, charset_t in2set)
{
	uint32_t *folded8_1 = NULL, *folded8_2 = NULL;
	int res;

	/* takes up more memory than necessary, but whatever */
	folded8_1 = charset_case_fold_to_set(in1, in1set, CHARSET_UCS4);
	if (!folded8_1)
		goto charsetfail;

	folded8_2 = charset_case_fold_to_set(in2, in2set, CHARSET_UCS4);
	if (!folded8_2)
		goto charsetfail;

	res = charset_strcmp(folded8_1, CHARSET_UCS4, folded8_2, CHARSET_UCS4);

	free(folded8_1);
	free(folded8_2);

	return res;

charsetfail:
	free(folded8_1);
	free(folded8_2);

#if HAVE_STRCASECMP
	return strcasecmp((const char *)in1, (const char *)in2);
#else
	const unsigned char *us1 = (const unsigned char *)in1, *us2 = (const unsigned char *)in2;

	while (tolower(*us1) == tolower(*us2++))
		if (*us1++ == '\0')
			return 0;

	return (tolower(*us1) - tolower(*--us2));
#endif
}

/* --------------------------------------------------------------------------- */
/* charset_strncasecmp[len] share the same implementation, but return different
 * results */

struct strncasecmp_impl_struct {
	int diff;
	size_t count;

	int success;
};

static struct strncasecmp_impl_struct charset_strncasecmp_impl(const void *in1, charset_t in1set, const void *in2, charset_t in2set, size_t num)
{
	struct strncasecmp_impl_struct result = {0};

	charset_decode_t decoder1 = {
		.in = (const unsigned char *)in1,
		.offset = 0,
		.size = SIZE_MAX,
	};

	charset_decode_t decoder2 = {
		.in = (const unsigned char *)in2,
		.offset = 0,
		.size = SIZE_MAX,
	};

	size_t i;
	for (i = 0; i < num; i++) {
		charset_decode_next(&decoder1, in1set);
		charset_decode_next(&decoder2, in2set);

		if (decoder1.state == DECODER_STATE_ERROR || decoder2.state == DECODER_STATE_ERROR)
			return result;

		uint32_t cp1[2] = {decoder1.codepoint, 0};
		uint32_t cp2[2] = {decoder2.codepoint, 0};

		uint32_t *cf1 = (uint32_t *)charset_case_fold_to_set((uint8_t *)cp1, CHARSET_UCS4, CHARSET_UCS4);
		if (!cf1)
			return result;

		uint32_t *cf2 = (uint32_t *)charset_case_fold_to_set((uint8_t *)cp2, CHARSET_UCS4, CHARSET_UCS4);
		if (!cf2) {
			free(cf1);
			return result;
		}

	    result.diff = charset_strcmp((uint8_t *)cf1, CHARSET_UCS4, (uint8_t *)cf2, CHARSET_UCS4);

	    free(cf1);
	    free(cf2);

	    if (decoder1.state == DECODER_STATE_DONE || decoder2.state == DECODER_STATE_DONE || result.diff)
	        break;
	}

	result.success = 1;
	result.count = i;

	return result;
}

/* ugh. (num is the number of CHARACTERS, not the number of bytes!!) */
int charset_strncasecmp(const void* in1, charset_t in1set, const void* in2, charset_t in2set, size_t num)
{
	struct strncasecmp_impl_struct result = charset_strncasecmp_impl(in1, in1set, in2, in2set, num);

	if (result.success)
		return result.diff;

	/* at least try *something* */
#if HAVE_STRNCASECMP
	return strncasecmp((const char *)in1, (const char *)in2, num);
#else
	const unsigned char *us1 = (const unsigned char *)in1, *us2 = (const unsigned char *)in2;

	do {
		if (tolower(*us1) != tolower(*us2++))
			return (tolower(*us1) - tolower(*--us2));
		if (*us1++ == '\0')
			break;
	} while (--num != 0);

	return 0;
#endif
}

/* this does the exact same as the above function but returns how many characters were passed */
size_t charset_strncasecmplen(const void* in1, charset_t in1set, const void* in2, charset_t in2set, size_t num)
{
	struct strncasecmp_impl_struct result = charset_strncasecmp_impl(in1, in1set, in2, in2set, num);

	if (result.success)
		return result.count;

	/* Whoops! You have to put the CD in your computer! */
	const unsigned char *us1 = (const unsigned char *)in1, *us2 = (const unsigned char *)in2;

	size_t i;
	for (i = 0; i < num; i++)
		if (tolower(us1[i]) != tolower(us2[i]))
			break;

	return i;
}

/* ------------------------------------------------------- */
/* fnmatch... */

#if HAVE_FNMATCH
# define _GNU_SOURCE /* ugh */
# include <fnmatch.h>
#endif

#define UCS4_ASTERISK UINT32_C(0x2A)
#define UCS4_PERIOD   UINT32_C(0x2E)
#define UCS4_QUESTION UINT32_C(0x3F)

/* expects UCS4 input that's case-folded if desired */
static inline int charset_fnmatch_impl(const uint32_t *m, const uint32_t *s)
{
	if (*m == UCS4_ASTERISK)
		for (++m; *s; ++s)
			if (!charset_fnmatch_impl(m, s))
				return 0;

	return (!*s || !(*m == UCS4_QUESTION || *s == *m))
		? (*m | *s) : charset_fnmatch_impl(m + 1, s + 1);
}

int charset_fnmatch(const void *match, charset_t match_set, const void *str, charset_t str_set, int flags)
{
	uint32_t *match_ucs4 = NULL, *str_ucs4 = NULL;

	if (flags & CHARSET_FNM_CASEFOLD) {
		match_ucs4 = charset_case_fold_to_set(match, match_set, CHARSET_UCS4);
		if (!match_ucs4)
			goto charsetfail;

		str_ucs4 = charset_case_fold_to_set(str, str_set, CHARSET_UCS4);
		if (!str_ucs4)
			goto charsetfail;
	} else {
		if (charset_iconv(match, &match_ucs4, match_set, CHARSET_UCS4, SIZE_MAX))
			goto charsetfail;

		if (charset_iconv(str, &str_ucs4, str_set, CHARSET_UCS4, SIZE_MAX))
			goto charsetfail;
	}

	int r = ((flags & CHARSET_FNM_PERIOD) && (*str_ucs4 == UCS4_PERIOD && *match_ucs4 != UCS4_PERIOD))
		? 0
		: charset_fnmatch_impl(match_ucs4, str_ucs4);

	free(match_ucs4);
	free(str_ucs4);

	return r;

charsetfail:
	free(match_ucs4);
	free(str_ucs4);

	/* fall back to the system implementation, if there even is one */

#if HAVE_FNMATCH
	if (match_set != CHARSET_CHAR && str_set != CHARSET_CHAR)
		return -1;

	int fnm_flags = 0;

	if (flags & CHARSET_FNM_PERIOD)
		fnm_flags |= FNM_PERIOD;

# ifdef FNM_CASEFOLD
	if (flags & CHARSET_FNM_CASEFOLD)
		fnm_flags |= FNM_CASEFOLD;
# endif

	return fnmatch((const char *)match, (const char *)str, fnm_flags);
#else
	return -1;
#endif
}
