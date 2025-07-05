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

#ifndef SCHISM_FONTS_H_
#define SCHISM_FONTS_H_

int font_load(const char *filename);

/* mostly for the itf editor */
int font_save(const char *filename);

void font_reset_lower(void);    /* ascii chars (0-127) */
void font_reset_upper(void);    /* itf chars (128-255) */
void font_reset(void);  /* everything (0-255) */
void font_reset_bios(void);     /* resets all chars to the alt font */
void font_reset_char(int c);     /* resets just one char */

/* this needs to be called before any char drawing.
 * it's pretty much the same as doing...
 *         if (!font_load("font.cfg"))
 *                 font_reset();
 * ... the main difference being font_init() is easier to deal with :) */
void font_init(void);

extern const uint8_t font_default_lower[];
extern const uint8_t font_default_upper_alt[];
extern const uint8_t font_default_upper_itf[];
extern const uint8_t font_half_width[];

/* These fonts were adapted from the font8x8 pack
 * which is in the public domain:
 *   https://github.com/dhepper/font8x8 */
extern const uint8_t font_hiragana[];
extern const uint8_t font_extended_latin[];
extern const uint8_t font_greek[];

/* ripped from KeyRus; the lower 128 chars have been
 * snipped from it because they're just ASCII */
extern const uint8_t font_cp866[];

extern uint8_t font_half_data[1024];
extern uint8_t *font_data; /* 2048 bytes */

#endif /* SCHISM_FONTS_H_ */
