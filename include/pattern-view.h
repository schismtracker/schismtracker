/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#ifndef PATTERN_VIEW_H
#define PATTERN_VIEW_H

/* NOTE: these functions need to be called with the screen LOCKED */

typedef void (*draw_channel_header_func) (int chan, int x, int y, int fg);
typedef void (*draw_note_func) (int x, int y, song_note * note,
                                int cursor_pos, int fg, int bg);
typedef void (*draw_mask_func) (int x, int y, int mask, int cursor_pos, int fg, int bg);

#define PATTERN_VIEW(n) \
        void draw_channel_header_##n(int chan, int x, int y, int fg); \
        void draw_note_##n(int x, int y, song_note * note, \
                           int cursor_pos, int fg, int bg); \
        void draw_mask_##n(int x, int y, int mask, int cursor_pos, int fg, int bg);

PATTERN_VIEW(13);
PATTERN_VIEW(10);
PATTERN_VIEW(7);
PATTERN_VIEW(6);
PATTERN_VIEW(3);
PATTERN_VIEW(2);
PATTERN_VIEW(1);

#undef PATTERN_VIEW

/* for the pattern editor masks (the ^^^ ^^ ^^ --- markers at the bottom) */
#define MASK_NOTE       1 /* immutable */
#define MASK_INSTRUMENT 2
#define MASK_VOLUME     4
#define MASK_EFFECT     8

#endif /* ! PATTERN_VIEW_H */

