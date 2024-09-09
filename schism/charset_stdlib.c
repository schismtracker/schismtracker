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

size_t charset_strlen(const uint8_t* in, charset_t inset) {
	charset_decode_t decoder = {
		.in = in,
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

int charset_strcmp(const uint8_t* in1, charset_t in1set, const uint8_t* in2, charset_t in2set) {
	charset_decode_t decoder1 = {
		.in = in1,
		.offset = 0,
		.size = SIZE_MAX,

		.codepoint = 0,
	};

	charset_decode_t decoder2 = {
		.in = in2,
		.offset = 0,
		.size = SIZE_MAX,

		.codepoint = 0,
	};

	for (;;) {
		charset_decode_next(&decoder1, in1set);
		charset_decode_next(&decoder2, in2set);

		if (decoder1.state == DECODER_STATE_ERROR || decoder2.state == DECODER_STATE_ERROR)
			goto charsetfail;

		if (decoder1.state == DECODER_STATE_DONE || decoder2.state == DECODER_STATE_DONE || decoder1.codepoint != decoder2.codepoint)
			break;
	}

	return decoder1.codepoint - decoder2.codepoint;

charsetfail:
	return strcmp(in1, in2);
}

/* this IS necessary to actually sort properly. */
int charset_strcasecmp(const uint8_t* in1, charset_t in1set, const uint8_t* in2, charset_t in2set) {
	uint8_t *folded8_1 = NULL, *folded8_2 = NULL;

	/* one at a time, please */
	folded8_1 = charset_case_fold_to_utf8(in1, in1set);
	if (!folded8_1)
		goto charsetfail;

	folded8_2 = charset_case_fold_to_utf8(in2, in2set);
	if (!folded8_2)
		goto charsetfail;

	charset_decode_t decoder1 = {
		.in = folded8_1,
		.offset = 0,
		.size = SIZE_MAX,
	};

	charset_decode_t decoder2 = {
		.in = folded8_2,
		.offset = 0,
		.size = SIZE_MAX,
	};

	for (;;) {
		charset_decode_next(&decoder1, CHARSET_UTF8);
		charset_decode_next(&decoder2, CHARSET_UTF8);

		/* can probably be removed; utf8proc guarantees valid UTF-8 */
		if (decoder1.state == DECODER_STATE_ERROR || decoder2.state == DECODER_STATE_ERROR)
			goto charsetfail;

		if (decoder1.state == DECODER_STATE_DONE || decoder2.state == DECODER_STATE_DONE || decoder1.codepoint != decoder2.codepoint)
			break;
	}

	free(folded8_1);
	free(folded8_2);

	return decoder1.codepoint - decoder2.codepoint;

charsetfail:
	free(folded8_1);
	free(folded8_2);

#if HAVE_STRCASECMP
	return strcasecmp(in1, in2);
#else
	while (tolower(*in1) == tolower(*in2++))
		if (*in1++ == '\0')
			return 0;

	return (tolower(*in1) - tolower(*--in2));
#endif
}

/* ugh. (num is the number of CHARACTERS, not the number of bytes!!) */
int charset_strncasecmp(const uint8_t* in1, charset_t in1set, const uint8_t* in2, charset_t in2set, size_t num) {
	uint8_t *folded8_1 = NULL, *folded8_2 = NULL;

	/* one at a time, please */
	folded8_1 = charset_case_fold_to_utf8(in1, in1set);
	if (!folded8_1)
		goto charsetfail;

	folded8_2 = charset_case_fold_to_utf8(in2, in2set);
	if (!folded8_2)
		goto charsetfail;

	charset_decode_t decoder1 = {
		.in = folded8_1,
		.offset = 0,
		.size = SIZE_MAX,
	};

	charset_decode_t decoder2 = {
		.in = folded8_2,
		.offset = 0,
		.size = SIZE_MAX,
	};

	for (;;) {
		charset_decode_next(&decoder1, CHARSET_UTF8);
		charset_decode_next(&decoder2, CHARSET_UTF8);

		/* can probably be removed; utf8proc guarantees valid UTF-8 */
		if (decoder1.state == DECODER_STATE_ERROR || decoder2.state == DECODER_STATE_ERROR)
			goto charsetfail;

		if (decoder1.state == DECODER_STATE_DONE || decoder2.state == DECODER_STATE_DONE || decoder1.codepoint != decoder2.codepoint)
			break;
	}

	free(folded8_1);
	free(folded8_2);

	return decoder1.codepoint - decoder2.codepoint;

charsetfail:
	free(folded8_1);
	free(folded8_2);

	/* commenting this out unless it's really necessary */
	/* if (in1set != CHARSET_CHAR && in2set != CHARSET_CHAR) return 0; */

#if HAVE_STRCASECMP
	return strncasecmp(in1, in2, num);
#else
	do {
		if (tolower(*in1) != tolower(*in2++))
			return (tolower(*in1) - tolower(*--in1));
		if (*in1++ == '\0')
			break;
	} while (--num != 0);

	return 0;
#endif
}

/* this does the exact same as the above function but returns how many characters were passed */
size_t charset_strncasecmplen(const uint8_t* in1, charset_t in1set, const uint8_t* in2, charset_t in2set, size_t num) {
	uint8_t *folded8_1 = NULL, *folded8_2 = NULL;

	/* one at a time, please */
	folded8_1 = charset_case_fold_to_utf8(in1, in1set);
	if (!folded8_1)
		goto charsetfail;

	folded8_2 = charset_case_fold_to_utf8(in2, in2set);
	if (!folded8_2)
		goto charsetfail;

	charset_decode_t decoder1 = {
		.in = folded8_1,
		.offset = 0,
		.size = SIZE_MAX,
	};

	charset_decode_t decoder2 = {
		.in = folded8_2,
		.offset = 0,
		.size = SIZE_MAX,
	};

	/* eh */
	size_t i;
	for (i = 0; i < num; i++) {
		charset_decode_next(&decoder1, CHARSET_UTF8);
		charset_decode_next(&decoder2, CHARSET_UTF8);

		/* can probably be removed; utf8proc guarantees valid UTF-8 */
		if (decoder1.state == DECODER_STATE_ERROR || decoder2.state == DECODER_STATE_ERROR)
			goto charsetfail;

		if (decoder1.state == DECODER_STATE_DONE || decoder2.state == DECODER_STATE_DONE || decoder1.codepoint != decoder2.codepoint)
			break;
	}

	free(folded8_1);
	free(folded8_2);

	return i;

charsetfail:
	free(folded8_1);
	free(folded8_2);

	/* Whoops! You have to put the CD in your computer! */
	for (i = 0; i < num; i++)
		if (tolower(in1[i]) != tolower(in2[i]))
			break;

	return i;
}
