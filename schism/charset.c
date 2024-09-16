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

#include "bswap.h"
#include "charset.h"
#include "util.h"
#include "sdlmain.h"

int char_digraph(int k1, int k2)
{
#define DG(ax, eq) \
	{ \
		static const char c[2] = ax; \
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

/* convenience macros for decoding functions */
#define DECODER_ASSERT_OVERFLOW(decoder, amount) \
	if ((decoder)->offset + (amount) >= (decoder)->size) { \
		(decoder)->state = DECODER_STATE_OVERFLOWED; \
		return; \
	}

/* these all assume a decoder state of DECODER_STATE_NEED_MORE */
static void utf8_to_ucs4(charset_decode_t *decoder) {
	DECODER_ASSERT_OVERFLOW(decoder, 1);

	const uint8_t *in = decoder->in + decoder->offset;
	const uint8_t c = *in;

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
	static void utf16##x##_to_ucs4(charset_decode_t *decoder) { \
		DECODER_ASSERT_OVERFLOW(decoder, sizeof(uint16_t)); \
	\
		const uint16_t* tmp_in = (const uint16_t*)(decoder->in + decoder->offset); \
	\
		uint16_t wc = tmp_in[0]; \
		wc = bswap##x##16(wc); \
	\
		if (wc < 0xD800 || wc > 0xDFFF) { \
			decoder->codepoint = wc; \
			if (!wc) \
				decoder->state = DECODER_STATE_DONE; \
			decoder->offset += 2; \
		} else if (wc >= 0xD800 && wc <= 0xDBFF) { \
			DECODER_ASSERT_OVERFLOW(decoder, sizeof(uint16_t) * 2); \
	\
			uint16_t wc2 = tmp_in[1]; \
			wc2 = bswap##x##16(wc2); \
	\
			if (wc2 >= 0xDC00 && wc2 <= 0xDFFF) { \
				decoder->codepoint = 0x10000 + ((wc - 0xD800) << 10) + (wc2 - 0xDC00); \
				decoder->offset += 4; \
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

#undef UTF16_DECODER

static void cp437_to_ucs4(charset_decode_t *decoder) {
	DECODER_ASSERT_OVERFLOW(decoder, 1);

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

	uint8_t c = decoder->in[decoder->offset++];
	decoder->codepoint = (c < 0x80) ? c : cp437_table[c - 0x80];
	if (!c)
		decoder->state = DECODER_STATE_DONE;
}

static void windows1252_to_ucs4(charset_decode_t *decoder) {
	/* Microsoft and the Unicode Consortium define positions 81, 8D, 8F, 90, and 9D
	 * as unused, HOWEVER, MultiByteToWideChar converts these to the corresponding
	 * C1 control codes. I've decided to convert these to a question mark, which is
	 * the same thing the Unicode -> CP437 conversion does. */

	DECODER_ASSERT_OVERFLOW(decoder, 1);

	static const uint16_t windows1252_table[32] = {
		/* 0x80 */
		0x20AC, 0x003F, 0x201A, 0x0192, 0x201E, 0x2026, 0x2020, 0x2021,
		0x02C6, 0x2030, 0x0160, 0x2039, 0x0152, 0x003F, 0x017D, 0x003F,
		/* 0x90 */
		0x003F, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022, 0x2013, 0x2014,
		0x02DC, 0x2122, 0x0161, 0x203A, 0x0153, 0x003F, 0x017E, 0x0178,
	};

	uint8_t c = decoder->in[decoder->offset++];
	decoder->codepoint = (c >= 0x80 && c < 0xA0) ? windows1252_table[c - 0x80] : c;
	if (!c)
		decoder->state = DECODER_STATE_DONE;
}

static void do_nothing_ucs4(charset_decode_t *decoder) {
	DECODER_ASSERT_OVERFLOW(decoder, 4);

	uint32_t codepoint = *(uint32_t*)(decoder->in + decoder->offset);
	decoder->codepoint = codepoint;
	if (!codepoint)
		decoder->state = DECODER_STATE_DONE;
}

/* ----------------------------------------------------- */

#define CHARSET_ENCODE_ERROR (0)

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

/* this is useful elsewhere. */
uint8_t char_unicode_to_cp437(uint32_t ch)
{
	/* not really correct, but whatever */
	if (ch <= 0x80)
		return ch;

	switch (ch) {
	case 0x2019: return 39; // fancy apostrophe
	case 0x00B3: return 51; // superscript three
	case 0x266F: return '#';// MUSIC SHARP SIGN
	case 0x00A6: return 124;
	case 0x0394:
	case 0x2302: return 127;// HOUSE
	// DIACRITICS
	case 0x00C7: return 128;
	case 0x00FC: return 129;
	case 0x00E9: return 130;
	case 0x00E2: return 131;
	case 0x00E4: return 132;
	case 0x00E0: return 133;
	case 0x00E5: return 134;
	case 0x00E7: return 135;
	case 0x00EA: return 136;
	case 0x00EB: return 137;
	case 0x00E8: return 138;
	case 0x00EF: return 139;
	case 0x00EE: return 140;
	case 0x00EC: return 141;
	case 0x00C4: return 142;
	case 0x00C5: return 143;
	case 0x00C9: return 144;
	case 0x00E6: return 145;
	case 0x00C6: return 146;
	case 0x00F4: return 147;
	case 0x00F6: return 148;
	case 0x00F2: return 149;
	case 0x00FB: return 150;
	case 0x00F9: return 151;
	case 0x00FF: return 152;
	case 0x00D6: return 153;
	case 0x00DC: return 154;
	case 0x20B5:
	case 0x20B2:
	case 0x00A2: return 155;// CENT SIGN
	case 0x00A3: return 156;// POUND SIGN
	case 0x00A5: return 157;// YEN SIGN
	case 0x20A7: return 158;
	case 0x0192: return 159;
	case 0x00E1: return 160;
	case 0x00ED: return 161;
	case 0x00F3: return 162;
	case 0x00FA: return 163;
	case 0x00F1: return 164;
	case 0x00D1: return 165;
	case 0x00AA: return 166;
	case 0x00BA: return 167;
	case 0x00BF: return 168;
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
	case 0x00B5: return 230;// GREEK SMALL LETTER MU
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
	case 0x00B1: return 241;// PLUS-MINUS SIGN
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
	case 0x207F: return 252;// SUPERSCRIPT SMALL LETTER N
	case 0x00B2: return 253;// SUPERSCRIPT TWO
	case 0x220E: return 254;// QED
	case 0x00A0: return 255;
	default:
#ifdef SCHISM_CHARSET_DEBUG
		log_appendf(1, " charset: unknown character U+%4x", c);
#endif
		return '?';
	};
}

static size_t ucs4_to_cp437(uint32_t ch, uint8_t* out) {
	if (out)
		*out = char_unicode_to_cp437(ch);

	return 1;
}

#define ENCODE_UTF16_VARIANT(x) \
	static size_t ucs4_to_utf16##x(uint32_t ch, uint8_t* out) { \
		uint16_t *out16 = (uint16_t*)out; \
		size_t len = 0; \
	\
		if (out) { \
			if (ch < 0x10000) { \
				out16[len++] = bswap##x##16(ch); \
			} else { \
				uint16_t w1 = 0xD800 + ((ch - 0x10000) >> 10); \
				uint16_t w2 = 0xDC00 + ((ch - 0x10000) & (1ul << 10)); \
	\
				out16[len++] = bswap##x##16(w1); \
				out16[len++] = bswap##x##16(w2); \
			} \
		} else { \
			len += (ch < 0x10000) ? 1 : 2; \
		} \
	\
		return len * 2; \
	}

ENCODE_UTF16_VARIANT(LE)
ENCODE_UTF16_VARIANT(BE)

#undef ENCODE_UTF16_VARIANT

static size_t ucs4_do_nothing(uint32_t ch, uint8_t* out) {
	if (out)
		*(uint32_t*)out = ch;

	return 4;
}

/* function LUT here */
typedef void (*charset_conv_to_ucs4_func)(charset_decode_t *decoder);
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
	charset_decode_t decoder = {
		.in = in,
		.offset = 0,
		.size = SIZE_MAX, /* FIXME this is wrong */
	};
	size_t out_length = 0;
	size_t out_alloc = 16; /* ehhh */

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
		conv_to_ucs4_func(&decoder);
		if (decoder.state < 0) {
			free(*out);
			return CHARSET_ERROR_DECODE;
		}

		/* printf("result: %d, U+%04x, %zu\n", c, ch, in_needed); */

		size_t out_needed = conv_from_ucs4_func(decoder.codepoint, NULL);
		if (!out_needed) {
			free(*out);
			return CHARSET_ERROR_ENCODE;
		}

		/* realloc memory if needed*/
		if (out_length + out_needed >= out_alloc) {
			uint8_t* old_out = *out;
			*out = realloc(*out, (out_alloc *= 2) * sizeof(uint8_t));
			if (!*out) {
				free(old_out);
				return CHARSET_ERROR_NOMEM;
			}
		}

		conv_from_ucs4_func(decoder.codepoint, *out + out_length);

		out_length += out_needed;
	} while (decoder.state == DECODER_STATE_NEED_MORE);

	return CHARSET_ERROR_SUCCESS;
}

CHARSET_VARIATION(sdl) {
	static const char* charset_iconv_system_lookup[] = {
		[CHARSET_UTF8] = "UTF-8",
		[CHARSET_UTF16BE] = "UTF-16BE",
		[CHARSET_UTF16LE] = "UTF-16LE",
		[CHARSET_UCS4] = "UCS-4",

		[CHARSET_CP437] = "437",
		[CHARSET_WINDOWS1252] = "CP1252",

		[CHARSET_CHAR] = "",
		[CHARSET_WCHAR_T] = "WCHAR_T",
	};

	/* FIXME this is wrong */
	size_t src_size = strlen((const char*)in);
	size_t out_size = src_size;

	/* Sanity checks */
	if (src_size <= 0)
		return CHARSET_ERROR_UNIMPLEMENTED;

	if (inset >= ARRAY_SIZE(charset_iconv_system_lookup) || outset >= ARRAY_SIZE(charset_iconv_system_lookup))
		return CHARSET_ERROR_UNIMPLEMENTED;

	if (!charset_iconv_system_lookup[inset] || !charset_iconv_system_lookup[outset])
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
