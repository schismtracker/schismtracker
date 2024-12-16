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
#include "page.h"
#include "keyboard.h"
#include "midi.h"
#include "song.h"
#include "widget.h"
#include "vgamem.h"

/* --------------------------------------------------------------------- */

static struct widget widgets_midiout[33];
static int zxx_top = 0;
static midi_config_t editcfg;

/* --------------------------------------------------------------------- */

static void midiout_draw_const(void)
{
	char buf[4] = "SFx";
	unsigned int i;

	draw_text(    "MIDI Start", 6, 13, 0, 2);
	draw_text(     "MIDI Stop", 7, 14, 0, 2);
	draw_text(     "MIDI Tick", 7, 15, 0, 2);
	draw_text(       "Note On", 9, 16, 0, 2);
	draw_text(      "Note Off", 8, 17, 0, 2);
	draw_text( "Change Volume", 3, 18, 0, 2);
	draw_text(    "Change Pan", 6, 19, 0, 2);
	draw_text(   "Bank Select", 5, 20, 0, 2);
	draw_text("Program Change", 2, 21, 0, 2);

	draw_text(   "Macro   SF0", 5, 24, 0, 2);
	draw_text(   "Setup   SF1", 5, 25, 0, 2);

	for (i = 2; i < 16; i++) {
		buf[2] = hexdigits[i];
		draw_text(buf, 13, i + 24, 0, 2);
	}

	draw_box(16, 12, 60, 22, BOX_THIN|BOX_INNER|BOX_INSET);
	draw_box(16, 23, 60, 40, BOX_THIN|BOX_INNER|BOX_INSET);
	draw_box(16, 41, 60, 49, BOX_THIN|BOX_INNER|BOX_INSET);

	for (i = 0; i < 7; i++) {
		sprintf(buf, "Z%02X", i + zxx_top + 0x80);
		draw_text(buf, 13, i + 42, 0, 2);
	}
}

static void copy_out(void)
{
	song_lock_audio();
	memcpy(&current_song->midi_config, &editcfg, sizeof(midi_config_t));
	song_unlock_audio();
}

static void copy_in(void)
{
	song_lock_audio();
	memcpy(&editcfg, &current_song->midi_config, sizeof(midi_config_t));
	song_unlock_audio();
}

static void zxx_setpos(int pos)
{
	int i;

	/* 128 items, scrolled on 7 lines */
	pos = CLAMP(pos, 0, 128 - 7);
	if (zxx_top == pos)
		return;
	zxx_top = pos;
	for (i = 0; i < 7; i++)
		widgets_midiout[25 + i].d.textentry.text = editcfg.zxx[zxx_top + i];

	status.flags |= NEED_UPDATE;
}


static int pre_handle_key(struct key_event *k)
{
	if (*selected_widget == 25 && k->sym == SCHISM_KEYSYM_UP) {
		/* scroll up */
		if (k->state == KEY_RELEASE)
			return 1;
		if (zxx_top == 0)
			return 0; /* let the normal key handler catch it and change focus */
		zxx_setpos(zxx_top - 1);
		return 1;
	}
	if (*selected_widget == 31 && k->sym == SCHISM_KEYSYM_DOWN) {
		/* scroll down */
		if (k->state == KEY_RELEASE)
			return 1;
		zxx_setpos(zxx_top + 1);
		return 1;
	}
	if ((*selected_widget) >= 25) {
		switch (k->sym) {
		case SCHISM_KEYSYM_PAGEUP:
			if (k->state == KEY_RELEASE)
				return 1;
			zxx_setpos(zxx_top - 7);
			return 1;
		case SCHISM_KEYSYM_PAGEDOWN:
			if (k->state == KEY_RELEASE)
				return 1;
			zxx_setpos(zxx_top + 7);
			return 1;
		default:
			break;
		};
	}
	return 0;
}

static void midiout_set_page(void)
{
	copy_in();
}

void midiout_load_page(struct page *page)
{
	int i;

	page->title = "MIDI Output Configuration";
	page->draw_const = midiout_draw_const;
	page->set_page = midiout_set_page;
	page->pre_handle_key = pre_handle_key;
	page->total_widgets = 32;
	page->widgets = widgets_midiout;
	page->help_index = HELP_MIDI_OUTPUT;

	char *editcfg_top[] = {
		editcfg.start, editcfg.stop, editcfg.tick, editcfg.note_on, editcfg.note_off,
		editcfg.set_volume, editcfg.set_panning, editcfg.set_bank, editcfg.set_program,
	};

	for (i = 0; i < 9; i++) {
		widget_create_textentry(widgets_midiout + i, 17, 13 + i, 43,
				(i == 0 ? 0 : (i - 1)), i + 1, 9,
				copy_out, editcfg_top[i], 31);
	}
	for (i = 0; i < 16; i++) {
		widget_create_textentry(widgets_midiout + 9 + i, 17, 24 + i, 43,
				9 + i - 1, 9 + i + 1, 25,
				copy_out, editcfg.sfx[i], 31);
	}
	for (i = 0; i < 7; i++) {
		widget_create_textentry(widgets_midiout + 25 + i, 17, 42 + i, 43,
				25 + i - 1, 25 + ((i == 6) ? 6 : (i + 1)), 0,
				copy_out, editcfg.zxx[i], 31);
	}
}

