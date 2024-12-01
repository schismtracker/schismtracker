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
#include "dmoz.h"
#include "config.h"
#include "fonts.h"
#include "util.h"
#include "osdefs.h"

#include <errno.h>

/* --------------------------------------------------------------------- */
/* globals */

uint8_t font_normal[2048];

/* There's no way to change the other fontsets at the moment.
 * (other than recompiling, of course) */
uint8_t font_alt[2048];
uint8_t font_half_data[1024];

uint8_t *font_data = font_normal; /* this only needs to be global for itf */

/* int font_width = 8, font_height = 8; */

/* --------------------------------------------------------------------- */
/* ITF loader */

static inline void make_half_width_middot(void)
{
	/* this copies the left half of char 184 in the normal font (two
	 * half-width dots) to char 173 of the half-width font (the
	 * middot), and the right half to char 184. thus, putting
	 * together chars 173 and 184 of the half-width font will
	 * produce the equivalent of 184 of the full-width font. */

	font_half_data[173 * 4 + 0] =
		(font_normal[184 * 8 + 0] & 0xf0) |
		(font_normal[184 * 8 + 1] & 0xf0) >> 4;
	font_half_data[173 * 4 + 1] =
		(font_normal[184 * 8 + 2] & 0xf0) |
		(font_normal[184 * 8 + 3] & 0xf0) >> 4;
	font_half_data[173 * 4 + 2] =
		(font_normal[184 * 8 + 4] & 0xf0) |
		(font_normal[184 * 8 + 5] & 0xf0) >> 4;
	font_half_data[173 * 4 + 3] =
		(font_normal[184 * 8 + 6] & 0xf0) |
		(font_normal[184 * 8 + 7] & 0xf0) >> 4;

	font_half_data[184 * 4 + 0] =
		(font_normal[184 * 8 + 0] & 0xf) << 4 |
		(font_normal[184 * 8 + 1] & 0xf);
	font_half_data[184 * 4 + 1] =
		(font_normal[184 * 8 + 2] & 0xf) << 4 |
		(font_normal[184 * 8 + 3] & 0xf);
	font_half_data[184 * 4 + 2] =
		(font_normal[184 * 8 + 4] & 0xf) << 4 |
		(font_normal[184 * 8 + 5] & 0xf);
	font_half_data[184 * 4 + 3] =
		(font_normal[184 * 8 + 6] & 0xf) << 4 |
		(font_normal[184 * 8 + 7] & 0xf);
}

/* just the non-itf chars */
void font_reset_lower(void)
{
	memcpy(font_normal, font_default_lower, 1024);
}

/* just the itf chars */
void font_reset_upper(void)
{
	memcpy(font_normal + 1024, font_default_upper_itf, 1024);
	make_half_width_middot();
}

/* all together now! */
void font_reset(void)
{
	memcpy(font_normal, font_default_lower, 1024);
	memcpy(font_normal + 1024, font_default_upper_itf, 1024);
	make_half_width_middot();
}

/* or kill the upper chars as well */
void font_reset_bios(void)
{
	font_reset_lower();
	memcpy(font_normal + 1024, font_default_upper_alt, 1024);
	make_half_width_middot();
}

/* ... or just one character */
void font_reset_char(int ch)
{
	uint8_t *base;
	int cx;

	ch <<= 3;
	cx = ch;
	if (ch >= 1024) {
		base = (uint8_t *) font_default_upper_itf;
		cx -= 1024;
	} else {
		base = (uint8_t *) font_default_lower;
	}
	/* update them both... */
	memcpy(font_normal + ch, base + cx, 8);

	/* update */
	make_half_width_middot();
}

/* --------------------------------------------------------------------- */

static int squeeze_8x16_font(slurp_t *fp)
{
	uint8_t data_8x16[4096];
	int n;

	if (slurp_read(fp, data_8x16, 4096) != 1)
		return -1;

	for (n = 0; n < 2048; n++)
		font_normal[n] = data_8x16[2 * n] | data_8x16[2 * n + 1];

	return 0;
}

/* Hmm. I could've done better with this one. */
int font_load(const char *filename)
{
	uint8_t data[4];
	char *font_file;
	{
		char *font_dir = dmoz_path_concat(cfg_dir_dotschism, "fonts");
		font_file = dmoz_path_concat(font_dir, filename);
		free(font_dir);
	}

	slurp_t fp = {0};

	if (slurp(&fp, font_file, NULL, 0) < 0) {
		free(font_file);
		return -1;
	}

	size_t len = slurp_length(&fp);
	switch (len) {
	case 2048: /* raw font data */
		break;
	case 2050: /* *probably* an ITF */
		slurp_seek(&fp, 2048, SEEK_SET);
		if (slurp_read(&fp, data, 2) != 2) {
			unslurp(&fp);
			free(font_file);
			return -1;
		}
		if (data[1] != 0x2 || (data[0] != 0x12 && data[0] != 9)) {
			unslurp(&fp);
			free(font_file);
			return -1;
		}
		slurp_rewind(&fp);
		break;
	case 4096: /* raw font data, 8x16 */
		if (squeeze_8x16_font(&fp) == 0) {
			make_half_width_middot();
			unslurp(&fp);
			free(font_file);
			return 0;
		} else {
			unslurp(&fp);
			free(font_file);
			return -1;
		}
		break;
	default:
		unslurp(&fp);
		free(font_file);
		return -1;
	}

	if (slurp_read(&fp, font_normal, 2048) != 2048) {
		unslurp(&fp);
		free(font_file);
		return -1;
	}

	make_half_width_middot();

	unslurp(&fp);
	free(font_file);
	return 0;
}

int font_save(const char *filename)
{
	uint8_t ver[2] = { 0x12, 0x2 };
	char *font_file;
	{
		char *font_dir = dmoz_path_concat(cfg_dir_dotschism, "fonts");
		font_file = dmoz_path_concat(font_dir, filename);
		free(font_dir);
	}

	disko_t fp = {0};
	if (disko_open(&fp, font_file) < 0) {
		free(font_file);
		return -1;
	}

	disko_write(&fp, font_normal, 2048);
	disko_write(&fp, ver, 2);

	disko_close(&fp, 0);

	free(font_file);

	return 0;
}

void font_init(void)
{
	memcpy(font_half_data, font_half_width, 1024);

	if (font_load(cfg_font) != 0)
		font_reset();

	memcpy(font_alt, font_default_lower, 1024);
	memcpy(font_alt + 1024, font_default_upper_alt, 1024);
}
