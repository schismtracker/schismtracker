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

/* real character set stuff will occur here eventually... */


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
char* str_unicode_to_cp437(const char* s)
{
	if (s == NULL)
		return NULL;

	int s_len = strlen(s), i;
	char* out = calloc(s_len + 1, sizeof(char));

	for ( i = 0 ; i < s_len ; i++ ) {
		out[i] = char_unicode_to_cp437(s[i]);
	}

	return out;
}

int char_unicode_to_cp437(unsigned int c)
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
	};
	return c;
}
