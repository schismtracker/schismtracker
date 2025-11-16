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

#include "bits.h"
#include "charset.h"
#include "util.h"
#include "disko.h"
#include "mem.h"
#include "log.h"
#ifdef SCHISM_WIN32
# include "osdefs.h"
# include <windows.h>
#endif

int char_digraph(int k1, int k2)
{
#define DG(ax, eq) \
	{ \
		SCHISM_NONSTRING static const char c[2] = ax; \
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
 * decoders */

/* Make sure we never overflow the size. */
#define DECODER_ASSERT_OVERFLOW(decoder, amount) \
	if ((decoder)->offset + (amount) > (decoder)->size) { \
		(decoder)->state = DECODER_STATE_OVERFLOWED; \
		return; \
	}

/* these all assume a decoder state of DECODER_STATE_NEED_MORE */
static void utf8_to_ucs4(charset_decode_t *decoder)
{
	DECODER_ASSERT_OVERFLOW(decoder, 1);

	const unsigned char *in = decoder->in + decoder->offset;
	const unsigned char c = in[0];

	if (c < 0x80) {
		decoder->codepoint = c;
		if (!c)
			decoder->state = DECODER_STATE_DONE;
		decoder->offset += 1;
	} else if (c < 0xC2) {
		decoder->state = DECODER_STATE_ILL_FORMED;
	} else if (c < 0xE0) {
		DECODER_ASSERT_OVERFLOW(decoder, 2);

		if ((in[1] ^ 0x80) < 0x40) {
			decoder->state = DECODER_STATE_NEED_MORE;
			decoder->offset += 2;
			decoder->codepoint = ((uint32_t) (c & 0x1f) << 6)
						 | (uint32_t) (in[1] ^ 0x80);
		} else {
			decoder->state = DECODER_STATE_ILL_FORMED;
		}
	} else if (c < 0xf0) {
		DECODER_ASSERT_OVERFLOW(decoder, 3);

		if ((in[1] ^ 0x80) < 0x40 && (in[2] ^ 0x80) < 0x40
				&& (c >= 0xE1 || in[1] >= 0xA0)
				&& (c != 0xED || in[1] < 0xA0)) {
			decoder->state = DECODER_STATE_NEED_MORE;
			decoder->codepoint = ((uint32_t) (c & 0x0f) << 12)
						 | ((uint32_t) (in[1] ^ 0x80) << 6)
						 | (uint32_t) (in[2] ^ 0x80);
			decoder->offset += 3;
		} else {
			decoder->state = DECODER_STATE_ILL_FORMED;
		}
	} else if (c < 0xf8) {
		DECODER_ASSERT_OVERFLOW(decoder, 4);

		if ((in[1] ^ 0x80) < 0x40 && (in[2] ^ 0x80) < 0x40
				&& (in[3] ^ 0x80) < 0x40
				&& (c >= 0xf1 || in[1] >= 0x90)
				&& (c < 0xf4 || (c == 0xf4 && in[1] < 0x90))) {
			decoder->state = DECODER_STATE_NEED_MORE;
			decoder->codepoint = ((uint32_t) (c & 0x07) << 18)
						 | ((uint32_t) (in[1] ^ 0x80) << 12)
						 | ((uint32_t) (in[2] ^ 0x80) << 6)
						 | (uint32_t) (in[3] ^ 0x80);
			decoder->offset += 4;
		} else {
			decoder->state = DECODER_STATE_ILL_FORMED;
		}
	} else decoder->state = DECODER_STATE_ILL_FORMED;
}

/* generic utf-16 decoder macro */
#define DECODE_UTF16_VARIANT(x) \
	static void utf16##x##_to_ucs4(charset_decode_t *decoder) \
	{ \
		DECODER_ASSERT_OVERFLOW(decoder, sizeof(uint16_t)); \
	\
		uint16_t wc; \
		memcpy(&wc, decoder->in + decoder->offset, sizeof(wc)); \
		wc = bswap##x##16(wc); \
		decoder->offset += 2; \
		decoder->state = DECODER_STATE_NEED_MORE; \
	\
		if (wc < 0xD800 || wc > 0xDFFF) { \
			decoder->codepoint = wc; \
			if (!wc) \
				decoder->state = DECODER_STATE_DONE; \
		} else if (wc >= 0xD800 && wc <= 0xDBFF) { \
			DECODER_ASSERT_OVERFLOW(decoder, sizeof(uint16_t)); \
	\
			uint16_t wc2; \
			memcpy(&wc2, decoder->in + decoder->offset, sizeof(wc2)); \
			wc2 = bswap##x##16(wc2); \
			decoder->offset += 2; \
	\
			if (wc2 >= 0xDC00 && wc2 <= 0xDFFF) { \
				decoder->codepoint = 0x10000 + ((wc - 0xD800) << 10) + (wc2 - 0xDC00); \
			} else decoder->state = DECODER_STATE_ILL_FORMED; \
		} \
	}

/* TODO: need to find some way to handle non-conforming file paths on win32;
 * one possible solution is storing as CESU-8[1] and interpreting CHARSET_CHAR
 * as it...
 *
 * [1]: https://www.unicode.org/reports/tr26/tr26-4.html */
DECODE_UTF16_VARIANT(LE)
DECODE_UTF16_VARIANT(BE)

#undef DECODE_UTF16_VARIANT

/* generic ucs-2 decoder macro */
#define DECODE_UCS2_VARIANT(x) \
	static void ucs2##x##_to_ucs4(charset_decode_t *decoder) \
	{ \
		DECODER_ASSERT_OVERFLOW(decoder, sizeof(uint16_t)); \
	\
		uint16_t wc; \
		memcpy(&wc, decoder->in + decoder->offset, sizeof(wc)); \
		decoder->codepoint = bswap##x##16(wc); \
		decoder->offset += sizeof(wc); \
		decoder->state = decoder->codepoint ? DECODER_STATE_NEED_MORE : DECODER_STATE_DONE; \
	}

DECODE_UCS2_VARIANT(LE)
DECODE_UCS2_VARIANT(BE)

#undef DECODE_UCS2_VARIANT

/* generic ucs-4 decoder macro */
#define DECODE_UCS4_VARIANT(x) \
	static void ucs4##x##_to_ucs4(charset_decode_t *decoder) \
	{ \
		DECODER_ASSERT_OVERFLOW(decoder, sizeof(uint32_t)); \
	\
		uint32_t wc; \
		memcpy(&wc, decoder->in + decoder->offset, 4); \
		decoder->codepoint = bswap##x##32(wc); \
		decoder->offset += 4; \
		decoder->state = decoder->codepoint ? DECODER_STATE_NEED_MORE : DECODER_STATE_DONE; \
	}

DECODE_UCS4_VARIANT(LE)
DECODE_UCS4_VARIANT(BE)

#undef DECODE_UCS4_VARIANT

static void itf_to_ucs4(charset_decode_t *decoder)
{
	/* this differs only from the cp437 table with 0x81, and 0xB3-0xE0.
	 * there are likely more characters that do not match up exactly */
	static const uint16_t itf_table[128] = {
		/* 0x80 */
		0x00C7, 0x203E, 0x00E9, 0x00E2, 0x00E4, 0x00E0, 0x00E5, 0x00E7,
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
		0x256A, 0x2518, 0x250C, 0x25A0, 0x2584, 0x258C, 0x2590, 0x2580,
		/* 0xE0 */
		0x03B1, 0x00DF, 0x0393, 0x03C0, 0x03A3, 0x03C3, 0x00B5, 0x03C4,
		0x03A6, 0x0398, 0x03A9, 0x03B4, 0x221E, 0x03C6, 0x03B5, 0x2229,
		/* 0xF0 */
		0x2261, 0x00B1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00F7, 0x2248,
		0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
	};

	DECODER_ASSERT_OVERFLOW(decoder, 1);

	unsigned char c = decoder->in[decoder->offset++];
	decoder->codepoint = (c < 0x80) ? c : itf_table[c - 0x80];
	decoder->state = (c ? DECODER_STATE_NEED_MORE : DECODER_STATE_DONE);
}

static void cp437_to_ucs4(charset_decode_t *decoder)
{
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
		0x00B0, 0x2219, 0x00B7, 0x221A, 0x207F, 0x00B2, 0x25A0, 0x00A0,
	};

	DECODER_ASSERT_OVERFLOW(decoder, 1);

	unsigned char c = decoder->in[decoder->offset++];
	decoder->codepoint = (c < 0x80) ? c : cp437_table[c - 0x80];
	decoder->state = c ? DECODER_STATE_NEED_MORE : DECODER_STATE_DONE;
}

static void windows1252_to_ucs4(charset_decode_t *decoder)
{
	/* Microsoft and the Unicode Consortium define positions 81, 8D, 8F, 90, and 9D
	 * as unused, HOWEVER, MultiByteToWideChar converts these to the corresponding
	 * C1 control codes. I've decided to convert these to a question mark, which is
	 * the same thing the Unicode -> CP437 conversion does. */
	static const uint16_t windows1252_table[32] = {
		/* 0x80 */
		0x20AC, 0x003F, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
		0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x003F, 0x017D, 0x003F,
		/* 0x90 */
		0x003F, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x003F, 0x017E, 0x0178,
	};

	DECODER_ASSERT_OVERFLOW(decoder, 1);

	unsigned char c = decoder->in[decoder->offset++];
	decoder->codepoint = (c >= 0x80 && c < 0xA0) ? windows1252_table[c - 0x80] : c;
	decoder->state = c ? DECODER_STATE_NEED_MORE : DECODER_STATE_DONE;
}

/* ------------------------------------------------------------------------ */

#define CHARSET_ENCODE_ERROR (-1)
#define CHARSET_ENCODE_SUCCESS (0)

static int ucs4_to_utf8(uint32_t ch, disko_t* out)
{
	unsigned char out_b[4];
	size_t len = 0;

	if (ch < 0x80) {
		out_b[len++] = (unsigned char)ch;
	} else if (ch < 0x800) {
		out_b[len++] = (unsigned char)((ch >> 6) | 0xC0);
		out_b[len++] = (unsigned char)((ch & 0x3F) | 0x80);
	} else if (ch < 0x10000) {
		out_b[len++] = (unsigned char)((ch >> 12) | 0xE0);
		out_b[len++] = (unsigned char)(((ch >> 6) & 0x3F) | 0x80);
		out_b[len++] = (unsigned char)((ch & 0x3F) | 0x80);
	} else if (ch < 0x110000) {
		out_b[len++] = (unsigned char)((ch >> 18) | 0xF0);
		out_b[len++] = (unsigned char)(((ch >> 12) & 0x3F) | 0x80);
		out_b[len++] = (unsigned char)(((ch >> 6) & 0x3F) | 0x80);
		out_b[len++] = (unsigned char)((ch & 0x3F) | 0x80);
	} else return CHARSET_ENCODE_ERROR; /* ZOMG NO WAY */

	disko_write(out, out_b, len);

	return len;
}

int char_unicode_to_cp437(uint32_t c)
{
	/* https://www.unicode.org/Public/MAPPINGS/VENDORS/MICSFT/PC/CP437.TXT */
	if (c <= 127) return c;

	switch (c) {
	case 0x00c7: return 0x80; /* LATIN CAPITAL LETTER C WITH CEDILLA */
	case 0x00fc: return 0x81; /* LATIN SMALL LETTER U WITH DIAERESIS */
	case 0x00e9: return 0x82; /* LATIN SMALL LETTER E WITH ACUTE */
	case 0x00e2: return 0x83; /* LATIN SMALL LETTER A WITH CIRCUMFLEX */
	case 0x00e4: return 0x84; /* LATIN SMALL LETTER A WITH DIAERESIS */
	case 0x00e0: return 0x85; /* LATIN SMALL LETTER A WITH GRAVE */
	case 0x00e5: return 0x86; /* LATIN SMALL LETTER A WITH RING ABOVE */
	case 0x00e7: return 0x87; /* LATIN SMALL LETTER C WITH CEDILLA */
	case 0x00ea: return 0x88; /* LATIN SMALL LETTER E WITH CIRCUMFLEX */
	case 0x00eb: return 0x89; /* LATIN SMALL LETTER E WITH DIAERESIS */
	case 0x00e8: return 0x8a; /* LATIN SMALL LETTER E WITH GRAVE */
	case 0x00ef: return 0x8b; /* LATIN SMALL LETTER I WITH DIAERESIS */
	case 0x00ee: return 0x8c; /* LATIN SMALL LETTER I WITH CIRCUMFLEX */
	case 0x00ec: return 0x8d; /* LATIN SMALL LETTER I WITH GRAVE */
	case 0x00c4: return 0x8e; /* LATIN CAPITAL LETTER A WITH DIAERESIS */
	case 0x00c5: return 0x8f; /* LATIN CAPITAL LETTER A WITH RING ABOVE */
	case 0x00c9: return 0x90; /* LATIN CAPITAL LETTER E WITH ACUTE */
	case 0x00e6: return 0x91; /* LATIN SMALL LIGATURE AE */
	case 0x00c6: return 0x92; /* LATIN CAPITAL LIGATURE AE */
	case 0x00f4: return 0x93; /* LATIN SMALL LETTER O WITH CIRCUMFLEX */
	case 0x00f6: return 0x94; /* LATIN SMALL LETTER O WITH DIAERESIS */
	case 0x00f2: return 0x95; /* LATIN SMALL LETTER O WITH GRAVE */
	case 0x00fb: return 0x96; /* LATIN SMALL LETTER U WITH CIRCUMFLEX */
	case 0x00f9: return 0x97; /* LATIN SMALL LETTER U WITH GRAVE */
	case 0x00ff: return 0x98; /* LATIN SMALL LETTER Y WITH DIAERESIS */
	case 0x00d6: return 0x99; /* LATIN CAPITAL LETTER O WITH DIAERESIS */
	case 0x00dc: return 0x9a; /* LATIN CAPITAL LETTER U WITH DIAERESIS */
	case 0x00a2: return 0x9b; /* CENT SIGN */
	case 0x00a3: return 0x9c; /* POUND SIGN */
	case 0x00a5: return 0x9d; /* YEN SIGN */
	case 0x20a7: return 0x9e; /* PESETA SIGN */
	case 0x0192: return 0x9f; /* LATIN SMALL LETTER F WITH HOOK */
	case 0x00e1: return 0xa0; /* LATIN SMALL LETTER A WITH ACUTE */
	case 0x00ed: return 0xa1; /* LATIN SMALL LETTER I WITH ACUTE */
	case 0x00f3: return 0xa2; /* LATIN SMALL LETTER O WITH ACUTE */
	case 0x00fa: return 0xa3; /* LATIN SMALL LETTER U WITH ACUTE */
	case 0x00f1: return 0xa4; /* LATIN SMALL LETTER N WITH TILDE */
	case 0x00d1: return 0xa5; /* LATIN CAPITAL LETTER N WITH TILDE */
	case 0x00aa: return 0xa6; /* FEMININE ORDINAL INDICATOR */
	case 0x00ba: return 0xa7; /* MASCULINE ORDINAL INDICATOR */
	case 0x00bf: return 0xa8; /* INVERTED QUESTION MARK */
	case 0x2310: return 0xa9; /* REVERSED NOT SIGN */
	case 0x00ac: return 0xaa; /* NOT SIGN */
	case 0x00bd: return 0xab; /* VULGAR FRACTION ONE HALF */
	case 0x00bc: return 0xac; /* VULGAR FRACTION ONE QUARTER */
	case 0x00a1: return 0xad; /* INVERTED EXCLAMATION MARK */
	case 0x00ab: return 0xae; /* LEFT-POINTING DOUBLE ANGLE QUOTATION MARK */
	case 0x00bb: return 0xaf; /* RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK */
	case 0x2591: return 0xb0; /* LIGHT SHADE */
	case 0x2592: return 0xb1; /* MEDIUM SHADE */
	case 0x2593: return 0xb2; /* DARK SHADE */
	case 0x2502: return 0xb3; /* BOX DRAWINGS LIGHT VERTICAL */
	case 0x2524: return 0xb4; /* BOX DRAWINGS LIGHT VERTICAL AND LEFT */
	case 0x2561: return 0xb5; /* BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE */
	case 0x2562: return 0xb6; /* BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE */
	case 0x2556: return 0xb7; /* BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE */
	case 0x2555: return 0xb8; /* BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE */
	case 0x2563: return 0xb9; /* BOX DRAWINGS DOUBLE VERTICAL AND LEFT */
	case 0x2551: return 0xba; /* BOX DRAWINGS DOUBLE VERTICAL */
	case 0x2557: return 0xbb; /* BOX DRAWINGS DOUBLE DOWN AND LEFT */
	case 0x255d: return 0xbc; /* BOX DRAWINGS DOUBLE UP AND LEFT */
	case 0x255c: return 0xbd; /* BOX DRAWINGS UP DOUBLE AND LEFT SINGLE */
	case 0x255b: return 0xbe; /* BOX DRAWINGS UP SINGLE AND LEFT DOUBLE */
	case 0x2510: return 0xbf; /* BOX DRAWINGS LIGHT DOWN AND LEFT */
	case 0x2514: return 0xc0; /* BOX DRAWINGS LIGHT UP AND RIGHT */
	case 0x2534: return 0xc1; /* BOX DRAWINGS LIGHT UP AND HORIZONTAL */
	case 0x252c: return 0xc2; /* BOX DRAWINGS LIGHT DOWN AND HORIZONTAL */
	case 0x251c: return 0xc3; /* BOX DRAWINGS LIGHT VERTICAL AND RIGHT */
	case 0x2500: return 0xc4; /* BOX DRAWINGS LIGHT HORIZONTAL */
	case 0x253c: return 0xc5; /* BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL */
	case 0x255e: return 0xc6; /* BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE */
	case 0x255f: return 0xc7; /* BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE */
	case 0x255a: return 0xc8; /* BOX DRAWINGS DOUBLE UP AND RIGHT */
	case 0x2554: return 0xc9; /* BOX DRAWINGS DOUBLE DOWN AND RIGHT */
	case 0x2569: return 0xca; /* BOX DRAWINGS DOUBLE UP AND HORIZONTAL */
	case 0x2566: return 0xcb; /* BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL */
	case 0x2560: return 0xcc; /* BOX DRAWINGS DOUBLE VERTICAL AND RIGHT */
	case 0x2550: return 0xcd; /* BOX DRAWINGS DOUBLE HORIZONTAL */
	case 0x256c: return 0xce; /* BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL */
	case 0x2567: return 0xcf; /* BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE */
	case 0x2568: return 0xd0; /* BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE */
	case 0x2564: return 0xd1; /* BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE */
	case 0x2565: return 0xd2; /* BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE */
	case 0x2559: return 0xd3; /* BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE */
	case 0x2558: return 0xd4; /* BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE */
	case 0x2552: return 0xd5; /* BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE */
	case 0x2553: return 0xd6; /* BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE */
	case 0x256b: return 0xd7; /* BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE */
	case 0x256a: return 0xd8; /* BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE */
	case 0x2518: return 0xd9; /* BOX DRAWINGS LIGHT UP AND LEFT */
	case 0x250c: return 0xda; /* BOX DRAWINGS LIGHT DOWN AND RIGHT */
	case 0x2588: return 0xdb; /* FULL BLOCK */
	case 0x2584: return 0xdc; /* LOWER HALF BLOCK */
	case 0x258c: return 0xdd; /* LEFT HALF BLOCK */
	case 0x2590: return 0xde; /* RIGHT HALF BLOCK */
	case 0x2580: return 0xdf; /* UPPER HALF BLOCK */
	case 0x03b1: return 0xe0; /* GREEK SMALL LETTER ALPHA */
	case 0x00df: return 0xe1; /* LATIN SMALL LETTER SHARP S */
	case 0x0393: return 0xe2; /* GREEK CAPITAL LETTER GAMMA */
	case 0x03c0: return 0xe3; /* GREEK SMALL LETTER PI */
	case 0x03a3: return 0xe4; /* GREEK CAPITAL LETTER SIGMA */
	case 0x03c3: return 0xe5; /* GREEK SMALL LETTER SIGMA */
	case 0x00b5: return 0xe6; /* MICRO SIGN */
	case 0x03c4: return 0xe7; /* GREEK SMALL LETTER TAU */
	case 0x03a6: return 0xe8; /* GREEK CAPITAL LETTER PHI */
	case 0x0398: return 0xe9; /* GREEK CAPITAL LETTER THETA */
	case 0x03a9: return 0xea; /* GREEK CAPITAL LETTER OMEGA */
	case 0x03b4: return 0xeb; /* GREEK SMALL LETTER DELTA */
	case 0x221e: return 0xec; /* INFINITY */
	case 0x03c6: return 0xed; /* GREEK SMALL LETTER PHI */
	case 0x03b5: return 0xee; /* GREEK SMALL LETTER EPSILON */
	case 0x2229: return 0xef; /* INTERSECTION */
	case 0x2261: return 0xf0; /* IDENTICAL TO */
	case 0x00b1: return 0xf1; /* PLUS-MINUS SIGN */
	case 0x2265: return 0xf2; /* GREATER-THAN OR EQUAL TO */
	case 0x2264: return 0xf3; /* LESS-THAN OR EQUAL TO */
	case 0x2320: return 0xf4; /* TOP HALF INTEGRAL */
	case 0x2321: return 0xf5; /* BOTTOM HALF INTEGRAL */
	case 0x00f7: return 0xf6; /* DIVISION SIGN */
	case 0x2248: return 0xf7; /* ALMOST EQUAL TO */
	case 0x00b0: return 0xf8; /* DEGREE SIGN */
	case 0x2219: return 0xf9; /* BULLET OPERATOR */
	case 0x00b7: return 0xfa; /* MIDDLE DOT */
	case 0x221a: return 0xfb; /* SQUARE ROOT */
	case 0x207f: return 0xfc; /* SUPERSCRIPT LATIN SMALL LETTER N */
	case 0x00b2: return 0xfd; /* SUPERSCRIPT TWO */
	case 0x25a0: return 0xfe; /* BLACK SQUARE */
	case 0x00a0: return 0xff; /* NO-BREAK SPACE */

	/* -- CUSTOM CASES */
	case 0x2019: return 39; // fancy apostrophe
	case 0x00B3: return 51; // superscript three
	}

	return -1;
}

/* for cyrillic (russian etc.) language rendering */
int char_unicode_to_cp866(uint32_t c)
{
	if (c < 0x80) return c;

	switch (c) {
	case 0x0410: return 0x80; // CYRILLIC CAPITAL LETTER A
	case 0x0411: return 0x81; // CYRILLIC CAPITAL LETTER BE
	case 0x0412: return 0x82; // CYRILLIC CAPITAL LETTER VE
	case 0x0413: return 0x83; // CYRILLIC CAPITAL LETTER GHE
	case 0x0414: return 0x84; // CYRILLIC CAPITAL LETTER DE
	case 0x0415: return 0x85; // CYRILLIC CAPITAL LETTER IE
	case 0x0416: return 0x86; // CYRILLIC CAPITAL LETTER ZHE
	case 0x0417: return 0x87; // CYRILLIC CAPITAL LETTER ZE
	case 0x0418: return 0x88; // CYRILLIC CAPITAL LETTER I
	case 0x0419: return 0x89; // CYRILLIC CAPITAL LETTER SHORT I
	case 0x041A: return 0x8A; // CYRILLIC CAPITAL LETTER KA
	case 0x041B: return 0x8B; // CYRILLIC CAPITAL LETTER EL
	case 0x041C: return 0x8C; // CYRILLIC CAPITAL LETTER EM
	case 0x041D: return 0x8D; // CYRILLIC CAPITAL LETTER EN
	case 0x041E: return 0x8E; // CYRILLIC CAPITAL LETTER O
	case 0x041F: return 0x8F; // CYRILLIC CAPITAL LETTER PE
	case 0x0420: return 0x90; // CYRILLIC CAPITAL LETTER ER
	case 0x0421: return 0x91; // CYRILLIC CAPITAL LETTER ES
	case 0x0422: return 0x92; // CYRILLIC CAPITAL LETTER TE
	case 0x0423: return 0x93; // CYRILLIC CAPITAL LETTER U
	case 0x0424: return 0x94; // CYRILLIC CAPITAL LETTER EF
	case 0x0425: return 0x95; // CYRILLIC CAPITAL LETTER HA
	case 0x0426: return 0x96; // CYRILLIC CAPITAL LETTER TSE
	case 0x0427: return 0x97; // CYRILLIC CAPITAL LETTER CHE
	case 0x0428: return 0x98; // CYRILLIC CAPITAL LETTER SHA
	case 0x0429: return 0x99; // CYRILLIC CAPITAL LETTER SHCHA
	case 0x042A: return 0x9A; // CYRILLIC CAPITAL LETTER HARD SIGN
	case 0x042B: return 0x9B; // CYRILLIC CAPITAL LETTER YERU
	case 0x042C: return 0x9C; // CYRILLIC CAPITAL LETTER SOFT SIGN
	case 0x042D: return 0x9D; // CYRILLIC CAPITAL LETTER E
	case 0x042E: return 0x9E; // CYRILLIC CAPITAL LETTER YU
	case 0x042F: return 0x9F; // CYRILLIC CAPITAL LETTER YA
	case 0x0430: return 0xA0; // CYRILLIC SMALL LETTER A
	case 0x0431: return 0xA1; // CYRILLIC SMALL LETTER BE
	case 0x0432: return 0xA2; // CYRILLIC SMALL LETTER VE
	case 0x0433: return 0xA3; // CYRILLIC SMALL LETTER GHE
	case 0x0434: return 0xA4; // CYRILLIC SMALL LETTER DE
	case 0x0435: return 0xA5; // CYRILLIC SMALL LETTER IE
	case 0x0436: return 0xA6; // CYRILLIC SMALL LETTER ZHE
	case 0x0437: return 0xA7; // CYRILLIC SMALL LETTER ZE
	case 0x0438: return 0xA8; // CYRILLIC SMALL LETTER I
	case 0x0439: return 0xA9; // CYRILLIC SMALL LETTER SHORT I
	case 0x043A: return 0xAA; // CYRILLIC SMALL LETTER KA
	case 0x043B: return 0xAB; // CYRILLIC SMALL LETTER EL
	case 0x043C: return 0xAC; // CYRILLIC SMALL LETTER EM
	case 0x043D: return 0xAD; // CYRILLIC SMALL LETTER EN
	case 0x043E: return 0xAE; // CYRILLIC SMALL LETTER O
	case 0x043F: return 0xAF; // CYRILLIC SMALL LETTER PE
	case 0x2591: return 0xB0; // LIGHT SHADE
	case 0x2592: return 0xB1; // MEDIUM SHADE
	case 0x2593: return 0xB2; // DARK SHADE
	case 0x2502: return 0xB3; // BOX DRAWINGS LIGHT VERTICAL
	case 0x2524: return 0xB4; // BOX DRAWINGS LIGHT VERTICAL AND LEFT
	case 0x2561: return 0xB5; // BOX DRAWINGS VERTICAL SINGLE AND LEFT DOUBLE
	case 0x2562: return 0xB6; // BOX DRAWINGS VERTICAL DOUBLE AND LEFT SINGLE
	case 0x2556: return 0xB7; // BOX DRAWINGS DOWN DOUBLE AND LEFT SINGLE
	case 0x2555: return 0xB8; // BOX DRAWINGS DOWN SINGLE AND LEFT DOUBLE
	case 0x2563: return 0xB9; // BOX DRAWINGS DOUBLE VERTICAL AND LEFT
	case 0x2551: return 0xBA; // BOX DRAWINGS DOUBLE VERTICAL
	case 0x2557: return 0xBB; // BOX DRAWINGS DOUBLE DOWN AND LEFT
	case 0x255D: return 0xBC; // BOX DRAWINGS DOUBLE UP AND LEFT
	case 0x255C: return 0xBD; // BOX DRAWINGS UP DOUBLE AND LEFT SINGLE
	case 0x255B: return 0xBE; // BOX DRAWINGS UP SINGLE AND LEFT DOUBLE
	case 0x2510: return 0xBF; // BOX DRAWINGS LIGHT DOWN AND LEFT
	case 0x2514: return 0xC0; // BOX DRAWINGS LIGHT UP AND RIGHT
	case 0x2534: return 0xC1; // BOX DRAWINGS LIGHT UP AND HORIZONTAL
	case 0x252C: return 0xC2; // BOX DRAWINGS LIGHT DOWN AND HORIZONTAL
	case 0x251C: return 0xC3; // BOX DRAWINGS LIGHT VERTICAL AND RIGHT
	case 0x2500: return 0xC4; // BOX DRAWINGS LIGHT HORIZONTAL
	case 0x253C: return 0xC5; // BOX DRAWINGS LIGHT VERTICAL AND HORIZONTAL
	case 0x255E: return 0xC6; // BOX DRAWINGS VERTICAL SINGLE AND RIGHT DOUBLE
	case 0x255F: return 0xC7; // BOX DRAWINGS VERTICAL DOUBLE AND RIGHT SINGLE
	case 0x255A: return 0xC8; // BOX DRAWINGS DOUBLE UP AND RIGHT
	case 0x2554: return 0xC9; // BOX DRAWINGS DOUBLE DOWN AND RIGHT
	case 0x2569: return 0xCA; // BOX DRAWINGS DOUBLE UP AND HORIZONTAL
	case 0x2566: return 0xCB; // BOX DRAWINGS DOUBLE DOWN AND HORIZONTAL
	case 0x2560: return 0xCC; // BOX DRAWINGS DOUBLE VERTICAL AND RIGHT
	case 0x2550: return 0xCD; // BOX DRAWINGS DOUBLE HORIZONTAL
	case 0x256C: return 0xCE; // BOX DRAWINGS DOUBLE VERTICAL AND HORIZONTAL
	case 0x2567: return 0xCF; // BOX DRAWINGS UP SINGLE AND HORIZONTAL DOUBLE
	case 0x2568: return 0xD0; // BOX DRAWINGS UP DOUBLE AND HORIZONTAL SINGLE
	case 0x2564: return 0xD1; // BOX DRAWINGS DOWN SINGLE AND HORIZONTAL DOUBLE
	case 0x2565: return 0xD2; // BOX DRAWINGS DOWN DOUBLE AND HORIZONTAL SINGLE
	case 0x2559: return 0xD3; // BOX DRAWINGS UP DOUBLE AND RIGHT SINGLE
	case 0x2558: return 0xD4; // BOX DRAWINGS UP SINGLE AND RIGHT DOUBLE
	case 0x2552: return 0xD5; // BOX DRAWINGS DOWN SINGLE AND RIGHT DOUBLE
	case 0x2553: return 0xD6; // BOX DRAWINGS DOWN DOUBLE AND RIGHT SINGLE
	case 0x256B: return 0xD7; // BOX DRAWINGS VERTICAL DOUBLE AND HORIZONTAL SINGLE
	case 0x256A: return 0xD8; // BOX DRAWINGS VERTICAL SINGLE AND HORIZONTAL DOUBLE
	case 0x2518: return 0xD9; // BOX DRAWINGS LIGHT UP AND LEFT
	case 0x250C: return 0xDA; // BOX DRAWINGS LIGHT DOWN AND RIGHT
	case 0x2588: return 0xDB; // FULL BLOCK
	case 0x2584: return 0xDC; // LOWER HALF BLOCK
	case 0x258C: return 0xDD; // LEFT HALF BLOCK
	case 0x2590: return 0xDE; // RIGHT HALF BLOCK
	case 0x2580: return 0xDF; // UPPER HALF BLOCK
	case 0x0440: return 0xE0; // CYRILLIC SMALL LETTER ER
	case 0x0441: return 0xE1; // CYRILLIC SMALL LETTER ES
	case 0x0442: return 0xE2; // CYRILLIC SMALL LETTER TE
	case 0x0443: return 0xE3; // CYRILLIC SMALL LETTER U
	case 0x0444: return 0xE4; // CYRILLIC SMALL LETTER EF
	case 0x0445: return 0xE5; // CYRILLIC SMALL LETTER HA
	case 0x0446: return 0xE6; // CYRILLIC SMALL LETTER TSE
	case 0x0447: return 0xE7; // CYRILLIC SMALL LETTER CHE
	case 0x0448: return 0xE8; // CYRILLIC SMALL LETTER SHA
	case 0x0449: return 0xE9; // CYRILLIC SMALL LETTER SHCHA
	case 0x044A: return 0xEA; // CYRILLIC SMALL LETTER HARD SIGN
	case 0x044B: return 0xEB; // CYRILLIC SMALL LETTER YERU
	case 0x044C: return 0xEC; // CYRILLIC SMALL LETTER SOFT SIGN
	case 0x044D: return 0xED; // CYRILLIC SMALL LETTER E
	case 0x044E: return 0xEE; // CYRILLIC SMALL LETTER YU
	case 0x044F: return 0xEF; // CYRILLIC SMALL LETTER YA
	case 0x0401: return 0xF0; // CYRILLIC CAPITAL LETTER IO
	case 0x0451: return 0xF1; // CYRILLIC SMALL LETTER IO
	case 0x0404: return 0xF2; // CYRILLIC CAPITAL LETTER UKRAINIAN IE
	case 0x0454: return 0xF3; // CYRILLIC SMALL LETTER UKRAINIAN IE
	case 0x0407: return 0xF4; // CYRILLIC CAPITAL LETTER YI
	case 0x0457: return 0xF5; // CYRILLIC SMALL LETTER YI
	case 0x040E: return 0xF6; // CYRILLIC CAPITAL LETTER SHORT U
	case 0x045E: return 0xF7; // CYRILLIC SMALL LETTER SHORT U
	case 0x00B0: return 0xF8; // DEGREE SIGN
	case 0x2219: return 0xF9; // BULLET OPERATOR
	case 0x00B7: return 0xFA; // MIDDLE DOT
	case 0x221A: return 0xFB; // SQUARE ROOT
	case 0x2116: return 0xFC; // NUMERO SIGN
	case 0x00A4: return 0xFD; // CURRENCY SIGN
	case 0x25A0: return 0xFE; // BLACK SQUARE
	}

	return -1;
}

int char_unicode_to_itf(uint32_t c)
{
	if (!c || (c >= 32 && c <= 127)) return c;

	switch (c) {
	case 0x263A: return 1;  // WHITE SMILING FACE
	case 0x263B: return 2;  // BLACK SMILING FACE
	case 0x2661:
	case 0x2665: return 3;  // BLACK HEART
	case 0x2662:
	case 0x25C6:
	case 0x2666: return 4;  // BLACK DIAMOND
	case 0x2667:
	case 0x2663: return 5;  // BLACK CLUBS
	case 0x2664:
	case 0x2660: return 6;  // BLACK SPADE
	case 0x25CF: return 7;  // BLACK CIRCLE
	case 0x25D8: return 8;  // INVERSE BULLET
	case 0x25CB:
	case 0x25E6:
	case 0x25EF: return 9;  // LARGE CIRCLE
	case 0x25D9: return 10; // INVERSE WHITE CIRCLE
	case 0x2642: return 11; // MALE / MARS
	case 0x2640: return 12; // FEMALE / VENUS
	case 0x266A: return 13; // EIGHTH NOTE
	case 0x266B: return 14; // BEAMED EIGHTH NOTES

	case 0x2195: return 18; // UP DOWN ARROW
	case 0x203C: return 19; // DOUBLE EXCLAMATION MARK
	case 0x00B6: return 20; // PILCROW SIGN
	case 0x00A7: return 21; // SECTION SIGN

	case 0x21A8: return 23; // UP DOWN ARROW WITH BASE
	case 0x2191: return 24; // UPWARD ARROW
	case 0x2193: return 25; // DOWNWARD ARROW
	case 0x2192: return 26; // RIGHTWARD ARROW
	case 0x2190: return 27; // LEFTWARD ARROW

	case 0x2194: return 29; // LEFT RIGHT ARROW

	case 0x266F: return '#';// MUSIC SHARP SIGN
	case 0x00A6: return 124;
	case 0x0394:
	case 0x2302: return 127;// HOUSE

	case 0x203E: return 129;// UNDERLINE (???)

	case 0x20B5:
	case 0x20B2:
	case 0x00A2: return 155;// CENT SIGN
	case 0x00A3: return 156;// POUND SIGN
	case 0x00A5: return 157;// YEN SIGN

	case 0x2310: return 169;// REVERSED NOT SIGN
	case 0x00AC: return 170;// NOT SIGN
	case 0x00BD: return 171;// 1/2
	case 0x00BC: return 172;// 1/4
	case 0x00A1: return 173;// INVERTED EXCLAMATION MARK
	case 0x00AB: return 174;// <<
	case 0x00BB: return 175;// >>

	case 0x2591: return 176;// LIGHT SHADE
	case 0x2592: return 177;// MEDIUM SHADE
	case 0x2593: return 178;// DARK SHADE

	// BOX DRAWING
	case 0x2502: return 179;
	case 0x2524: return 180;
	case 0x2561: return 181;
	case 0x2562: return 182;
	case 0x2556: return 183;
	case 0x2555: return 184;
	case 0x2563: return 185;
	case 0x2551: return 186;
	case 0x2557: return 187;
	case 0x255D: return 188;
	case 0x255C: return 189;
	case 0x255B: return 190;
	case 0x2510: return 191;
	case 0x2514: return 192;
	case 0x2534: return 193;
	case 0x252C: return 194;
	case 0x251C: return 195;
	case 0x2500: return 196;
	case 0x253C: return 197;
	case 0x255E: return 198;
	case 0x255F: return 199;
	case 0x255A: return 200;
	case 0x2554: return 201;
	case 0x2569: return 202;
	case 0x2566: return 203;
	case 0x2560: return 204;
	case 0x2550: return 205;
	case 0x256C: return 206;
	case 0x2567: return 207;
	case 0x2568: return 208;
	case 0x2564: return 209;
	case 0x2565: return 210;
	case 0x2559: return 211;
	case 0x2558: return 212;
	case 0x2552: return 213;
	case 0x2553: return 214;
	case 0x256B: return 215;
	case 0x256A: return 216;
	case 0x2518: return 217;
	case 0x250C: return 218;
	case 0x25A0: return 219;// BLACK SQUARE
	case 0x2584: return 220;// LOWER HALF BLOCK
	case 0x258C: return 221;// LEFT HALF BLOCK
	case 0x2590: return 222;// RIGHT HALF BLOCK
	case 0x2580: return 223;// UPPER HALF BLOCK

	case 0x03B1: return 224;// GREEK SMALL LETTER ALPHA
	case 0x03B2: return 225;// GREEK SMALL LETTER BETA
	case 0x0393: return 226;// GREEK CAPITAL LETTER GAMMA
	case 0x03C0: return 227;// mmm... pie...
	case 0x03A3:
	case 0x2211: return 228;// N-ARY SUMMATION / CAPITAL SIGMA
	case 0x03C3: return 229;// GREEK SMALL LETTER SIGMA
	case 0x03BC:
	case 0x00b5: return 230;// GREEK SMALL LETTER MU
	case 0x03C4:
	case 0x03D2: return 231;// GREEK UPSILON+HOOK

	case 0x03B8: return 233;// GREEK SMALL LETTER THETA
	case 0x03A9: return 234;// GREEK CAPITAL LETTER OMEGA
	case 0x03B4: return 235;// GREEK SMALL LETTER DELTA

	case 0x221E: return 236;// INFINITY
	case 0x00D8:
	case 0x00F8: return 237;// LATIN ... LETTER O WITH STROKE
	case 0x03F5: return 238;// GREEK LUNATE EPSILON SYMBOL
	case 0x2229:
	case 0x03A0: return 239;// GREEK CAPITAL LETTER PI
	case 0x039E: return 240;// GREEK CAPITAL LETTER XI
	case 0x00b1: return 241;// PLUS-MINUS SIGN
	case 0x2265: return 242;// GREATER-THAN OR EQUAL TO
	case 0x2264: return 243;// LESS-THAN OR EQUAL TO
	case 0x2320: return 244;// TOP HALF INTEGRAL
	case 0x2321: return 245;// BOTTOM HALF INTEGRAL
	case 0x00F7: return 246;// DIVISION SIGN
	case 0x2248: return 247;// ALMOST EQUAL TO
	case 0x00B0: return 248;// DEGREE SIGN
	case 0x00B7: return 249;// MIDDLE DOT
	case 0x2219:
	case 0x0387: return 250;// GREEK ANO TELEIA
	case 0x221A: return 251;// SQUARE ROOT
	// NO UNICODE ALLOCATION?
	case 0x00B2: return 253;// SUPERSCRIPT TWO
	case 0x220E: return 254;// QED

	// No idea if this is right ;P
	case 0x00A0: return 255;
	}

	/* nothing */
	return -1;
}

static int ucs4_to_cp437(uint32_t ch, disko_t* out)
{
	uint8_t out_c8;
	int out_c;

	out_c = char_unicode_to_cp437(ch);
	out_c8 = (out_c < 0) ? '?' : out_c;

	disko_write(out, &out_c8, sizeof(out_c8));

	return CHARSET_ENCODE_SUCCESS;
}

static int ucs4_to_itf(uint32_t ch, disko_t *out)
{
	uint8_t out_c8;
	int out_c;

	out_c = char_unicode_to_itf(ch);
	out_c8 = (out_c < 0) ? '?' : out_c;

	disko_write(out, &out_c8, sizeof(out_c8));

	return CHARSET_ENCODE_SUCCESS;
}

#define ENCODE_UTF16_VARIANT(x) \
	static int ucs4_to_utf16##x(uint32_t ch, disko_t* out) \
	{ \
		uint16_t out_b[2]; \
		size_t len = 0; \
	\
		if (ch < 0x10000) { \
			out_b[len++] = bswap##x##16(ch); \
		} else if (ch < 0x110000) { \
			uint16_t w1 = 0xD800 + ((ch - 0x10000) >> 10); \
			uint16_t w2 = 0xDC00 + ((ch - 0x10000) & 0x3FF); \
	\
			out_b[len++] = bswap##x##16(w1); \
			out_b[len++] = bswap##x##16(w2); \
		} else return CHARSET_ENCODE_ERROR; \
	\
		disko_write(out, out_b, len * 2);\
	\
		return CHARSET_ENCODE_SUCCESS; \
	}

ENCODE_UTF16_VARIANT(LE)
ENCODE_UTF16_VARIANT(BE)

#undef ENCODE_UTF16_VARIANT

#define ENCODE_UCS2_VARIANT(x) \
	static int ucs4_to_ucs2##x(uint32_t ch, disko_t* out) \
	{ \
		if (ch >= 0x10000) \
			return CHARSET_ERROR_ENCODE; \
	\
		uint16_t ch16 = bswap##x##16(ch); \
	\
		disko_write(out, &ch16, sizeof(ch16)); \
	\
		return CHARSET_ENCODE_SUCCESS; \
	}

ENCODE_UCS2_VARIANT(LE)
ENCODE_UCS2_VARIANT(BE)

#undef ENCODE_UCS2_VARIANT

#define ENCODE_UCS4_VARIANT(x) \
	static int ucs4_to_ucs4##x(uint32_t ch, disko_t* out) \
	{ \
		ch = bswap##x##32(ch); \
		disko_write(out, &ch, sizeof(ch)); \
		return CHARSET_ENCODE_SUCCESS; \
	}

ENCODE_UCS4_VARIANT(LE)
ENCODE_UCS4_VARIANT(BE)

#undef ENCODE_UCS4_VARIANT

/* ----------------------------------------------------------------------- */

#ifdef SCHISM_WIN32
/* Before Windows 2000, wchar_t was actually UCS-2, and not UTF-16. */

static void wchar_to_ucs4(charset_decode_t *decoder)
{
	return (win32_ntver_atleast(5, 0, 0) ? utf16LE_to_ucs4 : ucs2LE_to_ucs4)(decoder);
}

static int ucs4_to_wchar(uint32_t ch, disko_t *out)
{
	return (win32_ntver_atleast(5, 0, 0) ? ucs4_to_utf16LE : ucs4_to_ucs2LE)(ch, out);
}

static void ansi_to_ucs4(charset_decode_t *decoder)
{
	/* half-assed ANSI to UCS-4 converter */
	size_t i;
	size_t j;
	wchar_t wc[8];
	int r = 0;
	charset_decode_t wcd;

	switch (GetACP()) {
	//case 1252:
	//	windows1252_to_ucs4(decoder);
	//	return;
	case 437:
		cp437_to_ucs4(decoder);
		return;
	}

	/* ok, here's the thing: the ANSI windows codepage can be variable width,
	 * which means we can't simply convert one byte. eventually I'd like to
	 * do one big conversion (maybe 512 chars at a time) and cache it for
	 * later decoding, but eh
	 *
	 * This seems to work right, but only for windows-1252; shift-jis and
	 * other var-width charsets haven't been tested. */
	for (i = 1; i < (decoder->size - decoder->offset); i++) {
		/* this is royally stupid */
		j = 0;
		do {
			j++;
			r = MultiByteToWideChar(CP_ACP, 0, decoder->in + decoder->offset, i, wc, j);
		} while (j < 8 && r == 0 && GetLastError() == ERROR_INSUFFICIENT_BUFFER);

		if (r != 0)
			break;
	}

	/* :) */
	decoder->offset += i;

	if (r == 0) {
		decoder->state = DECODER_STATE_ILL_FORMED;
		decoder->codepoint = 0;
		return;
	}

	wcd.in = (const unsigned char *)wc;
	wcd.offset = 0;
	wcd.size = (r * sizeof(wchar_t));

	/* convert wchar to utf-8 */
	wchar_to_ucs4(&wcd);

	decoder->state = wcd.state;
	decoder->codepoint = wcd.codepoint;
}

static int ucs4_to_ansi(uint32_t ch, disko_t *out)
{
	uint16_t out_b[2];
	size_t len = 0;
	char *buf;
	int buflen;

	switch (GetACP()) {
	case 437:
		return ucs4_to_cp437(ch, out);
	}

	if (win32_ntver_atleast(5, 0, 0)) {
		/* copied from ucs4_to_utf16LE */
		if (ch < 0x10000) {
			out_b[len++] = bswapLE16(ch);
		} else if (ch < 0x110000) {
			uint16_t w1 = 0xD800 + ((ch - 0x10000) >> 10);
			uint16_t w2 = 0xDC00 + ((ch - 0x10000) & 0x3FF);

			out_b[len++] = bswapLE16(w1);
			out_b[len++] = bswapLE16(w2);
		} else return CHARSET_ENCODE_ERROR;	
	} else {
		/* wchar_t is UCS-2 */
		out_b[len++] = bswapLE16(ch);
	}

	/* WC_ERR_INVALID_CHARS would be nice here, but it's not supported
	 * for anything other than UTF-8. Thus we get sporadic failures when
	 * converting to/from ACP. This is less of a problem for old systems,
	 * where everything is going to be in ACP anyway. */
	buflen = WideCharToMultiByte(CP_ACP, 0, out_b, len, NULL, 0, NULL, NULL);
	if (buflen <= 0)
		return CHARSET_ENCODE_ERROR;

	buf = malloc(buflen);
	if (!buf)
		return CHARSET_ENCODE_ERROR; /* NOMEM ? */

	buflen = WideCharToMultiByte(CP_ACP, 0, out_b, len, buf, buflen, NULL, NULL);
	if (buflen <= 0) {
		free(buf);
		return CHARSET_ENCODE_ERROR;
	}

	disko_write(out, buf, buflen);

	free(buf);

	return CHARSET_ENCODE_SUCCESS;
}
#endif

/* ----------------------------------------------------------------------- */

/* function LUT here */
typedef void (*charset_conv_to_ucs4_func)(charset_decode_t *decoder);
typedef int (*charset_conv_from_ucs4_func)(uint32_t, disko_t*);

static const charset_conv_to_ucs4_func conv_to_ucs4_funcs[] = {
	[CHARSET_UTF8] = utf8_to_ucs4,
	[CHARSET_UCS4LE] = ucs4LE_to_ucs4,
	[CHARSET_UCS4BE] = ucs4BE_to_ucs4,
	/* primarily used on Windows */
	[CHARSET_UTF16LE] = utf16LE_to_ucs4,
	[CHARSET_UTF16BE] = utf16BE_to_ucs4,
	[CHARSET_UCS2LE] = ucs2LE_to_ucs4,
	[CHARSET_UCS2BE] = ucs2BE_to_ucs4,

	[CHARSET_ITF] = itf_to_ucs4,
	[CHARSET_CP437] = cp437_to_ucs4,
	[CHARSET_WINDOWS1252] = windows1252_to_ucs4,

	[CHARSET_CHAR] = utf8_to_ucs4,
#ifdef SCHISM_WIN32
	[CHARSET_ANSI] = ansi_to_ucs4,
	[CHARSET_WCHAR_T] = wchar_to_ucs4,
#elif defined(SCHISM_XBOX)
	[CHARSET_WCHAR_T] = utf16LE_to_ucs4,
#else
	/* unimplemented */
	[CHARSET_WCHAR_T] = NULL,
#endif
};

static const charset_conv_from_ucs4_func conv_from_ucs4_funcs[] = {
	[CHARSET_UTF8] = ucs4_to_utf8,
	[CHARSET_UTF16LE] = ucs4_to_utf16LE,
	[CHARSET_UTF16BE] = ucs4_to_utf16BE,
	[CHARSET_UCS2LE] = ucs4_to_ucs2LE,
	[CHARSET_UCS2BE] = ucs4_to_ucs2BE,
	[CHARSET_UCS4LE] = ucs4_to_ucs4LE,
	[CHARSET_UCS4BE] = ucs4_to_ucs4BE,

	/* these two share the same impl: */
	[CHARSET_ITF]   = ucs4_to_itf,
	[CHARSET_CP437] = ucs4_to_cp437,

	[CHARSET_CHAR] = ucs4_to_utf8,
#ifdef SCHISM_WIN32
	[CHARSET_ANSI] = ucs4_to_ansi,
	[CHARSET_WCHAR_T] = ucs4_to_wchar,
#elif defined(SCHISM_XBOX)
	[CHARSET_WCHAR_T] = ucs4_to_utf16LE,
#else
	/* unimplemented */
	[CHARSET_WCHAR_T] = NULL,
#endif
};

/* for debugging */
SCHISM_CONST const char* charset_iconv_error_lookup(charset_error_t err)
{
	switch (err) {
	case CHARSET_ERROR_SUCCESS:
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
	default:
		return "Unknown error";
	}
}

#define CHARSET_VARIATION(name) \
	static charset_error_t charset_iconv_##name##_(const void* in, void* out, charset_t inset, charset_t outset, size_t insize)

static const size_t charset_size_estimate_divisor[] = {
	[CHARSET_UCS4LE] = 4,
	[CHARSET_UCS4BE] = 4,
	[CHARSET_UTF16LE] = 4,
	[CHARSET_UTF16BE] = 4,
	[CHARSET_UCS2LE] = 2,
	[CHARSET_UCS2BE] = 2,
	[CHARSET_UTF8] = 4,
	[CHARSET_ITF] = 1,
	[CHARSET_CP437] = 1,
	[CHARSET_WINDOWS1252] = 1,
	[CHARSET_CHAR] = 4, /* UTF-8 ??? */
	[CHARSET_WCHAR_T] = sizeof(wchar_t),
#if defined(SCHISM_WIN32) || defined(SCHISM_XBOX)
	[CHARSET_ANSI] = 1,
#endif
#ifdef SCHISM_OS2
	[CHARSET_DOSCP] = 1,
#endif
#ifdef SCHISM_MACOS
	[CHARSET_SYSTEMSCRIPT] = 1,
#endif
};

/* our version of iconv; this has a much simpler API than the regular
 * iconv() because much of it isn't very necessary for our purposes
 *
 * note: do NOT put huge buffers into here, it will likely waste
 * lots of memory due to using a buffer of UCS4 inbetween
 *
 * all input is expected to be NULL-terminated
 *
 * example usage:
 *     unsigned char *cp437 = some_buf, *utf8 = NULL;
 *     charset_iconv(cp437, &utf8, CHARSET_CP437, CHARSET_UTF8);
 * 
 * [out] must be free'd by the caller */
CHARSET_VARIATION(internal)
{
	charset_decode_t decoder = {0};

	decoder.in = in;
	decoder.offset = 0;
	decoder.size = insize;

	if (inset >= ARRAY_SIZE(conv_to_ucs4_funcs) || outset >= ARRAY_SIZE(conv_from_ucs4_funcs))
		return CHARSET_ERROR_UNIMPLEMENTED;

	charset_conv_to_ucs4_func   conv_to_ucs4_func   = conv_to_ucs4_funcs[inset];
	charset_conv_from_ucs4_func conv_from_ucs4_func = conv_from_ucs4_funcs[outset];

	if (!conv_to_ucs4_func || !conv_from_ucs4_func)
		return CHARSET_ERROR_UNIMPLEMENTED;

	disko_t ds = {0};
	if (insize != SIZE_MAX) {
		/* if we know a size, give an estimate as to how
		 * much space it'll take up. */
		disko_memopen_estimate(&ds, (uint64_t)insize
			* charset_size_estimate_divisor[outset]
			/ charset_size_estimate_divisor[inset]);
	} else {
		disko_memopen(&ds);
	}

	do {
		conv_to_ucs4_func(&decoder);
		if (decoder.state < 0) {
			disko_memclose(&ds, 0);
			return CHARSET_ERROR_DECODE;
		}

		int out_needed = conv_from_ucs4_func(decoder.codepoint, &ds);
		if (out_needed < 0) {
			disko_memclose(&ds, 0);
			return CHARSET_ERROR_ENCODE;
		}
	} while (decoder.state == DECODER_STATE_NEED_MORE);

	// write a NUL terminator always
	uint32_t x = 0;
	disko_write(&ds, &x, sizeof(x));

	disko_memclose(&ds, 1);

	memcpy(out, &ds.data, sizeof(ds.data));

	return CHARSET_ERROR_SUCCESS;
}

/* ----------------------------------------------------------------------- */

#ifdef SCHISM_MACOS
static size_t charset_nulterm_string_size(const void *in, charset_t inset)
{
	/* ugly */
	charset_decode_t decoder = {0};
	charset_conv_to_ucs4_func   conv_to_ucs4_func;

	decoder.in = in;
	decoder.size = SIZE_MAX;

	if (inset >= ARRAY_SIZE(conv_to_ucs4_funcs))
		return CHARSET_ERROR_UNIMPLEMENTED;

	conv_to_ucs4_func   = conv_to_ucs4_funcs[inset];

	if (!conv_to_ucs4_func)
		return CHARSET_ERROR_UNIMPLEMENTED;

	do {
		conv_to_ucs4_func(&decoder);
		if (decoder.state < 0)
			return 0;
	} while (decoder.state == DECODER_STATE_NEED_MORE);

	return decoder.offset;
}
#endif

/* ----------------------------------------------------------------------- */

#if defined(SCHISM_XBOX)
# include <xboxkrnl/xboxkrnl.h>
#elif defined(SCHISM_MACOS)
# include <TextEncodingConverter.h>
# include <Script.h>

# ifndef kTECOutputBufferFullStatus
#  define kTECOutputBufferFullStatus (-8785)
# endif
#elif defined(SCHISM_OS2)
# include <uconv.h>
# define INCL_DOS
# include <os2.h>
#endif

typedef charset_error_t (*charset_impl)(const void* in, void* out, charset_t inset, charset_t outset, size_t insize);

static const charset_impl charset_impls[] = {
	charset_iconv_internal_,
};

static charset_error_t charset_iconv_impl_(const void *in, void *out, charset_t inset, charset_t outset, size_t insize)
{
	static void *const null = NULL;
	charset_error_t state = CHARSET_ERROR_UNIMPLEMENTED;

	for (size_t i = 0; i < ARRAY_SIZE(charset_impls); i++) {
		state = charset_impls[i](in, out, inset, outset, insize);
		if (state == CHARSET_ERROR_SUCCESS)
			return state;

		memcpy(out, &null, sizeof(void *));

		// give up if no memory left
		if (state == CHARSET_ERROR_NOMEM)
			return state;
	}

	return state;
}

#ifdef SCHISM_MACOS
static inline SCHISM_ALWAYS_INLINE int charset_iconv_get_system_encoding_(TextEncoding *penc)
{
	if (UpgradeScriptInfoToTextEncoding(smSystemScript, kTextLanguageDontCare, kTextRegionDontCare, NULL, penc) == noErr)
		return 1;

	/* Assume Mac OS Roman. */
	*penc = CreateTextEncoding(kTextEncodingMacRoman, kTextEncodingDefaultVariant, kTextEncodingDefaultFormat);

	return 1;
}

static charset_error_t charset_iconv_macos_preprocess_(const void *in, size_t insize, TextEncoding inenc, TextEncoding outenc, void *out, size_t *outsize)
{
	OSStatus err = noErr;

	TECObjectRef tec = NULL;

	err = TECCreateConverter(&tec, inenc, outenc);
	if (err != noErr)
		return CHARSET_ERROR_UNIMPLEMENTED;

	const unsigned char *srcptr = in;

	disko_t ds = {0};
	if (disko_memopen_estimate(&ds, insize * 2) < 0) {
		TECDisposeConverter(tec);
		return CHARSET_ERROR_NOMEM;
	}

	do {
		unsigned char buf[512];
		ByteCount bytes_consumed; // I love consuming media!
		ByteCount bytes_produced;

		err = TECConvertText(tec, (ConstTextPtr)srcptr, insize, &bytes_consumed, (TextPtr)buf, sizeof(buf), &bytes_produced);
		if (err != noErr && err != kTECOutputBufferFullStatus) {
#ifdef SCHISM_CHARSET_DEBUG
			log_appendf(4, "[MacOS] TECConvertText returned %d; giving up", (int)err);
#endif
			TECDisposeConverter(tec);
			disko_memclose(&ds, 0);
			return CHARSET_ERROR_DECODE;
		}

		// append to our heap buffer
		disko_write(&ds, buf, bytes_produced);

		srcptr += bytes_consumed;
		insize -= bytes_consumed;
	} while (err == kTECOutputBufferFullStatus);

	// force write a NUL terminator
	uint16_t x = 0;
	disko_write(&ds, &x, sizeof(x));

	disko_memclose(&ds, 1);

	// put the fake stuff in
	memcpy(out, &ds.data, sizeof(ds.data));
	if (outsize)
		*outsize = ds.length;

	return CHARSET_ERROR_SUCCESS;
}
#endif

#ifdef SCHISM_OS2
static inline int charset_os2_get_sys_cp(ULONG *pcp)
{
	ULONG aulCP[3];
	ULONG cCP;

	if (DosQueryCp(sizeof(aulCP), aulCP, &cCP))
		return 0;

	*pcp = aulCP[0];

	return 1;
}

static inline int charset_os2_uconv_build(ULONG cp, UconvObject *puconv)
{
	UniChar cpname[16];
	{
		char buf[16];

		snprintf(buf, 16, "IBM-%lu", cp);

		for (size_t i = 0; i < 16; i++) cpname[i] = buf[i];
	}

	return !UniCreateUconvObject(cpname, puconv);
}
#endif

charset_error_t charset_iconv(const void* in, void* out, charset_t inset, charset_t outset, size_t insize) {
	// This hacky mess is here, specifically for being able to handle system-specific volatile charsets like:
	//  Win32 - CHARSET_ANSI
	//  OS/2  - CHARSET_DOSCP
	//  MacOS - CHARSET_SYSTEMSCRIPT
	// TODO: We also have to be able to handle this in charset_decode_next, since charset_stdlib uses it.
	// Oh well.
	charset_t insetfake = inset, outsetfake = outset;
	const void *infake = in;
	void *outfake;
	size_t insizefake = insize;

	charset_error_t state;
	if (!in)
		return CHARSET_ERROR_NULLINPUT;

	if (!out)
		return CHARSET_ERROR_NULLOUTPUT;

	if (inset == outset)
		return CHARSET_ERROR_INPUTISOUTPUT;

	switch (inset) {
#ifdef SCHISM_XBOX
	case CHARSET_ANSI: {
		size_t source_size = (insize != SIZE_MAX) ? insize : strlen(in);
		ULONG size; /* in bytes */
		WCHAR *wstr;

		if (!NT_SUCCESS(RtlMultiByteToUnicodeSize(&size, (CHAR *)in, source_size)))
			return CHARSET_ERROR_DECODE;

		/* ReactOS source code doesn't append a NUL terminator.
		 * I assume this is the Win32 behavior as well. */
		wstr = mem_alloc(size + sizeof(WCHAR));

		if (!NT_SUCCESS(RtlMultiByteToUnicodeN(wstr, size, NULL, (CHAR *)in, source_size))) {
			free(wstr);
			return CHARSET_ERROR_DECODE;
		}

		wstr[size >> 1] = '\0';

		infake = wstr;
		insetfake = CHARSET_WCHAR_T;
		insizefake = (size + sizeof(WCHAR));
		break;
	}
#endif
#ifdef SCHISM_OS2
	case CHARSET_DOSCP: {
		ULONG cp;
		if (!charset_os2_get_sys_cp(&cp))
			return CHARSET_ERROR_UNIMPLEMENTED;

		if (cp == 437) {
			insetfake = inset = CHARSET_CP437;
		} else if (cp == 1252) {
			insetfake = inset = CHARSET_WINDOWS1252;
		} else {
			UconvObject uc;
			if (!charset_os2_uconv_build(cp, &uc))
				return CHARSET_ERROR_UNIMPLEMENTED; // probably

			size_t alloc = 0;
			uint16_t *ucs2 = NULL;
			for (;;) {
				free(ucs2);

				alloc = (alloc) ? (alloc * 2) : 128;
				ucs2 = mem_alloc(alloc * sizeof(*ucs2));

				int rc = UniStrToUcs(uc, ucs2, (CHAR *)in, alloc);

				if (rc == ULS_SUCCESS) {
					break;
				} else if (rc == ULS_BUFFERFULL) {
					continue;
				} else {
					// some error decoding
					free(ucs2);
					return CHARSET_ERROR_DECODE;
				}
			}

			infake = ucs2;
			insetfake = CHARSET_UCS2;
			insizefake = alloc * sizeof(*ucs2);
		}
		break;
	}
#endif
#ifdef SCHISM_MACOS
	case CHARSET_SYSTEMSCRIPT: {
		TextEncoding hfsenc;
		if (!charset_iconv_get_system_encoding_(&hfsenc))
			return CHARSET_ERROR_UNIMPLEMENTED;

		TextEncoding utf16enc = CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat);

		if (insize == SIZE_MAX)
			insize = strlen(in) + 1;

		state = charset_iconv_macos_preprocess_(in, insize, hfsenc, utf16enc, &infake, &insizefake);
		if (state != CHARSET_ERROR_SUCCESS)
			return state;

		insetfake = CHARSET_UTF16;
		break;
	}
#endif
	default: break;
	}

	switch (outset) {
#ifdef SCHISM_XBOX
	case CHARSET_ANSI:
		outsetfake = CHARSET_WCHAR_T;
		break;
#endif
#ifdef SCHISM_MACOS
	case CHARSET_SYSTEMSCRIPT:
		outsetfake = CHARSET_UTF16;
		break;
#endif
#ifdef SCHISM_OS2
	case CHARSET_DOSCP: {
		ULONG cp;
		if (!charset_os2_get_sys_cp(&cp))
			return CHARSET_ERROR_UNIMPLEMENTED;

		if (cp == 437) {
			outsetfake = outset = CHARSET_CP437;
		} else {
			outsetfake = CHARSET_UCS2;
		}
		break;
	}
#endif
	default: break;
	}

	state = charset_iconv_impl_(infake, &outfake, insetfake, outsetfake, insizefake);

	switch (inset) {
#ifdef SCHISM_XBOX
	case CHARSET_ANSI: {
		free((void *)infake);
		break;
	}
#endif
#ifdef SCHISM_MACOS
	case CHARSET_SYSTEMSCRIPT:
		free((void *)infake);
		break;
#endif
#ifdef SCHISM_OS2
	case CHARSET_DOSCP:
		free((void *)infake);
		break;
#endif
	default: break;
	}

	switch (outset) {
#ifdef SCHISM_XBOX
	case CHARSET_ANSI: {
		size_t source_size = wcslen(outfake) * sizeof(WCHAR);
		ULONG size; /* in bytes */
		CHAR *astr;

		if (!NT_SUCCESS(RtlUnicodeToMultiByteSize(&size, outfake, source_size))) {
			free(outfake);
			return CHARSET_ERROR_ENCODE;
		}

		/* ReactOS source code doesn't append a NUL terminator.
		 * I assume this is the Win32 behavior as well. */
		astr = mem_alloc(size + sizeof(CHAR));

		if (!NT_SUCCESS(RtlUnicodeToMultiByteN(astr, size, NULL, outfake, source_size))) {
			free(outfake);
			free(astr);
			return CHARSET_ERROR_ENCODE;
		}

		astr[size] = '\0';

		free(outfake);

		memcpy(out, &astr, sizeof(void *));

		break;
	}
#endif
#ifdef SCHISM_OS2
	case CHARSET_DOSCP: {
		ULONG cp;
		if (!charset_os2_get_sys_cp(&cp))
			return CHARSET_ERROR_UNIMPLEMENTED;

		UconvObject uc;
		if (!charset_os2_uconv_build(cp, &uc))
			return CHARSET_ERROR_UNIMPLEMENTED; // probably

		size_t alloc = 256;
		CHAR *sys = NULL;
		for (;;) {
			free(sys);

			sys = mem_alloc(alloc * sizeof(*sys));

			int rc = UniStrFromUcs(uc, sys, (UniChar *)outfake, alloc);

			if (rc == ULS_SUCCESS) {
				break;
			} else if (rc == ULS_BUFFERFULL) {
				alloc *= 2;
				continue;
			} else {
				return CHARSET_ERROR_DECODE;
			}
		}

		free(outfake);

		memcpy(out, &sys, sizeof(void *));

		break;
	}
#endif
#ifdef SCHISM_MACOS
	case CHARSET_SYSTEMSCRIPT: {
		/* can we return size so we don't have to do this? */
		size_t len = charset_nulterm_string_size(outfake, CHARSET_UTF16) + 2;

		TextEncoding hfsenc;
		if (!charset_iconv_get_system_encoding_(&hfsenc))
			return CHARSET_ERROR_UNIMPLEMENTED;

		TextEncoding utf16enc = CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat);

		state = charset_iconv_macos_preprocess_(outfake, len, utf16enc, hfsenc, out, NULL);

		free(outfake);

		break;
	}
#endif
	default:
		memcpy(out, &outfake, sizeof(void *));
		break;
	}

	return state;
}

charset_error_t charset_decode_next(charset_decode_t *decoder, charset_t inset)
{
	if (inset >= ARRAY_SIZE(conv_to_ucs4_funcs))
		return CHARSET_ERROR_UNIMPLEMENTED;

	charset_conv_to_ucs4_func   conv_to_ucs4_func   = conv_to_ucs4_funcs[inset];

	if (!conv_to_ucs4_func)
		return CHARSET_ERROR_UNIMPLEMENTED;

	conv_to_ucs4_func(decoder);

	if (decoder->state < 0)
		return CHARSET_ERROR_DECODE;

	return CHARSET_ERROR_SUCCESS;
}
