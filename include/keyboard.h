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

#ifndef SCHISM_KEYBOARD_H_
#define SCHISM_KEYBOARD_H_

int numeric_key_event(struct key_event *k, int kponly);

char *get_note_string(int note, char *buf);       /* "C-5" or "G#4" */
char *get_note_string_short(int note, char *buf); /* "c5" or "G4" */
char *get_volume_string(int volume, int volume_effect, char *buf);
char get_effect_char(int command);
int get_effect_number(char effect);
int get_ptm_effect_number(char effect);

/* values for kbd_sharp_flat_toggle */
typedef enum {
	KBD_SHARP_FLAT_TOGGLE = -1,
	KBD_SHARP_FLAT_SHARPS = 0,
	KBD_SHARP_FLAT_FLATS = 1,
} kbd_sharp_flat_t;

void kbd_init(void);
kbd_sharp_flat_t kbd_sharp_flat_state(void);
void kbd_sharp_flat_toggle(kbd_sharp_flat_t e);
int kbd_get_effect_number(struct key_event *k);
int kbd_char_to_hex(struct key_event *k);
int kbd_char_to_99(struct key_event *k);

int kbd_get_current_octave(void);
void kbd_set_current_octave(int new_octave);

int kbd_get_note(struct key_event *k);

int kbd_get_alnum(struct key_event *k);

void kbd_key_translate(struct key_event *k);

/* -------------------------------------------- */
/* key repeat */

void kbd_handle_key_repeat(void);
void kbd_cache_key_repeat(struct key_event *kk);
void kbd_empty_key_repeat(void);

/* use 0 for delay to (re)set the default rate. */
void kbd_set_key_repeat(int delay, int rate);

/* -------------------------------------------- */
/* text <-> keydowns */

void kbd_push_pending_keydown(struct key_event *kk);
void kbd_pop_pending_keydown(const uint8_t *text);
int kbd_have_pending_keydown(void);

#endif /* SCHISM_KEYBOARD_H_ */
