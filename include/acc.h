/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
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
#ifndef __acc_h
#define __acc_h

struct acc_state_info {
	int type, chord, table, base, solo;
	char chord_str[48];
};
struct acc_harmonize_info {
	char name[16];
	int notes, dist;
};
struct acc_chordt_table {
	char name[16];
	int a3, a6, a7;
};
struct acc_chord3_table {
	int type, base, di1, di2, inv2, inv3;
};
struct acc_chord4_table {
	int type, base, di1, di2, di3, inv2, inv3;
};
struct acc_chord5_table {
	int type, base, di1, di2, di3, di4;
};

#ifdef __cplusplus
extern "C" {
#endif

/* called by keydown/keyup if the capslock key is down when they're called */
void acc_state_keys(struct acc_state_info *nn, int ch[64]);


/* change current harmony */
void acc_set_harmonize_next(void);
void acc_set_harmonize_previous(void);


#ifdef __cplusplus
};
#endif

extern struct acc_state_info *acc_current;
extern struct acc_harmonize_info *acc_harmonize;

#endif
