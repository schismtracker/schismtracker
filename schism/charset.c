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

int char_digraph(int k1, int k2)
{
	//int c;
#define DG(ax) ((k1==ax[0] && k2 == ax[1])||(k2==ax[0] && k1==ax[1]))
	if (DG("NB")) return '#';
	if (DG("DO")) return '$';
	if (DG("At")) return '@';
	if (DG("<(")) return '[';
	if (DG("//")) return '\\';
	if (DG(")>")) return ']';
	if (DG("'>")) return '^';
	if (DG("'!")) return '`';
	if (DG("(!")) return '{';
	if (DG("!!")) return '|';
	if (DG("!)")) return '{';
	if (DG("'?")) return '~';
	if (DG("C,")) return 128; // LATIN CAPITAL LETTER C WITH CEDILLA
	if (DG("u:")) return 129; // LATIN SMALL LETTER U WITH DIAERESIS
	if (DG("e'")) return 130; // LATIN SMALL LETTER E WITH ACUTE
	if (DG("a>")) return 131; // LATIN SMALL LETTER A WITH CIRCUMFLEX
	if (DG("a:")) return 132; // LATIN SMALL LETTER A WITH DIAERESIS
	if (DG("a!")) return 133; // LATIN SMALL LETTER A WITH GRAVE
	if (DG("aa")) return 134; // LATIN SMALL LETTER A WITH RING ABOVE
	if (DG("c,")) return 135; // LATIN SMALL LETTER C WITH CEDILLA
	if (DG("e>")) return 136; // LATIN SMALL LETTER E WITH CIRCUMFLEX
	if (DG("e:")) return 137; // LATIN SMALL LETTER E WITH DIAERESIS
	if (DG("e!")) return 138; // LATIN SMALL LETTER E WITH GRAVE
	if (DG("i:")) return 139; // LATIN SMALL LETTER I WITH DIAERESIS
	if (DG("i>")) return 140; // LATIN SMALL LETTER I WITH CIRCUMFLEX
	if (DG("i!")) return 141; // LATIN SMALL LETTER I WITH GRAVE
	if (DG("A:")) return 142; // LATIN CAPITAL LETTER A WITH DIAERESIS
	if (DG("AA")) return 143; // LATIN CAPITAL LETTER A WITH RING ABOVE
	if (DG("E'")) return 144; // LATIN CAPITAL LETTER E WITH ACUTE
	if (DG("ae")) return 145; // LATIN SMALL LETTER AE
	if (DG("AE")) return 146; // LATIN CAPITAL LETTER AE
	if (DG("o>")) return 147; // LATIN SMALL LETTER O WITH CIRCUMFLEX
	if (DG("o:")) return 148; // LATIN SMALL LETTER O WITH DIAERESIS
	if (DG("o!")) return 149; // LATIN SMALL LETTER O WITH GRAVE
	if (DG("u>")) return 150; // LATIN SMALL LETTER U WITH CIRCUMFLEX
	if (DG("u!")) return 151; // LATIN SMALL LETTER U WITH GRAVE
	if (DG("y:")) return 152; // LATIN SMALL LETTER Y WITH DIAERESIS
	if (DG("O:")) return 153; // LATIN CAPITAL LETTER O WITH DIAERESIS
	if (DG("U:")) return 154; // LATIN CAPITAL LETTER U WITH DIAERESIS
	if (DG("Ct")) return 155; // CENT SIGN
	if (DG("Pd")) return 156; // POUND SIGN
	if (DG("Ye")) return 157; // YEN SIGN
	if (DG("Pt")) return 158;
	if (DG("ff")) return 159;
	if (DG("a'")) return 160; // LATIN SMALL LETTER A WITH ACUTE
	if (DG("i'")) return 161; // LATIN SMALL LETTER I WITH ACUTE
	if (DG("o'")) return 162; // LATIN SMALL LETTER O WITH ACUTE
	if (DG("u'")) return 163; // LATIN SMALL LETTER U WITH ACUTE
	if (DG("n?")) return 164; // LATIN SMALL LETTER N WITH TILDE
	if (DG("N?")) return 165; // LATIN CAPITAL LETTER N WITH TILDE
	if (DG("-a")) return 166; // FEMININE ORDINAL INDICATOR
	if (DG("-o")) return 167; // MASCULINE ORDINAL INDICATOR
	if (DG("?I")) return 168; // INVERTED QUESTION MARK

	if (DG("NO")) return 170; // NOT SIGN
	if (DG("12")) return 171; // VULGAR FRACTION ONE HALF
	if (DG("14")) return 174; // VULGAR FRACTION ONE QUARTER
	if (DG("!I")) return 175; // INVERTED EXCLAMATION MARK
	if (DG("<<")) return 176; // LEFT-POINTING DOUBLE ANGLE QUOTATION MARK
	if (DG(">>")) return 177; // RIGHT-POINTING DOUBLE ANGLE QUOTATION MARK

	if (DG("ss")) return 225; // LATIN SMALL LETTER SHARP S
	if (DG("pi")) return 227; // PI... mmm... pie...
	if (DG("My")) return 230; // MICRO SIGN
	if (DG("o/")) return 237; // LATIN SMALL LETTER O WITH STROKE
	if (DG("O/")) return 237; // LATIN SMALL LETTER O WITH STROKE
	if (DG("+-")) return 241; // PLUS-MINUS SIGN
	if (DG("-:")) return 246; // DIVISION SIGN
	if (DG("DG")) return 248; // DEGREE SIGN
	if (DG(".M")) return 249; // MIDDLE DOT
	if (DG("2S")) return 253; // SUPERSCRIPT TWO
	if (DG("nS")) return 252;

	if (DG("PI")) return 20;  // PILCROW SIGN
	if (DG("SE")) return 21;  // SECTION SIGN
#undef DG
	return 0;
}

uint8_t char_unicode_to_cp437(unsigned int c)
{
	if (c >= 32 && c <= 127) return c;
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
	case 0x263C: return 15;
	case 0x25BA: return 16;
	case 0x25C4: return 17;
	case 0x2195: return 18; // UP DOWN ARROW
	case 0x203C: return 19; // DOUBLE EXCLAMATION MARK
	case 0x00B6: return 20; // PILCROW SIGN
	case 0x00A7: return 21; // SECTION SIGN
	case 0x25AC: return 22;
	case 0x21A8: return 23; // UP DOWN ARROW WITH BASE
	case 0x2191: return 24; // UPWARD ARROW
	case 0x2193: return 25; // DOWNWARD ARROW
	case 0x2192: return 26; // RIGHTWARD ARROW
	case 0x2190: return 27; // LEFTWARD ARROW
	case 0x221F: return 28;
	case 0x2194: return 29; // LEFT RIGHT ARROW
	case 0x25B2: return 30;
	case 0x25BC: return 31;
	/* 32-126 is ASCII */
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
	case 0x207F: return 252;// SUPERSCRIPT SMALL LETTER N
	case 0x00B2: return 253;// SUPERSCRIPT TWO
	case 0x220E: return 254;// QED
	case 0x00A0: return 255;
	default: return '?';
	};
}

/* From GNU libiconv */
static const unsigned short cp437_2uni[128] = {
	/* 0x80 */
	0x00c7, 0x00fc, 0x00e9, 0x00e2, 0x00e4, 0x00e0, 0x00e5, 0x00e7,
	0x00ea, 0x00eb, 0x00e8, 0x00ef, 0x00ee, 0x00ec, 0x00c4, 0x00c5,
	/* 0x90 */
	0x00c9, 0x00e6, 0x00c6, 0x00f4, 0x00f6, 0x00f2, 0x00fb, 0x00f9,
	0x00ff, 0x00d6, 0x00dc, 0x00a2, 0x00a3, 0x00a5, 0x20a7, 0x0192,
	/* 0xa0 */
	0x00e1, 0x00ed, 0x00f3, 0x00fa, 0x00f1, 0x00d1, 0x00aa, 0x00ba,
	0x00bf, 0x2310, 0x00ac, 0x00bd, 0x00bc, 0x00a1, 0x00ab, 0x00bb,
	/* 0xb0 */
	0x2591, 0x2592, 0x2593, 0x2502, 0x2524, 0x2561, 0x2562, 0x2556,
	0x2555, 0x2563, 0x2551, 0x2557, 0x255d, 0x255c, 0x255b, 0x2510,
	/* 0xc0 */
	0x2514, 0x2534, 0x252c, 0x251c, 0x2500, 0x253c, 0x255e, 0x255f,
	0x255a, 0x2554, 0x2569, 0x2566, 0x2560, 0x2550, 0x256c, 0x2567,
	/* 0xd0 */
	0x2568, 0x2564, 0x2565, 0x2559, 0x2558, 0x2552, 0x2553, 0x256b,
	0x256a, 0x2518, 0x250c, 0x2588, 0x2584, 0x258c, 0x2590, 0x2580,
	/* 0xe0 */
	0x03b1, 0x00df, 0x0393, 0x03c0, 0x03a3, 0x03c3, 0x00b5, 0x03c4,
	0x03a6, 0x0398, 0x03a9, 0x03b4, 0x221e, 0x03c6, 0x03b5, 0x2229,
	/* 0xf0 */
	0x2261, 0x00b1, 0x2265, 0x2264, 0x2320, 0x2321, 0x00f7, 0x2248,
	0x00b0, 0x2219, 0x00b7, 0x221a, 0x207f, 0x00b2, 0x25a0, 0x00a0
};

// Copyright (c) 2008-2009 Bjoern Hoehrmann <bjoern@hoehrmann.de>
// See http://bjoern.hoehrmann.de/utf-8/decoder/dfa/ for details.

/* Unicode stuff begins here */

#define UTF8_ACCEPT 0
#define UTF8_REJECT 1

static const uint8_t utf8d[] = {
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 00..1f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 20..3f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 40..5f
	0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0, // 60..7f
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9,9, // 80..9f
	7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7,7, // a0..bf
	8,8,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2,2, // c0..df
	0xa,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x3,0x4,0x3,0x3, // e0..ef
	0xb,0x6,0x6,0x6,0x5,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8,0x8, // f0..ff
	0x0,0x1,0x2,0x3,0x5,0x8,0x7,0x1,0x1,0x1,0x4,0x6,0x1,0x1,0x1,0x1, // s0..s0
	1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0,1,1,1,1,1,0,1,0,1,1,1,1,1,1, // s1..s2
	1,2,1,1,1,1,1,2,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1, // s3..s4
	1,2,1,1,1,1,1,1,1,2,1,1,1,1,1,1,1,1,1,1,1,1,1,3,1,3,1,1,1,1,1,1, // s5..s6
	1,3,1,1,1,1,1,3,1,3,1,1,1,1,1,1,1,3,1,1,1,1,1,1,1,1,1,1,1,1,1,1, // s7..s8
};

static uint32_t utf8_decode(uint32_t* state, uint32_t* codep, uint32_t byte) {
	uint32_t type = utf8d[byte];

	*codep = (*state != UTF8_ACCEPT) ?
		(byte & 0x3fu) | (*codep << 6) :
		(0xff >> type) & (byte);

	*state = utf8d[256 + *state * 16 + type];
	return *state;
}

/* for these functions:
 * out can be NULL, and this means to just return the needed length
 * of the output string WITHOUT the null terminating character.
 *
 * the NUL char is added on in charset_iconv() */
static size_t utf8_to_ucs4(const uint8_t* in, uint32_t* out) {
	uint32_t state = 0, codepoint = 0;
	size_t len = 0;

	for (; *in; in++) {
		if (utf8_decode(&state, &codepoint, *in))
			continue;

		if (out)
			out[len] = codepoint;
		len++;
	}

	return len;
}

static size_t utf16le_to_ucs4(const uint8_t* in, uint32_t* out) {
	const uint16_t* tmp_in = (const uint16_t*)in;
	size_t len = 0;

	/* don't translate a BOM */
	if (in[0] == 0xFF && in[1] == 0xFE)
		len++;

	for (; *tmp_in; tmp_in) {
		uint16_t wc = *(tmp_in++);
		wc = bswapLE16(wc);

		if (wc < 0xD800 || wc > 0xDFFF) {
			if (out)
				out[len] = wc;
			len++;
		} else if (wc >= 0xD800 && wc <= 0xDBFF) {
			uint16_t wc2 = *(tmp_in++);
			wc2 = bswapLE16(wc2);

			if (!(wc2 >= 0xDC00 && wc2 <= 0xDFFF))
				return 0;

			if (out)
				out[len] = 0x10000 + ((wc - 0xD800) << 10) + (wc2 - 0xDC00);
			len++;
		} else {
			/* this should never happen, and things
			 * have gone very wrong if it does */
			return 0;
		}
	}

	return len;
}

static size_t utf16be_to_ucs4(const uint8_t* in, uint32_t* out) {
	const uint16_t* tmp_in = (const uint16_t*)in;
	size_t len = 0;

	/* don't translate a BOM */
	if (in[0] == 0xFE && in[1] == 0xFF)
		tmp_in++;

	for (; *tmp_in; tmp_in) {
		uint16_t wc = *(tmp_in++);
		wc = bswapBE16(wc);

		if (wc < 0xD800 || wc > 0xDFFF) {
			if (out)
				out[len] = wc;
			len++;
		} else if (wc >= 0xD800 && wc <= 0xDBFF) {
			uint16_t wc2 = *(tmp_in++);
			wc2 = bswapBE16(wc2);

			if (!(wc2 >= 0xDC00 && wc2 <= 0xDFFF))
				return 0;

			if (out)
				out[len] = 0x10000 + ((wc - 0xD800) << 10) + (wc2 - 0xDC00);
			len++;
		} else {
			return 0;
		}
	}

	return len;
}

static size_t cp437_to_ucs4(const uint8_t* in, uint32_t* out) {
	size_t len = 0;

	for (; *in; in++, len++) {
		uint8_t c = *in;

		out[len] = (c < 0x80) ? c : cp437_2uni[c - 0x80];
	}

	return len;
}

/* These conversion functions DO include the NUL byte because
 * we're dealing with essentially raw output data here: we don't
 * know how big each character is... */
static size_t ucs4_to_utf8(const uint32_t* in, uint8_t* out) {
	size_t len = 0;

	for (; *in; in++) {
		const uint32_t ch = *in;

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
			if (ch < 0x80)
				len += 1;
			else if (ch < 0x800)
				len += 2;
			else if (ch < 0x10000)
				len += 3;
			else if (ch < 0x110000)
				len += 4;
			else
				return 0;
		}
	}

	if (out)
		out[len] = '\0';
	len++;

	return len;
}

static size_t ucs4_to_cp437(const uint32_t* in, uint8_t* out) {
	size_t len = 0;

	for (; *in; in++, len++)
		if (out)
			out[len] = char_unicode_to_cp437(*in);

	if (out)
		out[len] = '\0';
	len++;

	return len;
}

static size_t ucs4_to_utf16le(const uint32_t* in, uint8_t* out) {
	size_t len = 0;

	for (; *in; in++) {
		const uint32_t ch = *in;

		if (out) {
			if (ch < 0x10000) {
				out[len++] = (uint8_t)(ch);
				out[len++] = (uint8_t)(ch >> 8);
			} else {
				uint16_t w1 = 0xD800 + ((ch - 0x10000) >> 10);
				uint16_t w2 = 0xDC00 + ((ch - 0x10000) & 0x3FF);

				out[len++] = (uint8_t)(w1);
				out[len++] = (uint8_t)(w1 >> 8);
				out[len++] = (uint8_t)(w2);
				out[len++] = (uint8_t)(w2 >> 8);
			}
		} else {
			len += (ch < 0x10000) ? 2 : 4;
		}
	}

	if (out) {
		out[len++] = '\0';
		out[len++] = '\0';
	} else {
		len += 2;
	}

	return len;
}

static size_t ucs4_to_utf16be(const uint32_t* in, uint8_t* out) {
	size_t len = 0;

	for (; *in; in++) {
		const uint32_t ch = *in;

		if (out) {
			if (ch < 0x10000) {
				out[len++] = (uint8_t)(ch >> 8);
				out[len++] = (uint8_t)(ch);
			} else {
				uint16_t w1 = 0xD800 + ((ch - 0x10000) >> 10);
				uint16_t w2 = 0xDC00 + ((ch - 0x10000) & 0x3FF);

				out[len++] = (uint8_t)(w1 >> 8);
				out[len++] = (uint8_t)(w1);
				out[len++] = (uint8_t)(w2 >> 8);
				out[len++] = (uint8_t)(w2);
			}
		} else {
			len += (ch < 0x10000) ? 2 : 4;
		}
	}

	if (out) {
		out[len++] = '\0';
		out[len++] = '\0';
	} else {
		len += 2;
	}

	return len;
}

/* function LUT here */
typedef size_t (*charset_conv_to_ucs4_func)(const uint8_t*, uint32_t*);
typedef size_t (*charset_conv_from_ucs4_func)(const uint32_t*, uint8_t*);

static charset_conv_to_ucs4_func conv_to_ucs4_funcs[] = {
	[CHARSET_UTF8] = utf8_to_ucs4,
	[CHARSET_UTF16LE] = utf16le_to_ucs4,
	[CHARSET_UTF16BE] = utf16be_to_ucs4,
#if WORDS_BIGENDIAN
	/* no point in duplicating code here */
	[CHARSET_UTF16] = utf16be_to_ucs4,
#else
	[CHARSET_UTF16] = utf16le_to_ucs4,
#endif

	[CHARSET_CP437] = cp437_to_ucs4,

#ifdef WIN32
# if WORDS_BIGENDIAN
	[CHARSET_WCHAR_T] = utf16be_to_ucs4,
# else
	[CHARSET_WCHAR_T] = utf16le_to_ucs4,
# endif
#else
	/* unimplemented */
	[CHARSET_WCHAR_T] = NULL,
#endif
};

static charset_conv_from_ucs4_func conv_from_ucs4_funcs[] = {
	[CHARSET_UTF8] = ucs4_to_utf8,
	[CHARSET_UTF16LE] = ucs4_to_utf16le,
	[CHARSET_UTF16BE] = ucs4_to_utf16be,
#if WORDS_BIGENDIAN
	[CHARSET_UTF16] = ucs4_to_utf16be,
#else
	[CHARSET_UTF16] = ucs4_to_utf16le,
#endif

	[CHARSET_CP437] = ucs4_to_cp437,

#ifdef WIN32
# if WORDS_BIGENDIAN
	[CHARSET_WCHAR_T] = ucs4_to_utf16be,
# else
	[CHARSET_WCHAR_T] = ucs4_to_utf16le,
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
			return "Success";
		case CHARSET_ERROR_UNIMPLEMENTED:
			return "Conversion unimplemented";
		case CHARSET_ERROR_NULLINPUT:
			return "Input pointer is NULL";
		case CHARSET_ERROR_NULLOUTPUT:
			return "Output pointer is NULL";
		case CHARSET_ERROR_INPUTISOUTPUT:
			return "Input and output charsets are the same";
	}
}

/* our version of iconv; this has a much simpler API than the regular
 * iconv() because much of it isn't very necessary for our purposes
 *
 * XXX: this currently uses an in-between buffer of UCS-4, when it's
 * probably better to just convert one character at a time
 *
 * all input is expected to be NULL-terminated
 *
 * example usage:
 *     uint8_t *cp437 = some_buf, *utf8 = NULL;
 *     charset_iconv(cp437, &utf8, CHARSET_CP437, CHARSET_UTF8);
 * 
 * [out] must be free'd by the caller */
charset_error_t charset_iconv(const uint8_t* in, uint8_t** out, charset_t inset, charset_t outset) {
	if (!in)
		return CHARSET_ERROR_NULLINPUT;

	if (!out)
		return CHARSET_ERROR_NULLOUTPUT;

	size_t in_length = 0;
	size_t out_length = 0;
	uint32_t* ucs4_buf = NULL;

	if (inset == outset) {
		fprintf(stderr, "charset_iconv: inset == outset, refusing to convert to the same charset. fix your code");
		return CHARSET_ERROR_INPUTISOUTPUT;
	}

	charset_conv_to_ucs4_func conv_to_ucs4_func = conv_to_ucs4_funcs[inset];
	if (!conv_to_ucs4_func)
		return CHARSET_ERROR_UNIMPLEMENTED;

	charset_conv_from_ucs4_func conv_from_ucs4_func = conv_from_ucs4_funcs[outset];
	if (!conv_from_ucs4_func)
		return CHARSET_ERROR_UNIMPLEMENTED;

	in_length = conv_to_ucs4_func(in, NULL);

	ucs4_buf = mem_calloc(in_length + 1, sizeof(uint32_t));

	conv_to_ucs4_func(in, ucs4_buf);

	/* add NUL character */
	ucs4_buf[in_length] = 0;

	if (outset == CHARSET_UCS4) {
		/* we're done here */
		*out = (uint8_t*)ucs4_buf;
		return CHARSET_ERROR_SUCCESS;
	}

	out_length = conv_from_ucs4_func(ucs4_buf, NULL);

	*out = mem_calloc(out_length, sizeof(uint8_t));

	conv_from_ucs4_func(ucs4_buf, *out);

	free(ucs4_buf);

	return CHARSET_ERROR_SUCCESS;
}
