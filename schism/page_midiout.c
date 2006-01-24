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

#include "headers.h"

#include "it.h"
#include "page.h"
#include "midi.h"
#include "song.h"

#include <SDL.h>
#include <time.h>

static struct widget widgets_midiout[33];
static int zbuf_top = 0;
static char zbuf[7*32];

static char mm_global[9*32];
static char mm_sfx[16*32];

static void midiout_draw_const(void)
{
	char buf[4];
	int i;

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

	buf[0] = 'S';
	buf[1] = 'F';
	buf[3] = '\0';
	for (i = 2; i < 10; i++) {
		buf[2] = i + '0';
		draw_text(buf, 13, 24+i, 0, 2);
	}
	draw_text(           "SFA", 13, 34, 0, 2);
	draw_text(           "SFB", 13, 35, 0, 2);
	draw_text(           "SFC", 13, 36, 0, 2);
	draw_text(           "SFD", 13, 37, 0, 2);
	draw_text(           "SFE", 13, 38, 0, 2);
	draw_text(           "SFF", 13, 39, 0, 2);

	draw_box(16, 12, 60, 22, BOX_THIN|BOX_INNER|BOX_INSET);
	draw_box(16, 23, 60, 40, BOX_THIN|BOX_INNER|BOX_INSET);
	draw_box(16, 41, 60, 49, BOX_THIN|BOX_INNER|BOX_INSET);

	for (i = 0; i < 7; i++) {
		sprintf(buf, "Z%02X", i+zbuf_top+0x80);
		draw_text(buf, 13, i+42, 0, 2);
	}
}
static void copyout_zbuf(void)
{
	midi_config *md;
	int i;
	md = song_get_midi_config();
	song_lock_audio();
	for (i = 0; i < 9; i++) {
		strcpy(md->midi_global_data+(i*32), mm_global+(i*32));
	}
	for (i = 0; i < 16; i++) {
		strcpy(md->midi_sfx+(i*32), mm_sfx+(i*32));
	}
	for (i = 0; i < 7; i++) {
		strcpy(md->midi_zxx+((zbuf_top+i)*32),
				zbuf+(i*32));
	}
	song_unlock_audio();
}
static void copyin_zbuf(void)
{
	midi_config *md;
	int i;
	md = song_get_midi_config();
	for (i = 0; i < 9; i++) {
		strcpy(mm_global+(i*32), md->midi_global_data+(i*32));
	}
	for (i = 0; i < 16; i++) {
		strcpy(mm_sfx+(i*32), md->midi_sfx+(i*32));
	}

	for (i = 0; i < 7; i++) {
		strcpy(zbuf+(i*32), md->midi_zxx+((zbuf_top+i)*32));
	}
}

static int pre_handle_key(struct key_event *k)
{
	if (k->sym == SDLK_ESCAPE) {
		if (!k->state) return 1;
		set_page(PAGE_MIDI);
		return 1;
	}
	if (*selected_widget == 25 && (k->sym == SDLK_UP || k->mouse == 2)) {
		/* scroll up */
		if (zbuf_top == 0) return 0;
		if (k->state) return 1;
		copyout_zbuf();
		zbuf_top--;
		copyin_zbuf();
		status.flags |= NEED_UPDATE;
		return 1;
	}
	if (*selected_widget == 31 && (k->sym == SDLK_DOWN || k->mouse == 3)) {
		/* scroll down */
		if (zbuf_top >= 121) return 0;
		if (k->state) return 1;
		copyout_zbuf();
		zbuf_top++;
		copyin_zbuf();
		status.flags |= NEED_UPDATE;
		return 1;
	}
	if ((*selected_widget) >= 25) {
		switch (k->sym) {
		case SDLK_PAGEUP:
			if (k->state) return 1;
			copyout_zbuf();
			zbuf_top -= 7;
			if (zbuf_top < 0) zbuf_top = 0;
			copyin_zbuf();
			status.flags |= NEED_UPDATE;
			return 1;
		case SDLK_PAGEDOWN:
			if (k->state) return 1;
			copyout_zbuf();
			zbuf_top += 7;
			if (zbuf_top >= 121) zbuf_top = 121;
			status.flags |= NEED_UPDATE;
			copyin_zbuf();
			return 1;
		};
	}
	return 0;
}

void midiout_set_page(void)
{
	copyin_zbuf();
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

	for (i = 0; i < 9; i++) {
		create_textentry(widgets_midiout+i, 17, 13+i, 43,
				(i == 0 ? 0 : (i-1)),
				i+1,
				i+1,
				copyout_zbuf, mm_global+(i*32),31);
	}
	for (i = 0; i < 16; i++) {
		create_textentry(widgets_midiout+9+i, 17, 24+i, 43,
				9+(i-1),
				9+(i+1),
				9+(i+1),
				copyout_zbuf, mm_sfx+(i*32),31);
	}
	for (i = 0; i < 7; i++) {
		create_textentry(widgets_midiout+25+i, 17, 42+i, 43,
				25+(i-1),
				25+((i == 6) ? 6 : (i+1)),
				25+((i == 6) ? 6 : (i+1)),
				copyout_zbuf, zbuf+(i*32),31);
	}
	zbuf_top = 0;
}

