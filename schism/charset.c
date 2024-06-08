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
#include "util.h"
#include "sdlmain.h"

#include "charset_data.c"

int char_digraph(int k1, int k2)
{
#define DG(ax, eq) \
	{ \
		static const char c[2] = (ax); \
		if ((k1 == c[0] && k2 == c[1]) || (k2 == c[0] && k1 == c[1])) \
			return eq; \
	}
	DG("NB", '#')
	DG("DO", '$')
	DG("At", '@')
	DG("<(", '[')
	DG("//", '\\')
	DG(")>", ']')
	DG("'>", '^')
	DG("'!", '`')
	DG("(!", '{')
	DG("!!", '|')
	DG("!)", '{')
	DG("'?", '~')
	DG("C,", 128) // LATIN CAPITAL LETTER C WITH CEDILLA
	DG("u:", 129) // LATIN SMALL LETTER U WITH DIAERESIS
	DG("e'", 130) // LATIN SMALL LETTER E WITH ACUTE
	DG("a>", 131) // LATIN SMALL LETTER A WITH CIRCUMFLEX
	DG("a:", 132) // LATIN SMALL LETTER A WITH DIAERESIS
	DG("a!", 133) // LATIN SMALL LETTER A WITH GRAVE
	DG("aa", 134) // LATIN SMALL LETTER A WITH RING ABOVE
	DG("c,", 135) // LATIN SMALL LETTER C WITH CEDILLA
	DG("e>", 136) // LATIN SMALL LETTER E WITH CIRCUMFLEX
	DG("e:", 137) // LATIN SMALL LETTER E WITH DIAERESIS
	DG("e!", 138) // LATIN SMALL LETTER E WITH GRAVE
	DG("i:", 139) // LATIN SMALL LETTER I WITH DIAERESIS
	DG("i>", 140) // LATIN SMALL LETTER I WITH CIRCUMFLEX
	DG("i!", 141) // LATIN SMALL LETTER I WITH GRAVE
	DG("A:", 142) // LATIN CAPITAL LETTER A WITH DIAERESIS
	DG("AA", 143) // LATIN CAPITAL LETTER A WITH RING ABOVE
	DG("E'", 144) // LATIN CAPITAL LETTER E WITH ACUTE
	DG("ae", 145) // LATIN SMALL LETTER AE
	DG("AE", 146) // LATIN CAPITAL LETTER AE
	DG("o>", 147) // LATIN SMALL LETTER O WITH CIRCUMFLEX
	DG("o:", 148) // LATIN SMALL LETTER O WITH DIAERESIS
	DG("o!", 149) // LATIN SMALL LETTER O WITH GRAVE
	DG("u>", 150) // LATIN SMALL LETTER U WITH CIRCUMFLEX
	DG("u!", 151) // LATIN SMALL LETTER U WITH GRAVE
	DG("y:", 152) // LATIN SMALL LETTER Y WITH DIAERESIS
	DG("O:", 153) // LATIN CAPITAL LETTER O WITH DIAERESIS
	DG("U:", 154) // LATIN CAPITAL LETTER U WITH DIAERESIS
	DG("Ct", 155) // CENT SIGN
	DG("Pd", 156) // POUND SIGN
	DG("Ye", 157) // YEN SIGN
	DG("Pt", 158)
	DG("ff", 159)
	DG("a'", 160) // LATIN SMALL LETTER A WITH ACUTE
	DG("i'", 161) // LATIN SMALL LETTER I WITH ACUTE
	DG("o'", 162) // LATIN SMALL LETTER O WITH ACUTE
	DG("u'", 163) // LATIN SMALL LETTER U WITH ACUTE
	DG("n?", 164) // LATIN SMALL LETTER N WITH TILDE
	DG("N?", 165) // LATIN CAPITAL LETTER N WITH TILDE
	DG("-a", 166) // FEMININE ORDINAL INDICATOR
	DG("-o", 167) // MASCULINE ORDINAL INDICATOR
	DG("?I", 168) // INVERTED QUESTION MARK

	DG("NO", 170) // NOT SIGN
	DG("12", 171) // VULGAR FRACTION ONE HALF
	DG("14", 174) // VULGAR FRACTION ONE QUARTER
	DG("!I", 175) // INVERTED EXCLAMATION MARK
	DG("<<", 176) // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
	DG(">>", 177) // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK

	DG("ss", 225) // LATIN SMALL LETTER SHARP S
	DG("pi", 227) // PI... mmm... pie...
	DG("My", 230) // MICRO SIGN
	DG("o/", 237) // LATIN SMALL LETTER O WITH STROKE
	DG("O/", 237) // LATIN SMALL LETTER O WITH STROKE
	DG("+-", 241) // PLUS-MINUS SIGN
	DG("-:", 246) // DIVISION SIGN
	DG("DG", 248) // DEGREE SIGN
	DG(".M", 249) // MIDDLE DOT
	DG("2S", 253) // SUPERSCRIPT TWO
	DG("nS", 252)

	DG("PI", 20)  // PILCROW SIGN
	DG("SE", 21)  // SECTION SIGN
#undef DG
	return 0;
}

/* -----------------------------------------------------------------------------
 *
 * these encode ONE character each time they are ran, and they must each
 * return a byte offset of which to append to "in" to start with the next
 * character. */

#define DECODER_ERROR   (-1)
#define DECODER_NEED_MORE (0)
#define DECODER_DONE    (1)

static int utf8_to_ucs4(const uint8_t* in, uint32_t* out, size_t* size_until_next) {
	uint8_t c = *in;

	if (c == 0x00) {
		return DECODER_DONE;
	} else if (c < 0x80) {
		*out = c;
		*size_until_next = 1;
	} else if (c < 0xC2) {
		return DECODER_ERROR;
	} else if (c < 0xE0) {
		if (!((in[1] ^ 0x80) < 0x40))
			return DECODER_ERROR;

		*out = ((uint32_t) (c & 0x1f) << 6)
					 | (uint32_t) (in[1] ^ 0x80);

		*size_until_next = 2;
	} else if (c < 0xf0) {
		if (!((in[1] ^ 0x80) < 0x40 && (in[2] ^ 0x80) < 0x40
					&& (c >= 0xE1 || in[1] >= 0xA0)
					&& (c != 0xED || in[1] < 0xA0)))
			return DECODER_ERROR;

		*out = ((uint32_t) (c & 0x0f) << 12)
					 | ((uint32_t) (in[1] ^ 0x80) << 6)
					 | (uint32_t) (in[2] ^ 0x80);

		*size_until_next = 3;
	} else if (c < 0xf8) {
		if (!((in[1] ^ 0x80) < 0x40 && (in[2] ^ 0x80) < 0x40
					&& (in[3] ^ 0x80) < 0x40
					&& (c >= 0xf1 || in[1] >= 0x90)
					&& (c < 0xf4 || (c == 0xf4 && in[1] < 0x90))))
			return DECODER_ERROR;

		*out = ((uint32_t) (c & 0x07) << 18)
					 | ((uint32_t) (in[1] ^ 0x80) << 12)
					 | ((uint32_t) (in[2] ^ 0x80) << 6)
					 | (uint32_t) (in[3] ^ 0x80);

		*size_until_next = 4;
	} else return DECODER_ERROR;

	return DECODER_NEED_MORE;
}

/* this is here so that when the UTF-16 code changes nothing gets lost between these two */
#define DECODE_UTF16_VARIANT(x) \
	static int utf16##x##_to_ucs4(const uint8_t* in, uint32_t* out, size_t* size_until_next) { \
		const uint16_t* tmp_in = (const uint16_t*)in; \
	\
		uint16_t wc = *(tmp_in++); \
		wc = bswap##x##16(wc); \
	\
		if (wc == 0x0000) {\
			return DECODER_DONE; \
		} else if (wc < 0xD800 || wc > 0xDFFF) { \
			*out = wc; \
			*size_until_next = 2; \
		} else if (wc >= 0xD800 && wc <= 0xDBFF) { \
			uint16_t wc2 = *(tmp_in++); \
			wc2 = bswap##x##16(wc2); \
	\
			if (wc2 >= 0xDC00 && wc2 <= 0xDFFF) { \
				*out = 0x10000 + ((wc - 0xD800) << 10) + (wc2 - 0xDC00); \
				*size_until_next = 4; \
			} else return DECODER_ERROR; \
		} \
	\
		return DECODER_NEED_MORE; \
	}

DECODE_UTF16_VARIANT(LE)
DECODE_UTF16_VARIANT(BE)

#undef UTF16_DECODER

static int cp437_to_ucs4(const uint8_t* in, uint32_t* out, size_t* size_until_next) {
	static const uint16_t cp437_table[128] = {
		/* 0x80 */
		0x00C7, 0x00FC, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
		0x00EA, 0x00EB, 0x00E8, 0x00EF, 0x00EE, 0x00EC, 0x00C4, 0x00C5,
		/* 0x90 */
		0x00C9, 0x00E6, 0x00C6, 0x00F4, 0x00F6, 0x00F2, 0x00FB, 0x00F9,
		0x00FF, 0x00D6, 0x00DC, 0x00A2, 0x00A3, 0x00A5, 0x20A7, 0x0192,
		/* 0xA0 */
		0x00E1, 0x00ED, 0x00F3, 0x00FA, 0x00F1, 0x00D1, 0x00AA, 0x00BA,
		0x00BF, 0x2310, 0x00AC, 0x00BD, 0x00BC, 0x00A1, 0x00AB, 0x00BB,
		/* 0xB0 */
		0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
		0x2555, 0x2563, 0x2551, 0x2557, 0x255D, 0x255C, 0x255B, 0x2510,
		/* 0xC0 */
		0x2514, 0x2534, 0x252C, 0x251C, 0x2500, 0x253C, 0x255E, 0x255F,
		0x255A, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256C, 0x2567,
		/* 0xD0 */
		0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256B,
		0x256A, 0x2518, 0x250C, 0x2588, 0x2584, 0x258C, 0x2590, 0x2580,
		/* 0xE0 */
		0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
		0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
		/* 0xF0 */
		0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
		0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0
	};

	uint8_t c = *in;
	*out = (c < 0x80) ? c : cp437_table[c - 0x80];

	*size_until_next = 1;
	return (c) ? DECODER_NEED_MORE : DECODER_DONE;
}

static int windows1252_to_ucs4(const uint8_t* in, uint32_t* out, size_t* size_until_next) {
	/* Microsoft and the Unicode Consortium define positions 81, 8D, 8F, 90, and 9D
	 * as unused, so we just error out on those chars. */
	static const uint16_t windows1252_table[32] = {
		/* 0x80 */
		0x20AC, 0xFFFF, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
		0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0xFFFF, 0x017D, 0xFFFF,
		/* 0x90 */
		0xFFFF, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0xFFFF, 0x017E, 0x0178
	};

	uint8_t c = *in;
	if (c >= 0x80 && c < 0xA0) {
		*out = windows1252_table[c - 0x80];
		if (*out == 0xFFFF)
			return DECODER_ERROR;
	} else {
		*out = c;
	}

	*size_until_next = 1;
	return (*out) ? DECODER_NEED_MORE : DECODER_DONE;
}

static int do_nothing_ucs4(const uint8_t* in, uint32_t* out, size_t* size_until_next) {
	*out = *(uint32_t*)in;
	*size_until_next = sizeof(uint32_t);
	return (*out) ? DECODER_NEED_MORE : DECODER_DONE;
}

/* ----------------------------------------------------- */
/* for these functions, 0 indicates an error in encoding */

static size_t ucs4_to_utf8(uint32_t ch, uint8_t* out) {
	size_t len = 0;

	if (out) {
		if (ch < 0x80) {
			out[len++] = (uint8_t)ch;
		} else if (ch < 0x800) {
			out[len++] = (uint8_t)((ch >> 6) | 0xC0);
			out[len++] = (uint8_t)((ch & 0x3F) | 0x80);
		} else if (ch < 0x10000) {
			out[len++] = (uint8_t)((ch >> 12) | 0xE0);
			out[len++] = (uint8_t)(((ch >> 6) & 0x3F) | 0x80);
			out[len++] = (uint8_t)((ch & 0x3F) | 0x80);
		} else if (ch < 0x110000) {
			out[len++] = (uint8_t)((ch >> 18) | 0xF0);
			out[len++] = (uint8_t)(((ch >> 12) & 0x3F) | 0x80);
			out[len++] = (uint8_t)(((ch >> 6) & 0x3F) | 0x80);
			out[len++] = (uint8_t)((ch & 0x3F) | 0x80);
		} else return 0; /* ZOMG NO WAY */
	} else {
		if (ch < 0x80)      len += 1;
		else if (ch < 0x800)  len += 2;
		else if (ch < 0x10000)  len += 3;
		else if (ch < 0x110000) len += 4;
		else return 0;
	}

	return len;
}

static size_t ucs4_to_cp437(uint32_t ch, uint8_t* out) {
	if (out)
		*out = char_unicode_to_cp437(ch);

	return 1;
}

#define ENCODE_UTF16_VARIANT(x) \
	static size_t ucs4_to_utf16##x(uint32_t ch, uint8_t* out) { \
		size_t len = 0; \
	\
		if (out) { \
			if (ch < 0x10000) { \
				APPEND_CHAR(ch); \
			} else { \
				uint16_t w1 = 0xD800 + ((ch - 0x10000) >> 10); \
				uint16_t w2 = 0xDC00 + ((ch - 0x10000) & 0x3FF); \
	\
				APPEND_CHAR(w1); \
				APPEND_CHAR(w2); \
			} \
		} else { \
			len += (ch < 0x10000) ? 2 : 4; \
		} \
	\
		return len; \
	}

#define APPEND_CHAR(x) \
	out[len++] = (uint8_t)(ch); \
	out[len++] = (uint8_t)(ch >> 8)

ENCODE_UTF16_VARIANT(LE)

#undef APPEND_CHAR

#define APPEND_CHAR(x) \
	out[len++] = (uint8_t)(ch >> 8); \
	out[len++] = (uint8_t)(ch);

ENCODE_UTF16_VARIANT(BE)

#undef APPEND_CHAR

#undef ENCODE_UTF16_VARIANT

static size_t ucs4_do_nothing(uint32_t ch, uint8_t* out) {
	if (out)
		*(uint32_t*)out = ch;

	return 4;
}

/* function LUT here */
typedef int (*charset_conv_to_ucs4_func)(const uint8_t*, uint32_t*, size_t*);
typedef size_t (*charset_conv_from_ucs4_func)(uint32_t, uint8_t*);

static charset_conv_to_ucs4_func conv_to_ucs4_funcs[] = {
	[CHARSET_UTF8] = utf8_to_ucs4,

	/* primarily used on Windows */
	[CHARSET_UTF16LE] = utf16LE_to_ucs4,
	[CHARSET_UTF16BE] = utf16BE_to_ucs4,

	[CHARSET_UCS4] = do_nothing_ucs4,

	[CHARSET_CP437] = cp437_to_ucs4,
	[CHARSET_WINDOWS1252] = windows1252_to_ucs4,

	[CHARSET_CHAR] = utf8_to_ucs4,
#ifdef SCHISM_WIN32
# if WORDS_BIGENDIAN
	[CHARSET_WCHAR_T] = utf16BE_to_ucs4,
# else
	[CHARSET_WCHAR_T] = utf16LE_to_ucs4,
# endif
#else
	/* unimplemented */
	[CHARSET_WCHAR_T] = NULL,
#endif
};

static charset_conv_from_ucs4_func conv_from_ucs4_funcs[] = {
	[CHARSET_UTF8] = ucs4_to_utf8,
	[CHARSET_UTF16LE] = ucs4_to_utf16LE,
	[CHARSET_UTF16BE] = ucs4_to_utf16BE,
	[CHARSET_UCS4] = ucs4_do_nothing,

	[CHARSET_CP437] = ucs4_to_cp437,

	[CHARSET_CHAR] = ucs4_to_utf8,
#ifdef SCHISM_WIN32
# if WORDS_BIGENDIAN
	[CHARSET_WCHAR_T] = ucs4_to_utf16BE,
# else
	[CHARSET_WCHAR_T] = ucs4_to_utf16LE,
# endif
#else
	/* unimplemented */
	[CHARSET_WCHAR_T] = NULL,
#endif
};

/* for debugging */
const char* charset_iconv_error_lookup(charset_error_t err) {
	switch (err) {
		case CHARSET_ERROR_SUCCESS:
		default:
			return "Success";
		case CHARSET_ERROR_UNIMPLEMENTED:
			return "Conversion unimplemented";
		case CHARSET_ERROR_NULLINPUT:
			return "Input pointer is NULL";
		case CHARSET_ERROR_NULLOUTPUT:
			return "Output pointer is NULL";
		case CHARSET_ERROR_INPUTISOUTPUT:
			return "Input and output charsets are the same";
		case CHARSET_ERROR_DECODE:
			return "An error occurred when decoding";
		case CHARSET_ERROR_ENCODE:
			return "An error occurred when encoding";
		case CHARSET_ERROR_NOMEM:
			return "Out of memory";
	}
}

#define CHARSET_VARIATION(name) \
	static charset_error_t charset_iconv_##name##_(const uint8_t* in, uint8_t** out, charset_t inset, charset_t outset)

/* our version of iconv; this has a much simpler API than the regular
 * iconv() because much of it isn't very necessary for our purposes
 *
 * note: do NOT put huge buffers into here, it will likely waste
 * lots of memory due to using a buffer of UCS4 inbetween
 *
 * all input is expected to be NULL-terminated
 *
 * example usage:
 *     uint8_t *cp437 = some_buf, *utf8 = NULL;
 *     charset_iconv(cp437, &utf8, CHARSET_CP437, CHARSET_UTF8);
 * 
 * [out] must be free'd by the caller */
CHARSET_VARIATION(internal) {
	size_t out_length = 0;
	size_t out_alloc = 16; /* estimate */
	int c;

	if (inset >= ARRAY_SIZE(conv_to_ucs4_funcs) || outset >= ARRAY_SIZE(conv_from_ucs4_funcs))
		return CHARSET_ERROR_UNIMPLEMENTED;

	charset_conv_to_ucs4_func   conv_to_ucs4_func   = conv_to_ucs4_funcs[inset];
	charset_conv_from_ucs4_func conv_from_ucs4_func = conv_from_ucs4_funcs[outset];

	if (!conv_to_ucs4_func || !conv_from_ucs4_func)
		return CHARSET_ERROR_UNIMPLEMENTED;

	*out = malloc((out_alloc) * sizeof(uint8_t));
	if (!*out)
		return CHARSET_ERROR_NOMEM;

	do {
		uint32_t ch = 0;
		size_t in_needed = 0;

		c = conv_to_ucs4_func(in, &ch, &in_needed);
		if (c == DECODER_ERROR) {
			free(*out);
			return CHARSET_ERROR_DECODE;
		}

		/* printf("result: %d, U+%04x, %zu\n", c, ch, in_needed); */

		size_t out_needed = conv_from_ucs4_func(ch, NULL);
		if (!out_needed) {
			free(*out);
			return CHARSET_ERROR_ENCODE;
		}

		if (out_length + out_needed >= out_alloc) {
			uint8_t* old_out = *out;
			*out = mem_realloc(*out, (out_alloc *= 2) * sizeof(uint8_t));
			if (!*out) {
				free(old_out);
				return CHARSET_ERROR_NOMEM;
			}
		}

		conv_from_ucs4_func(ch, *out + out_length);

		out_length += out_needed;
		in += in_needed;
	} while (c == DECODER_NEED_MORE);

	return CHARSET_ERROR_SUCCESS;
}

CHARSET_VARIATION(sdl) {
	static const char* charset_iconv_system_lookup[] = {
		[CHARSET_UTF8] = "UTF-8",
		[CHARSET_UTF16BE] = "UTF-16BE",
		[CHARSET_UTF16LE] = "UTF-16LE",
		[CHARSET_UCS4] = "UCS-4",

		[CHARSET_CP437] = "437",

		[CHARSET_CHAR] = "",
		[CHARSET_WCHAR_T] = "WCHAR_T"
	};

	/* A bit hacky, but whatever */
	size_t src_size = strlen((const char*)in);
	size_t out_size = src_size;

	if (src_size <= 0)
		return CHARSET_ERROR_UNIMPLEMENTED;

	if (inset >= ARRAY_SIZE(charset_iconv_system_lookup) || outset >= ARRAY_SIZE(charset_iconv_system_lookup))
		return CHARSET_ERROR_UNIMPLEMENTED;

	SDL_iconv_t cd = SDL_iconv_open(charset_iconv_system_lookup[inset], charset_iconv_system_lookup[outset]);
	if (cd == (SDL_iconv_t)(-1))
		return CHARSET_ERROR_UNIMPLEMENTED;

	*out = malloc(out_size + sizeof(uint32_t));
	if (!*out)
		return CHARSET_ERROR_NOMEM;

	/* now onto the real conversion */
	const char* in_ = (char*)in;
	char* out_ = (char*)(*out);
	size_t in_bytes_left = src_size;
	size_t out_bytes_left = out_size;

	memset(out_, 0, sizeof(uint32_t));

	while (in_bytes_left) {
		const size_t old_in_bytes_left = in_bytes_left;

		size_t rc = SDL_iconv(cd, &in_, &in_bytes_left, &out_, &out_bytes_left);
		switch (rc) {
		case SDL_ICONV_E2BIG: {
			const ptrdiff_t diff = (ptrdiff_t)((uint8_t*)out_ - *out);

			uint8_t* old_out = *out;

			out_size *= 2;
			*out = realloc(old_out, out_size + sizeof(uint32_t));
			if (!*out) {
				free(old_out);
				SDL_iconv_close(cd);
				return CHARSET_ERROR_NOMEM;
			}

			out_ = (char*)(*out + diff);
			out_bytes_left = out_size - diff;

			continue;
		}
		case SDL_ICONV_EILSEQ:
			/* try skipping? */
			in_++;
			in_bytes_left--;
			break;
		case SDL_ICONV_EINVAL:
		case SDL_ICONV_ERROR:
			/* t'was a good run */
			in_bytes_left = 0;
			memset(out_, 0, sizeof(uint32_t));
			SDL_iconv_close(cd);
			return CHARSET_ERROR_UNIMPLEMENTED;
		default:
			break;
		}

		/* avoid infinite loops */
		if (old_in_bytes_left == in_bytes_left)
			break;
	}

	memset(out_, 0, sizeof(uint32_t));

	SDL_iconv_close(cd);

	return CHARSET_ERROR_SUCCESS;
}

/* XXX need to change this to take length in as well */
charset_error_t charset_iconv(const uint8_t* in, uint8_t** out, charset_t inset, charset_t outset) {
	charset_error_t state;
	if (!in)
		return CHARSET_ERROR_NULLINPUT;

	if (!out)
		return CHARSET_ERROR_NULLOUTPUT;

	if (inset == outset)
		return CHARSET_ERROR_INPUTISOUTPUT;

#define TRY_VARIATION(name) \
	state = charset_iconv_##name##_(in, out, inset, outset); \
	if (state == CHARSET_ERROR_SUCCESS || state == CHARSET_ERROR_NOMEM) \
		return state;

	TRY_VARIATION(internal);
	TRY_VARIATION(sdl);

#undef TRY_VARIATION

	return CHARSET_ERROR_UNIMPLEMENTED;
}

/* ----------------------------------------------------------------------------------- */
/* now we get to charset-aware versions of C stdlib functions. */

/* this isn't *really* necessary, but nice to have */
int charset_strcmp(const uint8_t* in1, charset_t in1set, const uint8_t* in2, charset_t in2set) {
	uint32_t codepoint1, codepoint2;
	size_t in1_needed, in2_needed, in1_offset = 0, in2_offset = 0;
	int c1, c2;

	if (in1set >= ARRAY_SIZE(conv_to_ucs4_funcs) || in2set >= ARRAY_SIZE(conv_to_ucs4_funcs))
		goto charsetfail;

	charset_conv_to_ucs4_func conv1_to_ucs4_func = conv_to_ucs4_funcs[in1set],
	                          conv2_to_ucs4_func = conv_to_ucs4_funcs[in2set];

	if (!conv1_to_ucs4_func || !conv2_to_ucs4_func)
		goto charsetfail;

	for (;;) {
		c1 = conv1_to_ucs4_func(in1 + in1_offset, &codepoint1, &in1_needed);
		c2 = conv1_to_ucs4_func(in2 + in2_offset, &codepoint2, &in2_needed);

		if (c1 == DECODER_ERROR || c2 == DECODER_ERROR)
			goto charsetfail;

		if (c1 == DECODER_DONE || c2 == DECODER_DONE || codepoint1 != codepoint2)
			break;

		in1_offset += in1_needed;
		in2_offset += in2_needed;
	}

	return codepoint1 - codepoint2;

charsetfail:
	return strcmp(in1, in2);
}

/* this IS necessary to actually sort properly. */
int charset_strcasecmp(const uint8_t* in1, charset_t in1set, const uint8_t* in2, charset_t in2set) {
	uint32_t codepoint1, codepoint2;
	size_t in1_needed, in2_needed, in1_offset = 0, in2_offset = 0;
	int c1, c2;

	if (in1set >= ARRAY_SIZE(conv_to_ucs4_funcs) || in2set >= ARRAY_SIZE(conv_to_ucs4_funcs))
		goto charsetfail;

	charset_conv_to_ucs4_func conv1_to_ucs4_func = conv_to_ucs4_funcs[in1set],
	                          conv2_to_ucs4_func = conv_to_ucs4_funcs[in2set];

	if (!conv1_to_ucs4_func || !conv2_to_ucs4_func)
		goto charsetfail;

	for (;;) {
		c1 = conv1_to_ucs4_func(in1 + in1_offset, &codepoint1, &in1_needed);
		c2 = conv1_to_ucs4_func(in2 + in2_offset, &codepoint2, &in2_needed);

		if (c1 == DECODER_ERROR || c2 == DECODER_ERROR)
			goto charsetfail;

		if (c1 == DECODER_DONE || c2 == DECODER_DONE || charset_simple_case_fold(codepoint1) != charset_simple_case_fold(codepoint2))
			break;

		in1_offset += in1_needed;
		in2_offset += in2_needed;
	}

	return charset_simple_case_fold(codepoint1) - charset_simple_case_fold(codepoint2);

charsetfail:
#if HAVE_STRCASECMP
	return strcasecmp(in1, in2);
#else
	while (tolower(*in1) == tolower(*in2++))
		if (*in1++ == '\0')
			return 0;

	return (tolower(*in1) - tolower(*--in2));
#endif
}
