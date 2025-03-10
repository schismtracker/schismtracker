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

#ifndef SCHISM_PLAYER_TABLES_H_
#define SCHISM_PLAYER_TABLES_H_

// <eightbitbubsy> better than having a table.
#define SHORT_PANNING(i) (((((i) << 4) | (i)) + 2) >> 2)

/* TODO: I know just sticking _fast on all of these will break the player, but for some of 'em...? */

extern const uint8_t vc_portamento_table[16]; // volume column Gx

extern const uint16_t period_table[12];
extern const uint16_t finetune_table[16];

extern const int8_t sine_table[256];
extern const int8_t ramp_down_table[256];
extern const int8_t square_table[256];

extern const int8_t retrig_table_1[16];
extern const int8_t retrig_table_2[16];

extern const uint32_t fine_linear_slide_up_table[16];
extern const uint32_t fine_linear_slide_down_table[16];
extern const uint32_t linear_slide_up_table[256];
extern const uint32_t linear_slide_down_table[256];

extern const char *midi_group_names[17];
extern const char *midi_program_names[128];
extern const char *midi_percussion_names[61];

#endif /* SCHISM_PLAYER_TABLES_H_ */

