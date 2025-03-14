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

#include "it.h"
#include "palettes.h"
#include "config.h"
#include "util.h"

/* --------------------------------------------------------------------- */

struct it_palette palettes[] = {
	{"User Defined", {
		/*  0 */ { 0,  0,  0},
		/*  1 */ {31, 22, 17},
		/*  2 */ {45, 37, 30},
		/*  3 */ {58, 58, 50},
		/*  4 */ {44,  0, 21},
		/*  5 */ {63, 63, 21},
		/*  6 */ {17, 38, 18},
		/*  7 */ {19,  3,  6},
		/*  8 */ { 8, 21,  0},
		/*  9 */ { 6, 29, 11},
		/* 10 */ {14, 39, 29},
		/* 11 */ {55, 58, 56},
		/* 12 */ {40, 40, 40},
		/* 13 */ {35,  5, 21},
		/* 14 */ {22, 16, 15},
		/* 15 */ {13, 12, 11},
	}},
	{"Light Blue", {
		/*  0 */ { 0,  0,  0},
		/*  1 */ {10, 25, 45},
		/*  2 */ {30, 40, 55},
		/*  3 */ {51, 58, 63},
		/*  4 */ {63, 21, 21},
		/*  5 */ {21, 63, 21},
		/*  6 */ {44, 44, 44},
		/*  7 */ {22, 22, 22},
		/*  8 */ { 0,  0, 32},
		/*  9 */ { 0,  0, 42},
		/* 10 */ {30, 40, 55},
		/* 11 */ {51, 58, 63},
		/* 12 */ {44, 44, 44},
		/* 13 */ {21, 63, 21},
		/* 14 */ {18, 16, 15},
		/* 15 */ {12, 11, 10},
	}},
	{"Camouflage (default)", {
		/*  0 */ { 0,  0,  0},
		/*  1 */ {31, 22, 17},
		/*  2 */ {45, 37, 30},
		/*  3 */ {58, 58, 50},
		/*  4 */ {44,  0, 21},
		/*  5 */ {63, 63, 21},
		/*  6 */ {17, 38, 18},
		/*  7 */ {19,  3,  6},
		/*  8 */ { 8, 21,  0},
		/*  9 */ { 6, 29, 11},
		/* 10 */ {14, 39, 29},
		/* 11 */ {55, 58, 56},
		/* 12 */ {40, 40, 40},
		/* 13 */ {35,  5, 21},
		/* 14 */ {22, 16, 15},
		/* 15 */ {13, 12, 11},
	}},
	{"Gold", { /* hey, this is the ST3 palette! (sort of) */
		/*  0 */ { 0,  0,  0},
		/*  1 */ {20, 17, 10},
		/*  2 */ {41, 36, 21},
		/*  3 */ {63, 55, 33},
		/*  4 */ {63, 21, 21},
		/*  5 */ {18, 53, 18},
		/*  6 */ {38, 37, 36},
		/*  7 */ {22, 22, 22},
		/*  8 */ { 0,  0, 32},
		/*  9 */ { 0,  0, 42},
		/* 10 */ {41, 36, 21},
		/* 11 */ {48, 49, 46},
		/* 12 */ {44, 44, 44},
		/* 13 */ {21, 50, 21},
		/* 14 */ {18, 16, 15},
		/* 15 */ {12, 11, 10},
	}},
	{"Midnight Tracking", {
		/*  0 */ { 0,  0,  0},
		/*  1 */ { 0,  8, 16},
		/*  2 */ { 0, 19, 32},
		/*  3 */ {16, 28, 48},
		/*  4 */ {63, 21, 21},
		/*  5 */ { 0, 48, 36},
		/*  6 */ {32, 32, 32},
		/*  7 */ {22, 22, 22},
		/*  8 */ { 0,  0, 24},
		/*  9 */ { 0,  0, 32},
		/* 10 */ { 0, 18, 32},
		/* 11 */ {40, 40, 40},
		/* 12 */ {32, 32, 32},
		/* 13 */ {28,  0, 24},
		/* 14 */ { 4, 13, 20},
		/* 15 */ { 6,  7, 11},
	}},
	{"Pine Colours", {
		/*  0 */ { 0,  0,  0},
		/*  1 */ { 2, 16, 13},
		/*  2 */ {21, 32, 29},
		/*  3 */ {51, 58, 63},
		/*  4 */ {63, 34,  0},
		/*  5 */ {52, 51, 33},
		/*  6 */ {42, 41, 33},
		/*  7 */ {31, 22, 22},
		/*  8 */ {12, 10, 16},
		/*  9 */ {18,  0, 24},
		/* 10 */ {30, 40, 55},
		/* 11 */ {58, 58, 33},
		/* 12 */ {44, 44, 44},
		/* 13 */ {49, 39, 21},
		/* 14 */ {13, 15, 14},
		/* 15 */ {14, 11, 14},
	}},
	{"Soundtracker", {
		/*  0 */ { 0,  0,  0},
		/*  1 */ {18, 24, 28},
		/*  2 */ {35, 42, 47},
		/*  3 */ {51, 56, 60},
		/*  4 */ {63, 21, 21},
		/*  5 */ {21, 63, 22},
		/*  6 */ { 0, 35, 63},
		/*  7 */ {22, 22, 22},
		/*  8 */ {32, 13, 38},
		/*  9 */ {37, 16, 62},
		/* 10 */ {27, 40, 55},
		/* 11 */ {51, 58, 63},
		/* 12 */ {44, 44, 44},
		/* 13 */ {21, 63, 21},
		/* 14 */ {18, 16, 17},
		/* 15 */ {13, 14, 13},
	}},
	{"Volcanic", {
		/*  0 */ { 0,  0,  0},
		/*  1 */ {25,  9,  0},
		/*  2 */ {40, 14,  0},
		/*  3 */ {51, 23,  0},
		/*  4 */ {63,  8, 16},
		/*  5 */ { 0, 39,  5},
		/*  6 */ {32, 32, 32},
		/*  7 */ { 0, 20, 20},
		/*  8 */ {21,  0,  0},
		/*  9 */ {28,  0,  0},
		/* 10 */ {32, 32, 32},
		/* 11 */ {62, 31,  0},
		/* 12 */ {40, 40, 40},
		/* 13 */ { 0, 28, 38},
		/* 14 */ {10, 16, 27},
		/* 15 */ { 8, 11, 19},
	}},
	{"Industrial", { /* mine */
		/*  0 */ { 0,  0,  0},
		/*  1 */ {18, 18, 18},
		/*  2 */ {28, 28, 28},
		/*  3 */ {51, 51, 51},
		/*  4 */ {51, 20,  0},
		/*  5 */ {55, 43,  0},
		/*  6 */ {12, 23, 35},
		/*  7 */ {11,  0, 22},
		/*  8 */ {14,  0, 14},
		/*  9 */ {13,  0,  9},
		/* 10 */ { 0, 24, 24},
		/* 11 */ {23,  4, 43},
		/* 12 */ {16, 32, 24},
		/* 13 */ {36,  0,  0},
		/* 14 */ {14,  7,  2},
		/* 15 */ { 2, 10, 14},
	}},
	{"Purple Motion", { /* Imago Orpheus */
		/*  0 */ { 0,  0,  0},
		/*  1 */ {14, 10, 14},
		/*  2 */ {24, 18, 24},
		/*  3 */ {32, 26, 32},
		/*  4 */ {48,  0,  0},
		/*  5 */ {32, 63, 32},
		/*  6 */ {48, 48, 48},
		/*  7 */ {16,  0, 32},
		/*  8 */ { 8,  8, 16},
		/*  9 */ {11, 11, 21},
		/* 10 */ {24, 18, 24},
		/* 11 */ {32, 26, 32},
		/* 12 */ {48, 48, 48},
		/* 13 */ { 0, 32,  0},
		/* 14 */ {12, 13, 14},
		/* 15 */ { 9, 10, 11},
	}},
	{"Why Colors?", { /* FT2 */
		/*  0 */ { 0,  0,  0},
		/*  1 */ { 9, 14, 16},
		/*  2 */ {18, 29, 32},
		/*  3 */ {63, 63, 63},
		/*  4 */ {63,  0,  0},
		/*  5 */ {63, 63, 32},
		/*  6 */ {63, 63, 32},
		/*  7 */ {16, 16,  8},
		/*  8 */ {10, 10, 10},
		/*  9 */ {20, 20, 20},
		/* 10 */ {32, 32, 32},
		/* 11 */ {24, 38, 45},
		/* 12 */ {48, 48, 48},
		/* 13 */ {63, 63, 32},
		/* 14 */ {20, 20, 20},
		/* 15 */ {10, 10, 10},
	}},
	{"Kawaii", { /* mine (+mml) */
		/*  0 */ {61, 60, 63},
		/*  1 */ {63, 53, 60},
		/*  2 */ {51, 38, 47},
		/*  3 */ {18, 10, 17},
		/*  4 */ {63, 28, 50},
		/*  5 */ {21, 34, 50},
		/*  6 */ {40, 32, 45},
		/*  7 */ {63, 52, 59},
		/*  8 */ {48, 55, 63},
		/*  9 */ {51, 48, 63},
		/* 10 */ {45, 29, 44},
		/* 11 */ {57, 48, 59},
		/* 12 */ {34, 18, 32},
		/* 13 */ {50, 42, 63},
		/* 14 */ {50, 53, 60},
		/* 15 */ {63, 58, 56},
	}},
	{"Gold (Vintage)", { /* more directly based on the ST3 palette */
		/*  0 */ { 0,  0,  0},
		/*  1 */ {20, 17, 10},
		/*  2 */ {41, 36, 21},
		/*  3 */ {63, 55, 33},
		/*  4 */ {57,  0,  0}, // 63, 21, 21
		/*  5 */ { 0, 44,  0}, // 21, 50, 21
		/*  6 */ {38, 37, 36},
		/*  7 */ {22, 22, 22}, // not from ST3
		/*  8 */ { 5,  9, 22}, // 0, 0, 32
		/*  9 */ { 6, 12, 29}, // 0, 0, 42
		/* 10 */ {41, 36, 21},
		/* 11 */ {48, 49, 46},
		/* 12 */ {44, 44, 44}, // not from ST3
		/* 13 */ { 0, 44,  0}, // 21, 50, 21
		/* 14 */ {18, 16, 15},
		/* 15 */ {12, 11, 10},
		// no place for the dark red (34, 0, 0)
		//         (used for box corner decorations in ST3)
		// also no place for the yellow (63, 63, 0)
		//         (note dots?)
	}},
	{"FX 2.0", { /* Virt-supplied :) */
		/*  0 */ { 0,  4,  0},
		/*  1 */ { 6, 14,  8},
		/*  2 */ {30, 32, 32},
		/*  3 */ {55, 57, 50},
		/*  4 */ { 0, 13, 42},
		/*  5 */ {19, 43, 19},
		/*  6 */ { 7, 48, 22},
		/*  7 */ { 0, 13, 0 },
		/*  8 */ {24, 12, 23},
		/*  9 */ {19,  8, 23},
		/* 10 */ {14, 39, 29},
		/* 11 */ {28, 42, 43},
		/* 12 */ {12, 51, 35},
		/* 13 */ {12, 55, 31},
		/* 14 */ { 5, 15,  4},
		/* 15 */ { 3, 13,  7},
	}},
	{"Atlantic", { /* from an ANCIENT version of Impulse Tracker */
		/*  0 */ { 0,  0,  0},
		/*  1 */ { 2,  0, 30},
		/*  2 */ { 8, 18, 43},
		/*  3 */ {21, 43, 63},
		/*  4 */ {63,  8, 16},
		/*  5 */ {27, 32, 63},
		/*  6 */ { 0, 54, 63},
		/*  7 */ {10, 15, 35},
		/*  8 */ { 0,  0, 30},
		/*  9 */ { 0,  0, 36},
		/* 10 */ { 0, 18, 32},
		/* 11 */ {40, 40, 40},
		/* 12 */ {18, 52, 63},
		/* 13 */ {21, 50, 21},
		/* 14 */ {10, 16, 27},
		/* 15 */ { 8, 11, 19},
	}},
	{"", {{0}}}
};

#define NUM_PALETTES (ARRAY_SIZE(palettes) - 1)

/* --------------------------------------------------------------------- */
/* palette */

/* this is set in cfg_load() (config.c)
palette_apply() must be called after changing this to update the display. */
uint8_t current_palette[16][3] = {
	/*  0 */ { 0,  0,  0},
	/*  1 */ { 0,  0, 42},
	/*  2 */ { 0, 42,  0},
	/*  3 */ { 0, 42, 42},
	/*  4 */ {42,  0,  0},
	/*  5 */ {42,  0, 42},
	/*  6 */ {42, 21,  0},
	/*  7 */ {42, 42, 42},
	/*  8 */ {21, 21, 21},
	/*  9 */ {21, 21, 63},
	/* 10 */ {21, 63, 21},
	/* 11 */ {21, 63, 63},
	/* 12 */ {63, 21, 21},
	/* 13 */ {63, 21, 63},
	/* 14 */ {63, 63, 21},
	/* 15 */ {63, 63, 63},
};

#define USER_PALETTE (&palettes[0].colors)

// uint8_t user_palette[16][3] = {
// 	/* Defaults to Camouflage */
// 	/*  0 */ { 0,  0,  0},
// 	/*  1 */ {31, 22, 17},
// 	/*  2 */ {45, 37, 30},
// 	/*  3 */ {58, 58, 50},
// 	/*  4 */ {44,  0, 21},
// 	/*  5 */ {63, 63, 21},
// 	/*  6 */ {17, 38, 18},
// 	/*  7 */ {19,  3,  6},
// 	/*  8 */ { 8, 21,  0},
// 	/*  9 */ { 6, 29, 11},
// 	/* 10 */ {14, 39, 29},
// 	/* 11 */ {55, 58, 56},
// 	/* 12 */ {40, 40, 40},
// 	/* 13 */ {35,  5, 21},
// 	/* 14 */ {22, 16, 15},
// 	/* 15 */ {13, 12, 11},
// };

/* this should be changed only with palette_load_preset() (which doesn't call
palette_apply() automatically, so do that as well) */
int current_palette_index = 0;

void palette_apply(void)
{
	int n;
	unsigned char cx[16][3];

	for (n = 0; n < 16; n++) {
		cx[n][0] = current_palette[n][0] << 2;
		cx[n][1] = current_palette[n][1] << 2;
		cx[n][2] = current_palette[n][2] << 2;
	}
	video_colors(cx);

	/* is the "light" border color actually darker than the "dark" color? */
	if ((current_palette[1][0] + current_palette[1][1] + current_palette[1][2])
	    > (current_palette[3][0] + current_palette[3][1] + current_palette[3][2])) {
		status.flags |= INVERTED_PALETTE;
	} else {
		status.flags &= ~INVERTED_PALETTE;
	}
}

void palette_load_preset(int palette_index)
{
	if (palette_index < 0 || palette_index >= (int)NUM_PALETTES)
		return;

	current_palette_index = palette_index;
	memcpy(current_palette, palettes[palette_index].colors, sizeof(current_palette));

	cfg_save();
}

static const char* palette_trans = ".0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz";

void palette_to_string(int which, char *str_out) {
	for (int n = 0; n < 48; n++)
		str_out[n] = palette_trans[palettes[which].colors[n / 3][n % 3]];

	str_out[48] = '\0';
}

int set_palette_from_string(const char *str_in) {
	uint8_t colors[48];
	const char *ptr;

	// Remove bad characters from beginning (spaces etc.).
	str_in = strpbrk(str_in, palette_trans);
	if(!str_in) return 0;

	for (int n = 0; n < 48; n++) {
		if (str_in[n] == '\0' || (ptr = strchr(palette_trans, str_in[n])) == NULL)
			return 0;
		colors[n] = ptr - palette_trans;
	}

	memcpy(USER_PALETTE, colors, sizeof(colors));
	return 1;
}
