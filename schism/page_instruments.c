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

/* This is getting almost as disturbing as the pattern editor. */

#include "headers.h"
#include "config.h"
#include "it.h"
#include "page.h"
#include "song.h"
#include "dmoz.h"
#include "video.h"
#include "keyboard.h"
#include "fakemem.h"
#include "widget.h"
#include "dialog.h"
#include "vgamem.h"
#include "osdefs.h"

#include <sys/stat.h>

/* --------------------------------------------------------------------- */
/* just one global variable... */

int instrument_list_subpage = PAGE_INSTRUMENT_LIST_GENERAL;

/* --------------------------------------------------------------------- */
/* ... but tons o' ugly statics */

static struct widget widgets_general[18];
static struct widget widgets_volume[17];
static struct widget widgets_panning[19];
static struct widget widgets_pitch[20];

/* rastops for envelope */
static struct vgamem_overlay env_overlay = {
	32, 18, 65, 25,
	NULL, 0, 0, 0
};

/* toggled when pressing "," on the note table's sample field
 * more of a boolean than a bit mask  -delt.
 */
static int note_sample_mask = 1;

static struct widget *get_page_widgets(void)
{
	switch (instrument_list_subpage) {
	case PAGE_INSTRUMENT_LIST_GENERAL:
		return widgets_general;
	case PAGE_INSTRUMENT_LIST_VOLUME:
		return widgets_volume;
	case PAGE_INSTRUMENT_LIST_PANNING:
		return widgets_panning;
	case PAGE_INSTRUMENT_LIST_PITCH:
		return widgets_pitch;
	};
	return widgets_general;
}

static const int subpage_switches_group[5] = { 1, 2, 3, 4, -1 };
static const int nna_group[5] = { 6, 7, 8, 9, -1 };
static const int dct_group[5] = { 10, 11, 12, 13, -1 };
static const int dca_group[4] = { 14, 15, 16, -1 };

static const char *const pitch_envelope_states[] = { "Off", "On Pitch", "On Filter", NULL };

static int top_instrument = 1;
static int current_instrument = 1;
static int _altswap_lastvis = 99; // for alt-down instrument-swapping
static int instrument_cursor_pos = 25;  /* "play" mode */

static int note_trans_top_line = 0;
static int note_trans_sel_line = 0;

static int note_trans_cursor_pos = 0;

/* shared by all the numentries on a page
 * (0 = volume, 1 = panning, 2 = pitch) */
static int numentry_cursor_pos[3] = { 0 };

static int current_node_vol = 0;
static int current_node_pan = 0;
static int current_node_pitch = 0;

static int envelope_edit_mode = 0;
static int envelope_mouse_edit = 0;
static int envelope_tick_limit = 0;

static void set_subpage(int page);

/* playback */
static int last_note = 61;              /* C-5 */

/* strange saved envelopes */
static song_envelope_t saved_env[10];
static unsigned int flags[10] = {0};

/* --------------------------------------------------------------------------------------------------------- */

static void save_envelope(int slot, song_envelope_t *e, unsigned int sec)
{
	song_instrument_t *ins;

	slot = ((unsigned)slot)%10;

	ins = song_get_instrument(current_instrument);
	memcpy(&saved_env[slot], e, sizeof(song_envelope_t));

	switch (sec) {
	case ENV_VOLUME:
		flags[slot] = ins->flags & (ENV_VOLUME|ENV_VOLSUSTAIN|ENV_VOLLOOP|ENV_VOLCARRY);
		break;
	case ENV_PANNING:
		flags[slot] =
			((ins->flags & ENV_PANNING) ? ENV_VOLUME : 0)
		|       ((ins->flags & ENV_PANSUSTAIN) ? ENV_VOLSUSTAIN : 0)
		|       ((ins->flags & ENV_PANLOOP) ? ENV_VOLLOOP : 0)
		|       ((ins->flags & ENV_PANCARRY) ? ENV_VOLCARRY : 0)
		|       (ins->flags & ENV_SETPANNING);
		break;
	case ENV_PITCH:
		flags[slot] =
			((ins->flags & ENV_PITCH) ? ENV_VOLUME : 0)
		|       ((ins->flags & ENV_PITCHSUSTAIN) ? ENV_VOLSUSTAIN : 0)
		|       ((ins->flags & ENV_PITCHLOOP) ? ENV_VOLLOOP : 0)
		|       ((ins->flags & ENV_PITCHCARRY) ? ENV_VOLCARRY : 0)
		|       (ins->flags & ENV_FILTER);
		break;
	};
}
static void restore_envelope(int slot, song_envelope_t *e, unsigned int sec)
{
	song_instrument_t *ins;

	song_lock_audio();

	slot = ((unsigned)slot)%10;

	ins = song_get_instrument(current_instrument);
	memcpy(e, &saved_env[slot], sizeof(song_envelope_t));

	switch (sec) {
	case ENV_VOLUME:
		ins->flags &= ~(ENV_VOLUME|ENV_VOLSUSTAIN|ENV_VOLLOOP|ENV_VOLCARRY);
		ins->flags |= (flags[slot] & (ENV_VOLUME|ENV_VOLSUSTAIN|ENV_VOLLOOP|ENV_VOLCARRY));
		break;

	case ENV_PANNING:
		ins->flags &= ~(ENV_PANNING|ENV_PANSUSTAIN|ENV_PANLOOP|ENV_PANCARRY|ENV_SETPANNING);
		if (flags[slot] & ENV_VOLUME) ins->flags |= ENV_PANNING;
		if (flags[slot] & ENV_VOLSUSTAIN) ins->flags |= ENV_PANSUSTAIN;
		if (flags[slot] & ENV_VOLLOOP) ins->flags |= ENV_PANLOOP;
		if (flags[slot] & ENV_VOLCARRY) ins->flags |= ENV_PANCARRY;
		ins->flags |= (flags[slot] & ENV_SETPANNING);
		break;

	case ENV_PITCH:
		ins->flags &= ~(ENV_PITCH|ENV_PITCHSUSTAIN|ENV_PITCHLOOP|ENV_PITCHCARRY|ENV_FILTER);
		if (flags[slot] & ENV_VOLUME) ins->flags |= ENV_PITCH;
		if (flags[slot] & ENV_VOLSUSTAIN) ins->flags |= ENV_PITCHSUSTAIN;
		if (flags[slot] & ENV_VOLLOOP) ins->flags |= ENV_PITCHLOOP;
		if (flags[slot] & ENV_VOLCARRY) ins->flags |= ENV_PITCHCARRY;
		ins->flags |= (flags[slot] & ENV_FILTER);
		break;

	};

	song_unlock_audio();

	status.flags |= SONG_NEEDS_SAVE;
}



/* --------------------------------------------------------------------------------------------------------- */

static void instrument_list_draw_list(void);

/* --------------------------------------------------------------------------------------------------------- */
static int _last_vis_inst(void)
{
	int i, j, n;

	n = 99;
	j = 0;
	/* 65 is first visible sample on last page */
	for (i = 65; i < MAX_INSTRUMENTS; i++) {
		if (!csf_instrument_is_empty(current_song->instruments[i])) {
			j = i;
		}
	}
	while ((j + 34) > n)
		n += 34;
	return MIN(n, MAX_INSTRUMENTS - 1);
}
/* the actual list */

static void instrument_list_reposition(void)
{
	if (current_instrument < top_instrument) {
		top_instrument = current_instrument;
		if (top_instrument < 1) {
			top_instrument = 1;
		}
	} else if (current_instrument > top_instrument + 34) {
		top_instrument = current_instrument - 34;
	}
}

int instrument_get_current(void)
{
	return current_instrument;
}

void instrument_set(int n)
{
	int new_ins = n;
	song_instrument_t *ins;

	if (page_is_instrument_list(status.current_page)) {
		new_ins = CLAMP(n, 1, _last_vis_inst());
	} else {
		new_ins = CLAMP(n, 0, _last_vis_inst());
	}

	if (current_instrument == new_ins)
		return;

	envelope_edit_mode = 0;
	current_instrument = new_ins;
	instrument_list_reposition();

	ins = song_get_instrument(current_instrument);

	current_node_vol = ins->vol_env.nodes ? CLAMP(current_node_vol, 0, ins->vol_env.nodes - 1) : 0;
	current_node_pan = ins->pan_env.nodes ? CLAMP(current_node_vol, 0, ins->pan_env.nodes - 1) : 0;
	current_node_pitch = ins->pitch_env.nodes ? CLAMP(current_node_vol, 0, ins->pan_env.nodes - 1) : 0;

	status.flags |= NEED_UPDATE;
}

void instrument_synchronize_to_sample(void)
{
	song_instrument_t *ins;
	int sample = sample_get_current();
	int n, pos;

	/* 1. if the instrument with the same number as the current sample
	 * has the sample in its sample_map, change to that instrument. */
	ins = song_get_instrument(sample);
	for (pos = 0; pos < 120; pos++) {
		if ((int)(ins->sample_map[pos]) == sample) {
			instrument_set(sample);
			return;
		}
	}

	/* 2. look through the instrument list for the first instrument
	 * that uses the selected sample. */
	for (n = 1; n < 100; n++) {
		if (n == sample)
			continue;
		ins = song_get_instrument(n);
		for (pos = 0; pos < 120; pos++) {
			if ((int)(ins->sample_map[pos]) == sample) {
				instrument_set(n);
				return;
			}
		}
	}

	/* 3. if no instruments are using the sample, just change to the
	 * same-numbered instrument. */
	instrument_set(sample);
}

/* --------------------------------------------------------------------- */

static int instrument_list_add_char(int c)
{
	song_instrument_t *ins;

	if (c < 32)
		return 0;
	ins = song_get_instrument(current_instrument);
	text_add_char(ins->name, c, &instrument_cursor_pos, 25);
	if (instrument_cursor_pos == 25)
		instrument_cursor_pos--;

	get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
	return 1;
}

static void instrument_list_delete_char(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	text_delete_char(ins->name, &instrument_cursor_pos, 25);

	get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

static void instrument_list_delete_next_char(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	text_delete_next_char(ins->name, &instrument_cursor_pos, 25);

	get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

static void clear_instrument_text(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	memset(ins->filename, 0, 14);
	memset(ins->name, 0, 26);
	if (instrument_cursor_pos != 25)
		instrument_cursor_pos = 0;

	get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------- */

static void do_swap_instrument(int n)
{
	if (n >= 1 && n <= _last_vis_inst()) {
		song_swap_instruments(current_instrument, n);
	}
}

static void do_exchange_instrument(int n)
{
	if (n >= 1 && n <= _last_vis_inst()) {
		song_exchange_instruments(current_instrument, n);
	}
}

static void do_copy_instrument(int n)
{
	if (n >= 1 && n <= _last_vis_inst()) {
		song_copy_instrument(current_instrument, n);
	}
}

static void do_replace_instrument(int n)
{
	if (n >= 1 && n <= _last_vis_inst()) {
		song_replace_instrument(current_instrument, n);
	}
}

/* --------------------------------------------------------------------- */

static void instrument_list_draw_list(void)
{
	int pos, n;
	song_instrument_t *ins;
	int selected = (ACTIVE_PAGE.selected_widget == 0);
	int is_current;
	int ss, cl = 0, cr = 0;
	int is_playing[MAX_INSTRUMENTS];
	char buf[4];

	ss = -1;

	song_get_playing_instruments(is_playing);

	for (pos = 0, n = top_instrument; pos < 35; pos++, n++) {
		ins = song_get_instrument(n);
		is_current = (n == current_instrument);

		if (ins->played)
			draw_char(is_playing[n] > 1 ? 183 : 173, 1, 13 + pos, is_playing[n] ? 3 : 1, 2);

		draw_text(str_from_num99(n, buf), 2, 13 + pos, 0, 2);
		if (instrument_cursor_pos < 25) {
			/* it's in edit mode */
			if (is_current) {
				draw_text_len(ins->name, 25, 5, 13 + pos, 6, 14);
				if (selected) {
					draw_char(ins->name[instrument_cursor_pos],
						  5 + instrument_cursor_pos,
						  13 + pos, 0, 3);
				}
			} else {
				draw_text_len(ins->name, 25, 5, 13 + pos, 6, 0);
			}
		} else {
			draw_text_len(ins->name, 25, 5, 13 + pos,
				      ((is_current && selected) ? 0 : 6),
				      (is_current ? (selected ? 3 : 14) : 0));
		}
		if (ss == n) {
			draw_text_len(ins->name + cl, (cr-cl)+1,
					5 + cl, 13 + pos,
					(is_current ? 3 : 11), 8);
		}
	}
}

static int instrument_list_handle_text_input_on_list(const char* text) {
	int success = 0;
	for (; *text; text++)
		if (instrument_cursor_pos < 25 && instrument_list_add_char(*(unsigned char *)text))
			success = 1;
	return success;
}

static int instrument_list_handle_key_on_list(struct key_event * k)
{
	int new_ins = current_instrument;

	if (k->state == KEY_PRESS && k->mouse != MOUSE_NONE && k->y >= 13 && k->y <= 47 && k->x >= 5 && k->x <= 30) {
		if (k->mouse == MOUSE_CLICK) {
			new_ins = (k->y - 13) + top_instrument;
			if (instrument_cursor_pos < 25)
				instrument_cursor_pos = MIN(k->x - 5, 24);
			status.flags |= NEED_UPDATE;
		} else if (k->mouse == MOUSE_DBLCLICK) {
			/* this doesn't seem to work, but I think it'd be
			more useful if double click switched to edit mode */
			if (instrument_cursor_pos < 25) {
				instrument_cursor_pos = 25;
				get_page_widgets()->accept_text = 0;
			} else {
				set_page(PAGE_LOAD_INSTRUMENT);
			}
			status.flags |= NEED_UPDATE;
			return 1;

		} else if (k->mouse == MOUSE_SCROLL_UP) {
			top_instrument -= MOUSE_SCROLL_LINES;
			if (top_instrument < 1) top_instrument = 1;
			status.flags |= NEED_UPDATE;
			return 1;
		} else if (k->mouse == MOUSE_SCROLL_DOWN) {
			top_instrument += MOUSE_SCROLL_LINES;
			if (top_instrument > (_last_vis_inst()-34)) top_instrument = _last_vis_inst()-34;
			status.flags |= NEED_UPDATE;
			return 1;
		}
	} else {
		switch (k->sym) {
		case SCHISM_KEYSYM_UP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_ALT) {
				if (current_instrument > 1) {
					new_ins = current_instrument - 1;
					song_swap_instruments(current_instrument, new_ins);
				}
			} else if (!NO_MODIFIER(k->mod)) {
				return 0;
			} else {
				new_ins--;
			}
			break;
		case SCHISM_KEYSYM_DOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_ALT) {
				// restrict position to the "old" value of _last_vis_inst()
				// (this is entirely for aesthetic reasons)
				if (status.last_keysym != SCHISM_KEYSYM_DOWN && !k->is_repeat)
					_altswap_lastvis = _last_vis_inst();
				if (current_instrument < _altswap_lastvis) {
					new_ins = current_instrument + 1;
					song_swap_instruments(current_instrument, new_ins);
				}
			} else if (!NO_MODIFIER(k->mod)) {
				return 0;
			} else {
				new_ins++;
			}
			break;
		case SCHISM_KEYSYM_PAGEUP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_CTRL)
				new_ins = 1;
			else
				new_ins -= 16;
			break;
		case SCHISM_KEYSYM_PAGEDOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_CTRL)
				new_ins = _last_vis_inst();
			else
				new_ins += 16;
			break;
		case SCHISM_KEYSYM_HOME:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (instrument_cursor_pos < 25) {
				instrument_cursor_pos = 0;
				get_page_widgets()->accept_text = 1;
				status.flags |= NEED_UPDATE;
			}
			return 1;
		case SCHISM_KEYSYM_END:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (instrument_cursor_pos < 24) {
				instrument_cursor_pos = 24;
				get_page_widgets()->accept_text = 1;
				status.flags |= NEED_UPDATE;
			}
			return 1;
		case SCHISM_KEYSYM_LEFT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (instrument_cursor_pos < 25 && instrument_cursor_pos > 0) {
				instrument_cursor_pos--;
				get_page_widgets()->accept_text = 1;
				status.flags |= NEED_UPDATE;
			}
			return 1;
		case SCHISM_KEYSYM_RIGHT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (instrument_cursor_pos == 25) {
				get_page_widgets()->accept_text = 0;
				widget_change_focus_to(1);
			} else if (instrument_cursor_pos < 24) {
				get_page_widgets()->accept_text = 1;
				instrument_cursor_pos++;
				status.flags |= NEED_UPDATE;
			}
			return 1;
		case SCHISM_KEYSYM_RETURN:
			if (k->state == KEY_PRESS)
				return 0;
			if (instrument_cursor_pos < 25) {
				instrument_cursor_pos = 25;
				get_page_widgets()->accept_text = 0;
				status.flags |= NEED_UPDATE;
			} else {
				get_page_widgets()->accept_text = 1;
				set_page(PAGE_LOAD_INSTRUMENT);
			}
			return 1;
		case SCHISM_KEYSYM_ESCAPE:
			if ((k->mod & SCHISM_KEYMOD_SHIFT) || instrument_cursor_pos < 25) {
				if (k->state == KEY_RELEASE)
					return 1;
				instrument_cursor_pos = 25;
				get_page_widgets()->accept_text = 0;
				status.flags |= NEED_UPDATE;
				return 1;
			}
			return 0;
		case SCHISM_KEYSYM_BACKSPACE:
			if (k->state == KEY_RELEASE)
				return 0;
			if (instrument_cursor_pos == 25)
				return 0;
			if ((k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT)) == 0)
				instrument_list_delete_char();
			else if (k->mod & SCHISM_KEYMOD_CTRL)
				instrument_list_add_char(127);
			return 1;
		case SCHISM_KEYSYM_INSERT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_ALT) {
				song_insert_instrument_slot(current_instrument);
				status.flags |= NEED_UPDATE;
				return 1;
			}
			return 0;
		case SCHISM_KEYSYM_DELETE:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_ALT) {
				song_remove_instrument_slot(current_instrument);
				status.flags |= NEED_UPDATE;
				return 1;
			} else if ((k->mod & SCHISM_KEYMOD_CTRL) == 0) {
				if (instrument_cursor_pos == 25)
					return 0;
				instrument_list_delete_next_char();
				return 1;
			}
			return 0;
		default:
			if (k->state == KEY_RELEASE)
				return 0;

			if (k->mod & SCHISM_KEYMOD_ALT) {
				if (k->sym == SCHISM_KEYSYM_c) {
					clear_instrument_text();
					return 1;
				}
			} else if ((k->mod & SCHISM_KEYMOD_CTRL) == 0) {
				if (instrument_cursor_pos < 25) {
					if (k->text)
						return instrument_list_handle_text_input_on_list(k->text);
				} else if (k->sym == SCHISM_KEYSYM_SPACE) {
					instrument_cursor_pos = 0;
					get_page_widgets()->accept_text = 0;
					status.flags |= NEED_UPDATE;
					memused_songchanged();
					return 1;
				}
			}

			return 0;
		};
	}

	new_ins = CLAMP(new_ins, 1, _last_vis_inst());
	if (new_ins != current_instrument) {
		instrument_set(new_ins);
		status.flags |= NEED_UPDATE;
		memused_songchanged();
	}

	return 1;
}

/* --------------------------------------------------------------------- */
/* note translation table */

static void note_trans_reposition(void)
{
	if (note_trans_sel_line < note_trans_top_line) {
		note_trans_top_line = note_trans_sel_line;
	} else if (note_trans_sel_line > note_trans_top_line + 31) {
		note_trans_top_line = note_trans_sel_line - 31;
	}
}

static void note_trans_draw(void)
{
	int pos, n;
	int is_selected = (ACTIVE_PAGE.selected_widget == 5);
	int bg, sel_bg = (is_selected ? 14 : 0);
	song_instrument_t *ins = song_get_instrument(current_instrument);
	char buf[4];

	for (pos = 0, n = note_trans_top_line; pos < 32; pos++, n++) {
		bg = ((n == note_trans_sel_line) ? sel_bg : 0);

		/* invalid notes are translated to themselves (and yes, this edits the actual instrument) */
		if (ins->note_map[n] < 1 || ins->note_map[n] > 120)
			ins->note_map[n] = n + 1;

		draw_text(get_note_string(n + 1, buf), 32, 16 + pos, 2, bg);
		draw_char(168, 35, 16 + pos, 2, bg);
		draw_text(get_note_string(ins->note_map[n], buf), 36, 16 + pos, 2, bg);
		if (is_selected && n == note_trans_sel_line) {
			if (note_trans_cursor_pos == 0)
				draw_char(buf[0], 36, 16 + pos, 0, 3);
			else if (note_trans_cursor_pos == 1)
				draw_char(buf[2], 38, 16 + pos, 0, 3);
		}
		draw_char(0, 39, 16 + pos, 2, bg);
		if (ins->sample_map[n]) {
			str_from_num99(ins->sample_map[n], buf);
		} else {
			buf[0] = buf[1] = '\xAD';
			buf[2] = 0;
		}
		draw_text(buf, 40, 16 + pos, 2, bg);
		if (is_selected && n == note_trans_sel_line) {
			if (note_trans_cursor_pos == 2)
				draw_char(buf[0], 40, 16 + pos, 0, 3);
			else if (note_trans_cursor_pos == 3)
				draw_char(buf[1], 41, 16 + pos, 0, 3);
		}
	}

	/* draw the little mask thingy at the bottom. Could optimize this....  -delt.
	   Sure can! This could share the same track-view functions that the
	   pattern editor ought to be using. -Storlek */
	if (is_selected && !(status.flags & CLASSIC_MODE)) {
		switch (note_trans_cursor_pos) {
		case 0:
			draw_char(171, 36, 48, 3, 2);
			draw_char(171, 37, 48, 3, 2);
			draw_char(169, 38, 48, 3, 2);
			if (note_sample_mask) {
				draw_char(169, 40, 48, 3, 2);
				draw_char(169, 41, 48, 3, 2);
			}
			break;
		case 1:
			draw_char(169, 38, 48, 3, 2);
			if (note_sample_mask) {
				draw_char(170, 40, 48, 3, 2);
				draw_char(170, 41, 48, 3, 2);
			}
			break;
		case 2:
		case 3:
			draw_char(note_sample_mask ? 171 : 169, 40, 48, 3, 2);
			draw_char(note_sample_mask ? 171 : 169, 41, 48, 3, 2);
			break;
		};
	}
}

static void instrument_note_trans_transpose(song_instrument_t *ins, int dir)
{
	int i;
	for (i = 0; i < 120; i++) {
		ins->note_map[i] = CLAMP(ins->note_map[i]+dir, 1, 120);
	}
}

static void instrument_note_trans_insert(song_instrument_t *ins, int pos)
{
	int i;
	for (i = 119; i > pos; i--) {
		ins->note_map[i] = ins->note_map[i-1];
		ins->sample_map[i] = ins->sample_map[i-1];
	}
	if (pos) {
		ins->note_map[pos] = ins->note_map[pos-1]+1;
	} else {
		ins->note_map[0] = 1;
	}
}

static void instrument_note_trans_delete(song_instrument_t *ins, int pos)
{
	int i;
	for (i = pos; i < 120; i++) {
		ins->note_map[i] = ins->note_map[i+1];
		ins->sample_map[i] = ins->sample_map[i+1];
	}
	ins->note_map[119] = ins->note_map[118]+1;
}

static int note_trans_handle_key(struct key_event * k)
{
	int prev_line = note_trans_sel_line;
	int new_line = prev_line;
	int prev_pos = note_trans_cursor_pos;
	int new_pos = prev_pos;
	song_instrument_t *ins = song_get_instrument(current_instrument);
	int c, n;

	if (k->mouse == MOUSE_CLICK && k->mouse_button == MOUSE_BUTTON_MIDDLE) {
		if (k->state == KEY_RELEASE)
			status.flags |= CLIPPY_PASTE_SELECTION;
		return 1;
	} else if (k->mouse == MOUSE_SCROLL_UP || k->mouse == MOUSE_SCROLL_DOWN) {
		if (k->state == KEY_PRESS) {
			note_trans_top_line += (k->mouse == MOUSE_SCROLL_UP) ? -3 : 3;
			note_trans_top_line = CLAMP(note_trans_top_line, 0, 119 - 31);
			status.flags |= NEED_UPDATE;
		}
		return 1;
	} else if (k->mouse != MOUSE_NONE) {
		if (k->x >= 32 && k->x <= 41 && k->y >= 16 && k->y <= 47) {
			new_line = note_trans_top_line + k->y - 16;
			if (new_line == prev_line) {
				switch (k->x - 36) {
				case 2:
					new_pos = 1;
					break;
				case 4:
					new_pos = 2;
					break;
				case 5:
					new_pos = 3;
					break;
				default:
					new_pos = 0;
					break;
				};
			}
		}
	} else if (k->mod & SCHISM_KEYMOD_ALT) {
		if (k->state == KEY_RELEASE)
			return 0;
		switch (k->sym) {
		case SCHISM_KEYSYM_UP:
			instrument_note_trans_transpose(ins, 1);
			break;
		case SCHISM_KEYSYM_DOWN:
			instrument_note_trans_transpose(ins, -1);
			break;
		case SCHISM_KEYSYM_INSERT:
			instrument_note_trans_insert(ins, note_trans_sel_line);
			break;
		case SCHISM_KEYSYM_DELETE:
			instrument_note_trans_delete(ins, note_trans_sel_line);
			break;
		case SCHISM_KEYSYM_n:
			n = note_trans_sel_line - 1; // the line to copy *from*
			if (n < 0 || ins->note_map[n] == NOTE_LAST)
				break;
			ins->note_map[note_trans_sel_line] = ins->note_map[n] + 1;
			ins->sample_map[note_trans_sel_line] = ins->sample_map[n];
			new_line++;
			break;
		case SCHISM_KEYSYM_p:
			n = note_trans_sel_line + 1; // the line to copy *from*
			if (n > (NOTE_LAST - NOTE_FIRST) || ins->note_map[n] == NOTE_FIRST)
				break;
			ins->note_map[note_trans_sel_line] = ins->note_map[n] - 1;
			ins->sample_map[note_trans_sel_line] = ins->sample_map[n];
			new_line--;
			break;
		case SCHISM_KEYSYM_a:
			c = sample_get_current();
			for (n = 0; n < (NOTE_LAST - NOTE_FIRST + 1); n++)
				ins->sample_map[n] = c;
			if (k->mod & SCHISM_KEYMOD_SHIFT) {
				// Copy the name too.
				memcpy(ins->name, current_song->samples[c].name, 32);
			}
			break;
		default:
			return 0;
		}
	} else {
		switch (k->sym) {
		case SCHISM_KEYSYM_UP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_CTRL)
				sample_set(sample_get_current () - 1);
			if (!NO_MODIFIER(k->mod))
				return 0;
			if (--new_line < 0) {
				widget_change_focus_to(1);
				return 1;
			}
			break;
		case SCHISM_KEYSYM_DOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_CTRL)
				sample_set(sample_get_current () + 1);
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_line++;
			break;
		case SCHISM_KEYSYM_PAGEUP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_CTRL) {
				instrument_set(current_instrument - 1);
				return 1;
			}
			new_line -= 16;
			break;
		case SCHISM_KEYSYM_PAGEDOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_CTRL) {
				instrument_set(current_instrument + 1);
				return 1;
			}
			new_line += 16;
			break;
		case SCHISM_KEYSYM_HOME:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_line = 0;
			break;
		case SCHISM_KEYSYM_END:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_line = 119;
			break;
		case SCHISM_KEYSYM_LEFT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_pos--;
			break;
		case SCHISM_KEYSYM_RIGHT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_pos++;
			break;
		case SCHISM_KEYSYM_RETURN:
			if (k->state == KEY_PRESS)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			sample_set(ins->sample_map[note_trans_sel_line]);
			get_page_widgets()->accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
			return 1;
		case SCHISM_KEYSYM_LESS:
		case SCHISM_KEYSYM_SEMICOLON:
		case SCHISM_KEYSYM_COLON:
			if (k->state == KEY_RELEASE)
				return 0;
			sample_set(sample_get_current() - 1);
			return 1;
		case SCHISM_KEYSYM_GREATER:
		case SCHISM_KEYSYM_QUOTE:
		case SCHISM_KEYSYM_QUOTEDBL:
			if (k->state == KEY_RELEASE)
				return 0;
			sample_set(sample_get_current() + 1);
			return 1;

		default:
			if (k->state == KEY_RELEASE)
				return 0;
			switch (note_trans_cursor_pos) {
			case 0:        /* note */
				n = kbd_get_note(k);
				if (!NOTE_IS_NOTE(n))
					return 0;
				ins->note_map[note_trans_sel_line] = n;
				if (note_sample_mask || (status.flags & CLASSIC_MODE))
					ins->sample_map[note_trans_sel_line] = sample_get_current();
				new_line++;
				break;
			case 1:        /* octave */
				c = kbd_char_to_hex(k);
				if (c < 0 || c > 9) return 0;
				n = ins->note_map[note_trans_sel_line];
				n = ((n - 1) % 12) + (12 * c) + 1;
				ins->note_map[note_trans_sel_line] = n;
				new_line++;
				break;

				/* Made it possible to enter H to R letters
				on 1st digit for expanded sample slots.  -delt. */

			case 2:        /* instrument, first digit */
			case 3:        /* instrument, second digit */
				if (k->sym == SCHISM_KEYSYM_SPACE) {
					ins->sample_map[note_trans_sel_line] =
						sample_get_current();
					new_line++;
					break;
				}

				if ((k->sym == SCHISM_KEYSYM_PERIOD && NO_MODIFIER(k->mod)) || k->sym == SCHISM_KEYSYM_DELETE) {
					ins->sample_map[note_trans_sel_line] = 0;
					new_line += (k->sym == SCHISM_KEYSYM_PERIOD) ? 1 : 0;
					break;
				}
				if (k->sym == SCHISM_KEYSYM_COMMA && NO_MODIFIER(k->mod)) {
					note_sample_mask = note_sample_mask ? 0 : 1;
					break;
				}

				n = ins->sample_map[note_trans_sel_line];
				if (note_trans_cursor_pos == 2) {
					c = kbd_char_to_99(k);
					if (c < 0) return 0;
					n = (c * 10) + (n % 10);
					new_pos++;
				} else {
					c = kbd_char_to_hex(k);
					if (c < 0 || c > 9) return 0;
					n = ((n / 10) * 10) + c;
					new_pos--;
					new_line++;
				}
				n = MIN(n, MAX_SAMPLES - 1);
				ins->sample_map[note_trans_sel_line] = n;
				sample_set(n);
				break;
			}
			break;
		}
	}

	new_line = CLAMP(new_line, 0, 119);
	note_trans_cursor_pos = CLAMP(new_pos, 0, 3);
	if (new_line != prev_line) {
		note_trans_sel_line = new_line;
		note_trans_reposition();
	}

	/* this causes unneeded redraws in some cases... oh well :P */
	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */
/* envelope helper functions */

static void _env_draw_axes(int middle)
{
	int n, y = middle ? 31 : 62;
	for (n = 0; n < 64; n += 2)
		vgamem_ovl_drawpixel(&env_overlay, 3, n, 12);
	for (n = 0; n < 256; n += 2)
		vgamem_ovl_drawpixel(&env_overlay, 1 + n, y, 12);
}

static void _env_draw_node(int x, int y, int on)
{
	int c = (status.flags & CLASSIC_MODE) ? 12 : 5;

	vgamem_ovl_drawpixel(&env_overlay, x - 1, y - 1, c);
	vgamem_ovl_drawpixel(&env_overlay, x - 1, y, c);
	vgamem_ovl_drawpixel(&env_overlay, x - 1, y + 1, c);

	vgamem_ovl_drawpixel(&env_overlay, x, y - 1, c);
	vgamem_ovl_drawpixel(&env_overlay, x, y, c);
	vgamem_ovl_drawpixel(&env_overlay, x, y + 1, c);

	vgamem_ovl_drawpixel(&env_overlay, x + 1, y - 1,c);
	vgamem_ovl_drawpixel(&env_overlay, x + 1, y,c);
	vgamem_ovl_drawpixel(&env_overlay, x + 1, y + 1,c);

	if (on) {
		vgamem_ovl_drawpixel(&env_overlay, x - 3, y - 1,c);
		vgamem_ovl_drawpixel(&env_overlay, x - 3, y,c);
		vgamem_ovl_drawpixel(&env_overlay, x - 3, y + 1,c);

		vgamem_ovl_drawpixel(&env_overlay, x + 3, y - 1,c);
		vgamem_ovl_drawpixel(&env_overlay, x + 3, y,c);
		vgamem_ovl_drawpixel(&env_overlay, x + 3, y + 1,c);
	}
}

static void _env_draw_loop(int xs, int xe, int sustain)
{
	int y = 0;
	int c = (status.flags & CLASSIC_MODE) ? 12 : 3;

	if (sustain) {
		while (y < 62) {
			/* unrolled once */
			vgamem_ovl_drawpixel(&env_overlay, xs, y, c);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, c); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, 0);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, 0); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, c);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, c); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, 0);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, 0); y++;
		}
	} else {
		while (y < 62) {
			vgamem_ovl_drawpixel(&env_overlay, xs, y, 0);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, 0); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, c);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, c); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, c);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, c); y++;
			vgamem_ovl_drawpixel(&env_overlay, xs, y, 0);
			vgamem_ovl_drawpixel(&env_overlay, xe, y, 0); y++;
		}
	}
}

static void _env_draw(const song_envelope_t *env, int middle, int current_node,
			int env_on, int loop_on, int sustain_on, int env_num)
{
	song_voice_t *channel;
	uint32_t *channel_list;
	char buf[16];
	unsigned int envpos[3];
	int x, y, n, m, c;
	int last_x = 0, last_y = 0;
	int max_ticks = 50;

	while (env->ticks[env->nodes - 1] >= max_ticks)
		max_ticks *= 2;

	vgamem_ovl_clear(&env_overlay, 0);

	/* draw the axis lines */
	_env_draw_axes(middle);

	for (n = 0; n < env->nodes; n++) {
		x = 4 + env->ticks[n] * 256 / max_ticks;

		/* 65 values are being crammed into 62 pixels => have to lose three pixels somewhere.
		 * This is where IT compromises -- I don't quite get how the lines are drawn, though,
		 * because it changes for each value... (apart from drawing 63 and 64 the same way) */
		y = env->values[n];
		if (y > 63) y--;
		if (y > 42) y--;
		if (y > 21) y--;
		y = 62 - y;

		_env_draw_node(x, y, n == current_node);

		if (last_x)
			vgamem_ovl_drawline(&env_overlay,
				last_x, last_y, x, y, 12);

		last_x = x;
		last_y = y;
	}

	if (sustain_on)
		_env_draw_loop(4 + env->ticks[env->sustain_start] * 256 / max_ticks,
			       4 + env->ticks[env->sustain_end] * 256 / max_ticks, 1);
	if (loop_on)
		_env_draw_loop(4 + env->ticks[env->loop_start] * 256 / max_ticks,
			       4 + env->ticks[env->loop_end] * 256 / max_ticks, 0);

	if (env_on) {
		max_ticks = env->ticks[env->nodes-1];
		m = max_ticks ? song_get_mix_state(&channel_list) : 0;
		while (m--) {
			channel = song_get_mix_channel(channel_list[m]);
			if (channel->ptr_instrument != song_get_instrument(current_instrument))
				continue;

			envpos[0] = channel->vol_env_position;
			envpos[1] = channel->pan_env_position;
			envpos[2] = channel->pitch_env_position;

			x = 4 + (envpos[env_num] * (last_x-4) / max_ticks);
			if (x > last_x)
				x = last_x;
			c =  (status.flags & CLASSIC_MODE)
				? 12
				: ((channel->flags & (CHN_KEYOFF | CHN_NOTEFADE)) ? 8 : 6);
			for (y = 0; y < 62; y++)
				vgamem_ovl_drawpixel(&env_overlay, x, y, c);
		}
	}

	draw_fill_chars(65, 18, 76, 25, DEFAULT_FG, 0);
	vgamem_ovl_apply(&env_overlay);

	sprintf(buf, "Node %d/%d", current_node, env->nodes);
	draw_text(buf, 66, 19, 2, 0);
	sprintf(buf, "Tick %d", env->ticks[current_node]);
	draw_text(buf, 66, 21, 2, 0);
	sprintf(buf, "Value %d", (int)(env->values[current_node] - (middle ? 32 : 0)));
	draw_text(buf, 66, 23, 2, 0);
}

/* return: the new current node */
static int _env_node_add(song_envelope_t *env, int current_node, int override_tick, int override_value)
{
	int newtick, newvalue;

	status.flags |= SONG_NEEDS_SAVE;

	if (env->nodes > 24 || current_node == env->nodes - 1)
		return current_node;

	newtick = (env->ticks[current_node] + env->ticks[current_node + 1]) / 2;
	newvalue = (env->values[current_node] + env->values[current_node + 1]) / 2;
	if (override_tick > -1 && override_value > -1) {
		newtick = override_tick;
		newvalue = override_value;
	} else if (newtick == env->ticks[current_node] || newtick == env->ticks[current_node + 1]) {
		printf("Not enough room!\n");
		return current_node;
	}

	env->nodes++;
	memmove(env->ticks + current_node + 1, env->ticks + current_node,
		(env->nodes - current_node - 1) * sizeof(env->ticks[0]));
	memmove(env->values + current_node + 1, env->values + current_node,
		(env->nodes - current_node - 1) * sizeof(env->values[0]));
	env->ticks[current_node + 1] = newtick;
	env->values[current_node + 1] = newvalue;
	if (env->loop_end > current_node) env->loop_end++;
	if (env->loop_start > current_node) env->loop_start++;
	if (env->sustain_end > current_node) env->sustain_end++;
	if (env->sustain_start > current_node) env->sustain_start++;

	return current_node;
}

/* return: the new current node */
static int _env_node_remove(song_envelope_t *env, int current_node)
{
	status.flags |= SONG_NEEDS_SAVE;

	if (current_node == 0 || env->nodes < 3)
		return current_node;

	memmove(env->ticks + current_node, env->ticks + current_node + 1,
		(env->nodes - current_node - 1) * sizeof(env->ticks[0]));
	memmove(env->values + current_node, env->values + current_node + 1,
		(env->nodes - current_node - 1) * sizeof(env->values[0]));
	env->nodes--;

	if (env->loop_start >= env->nodes)
		env->loop_start = env->nodes - 1;
	else if (env->loop_start > current_node)
		env->loop_start--;
	if (env->loop_end >= env->nodes)
		env->loop_end = env->nodes - 1;
	else if (env->loop_end > current_node)
		env->loop_end--;
	if (env->sustain_start >= env->nodes)
		env->sustain_start = env->nodes - 1;
	else if (env->sustain_start > current_node)
		env->sustain_start--;
	if (env->sustain_end >= env->nodes)
		env->sustain_end = env->nodes - 1;
	else if (env->sustain_end > current_node)
		env->sustain_end--;
	if (current_node >= env->nodes)
		current_node = env->nodes - 1;

	return current_node;
}

static void do_pre_loop_cut(void *ign)
{
	song_envelope_t *env = (song_envelope_t *)ign;
	unsigned int bt;
	int i;
	bt = env->ticks[env->loop_start];
	for (i = env->loop_start; i < 32; i++) {
		env->ticks[i - env->loop_start] = env->ticks[i] - bt;
		env->values[i - env->loop_start] = env->values[i];
	}
	env->nodes -= env->loop_start;
	if (env->sustain_start > env->loop_start) {
		env->sustain_start -= env->loop_start;
	} else {
		env->sustain_start = 0;
	}
	if (env->sustain_end > env->loop_start) {
		env->sustain_end -= env->loop_start;
	} else {
		env->sustain_end = 0;
	}
	if (env->loop_end > env->loop_start) {
		env->loop_end -= env->loop_start;
	} else {
		env->loop_end = 0;
	}
	env->loop_start = 0;
	if (env->loop_start > env->loop_end)
		env->loop_end = env->loop_start;
	if (env->sustain_start > env->sustain_end)
		env->sustain_end = env->sustain_start;
	status.flags |= NEED_UPDATE;
}

static void do_post_loop_cut(void *ign)
{
	song_envelope_t *env = (song_envelope_t *)ign;
	env->nodes = env->loop_end+1;
}


static void env_resize(song_envelope_t *env, int ticks)
{
	int old = env->ticks[env->nodes - 1];
	int n, t;

	if (ticks > 9999)
		ticks = 9999;
	for (n = 1; n < env->nodes; n++) {
		t = env->ticks[n] * ticks / old;
		env->ticks[n] = MAX(t, env->ticks[n - 1] + 1);
	}
	status.flags |= NEED_UPDATE;
}


static struct widget env_resize_widgets[2];
static int env_resize_cursor;

static void do_env_resize(void *data)
{
	env_resize((song_envelope_t *) data, env_resize_widgets[0].d.numentry.value);
}

static void env_resize_draw_const(void)
{
	draw_text("Resize Envelope", 34, 24, 3, 2);
	draw_text("New Length", 31, 27, 0, 2);
	draw_box(41, 26, 49, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void env_resize_dialog(song_envelope_t *env)
{
	struct dialog *dialog;

	env_resize_cursor = 0;
	widget_create_numentry(env_resize_widgets + 0, 42, 27, 7, 0, 1, 1, NULL, 0, 9999, &env_resize_cursor);
	env_resize_widgets[0].d.numentry.value = env->ticks[env->nodes - 1];
	widget_create_button(env_resize_widgets + 1, 36, 30, 6, 0, 1, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(26, 22, 29, 11, env_resize_widgets, 2, 0, env_resize_draw_const, env);
	dialog->action_yes = do_env_resize;
}



static struct widget env_adsr_widgets[4];
static int env_adsr_cursor = 0;

static void do_env_adsr(void *data)
{
	// FIXME | move env flags into the envelope itself, where they should be in the first place.
	// FIXME | then this nonsense can go away.
	song_instrument_t *ins = (song_instrument_t *) data;
	song_envelope_t *env = &ins->vol_env;
	int a = env_adsr_widgets[0].d.thumbbar.value;
	int d = env_adsr_widgets[1].d.thumbbar.value;
	int s = env_adsr_widgets[2].d.thumbbar.value;
	int r = env_adsr_widgets[3].d.thumbbar.value;
	int v1 = MAX(a, a * a / 16);
	int v2 = MAX(v1 + d * d / 16, v1 + d);
	int v3 = MAX(v2 + r * r / 4, v2 + r);
	int n = 0;

	if (a) {
		env->ticks[n] = 0;
		env->values[n++] = 0;
	}
	if (d) {
		env->ticks[n] = v1;
		env->values[n++] = 64;
	}
	env->sustain_start = env->sustain_end = n;
	env->ticks[n] = v2;
	env->values[n++] = s / 2;
	env->ticks[n] = v3;
	env->values[n++] = 0;
	env->nodes = n;
	for (n = 0; n < env->nodes - 1; n++)
		if (env->ticks[n] >= env->ticks[n + 1])
			env->ticks[n + 1] = env->ticks[n] + 1;
	ins->flags |= ENV_VOLSUSTAIN | ENV_VOLUME; // arghhhhh
}

static void env_adsr_draw_const(void)
{
	draw_text("Envelope Generator", 32, 22, 0, 2);
	draw_text("Attack", 27, 24, 0, 2);
	draw_text("Decay", 28, 25, 0, 2);
	draw_text("Sustain", 26, 26, 0, 2);
	draw_text("Release", 26, 27, 0, 2);

	draw_box(33, 23, 51, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void env_adsr_dialog(SCHISM_UNUSED song_envelope_t *env)
{
	struct dialog *dialog;
	song_instrument_t *ins = song_get_instrument(current_instrument); // ARGHHH

	env_adsr_cursor = 0;
	widget_create_thumbbar(env_adsr_widgets + 0, 34, 24, 17, 4, 1, 4, NULL, 0, 128);
	widget_create_thumbbar(env_adsr_widgets + 1, 34, 25, 17, 0, 2, 4, NULL, 0, 128);
	widget_create_thumbbar(env_adsr_widgets + 2, 34, 26, 17, 1, 3, 4, NULL, 0, 128);
	widget_create_thumbbar(env_adsr_widgets + 3, 34, 27, 17, 2, 4, 4, NULL, 0, 128);
	widget_create_button(env_adsr_widgets + 4, 36, 30, 6, 3, 0, 4, 4, 0, dialog_cancel_NULL, "Cancel", 1);

	dialog = dialog_create_custom(25, 21, 31, 12, env_adsr_widgets, 5, 0, env_adsr_draw_const, ins);
	dialog->action_yes = do_env_adsr;
}




/* the return value here is actually a bitmask:
r & 1 => the key was handled
r & 2 => the envelope changed (i.e., it should be enabled) */
static int _env_handle_key_viewmode(struct key_event *k, song_envelope_t *env, int *current_node, unsigned int sec)
{
	int new_node = *current_node;
	int n;

	switch (k->sym) {
	case SCHISM_KEYSYM_UP:
		if (k->state == KEY_RELEASE)
			return 0;
		widget_change_focus_to(1);
		return 1;
	case SCHISM_KEYSYM_DOWN:
		if (k->state == KEY_RELEASE)
			return 0;
		widget_change_focus_to(6);
		return 1;
	case SCHISM_KEYSYM_LEFT:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_node--;
		break;
	case SCHISM_KEYSYM_RIGHT:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_node++;
		break;
	case SCHISM_KEYSYM_INSERT:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		*current_node = _env_node_add(env, *current_node, -1, -1);
		status.flags |= NEED_UPDATE;
		return 1 | 2;
	case SCHISM_KEYSYM_DELETE:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		*current_node = _env_node_remove(env, *current_node);
		status.flags |= NEED_UPDATE;
		return 1 | 2;
	case SCHISM_KEYSYM_SPACE:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->is_repeat)
			return 1;

		if (k->state == KEY_PRESS) {
			song_keydown(KEYJAZZ_NOINST, current_instrument, last_note, 64, KEYJAZZ_CHAN_CURRENT);
			return 1;
		} else if (k->state == KEY_RELEASE) {
			song_keyup(KEYJAZZ_NOINST, current_instrument, last_note);
			return 1;
		}

		return 0;
	case SCHISM_KEYSYM_RETURN:
		if (k->state == KEY_PRESS)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		envelope_edit_mode = 1;
		status.flags |= NEED_UPDATE;
		return 1 | 2;
	case SCHISM_KEYSYM_l:
		if (k->state == KEY_PRESS)
			return 0;
		if (!(k->mod & SCHISM_KEYMOD_ALT)) return 0;
		if (env->loop_end < (env->nodes-1))  {
			dialog_create(DIALOG_OK_CANCEL, "Cut envelope?", do_post_loop_cut, NULL, 1, env);
			return 1;
		}
		return 0;
	case SCHISM_KEYSYM_b:
		if (k->state == KEY_PRESS)
			return 0;
		if (!(k->mod & SCHISM_KEYMOD_ALT)) return 0;
		if (env->loop_start > 0) {
			dialog_create(DIALOG_OK_CANCEL, "Cut envelope?", do_pre_loop_cut, NULL, 1, env);
			return 1;
		}
		return 0;

	// F/G for key symmetry with pattern double/halve block
	// E for symmetry with sample resize
	case SCHISM_KEYSYM_f:
		if (k->state == KEY_PRESS)
			return 0;
		if (!(k->mod & SCHISM_KEYMOD_ALT)) return 0;
		env_resize(env, env->ticks[env->nodes - 1] * 2);
		return 1;
	case SCHISM_KEYSYM_g:
		if (k->state == KEY_PRESS)
			return 0;
		if (!(k->mod & SCHISM_KEYMOD_ALT)) return 0;
		env_resize(env, env->ticks[env->nodes - 1] / 2);
		return 1;
	case SCHISM_KEYSYM_e:
		if (k->state == KEY_PRESS)
			return 0;
		if (!(k->mod & SCHISM_KEYMOD_ALT)) return 0;
		env_resize_dialog(env);
		return 1;

	case SCHISM_KEYSYM_z:
		if (k->state == KEY_PRESS)
			return 0;
		if (!(k->mod & SCHISM_KEYMOD_ALT)) return 0;
		env_adsr_dialog(env);
		return 1;

	default:
		if (k->state == KEY_PRESS)
			return 0;

		n = numeric_key_event(k, 0);
		if (n > -1) {
			if (k->mod & (SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_CTRL)) {
				save_envelope(n, env, sec);
				status_text_flash("Envelope copied into slot %d", n);
			} else if (k->mod & SCHISM_KEYMOD_SHIFT) {
				restore_envelope(n, env, sec);
				if (!(status.flags & CLASSIC_MODE))
					status_text_flash("Pasted envelope from slot %d", n);
			}
			return 1;
		}
		return 0;
	}

	new_node = CLAMP(new_node, 0, env->nodes - 1);
	if (*current_node != new_node) {
		*current_node = new_node;
		status.flags |= NEED_UPDATE;
	}

	return 1;
}

/* mouse handling routines for envelope */
static int _env_handle_mouse(struct key_event *k, song_envelope_t *env, int *current_node)
{
	int x, y, i;
	int max_ticks = 50;

	if (k->mouse != MOUSE_CLICK) return 0;

	if (k->state == KEY_RELEASE) {
		/* mouse release */
		if (envelope_mouse_edit) {
			video_set_mousecursor_shape(CURSOR_SHAPE_ARROW);
			if (current_node && *current_node) {
				for (i = 0; i < env->nodes-1; i++) {
					if (*current_node == i) continue;
					if (env->ticks[ *current_node ] == env->ticks[i]
					&& env->values[ *current_node ] == env->values[i]) {
						status_text_flash("Removed node %d", (int)(*current_node));
						status.flags |= SONG_NEEDS_SAVE;

						*current_node = _env_node_remove(env, *current_node);
						break;
					}
				}

			}
			status.flags |= NEED_UPDATE;
		}
		memused_songchanged();
		envelope_mouse_edit = 0;
		return 1;
	}

	while (env->ticks[env->nodes - 1] >= max_ticks)
		max_ticks *= 2;

	if (envelope_mouse_edit) {
		video_set_mousecursor_shape(CURSOR_SHAPE_CROSSHAIR);
		if (k->fx < 259)
			x = 0;
		else
			x = (k->fx - 259) * max_ticks / 256;
		y = 64 - (k->fy - 144);
		if (y > 63) y++;
		if (y > 42) y++;
		if (y > 21) y++;
		if (y > 64) y = 64;
		if (y < 0) y = 0;

		if (*current_node && env->ticks[ (*current_node)-1 ] >= x) {
			x = env->ticks[ (*current_node)-1 ]+1;
		}
		if (*current_node < (env->nodes-1)) {
			if (env->ticks[ (*current_node)+1 ] <= x) {
				x = env->ticks[ (*current_node)+1 ]-1;
			}
		}
		if (env->ticks[*current_node] == x && env->ticks[*current_node] == y) {
			return 1;
		}
		if (x < 0) x = 0;
		if (x > envelope_tick_limit) x = envelope_tick_limit;
		if (x > 9999) x = 9999;
		if (*current_node) env->ticks[ *current_node ] = x;
		env->values[ *current_node ] = y;
		status.flags |= SONG_NEEDS_SAVE;
		status.flags |= NEED_UPDATE;
	} else {
		int n;
		int dist, dx, dy;
		int best_dist = 0;
		int best_dist_node;

		best_dist_node = -1;

		if (k->x < 32 || k->y < 18 || k->x > 32+45 || k->y > 18+8)
			return 0;

		for (n = 0; n < env->nodes; n++) {
			x = 259 + env->ticks[n] * 256 / max_ticks;
			y = env->values[n];
			if (y > 63) y--;
			if (y > 42) y--;
			if (y > 21) y--;
			y = 206 - y;

			dx = abs(x - (int) k->fx);
			dy = abs(y - (int) k->fy);
			dist = i_sqrt((dx*dx)+(dy*dy));
			if (best_dist_node == -1 || dist < best_dist) {
				if (dist <= 5) {
					best_dist = dist;
					best_dist_node = n;
				}
			}
		}
		if (best_dist_node == -1) {
			x = (k->fx - 259) * max_ticks / 256;
			y = 64 - (k->fy - 144);
			if (y > 63) y++;
			if (y > 42) y++;
			if (y > 21) y++;
			if (y > 64) y = 64;
			if (y < 0) y = 0;
			if (x > 0 && x < max_ticks) {
				*current_node = 0;
				for (i = 1; i < env->nodes; i++) {
					/* something too close */
					if (env->ticks[i] <= x) *current_node = i;
					if (abs(env->ticks[i] - x) < 2) return 0;
				}
				best_dist_node = (_env_node_add(env, *current_node, x, y))+1;
				status_text_flash("Created node %d", best_dist_node);
			}
			if (best_dist_node == -1) return 0;
		}

		envelope_tick_limit = env->ticks[env->nodes - 1] * 2;
		envelope_mouse_edit = 1;
		*current_node = best_dist_node;
		status.flags |= SONG_NEEDS_SAVE;
		status.flags |= NEED_UPDATE;
		return 1;
	}
	return 0;
}



/* - this function is only ever called when the envelope is in edit mode
   - envelope_edit_mode is only ever assigned a true value once, in _env_handle_key_viewmode.
   - when _env_handle_key_viewmode enables envelope_edit_mode, it indicates in its return value
     that the envelope should be enabled.
   - therefore, the envelope will always be enabled when this function is called, so there is
     no reason to indicate a change in the envelope here. */
static int _env_handle_key_editmode(struct key_event *k, song_envelope_t *env, int *current_node)
{
	int new_node = *current_node, new_tick = env->ticks[*current_node],
		new_value = env->values[*current_node];

	/* TODO: when does adding/removing a node alter loop points? */

	switch (k->sym) {
	case SCHISM_KEYSYM_UP:
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mod & SCHISM_KEYMOD_ALT)
			new_value += 16;
		else
			new_value++;
		break;
	case SCHISM_KEYSYM_DOWN:
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mod & SCHISM_KEYMOD_ALT)
			new_value -= 16;
		else
			new_value--;
		break;
	case SCHISM_KEYSYM_PAGEUP:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_value += 16;
		break;
	case SCHISM_KEYSYM_PAGEDOWN:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_value -= 16;
		break;
	case SCHISM_KEYSYM_LEFT:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & SCHISM_KEYMOD_CTRL)
			new_node--;
		else if (k->mod & SCHISM_KEYMOD_ALT)
			new_tick -= 16;
		else
			new_tick--;
		break;
	case SCHISM_KEYSYM_RIGHT:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & SCHISM_KEYMOD_CTRL)
			new_node++;
		else if (k->mod & SCHISM_KEYMOD_ALT)
			new_tick += 16;
		else
			new_tick++;
		break;
	case SCHISM_KEYSYM_TAB:
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mod & SCHISM_KEYMOD_SHIFT)
			new_tick -= 16;
		else
			new_tick += 16;
		break;
	case SCHISM_KEYSYM_HOME:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_tick = 0;
		break;
	case SCHISM_KEYSYM_END:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_tick = 10000;
		break;
	case SCHISM_KEYSYM_INSERT:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		*current_node = _env_node_add(env, *current_node, -1, -1);
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_DELETE:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		*current_node = _env_node_remove(env, *current_node);
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_SPACE:
		if (k->state == KEY_RELEASE)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		song_keyup(KEYJAZZ_NOINST, current_instrument, last_note);
		song_keydown(KEYJAZZ_NOINST, current_instrument, last_note, 64, KEYJAZZ_CHAN_CURRENT);
		return 1;
	case SCHISM_KEYSYM_RETURN:
		if (k->state == KEY_PRESS)
			return 0;
		if (!NO_MODIFIER(k->mod))
			return 0;
		envelope_edit_mode = 0;
		memused_songchanged();
		status.flags |= NEED_UPDATE;
		break;
	default:
		return 0;
	}

	new_node = CLAMP(new_node, 0, env->nodes - 1);
	if (new_node != *current_node) {
		status.flags |= NEED_UPDATE;
		*current_node = new_node;
		return 1;
	}

	new_tick = (new_node == 0) ? 0 : CLAMP(new_tick,
					env->ticks[new_node - 1] + 1,
				       ((new_node == env->nodes - 1)
					? 10000 : env->ticks[new_node + 1]) - 1);
	if (new_tick != env->ticks[new_node]) {
		env->ticks[*current_node] = new_tick;
		status.flags |= SONG_NEEDS_SAVE;
		status.flags |= NEED_UPDATE;
		return 1;
	}
	new_value = CLAMP(new_value, 0, 64);

	if (new_value != (int)env->values[new_node]) {
		env->values[*current_node] = (unsigned int)new_value;
		status.flags |= SONG_NEEDS_SAVE;
		status.flags |= NEED_UPDATE;
		return 1;
	}

	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */
/* envelope stuff (draw()'s and handle_key()'s) */

static void _draw_env_label(const char *env_name, int is_selected)
{
	int pos = 33;

	pos += draw_text(env_name, pos, 16, is_selected ? 3 : 0, 2);
	pos += draw_text(" Envelope", pos, 16, is_selected ? 3 : 0, 2);
	if (envelope_edit_mode || envelope_mouse_edit)
		draw_text(" (Edit)", pos, 16, is_selected ? 3 : 0, 2);
}

static void volume_envelope_draw(void)
{
	int is_selected = (ACTIVE_PAGE.selected_widget == 5);
	song_instrument_t *ins = song_get_instrument(current_instrument);

	_draw_env_label("Volume", is_selected);
	_env_draw(&ins->vol_env, 0, current_node_vol,
		ins->flags & ENV_VOLUME,
		  ins->flags & ENV_VOLLOOP, ins->flags & ENV_VOLSUSTAIN, 0);
}

static void panning_envelope_draw(void)
{
	int is_selected = (ACTIVE_PAGE.selected_widget == 5);
	song_instrument_t *ins = song_get_instrument(current_instrument);

	_draw_env_label("Panning", is_selected);
	_env_draw(&ins->pan_env, 1, current_node_pan,
		ins->flags & ENV_PANNING,
		  ins->flags & ENV_PANLOOP, ins->flags & ENV_PANSUSTAIN, 1);
}

static void pitch_envelope_draw(void)
{
	int is_selected = (ACTIVE_PAGE.selected_widget == 5);
	song_instrument_t *ins = song_get_instrument(current_instrument);

	_draw_env_label("Frequency", is_selected);
	_env_draw(&ins->pitch_env, (ins->flags & ENV_FILTER) ? 0 : 1, current_node_pitch,
		ins->flags & (ENV_PITCH|ENV_FILTER),
		  ins->flags & ENV_PITCHLOOP, ins->flags & ENV_PITCHSUSTAIN, 2);
}

static int volume_envelope_handle_key(struct key_event * k)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	int r;

	if (_env_handle_mouse(k, &ins->vol_env, &current_node_vol)) {
		ins->flags |= ENV_VOLUME;
		return 1;
	}
	if (envelope_edit_mode)
		r = _env_handle_key_editmode(k, &ins->vol_env, &current_node_vol);
	else
		r = _env_handle_key_viewmode(k, &ins->vol_env, &current_node_vol, ENV_VOLUME);
	if (r & 2) {
		r ^= 2;
		ins->flags |= ENV_VOLUME;
	}
	return r;
}

static int panning_envelope_handle_key(struct key_event * k)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	int r;

	if (_env_handle_mouse(k, &ins->pan_env, &current_node_pan)) {
		ins->flags |= ENV_PANNING;
		return 1;
	}

	if (envelope_edit_mode)
		r = _env_handle_key_editmode(k, &ins->pan_env, &current_node_pan);
	else
		r = _env_handle_key_viewmode(k, &ins->pan_env, &current_node_pan, ENV_PANNING);
	if (r & 2) {
		r ^= 2;
		ins->flags |= ENV_PANNING;
	}
	return r;
}

static int pitch_envelope_handle_key(struct key_event * k)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	int r;

	if (_env_handle_mouse(k, &ins->pitch_env, &current_node_pitch)) {
		ins->flags |= ENV_PITCH;
		return 1;
	}
	if (envelope_edit_mode)
		r = _env_handle_key_editmode(k, &ins->pitch_env, &current_node_pitch);
	else
		r = _env_handle_key_viewmode(k, &ins->pitch_env, &current_node_pitch, ENV_PITCH);
	if (r & 2) {
		r ^= 2;
		ins->flags |= ENV_PITCH;
	}
	return r;
}

/* --------------------------------------------------------------------------------------------------------- */
/* pitch-pan center */

static int pitch_pan_center_handle_key(struct key_event *k)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	int ppc = ins->pitch_pan_center;

	if (k->state == KEY_RELEASE)
		return 0;
	switch (k->sym) {
	case SCHISM_KEYSYM_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		ppc--;
		break;
	case SCHISM_KEYSYM_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		ppc++;
		break;
	default:
		if ((k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT)) == 0) {
			ppc = kbd_get_note(k);
			if (ppc < 1 || ppc > 120)
				return 0;
			ppc--;
			break;
		}
		return 0;
	}
	if ((unsigned int)ppc != ins->pitch_pan_center
	&& ppc >= 0 && ppc < 120) {
		ins->pitch_pan_center = (unsigned int)ppc;
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

static void pitch_pan_center_draw(void)
{
	char buf[4];
	int selected = (ACTIVE_PAGE.selected_widget == 16);
	song_instrument_t *ins = song_get_instrument(current_instrument);

	draw_text(get_note_string(ins->pitch_pan_center + 1, buf), 54, 45, selected ? 3 : 2, 0);
}

/* ----------------------------------------------------------------------------- */
/* generic ITI saving routines */

/* filename can be NULL, in which case the instrument filename is used (quick save) */
struct instrument_save_data {
	char *path;
	/* char *options? */
	const char *format;
};

static void save_instrument_free_data(void *ptr)
{
	struct instrument_save_data *data = (struct instrument_save_data *) ptr;
	if (data->path)
		free(data->path);
	free(data);
}

static void do_save_instrument(void *ptr)
{
	struct instrument_save_data *data = (struct instrument_save_data *) ptr;

	song_save_instrument(data->path, data->format, song_get_instrument(current_instrument), current_instrument);
	save_instrument_free_data(ptr);
}

static void instrument_save(const char *filename, const char *format)
{
	song_instrument_t *penv = song_get_instrument(current_instrument);
	char *ptr;
	struct stat buf;

	if (filename) {
		ptr = (char *) dmoz_path_concat(cfg_dir_instruments, filename);
	} else if (penv->filename[0]) {
		ptr = (char *) dmoz_path_concat(cfg_dir_instruments, penv->filename);
	} else {
		status_text_flash("Error: Instrument %d NOT saved! (No Filename?)", current_instrument);
		return;
	}

	struct instrument_save_data *data = mem_alloc(sizeof(*data));
	data->path = ptr;
	data->format = format;

	if (!os_stat(ptr, &buf)) {
		if (S_ISDIR(buf.st_mode)) {
			status_text_flash("%s is a directory", filename);
			return;
		} else if (S_ISREG(buf.st_mode)) {
			dialog_create(DIALOG_OK_CANCEL,
				"Overwrite file?", do_save_instrument,
				save_instrument_free_data, 1, data);
			return;
		} else {
			status_text_flash("%s is not a regular file", filename);
			return;
		}
	} else {
		do_save_instrument(data);
	}
}

/* ----------------------------------------------------------------------------- */
/* export instrument dialog */

static struct widget export_instrument_widgets[4];
static char export_instrument_filename[SCHISM_NAME_MAX + 1] = "";
static int export_instrument_format = 0;
static int num_save_formats = 0;

static void do_export_instrument(SCHISM_UNUSED void *data)
{
	instrument_save(export_instrument_filename, instrument_save_formats[export_instrument_format].label);
}

static void export_instrument_list_draw(void)
{
	int n, focused = (*selected_widget == 3);

	draw_fill_chars(53, 24, 56, 31, DEFAULT_FG, 0);
	for (n = 0; instrument_save_formats[n].label; n++) {
		int fg = 6, bg = 0;
		if (focused && n == export_instrument_format) {
			fg = 0;
			bg = 3;
		} else if (n == export_instrument_format) {
			bg = 14;
		}
		draw_text_len(instrument_save_formats[n].label, 4, 53, 24 + n, fg, bg);
	}
}

static int export_instrument_list_handle_key(struct key_event * k)
{
	int new_format = export_instrument_format;

	if (k->state == KEY_RELEASE)
		return 0;
	switch (k->sym) {
	case SCHISM_KEYSYM_UP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_format--;
		break;
	case SCHISM_KEYSYM_DOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_format++;
		break;
	case SCHISM_KEYSYM_PAGEUP:
	case SCHISM_KEYSYM_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_format = 0;
		break;
	case SCHISM_KEYSYM_PAGEDOWN:
	case SCHISM_KEYSYM_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_format = num_save_formats - 1;
		break;
	case SCHISM_KEYSYM_TAB:
		if (k->mod & SCHISM_KEYMOD_SHIFT) {
			widget_change_focus_to(0);
			return 1;
		}
		/* fall through */
	case SCHISM_KEYSYM_LEFT:
	case SCHISM_KEYSYM_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		widget_change_focus_to(0); /* should focus 0/1/2 depending on what's closest */
		return 1;
	default:
		return 0;
	}

	new_format = CLAMP(new_format, 0, num_save_formats - 1);
	if (new_format != export_instrument_format) {
		/* update the option string */
		export_instrument_format = new_format;
		status.flags |= NEED_UPDATE;
	}

	return 1;
}

static void export_instrument_draw_const(void)
{
	draw_text("Export Instrument", 34, 21, 0, 2);

	draw_text("Filename", 24, 24, 0, 2);
	draw_box(32, 23, 51, 25, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_box(52, 23, 57, 32, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void export_instrument_dialog(void)
{
	song_instrument_t *instrument = song_get_instrument(current_instrument);
	struct dialog *dialog;

	widget_create_textentry(export_instrument_widgets + 0, 33, 24, 18, 0, 1, 3, NULL,
			 export_instrument_filename, ARRAY_SIZE(export_instrument_filename) - 1);
	widget_create_button(export_instrument_widgets + 1, 31, 35, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
	widget_create_button(export_instrument_widgets + 2, 42, 35, 6, 3, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	widget_create_other(export_instrument_widgets + 3, 0, export_instrument_list_handle_key, NULL, export_instrument_list_draw);

	strncpy(export_instrument_filename, instrument->filename, ARRAY_SIZE(export_instrument_filename) - 1);
	export_instrument_filename[ARRAY_SIZE(export_instrument_filename) - 1] = 0;

	dialog = dialog_create_custom(21, 20, 39, 18, export_instrument_widgets, 4, 0,
				      export_instrument_draw_const, NULL);
	dialog->action_yes = do_export_instrument;
}


static void do_delete_inst(SCHISM_UNUSED void *ign)
{
	song_delete_instrument(current_instrument, 0);
}

static void do_delete_inst_preserve(SCHISM_UNUSED void *ign)
{
	song_delete_instrument(current_instrument, 1);
}

static void instrument_list_handle_alt_key(struct key_event *k)
{
	/* song_instrument_t *ins = song_get_instrument(current_instrument); */

	if (k->state == KEY_RELEASE)
		return;
	switch (k->sym) {
	case SCHISM_KEYSYM_n:
		song_toggle_multichannel_mode();
		return;
	case SCHISM_KEYSYM_o:
		instrument_save(NULL, "ITI");
		return;
	case SCHISM_KEYSYM_r:
		smpprompt_create("Replace instrument with:", "Instrument", do_replace_instrument);
		return;
	case SCHISM_KEYSYM_s:
		// extra space to align the text like IT
		smpprompt_create("Swap instrument with: ", "Instrument", do_swap_instrument);
		return;
	case SCHISM_KEYSYM_x:
		smpprompt_create("Exchange instrument with:", "Instrument", do_exchange_instrument);
		return;
	case SCHISM_KEYSYM_p:
		smpprompt_create("Copy instrument:", "Instrument", do_copy_instrument);
		return;
	case SCHISM_KEYSYM_w:
		song_wipe_instrument(current_instrument);
		break;
	case SCHISM_KEYSYM_d:
        if (k->mod & SCHISM_KEYMOD_SHIFT) {
            dialog_create(DIALOG_OK_CANCEL,
                "Delete Instrument? (preserve shared samples)",
                do_delete_inst_preserve, NULL, 1, NULL);
        } else {
            dialog_create(DIALOG_OK_CANCEL,
                "Delete Instrument?",
                do_delete_inst, NULL, 1, NULL);
        }
		return;
	case SCHISM_KEYSYM_t:
		export_instrument_dialog();
		break;
	default:
		return;
	}

	status.flags |= NEED_UPDATE;
}

static int instrument_list_pre_handle_key(struct key_event * k)
{
	// Only handle plain F4 key when no dialog is active.
	if (status.dialog_type != DIALOG_NONE || k->sym != SCHISM_KEYSYM_F4 || (k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT)))
		return 0;
	if (k->state == KEY_RELEASE)
		return 1;

	if (song_is_instrument_mode()) {
		int csamp = sample_get_current();
		sample_synchronize_to_instrument();
		if (csamp != sample_get_current())
			return 0;
	}

	if (k->mod & SCHISM_KEYMOD_SHIFT) {
		switch (status.current_page) {
			default:
			case PAGE_INSTRUMENT_LIST_VOLUME:  set_subpage(PAGE_INSTRUMENT_LIST_GENERAL); break;
			case PAGE_INSTRUMENT_LIST_PANNING: set_subpage(PAGE_INSTRUMENT_LIST_VOLUME);  break;
			case PAGE_INSTRUMENT_LIST_PITCH:   set_subpage(PAGE_INSTRUMENT_LIST_PANNING); break;
			case PAGE_INSTRUMENT_LIST_GENERAL: set_subpage(PAGE_INSTRUMENT_LIST_PITCH);   break;
		}
	} else {
		switch (status.current_page) {
			default:
			case PAGE_INSTRUMENT_LIST_PITCH:   set_subpage(PAGE_INSTRUMENT_LIST_GENERAL); break;
			case PAGE_INSTRUMENT_LIST_GENERAL: set_subpage(PAGE_INSTRUMENT_LIST_VOLUME);  break;
			case PAGE_INSTRUMENT_LIST_VOLUME:  set_subpage(PAGE_INSTRUMENT_LIST_PANNING); break;
			case PAGE_INSTRUMENT_LIST_PANNING: set_subpage(PAGE_INSTRUMENT_LIST_PITCH);   break;
		}
	}
	return 1;
}
static void instrument_list_handle_key(struct key_event * k)
{
	switch (k->sym) {
	case SCHISM_KEYSYM_COMMA:
		if (NO_MODIFIER(k->mod)) {
			if (!(status.flags & CLASSIC_MODE)
			&& ACTIVE_PAGE.selected_widget == 5) return;
		}
	case SCHISM_KEYSYM_LESS:
		if (k->state == KEY_RELEASE)
			return;
		song_change_current_play_channel(-1, 0);
		return;
	case SCHISM_KEYSYM_PERIOD:
		if (NO_MODIFIER(k->mod)) {
			if (!(status.flags & CLASSIC_MODE)
			&& ACTIVE_PAGE.selected_widget == 5) return;
		}
	case SCHISM_KEYSYM_GREATER:
		if (k->state == KEY_RELEASE)
			return;
		song_change_current_play_channel(1, 0);
		return;

	case SCHISM_KEYSYM_PAGEUP:
		if (k->state == KEY_RELEASE)
			return;
		instrument_set(current_instrument - 1);
		break;
	case SCHISM_KEYSYM_PAGEDOWN:
		if (k->state == KEY_RELEASE)
			return;
		instrument_set(current_instrument + 1);
		break;
	case SCHISM_KEYSYM_ESCAPE:
		if ((k->mod & SCHISM_KEYMOD_SHIFT) || instrument_cursor_pos < 25) {
			if (k->state == KEY_RELEASE)
				return;
			instrument_cursor_pos = 25;
			get_page_widgets()->accept_text = 0;
			widget_change_focus_to(0);
			status.flags |= NEED_UPDATE;
			return;
		}
		return;
	default:
		if (k->mod & (SCHISM_KEYMOD_ALT)) {
			instrument_list_handle_alt_key(k);
		} else {
			int n, v;

			if (k->midi_note > -1) {
				n = k->midi_note;
				if (k->midi_volume > -1) {
					v = k->midi_volume / 2;
				} else {
					v = 64;
				}
			} else {
				v = 64;
				n = kbd_get_note(k);
				if (n <= 0 || n > 120)
					return;
			}

			if (k->state == KEY_RELEASE) {
				song_keyup(KEYJAZZ_NOINST, current_instrument, n);
				status.last_keysym = 0;
			} else if (!k->is_repeat) {
				song_keydown(KEYJAZZ_NOINST, current_instrument, n, v, KEYJAZZ_CHAN_AUTO);
			}
			last_note = n;
		}
		return;
	}
}

/* --------------------------------------------------------------------- */

static void set_subpage(int page)
{
	int widget = ACTIVE_PAGE.selected_widget;
	int b = 1;
	switch (page) {
	case PAGE_INSTRUMENT_LIST_GENERAL: b = 1; break;
	case PAGE_INSTRUMENT_LIST_VOLUME:  b = 2; break;
	case PAGE_INSTRUMENT_LIST_PANNING: b = 3; break;
	case PAGE_INSTRUMENT_LIST_PITCH:   b = 4; break;
	default: return;
	};
	widget_togglebutton_set(pages[page].widgets, b, 0);
	set_page(page);
	if (widget >= ACTIVE_PAGE.total_widgets)
		widget = ACTIVE_PAGE.total_widgets - 1;
	ACTIVE_PAGE.selected_widget = widget;
	instrument_list_subpage = page;
	status.flags |= NEED_UPDATE;
}

static void change_subpage(void)
{
	int widget = ACTIVE_PAGE.selected_widget;
	int p[] = {
		PAGE_INSTRUMENT_LIST_GENERAL,
		PAGE_INSTRUMENT_LIST_VOLUME,
		PAGE_INSTRUMENT_LIST_PANNING,
		PAGE_INSTRUMENT_LIST_PITCH,
	};
	set_subpage(p[CLAMP(widget - 1, 0, 3)]);
}

/* --------------------------------------------------------------------- */
/* predraw hooks... */

static void instrument_list_general_predraw_hook(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	widget_togglebutton_set(widgets_general, 6 + (ins->nna % 4), 0);
	widget_togglebutton_set(widgets_general, 10 + (ins->dct % 4), 0);
	widget_togglebutton_set(widgets_general, 14 + (ins->dca % 3), 0);

	widgets_general[17].d.textentry.text = ins->filename;
}

static void instrument_list_volume_predraw_hook(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	widgets_volume[6].d.toggle.state = !!(ins->flags & ENV_VOLUME);
	widgets_volume[7].d.toggle.state = !!(ins->flags & ENV_VOLCARRY);
	widgets_volume[8].d.toggle.state = !!(ins->flags & ENV_VOLLOOP);
	widgets_volume[11].d.toggle.state = !!(ins->flags & ENV_VOLSUSTAIN);

	/* FIXME: this is the wrong place for this.
	... and it's probably not even right -- how does Impulse Tracker handle loop constraints?
	See below for panning/pitch envelopes; same deal there. */
	if (ins->vol_env.loop_start > ins->vol_env.loop_end)
		ins->vol_env.loop_end = ins->vol_env.loop_start;
	if (ins->vol_env.sustain_start > ins->vol_env.sustain_end)
		ins->vol_env.sustain_end = ins->vol_env.sustain_start;

	widgets_volume[9].d.numentry.max = ins->vol_env.nodes - 1;
	widgets_volume[10].d.numentry.max = ins->vol_env.nodes - 1;
	widgets_volume[12].d.numentry.max = ins->vol_env.nodes - 1;
	widgets_volume[13].d.numentry.max = ins->vol_env.nodes - 1;

	widgets_volume[9].d.numentry.value = ins->vol_env.loop_start;
	widgets_volume[10].d.numentry.value = ins->vol_env.loop_end;
	widgets_volume[12].d.numentry.value = ins->vol_env.sustain_start;
	widgets_volume[13].d.numentry.value = ins->vol_env.sustain_end;

	/* current_song hack: shifting values all over the place here, ugh */
	widgets_volume[14].d.thumbbar.value = ins->global_volume;
	widgets_volume[15].d.thumbbar.value = ins->fadeout >> 5;
	widgets_volume[16].d.thumbbar.value = ins->vol_swing;
}

static void instrument_list_panning_predraw_hook(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	widgets_panning[6].d.toggle.state = !!(ins->flags & ENV_PANNING);
	widgets_panning[7].d.toggle.state = !!(ins->flags & ENV_PANCARRY);
	widgets_panning[8].d.toggle.state = !!(ins->flags & ENV_PANLOOP);
	widgets_panning[11].d.toggle.state = !!(ins->flags & ENV_PANSUSTAIN);

	if (ins->pan_env.loop_start > ins->pan_env.loop_end)
		ins->pan_env.loop_end = ins->pan_env.loop_start;
	if (ins->pan_env.sustain_start > ins->pan_env.sustain_end)
		ins->pan_env.sustain_end = ins->pan_env.sustain_start;

	widgets_panning[9].d.numentry.max = ins->pan_env.nodes - 1;
	widgets_panning[10].d.numentry.max = ins->pan_env.nodes - 1;
	widgets_panning[12].d.numentry.max = ins->pan_env.nodes - 1;
	widgets_panning[13].d.numentry.max = ins->pan_env.nodes - 1;

	widgets_panning[9].d.numentry.value = ins->pan_env.loop_start;
	widgets_panning[10].d.numentry.value = ins->pan_env.loop_end;
	widgets_panning[12].d.numentry.value = ins->pan_env.sustain_start;
	widgets_panning[13].d.numentry.value = ins->pan_env.sustain_end;

	widgets_panning[14].d.toggle.state = !!(ins->flags & ENV_SETPANNING);
	widgets_panning[15].d.thumbbar.value = ins->panning >> 2;
	/* (widgets_panning[16] is the pitch-pan center) */
	widgets_panning[17].d.thumbbar.value = ins->pitch_pan_separation;
	widgets_panning[18].d.thumbbar.value = ins->pan_swing;
}

static void instrument_list_pitch_predraw_hook(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	widgets_pitch[6].d.menutoggle.state = ((ins->flags & ENV_PITCH)
					     ? ((ins->flags & ENV_FILTER)
						? 2 : 1) : 0);
	widgets_pitch[7].d.toggle.state = !!(ins->flags & ENV_PITCHCARRY);
	widgets_pitch[8].d.toggle.state = !!(ins->flags & ENV_PITCHLOOP);
	widgets_pitch[11].d.toggle.state = !!(ins->flags & ENV_PITCHSUSTAIN);

	if (ins->pitch_env.loop_start > ins->pitch_env.loop_end)
		ins->pitch_env.loop_end = ins->pitch_env.loop_start;
	if (ins->pitch_env.sustain_start > ins->pitch_env.sustain_end)
		ins->pitch_env.sustain_end = ins->pitch_env.sustain_start;

	widgets_pitch[9].d.numentry.max = ins->pitch_env.nodes - 1;
	widgets_pitch[10].d.numentry.max = ins->pitch_env.nodes - 1;
	widgets_pitch[12].d.numentry.max = ins->pitch_env.nodes - 1;
	widgets_pitch[13].d.numentry.max = ins->pitch_env.nodes - 1;

	widgets_pitch[9].d.numentry.value = ins->pitch_env.loop_start;
	widgets_pitch[10].d.numentry.value = ins->pitch_env.loop_end;
	widgets_pitch[12].d.numentry.value = ins->pitch_env.sustain_start;
	widgets_pitch[13].d.numentry.value = ins->pitch_env.sustain_end;

	if (ins->ifc & 0x80)
		widgets_pitch[14].d.thumbbar.value = ins->ifc & 0x7f;
	else
		widgets_pitch[14].d.thumbbar.value = -1;
	if (ins->ifr & 0x80)
		widgets_pitch[15].d.thumbbar.value = ins->ifr & 0x7f;
	else
		widgets_pitch[15].d.thumbbar.value = -1;

	/* printf("ins%02d: ch%04d pgm%04d bank%06d drum%04d\n", current_instrument,
		ins->midi_channel, ins->midi_program, ins->midi_bank, ins->midi_drum_key); */
	widgets_pitch[16].d.bitset.value           = ins->midi_channel_mask;
	widgets_pitch[17].d.thumbbar.value = (signed char) ins->midi_program;
	widgets_pitch[18].d.thumbbar.value = (signed char) (ins->midi_bank & 0xff);
	widgets_pitch[19].d.thumbbar.value = (signed char) (ins->midi_bank >> 8);
	/* what is midi_drum_key for? */
}

/* --------------------------------------------------------------------- */
/* update values in song */

static void instrument_list_general_update_values(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	status.flags |= SONG_NEEDS_SAVE;
	for (ins->nna = 4; ins->nna--;)
		if (widgets_general[ins->nna + 6].d.togglebutton.state)
			break;
	for (ins->dct = 4; ins->dct--;)
		if (widgets_general[ins->dct + 10].d.togglebutton.state)
			break;
	for (ins->dca = 3; ins->dca--;)
		if (widgets_general[ins->dca + 14].d.togglebutton.state)
			break;
}

static void update_filename(void)
{
	status.flags |= SONG_NEEDS_SAVE;
}

#define CHECK_SET(a,b,c) if (a != b) { a = b; c; }

static void instrument_list_volume_update_values(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	status.flags |= SONG_NEEDS_SAVE;
	ins->flags &= ~(ENV_VOLUME | ENV_VOLCARRY | ENV_VOLLOOP | ENV_VOLSUSTAIN);
	if (widgets_volume[6].d.toggle.state)
		ins->flags |= ENV_VOLUME;
	if (widgets_volume[7].d.toggle.state)
		ins->flags |= ENV_VOLCARRY;
	if (widgets_volume[8].d.toggle.state)
		ins->flags |= ENV_VOLLOOP;
	if (widgets_volume[11].d.toggle.state)
		ins->flags |= ENV_VOLSUSTAIN;

	CHECK_SET(ins->vol_env.loop_start, widgets_volume[9].d.numentry.value,
		  ins->flags |= ENV_VOLLOOP);
	CHECK_SET(ins->vol_env.loop_end, widgets_volume[10].d.numentry.value,
		  ins->flags |= ENV_VOLLOOP);
	CHECK_SET(ins->vol_env.sustain_start, widgets_volume[12].d.numentry.value,
		  ins->flags |= ENV_VOLSUSTAIN);
	CHECK_SET(ins->vol_env.sustain_end, widgets_volume[13].d.numentry.value,
		  ins->flags |= ENV_VOLSUSTAIN);

	/* more ugly shifts */
	ins->global_volume = widgets_volume[14].d.thumbbar.value;
	ins->fadeout = widgets_volume[15].d.thumbbar.value << 5;
	ins->vol_swing = widgets_volume[16].d.thumbbar.value;

	song_update_playing_instrument(current_instrument);
}

static void instrument_list_panning_update_values(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	int n;

	status.flags |= SONG_NEEDS_SAVE;
	ins->flags &= ~(ENV_PANNING | ENV_PANCARRY | ENV_PANLOOP | ENV_PANSUSTAIN | ENV_SETPANNING);
	if (widgets_panning[6].d.toggle.state)
		ins->flags |= ENV_PANNING;
	if (widgets_panning[7].d.toggle.state)
		ins->flags |= ENV_PANCARRY;
	if (widgets_panning[8].d.toggle.state)
		ins->flags |= ENV_PANLOOP;
	if (widgets_panning[11].d.toggle.state)
		ins->flags |= ENV_PANSUSTAIN;
	if (widgets_panning[14].d.toggle.state)
		ins->flags |= ENV_SETPANNING;

	CHECK_SET(ins->pan_env.loop_start, widgets_panning[9].d.numentry.value,
		  ins->flags |= ENV_PANLOOP);
	CHECK_SET(ins->pan_env.loop_end, widgets_panning[10].d.numentry.value,
		  ins->flags |= ENV_PANLOOP);
	CHECK_SET(ins->pan_env.sustain_start, widgets_panning[12].d.numentry.value,
		  ins->flags |= ENV_PANSUSTAIN);
	CHECK_SET(ins->pan_env.sustain_end, widgets_panning[13].d.numentry.value,
		  ins->flags |= ENV_PANSUSTAIN);

	n = widgets_panning[15].d.thumbbar.value << 2;
	if (ins->panning != (unsigned int)n) {
		ins->panning = (unsigned int)n;
		ins->flags |= ENV_SETPANNING;
	}
	/* (widgets_panning[16] is the pitch-pan center) */
	ins->pitch_pan_separation = widgets_panning[17].d.thumbbar.value;
	ins->pan_swing = widgets_panning[18].d.thumbbar.value;

	song_update_playing_instrument(current_instrument);
}

static void instrument_list_pitch_update_values(void)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);

	status.flags |= SONG_NEEDS_SAVE;
	ins->flags &= ~(ENV_PITCH | ENV_PITCHCARRY | ENV_PITCHLOOP | ENV_PITCHSUSTAIN | ENV_FILTER);

	switch (widgets_pitch[6].d.menutoggle.state) {
	case 2: ins->flags |= ENV_FILTER;
	case 1: ins->flags |= ENV_PITCH;
	}

	if (widgets_pitch[6].d.menutoggle.state)
		ins->flags |= ENV_PITCH;
	if (widgets_pitch[7].d.toggle.state)
		ins->flags |= ENV_PITCHCARRY;
	if (widgets_pitch[8].d.toggle.state)
		ins->flags |= ENV_PITCHLOOP;
	if (widgets_pitch[11].d.toggle.state)
		ins->flags |= ENV_PITCHSUSTAIN;

	CHECK_SET(ins->pitch_env.loop_start, widgets_pitch[9].d.numentry.value,
		  ins->flags |= ENV_PITCHLOOP);
	CHECK_SET(ins->pitch_env.loop_end, widgets_pitch[10].d.numentry.value,
		  ins->flags |= ENV_PITCHLOOP);
	CHECK_SET(ins->pitch_env.sustain_start, widgets_pitch[12].d.numentry.value,
		  ins->flags |= ENV_PITCHSUSTAIN);
	CHECK_SET(ins->pitch_env.sustain_end, widgets_pitch[13].d.numentry.value,
		  ins->flags |= ENV_PITCHSUSTAIN);
	if (widgets_pitch[14].d.thumbbar.value > -1) {
		ins->ifc = widgets_pitch[14].d.thumbbar.value | 0x80;
	} else {
		ins->ifc = 0x7f;
	}
	if (widgets_pitch[15].d.thumbbar.value > -1) {
		ins->ifr = widgets_pitch[15].d.thumbbar.value | 0x80;
	} else {
		ins->ifr = 0x7f;
	}
	ins->midi_channel_mask = widgets_pitch[16].d.bitset.value;
	ins->midi_program = widgets_pitch[17].d.thumbbar.value;
	ins->midi_bank = ((widgets_pitch[19].d.thumbbar.value << 8)
			  | (widgets_pitch[18].d.thumbbar.value & 0xff));

	song_update_playing_instrument(current_instrument);
}

/* --------------------------------------------------------------------- */
/* draw_const functions */

static void instrument_list_draw_const(void)
{
	draw_box(4, 12, 30, 48, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void instrument_list_general_draw_const(void)
{
	int n;

	instrument_list_draw_const();

	draw_box(31, 15, 42, 48, BOX_THICK | BOX_INNER | BOX_INSET);

	/* Kind of a hack, and not really useful, but... :) */
	if (status.flags & CLASSIC_MODE) {
		draw_box(55, 46, 73, 48, BOX_THICK | BOX_INNER | BOX_INSET);
		draw_text("    ", 69, 47, 1, 0);
	} else {
		draw_box(55, 46, 69, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	}

	draw_text("New Note Action", 54, 17, 0, 2);
	draw_text("Duplicate Check Type & Action", 47, 32, 0, 2);
	draw_text("Filename", 47, 47, 0, 2);

	for (n = 0; n < 35; n++) {
		draw_char(134, 44 + n, 15, 0, 2);
		draw_char(134, 44 + n, 30, 0, 2);
		draw_char(154, 44 + n, 45, 0, 2);
	}
}

static void instrument_list_volume_draw_const(void)
{
	instrument_list_draw_const();

	draw_fill_chars(57, 28, 62, 29, DEFAULT_FG, 0);
	draw_fill_chars(57, 32, 62, 34, DEFAULT_FG, 0);
	draw_fill_chars(57, 37, 62, 39, DEFAULT_FG, 0);

	draw_box(31, 17, 77, 26, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 27, 63, 30, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 31, 63, 35, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 36, 63, 40, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 41, 71, 44, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 45, 71, 47, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_text("Volume Envelope", 38, 28, 0, 2);
	draw_text("Carry", 48, 29, 0, 2);
	draw_text("Envelope Loop", 40, 32, 0, 2);
	draw_text("Loop Begin", 43, 33, 0, 2);
	draw_text("Loop End", 45, 34, 0, 2);
	draw_text("Sustain Loop", 41, 37, 0, 2);
	draw_text("SusLoop Begin", 40, 38, 0, 2);
	draw_text("SusLoop End", 42, 39, 0, 2);
	draw_text("Global Volume", 40, 42, 0, 2);
	draw_text("Fadeout", 46, 43, 0, 2);
	draw_text("Volume Swing %", 39, 46, 0, 2);
}

static void instrument_list_panning_draw_const(void)
{
	instrument_list_draw_const();

	draw_fill_chars(57, 28, 62, 29, DEFAULT_FG, 0);
	draw_fill_chars(57, 32, 62, 34, DEFAULT_FG, 0);
	draw_fill_chars(57, 37, 62, 39, DEFAULT_FG, 0);
	draw_fill_chars(57, 42, 62, 45, DEFAULT_FG, 0);

	draw_box(31, 17, 77, 26, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 27, 63, 30, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 31, 63, 35, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 36, 63, 40, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 41, 63, 48, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_text("Panning Envelope", 37, 28, 0, 2);
	draw_text("Carry", 48, 29, 0, 2);
	draw_text("Envelope Loop", 40, 32, 0, 2);
	draw_text("Loop Begin", 43, 33, 0, 2);
	draw_text("Loop End", 45, 34, 0, 2);
	draw_text("Sustain Loop", 41, 37, 0, 2);
	draw_text("SusLoop Begin", 40, 38, 0, 2);
	draw_text("SusLoop End", 42, 39, 0, 2);
	draw_text("Default Pan", 42, 42, 0, 2);
	draw_text("Pan Value", 44, 43, 0, 2);
	draw_text("Pitch-Pan Center", 37, 45, 0, 2);
	draw_text("Pitch-Pan Separation", 33, 46, 0, 2);
	if (status.flags & CLASSIC_MODE) {
		/* Hmm. The 's' in swing isn't capitalised. ;) */
		draw_text("Pan swing", 44, 47, 0, 2);
	} else {
		draw_text("Pan Swing", 44, 47, 0, 2);
	}

	draw_text("\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a", 54, 44, 2, 0);
}

static void instrument_list_pitch_draw_const(void)
{
	instrument_list_draw_const();

	draw_fill_chars(57, 28, 62, 29, DEFAULT_FG, 0);
	draw_fill_chars(57, 32, 62, 34, DEFAULT_FG, 0);
	draw_fill_chars(57, 37, 62, 39, DEFAULT_FG, 0);

	draw_box(31, 17, 77, 26, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 27, 63, 30, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 31, 63, 35, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 36, 63, 40, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(53, 41, 71, 48, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_text("Frequency Envelope", 35, 28, 0, 2);
	draw_text("Carry", 48, 29, 0, 2);
	draw_text("Envelope Loop", 40, 32, 0, 2);
	draw_text("Loop Begin", 43, 33, 0, 2);
	draw_text("Loop End", 45, 34, 0, 2);
	draw_text("Sustain Loop", 41, 37, 0, 2);
	draw_text("SusLoop Begin", 40, 38, 0, 2);
	draw_text("SusLoop End", 42, 39, 0, 2);
	draw_text("Default Cutoff", 36, 42, 0, 2);
	draw_text("Default Resonance", 36, 43, 0, 2);
	draw_text("MIDI Channels", 36, 44, 0, 2);
	draw_text("MIDI Program", 36, 45, 0, 2);
	draw_text("MIDI Bank Low", 36, 46, 0, 2);
	draw_text("MIDI Bank High", 36, 47, 0, 2);
}

/* --------------------------------------------------------------------- */
/* load_page functions */

static void _load_page_common(struct page *page, struct widget *page_widgets)
{
	int i;

	memset(saved_env, 0, sizeof(saved_env));
	for (i = 0; i < 10; i++) {
		saved_env[i].nodes = 2;
		saved_env[i].ticks[0] = 0;
		saved_env[i].ticks[1] = 100;
		saved_env[i].values[0] = 32;
		saved_env[i].values[1] = 32;
	}
	vgamem_ovl_alloc(&env_overlay);

	page->title = "Instrument List (F4)";
	page->pre_handle_key = instrument_list_pre_handle_key;
	page->handle_key = instrument_list_handle_key;
	page->widgets = page_widgets;
	page->help_index = HELP_INSTRUMENT_LIST;
	page->set_page = instrument_list_reposition;

	/* the first five widgets are the same for all four pages. */

	/* 0 = instrument list */
	widget_create_other(page_widgets + 0, 1, instrument_list_handle_key_on_list,
		instrument_list_handle_text_input_on_list, instrument_list_draw_list);
	page_widgets[0].accept_text = (instrument_cursor_pos == 25 ? 0 : 1);
	page_widgets[0].x = 5;
	page_widgets[0].y = 13;
	page_widgets[0].width = 24;
	page_widgets[0].height = 34;

	/* 1-4 = subpage switches */
	widget_create_togglebutton(page_widgets + 1, 32, 13, 7, 1, 5, 0, 2, 2, change_subpage, "General",
			    1, subpage_switches_group);
	widget_create_togglebutton(page_widgets + 2, 44, 13, 7, 2, 5, 1, 3, 3, change_subpage, "Volume",
			    1, subpage_switches_group);
	widget_create_togglebutton(page_widgets + 3, 56, 13, 7, 3, 5, 2, 4, 4, change_subpage, "Panning",
			    1, subpage_switches_group);
	widget_create_togglebutton(page_widgets + 4, 68, 13, 7, 4, 5, 3, 0, 0, change_subpage, "Pitch",
			    2, subpage_switches_group);
}

void instrument_list_general_load_page(struct page *page)
{
	_load_page_common(page, widgets_general);

	page->draw_const = instrument_list_general_draw_const;
	page->predraw_hook = instrument_list_general_predraw_hook;
	page->total_widgets = 18;

	/* special case stuff */
	widgets_general[1].d.togglebutton.state = 1;
	widgets_general[2].next.down = widgets_general[3].next.down = widgets_general[4].next.down = 6;

	/* 5 = note trans table */
	widget_create_other(widgets_general + 5, 6, note_trans_handle_key, NULL, note_trans_draw);
	widgets_general[5].x = 32;
	widgets_general[5].y = 16;
	widgets_general[5].width = 9;
	widgets_general[5].height = 31;
	widgets_general[5].next.down = 6;

	/* 6-9 = nna toggles */
	widget_create_togglebutton(widgets_general + 6, 46, 19, 29, 2, 7, 5, 0, 0,
			    instrument_list_general_update_values,
			    "Note Cut", 2, nna_group);
	widget_create_togglebutton(widgets_general + 7, 46, 22, 29, 6, 8, 5, 0, 0,
			    instrument_list_general_update_values,
			    "Continue", 2, nna_group);
	widget_create_togglebutton(widgets_general + 8, 46, 25, 29, 7, 9, 5, 0, 0,
			    instrument_list_general_update_values,
			    "Note Off", 2, nna_group);
	widget_create_togglebutton(widgets_general + 9, 46, 28, 29, 8, 10, 5, 0, 0,
			    instrument_list_general_update_values,
			    "Note Fade", 2, nna_group);

	/* 10-13 = dct toggles */
	widget_create_togglebutton(widgets_general + 10, 46, 34, 12, 9, 11, 5, 14,
			    14, instrument_list_general_update_values,
			    "Disabled", 2, dct_group);
	widget_create_togglebutton(widgets_general + 11, 46, 37, 12, 10, 12, 5, 15,
			    15, instrument_list_general_update_values,
			    "Note", 2, dct_group);
	widget_create_togglebutton(widgets_general + 12, 46, 40, 12, 11, 13, 5, 16,
			    16, instrument_list_general_update_values,
			    "Sample", 2, dct_group);
	widget_create_togglebutton(widgets_general + 13, 46, 43, 12, 12, 17, 5, 13,
			    13, instrument_list_general_update_values,
			    "Instrument", 2, dct_group);
	/* 14-16 = dca toggles */
	widget_create_togglebutton(widgets_general + 14, 62, 34, 13, 9, 15, 10, 0,
			    0, instrument_list_general_update_values,
			    "Note Cut", 2, dca_group);
	widget_create_togglebutton(widgets_general + 15, 62, 37, 13, 14, 16, 11, 0,
			    0, instrument_list_general_update_values,
			    "Note Off", 2, dca_group);
	widget_create_togglebutton(widgets_general + 16, 62, 40, 13, 15, 17, 12, 0,
			    0, instrument_list_general_update_values,
			    "Note Fade", 2, dca_group);
	/* 17 = filename */
	/* impulse tracker has a 17-char-wide box for the filename for
	 * some reason, though it still limits the actual text to 12
	 * characters. go figure... */
	widget_create_textentry(widgets_general + 17, 56, 47, 13, 13, 17, 0, update_filename,
			 NULL, 12);
}

static int _fixup_mouse_instpage_volume(struct key_event *k)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	if (envelope_mouse_edit && ins) {
		if (_env_handle_mouse(k, &ins->vol_env, &current_node_vol)) {
			ins->flags |= ENV_VOLUME;
			return 1;
		}
	}
	if ((k->sym == SCHISM_KEYSYM_l || k->sym == SCHISM_KEYSYM_b) && (k->mod & SCHISM_KEYMOD_ALT)) {
		return _env_handle_key_viewmode(k, &ins->vol_env, &current_node_vol, ENV_VOLUME);
	}
	return instrument_list_pre_handle_key(k);
}

void instrument_list_volume_load_page(struct page *page)
{
	_load_page_common(page, widgets_volume);

	page->pre_handle_key = _fixup_mouse_instpage_volume;
	page->draw_const = instrument_list_volume_draw_const;
	page->predraw_hook = instrument_list_volume_predraw_hook;
	page->total_widgets = 17;

	/* 5 = volume envelope */
	widget_create_other(widgets_volume + 5, 0, volume_envelope_handle_key, NULL, volume_envelope_draw);
	widgets_volume[5].x = 32;
	widgets_volume[5].y = 18;
	widgets_volume[5].width = 45;
	widgets_volume[5].height = 8;
	widgets_volume[5].next.down = 6;

	/* 6-7 = envelope switches */
	widget_create_toggle(widgets_volume + 6, 54, 28, 5, 7, 0, 0, 0,
		      instrument_list_volume_update_values);
	widget_create_toggle(widgets_volume + 7, 54, 29, 6, 8, 0, 0, 0,
		      instrument_list_volume_update_values);

	/* 8-10 envelope loop settings */
	widget_create_toggle(widgets_volume + 8, 54, 32, 7, 9, 0, 0, 0,
		      instrument_list_volume_update_values);
	widget_create_numentry(widgets_volume + 9, 54, 33, 3, 8, 10, 0,
			instrument_list_volume_update_values, 0, 1,
			numentry_cursor_pos + 0);
	widget_create_numentry(widgets_volume + 10, 54, 34, 3, 9, 11, 0,
			instrument_list_volume_update_values, 0, 1,
			numentry_cursor_pos + 0);

	/* 11-13 = susloop settings */
	widget_create_toggle(widgets_volume + 11, 54, 37, 10, 12, 0, 0, 0,
		      instrument_list_volume_update_values);
	widget_create_numentry(widgets_volume + 12, 54, 38, 3, 11, 13, 0,
			instrument_list_volume_update_values, 0, 1,
			numentry_cursor_pos + 0);
	widget_create_numentry(widgets_volume + 13, 54, 39, 3, 12, 14, 0,
			instrument_list_volume_update_values, 0, 1,
			numentry_cursor_pos + 0);

	/* 14-16 = volume thumbbars */
	widget_create_thumbbar(widgets_volume + 14, 54, 42, 17, 13, 15, 0,
			instrument_list_volume_update_values, 0, 128);
	widget_create_thumbbar(widgets_volume + 15, 54, 43, 17, 14, 16, 0,
			instrument_list_volume_update_values, 0, 256);
	widget_create_thumbbar(widgets_volume + 16, 54, 46, 17, 15, 16, 0,
			instrument_list_volume_update_values, 0, 100);
}

static int _fixup_mouse_instpage_panning(struct key_event *k)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	if (envelope_mouse_edit && ins) {
		if (_env_handle_mouse(k, &ins->pan_env, &current_node_pan)) {
			ins->flags |= ENV_PANNING;
			return 1;
		}
	}
	if ((k->sym == SCHISM_KEYSYM_l || k->sym == SCHISM_KEYSYM_b) && (k->mod & SCHISM_KEYMOD_ALT)) {
		return _env_handle_key_viewmode(k, &ins->pan_env, &current_node_pan, ENV_PANNING);
	}
	return instrument_list_pre_handle_key(k);
}
void instrument_list_panning_load_page(struct page *page)
{
	_load_page_common(page, widgets_panning);

	page->pre_handle_key = _fixup_mouse_instpage_panning;
	page->draw_const = instrument_list_panning_draw_const;
	page->predraw_hook = instrument_list_panning_predraw_hook;
	page->total_widgets = 19;

	/* 5 = panning envelope */
	widget_create_other(widgets_panning + 5, 0, panning_envelope_handle_key, NULL, panning_envelope_draw);
	widgets_panning[5].x = 32;
	widgets_panning[5].y = 18;
	widgets_panning[5].width = 45;
	widgets_panning[5].height = 8;
	widgets_panning[5].next.down = 6;

	/* 6-7 = envelope switches */
	widget_create_toggle(widgets_panning + 6, 54, 28, 5, 7, 0, 0, 0,
		      instrument_list_panning_update_values);
	widget_create_toggle(widgets_panning + 7, 54, 29, 6, 8, 0, 0, 0,
		      instrument_list_panning_update_values);

	/* 8-10 envelope loop settings */
	widget_create_toggle(widgets_panning + 8, 54, 32, 7, 9, 0, 0, 0,
		      instrument_list_panning_update_values);
	widget_create_numentry(widgets_panning + 9, 54, 33, 3, 8, 10, 0,
			instrument_list_panning_update_values, 0, 1,
			numentry_cursor_pos + 1);
	widget_create_numentry(widgets_panning + 10, 54, 34, 3, 9, 11, 0,
			instrument_list_panning_update_values, 0, 1,
			numentry_cursor_pos + 1);

	/* 11-13 = susloop settings */
	widget_create_toggle(widgets_panning + 11, 54, 37, 10, 12, 0, 0, 0,
		      instrument_list_panning_update_values);
	widget_create_numentry(widgets_panning + 12, 54, 38, 3, 11, 13, 0,
			instrument_list_panning_update_values, 0, 1,
			numentry_cursor_pos + 1);
	widget_create_numentry(widgets_panning + 13, 54, 39, 3, 12, 14, 0,
			instrument_list_panning_update_values, 0, 1,
			numentry_cursor_pos + 1);

	/* 14-15 = default panning */
	widget_create_toggle(widgets_panning + 14, 54, 42, 13, 15, 0, 0, 0,
		      instrument_list_panning_update_values);
	widget_create_thumbbar(widgets_panning + 15, 54, 43, 9, 14, 16, 0,
			instrument_list_panning_update_values, 0, 64);

	/* 16 = pitch-pan center */
	widget_create_other(widgets_panning + 16, 0, pitch_pan_center_handle_key, NULL, pitch_pan_center_draw);
	widgets_panning[16].next.up = 15;
	widgets_panning[16].next.down = 17;

	/* 17-18 = other panning stuff */
	widget_create_thumbbar(widgets_panning + 17, 54, 46, 9, 16, 18, 0,
			instrument_list_panning_update_values, -32, 32);
	widget_create_thumbbar(widgets_panning + 18, 54, 47, 9, 17, 18, 0,
			instrument_list_panning_update_values, 0, 64);
}

static int _fixup_mouse_instpage_pitch(struct key_event *k)
{
	song_instrument_t *ins = song_get_instrument(current_instrument);
	if (envelope_mouse_edit && ins) {
		if (_env_handle_mouse(k, &ins->pitch_env, &current_node_pitch)) {
			ins->flags |= ENV_PITCH;
			return 1;
		}
	}
	if ((k->sym == SCHISM_KEYSYM_l || k->sym == SCHISM_KEYSYM_b) && (k->mod & SCHISM_KEYMOD_ALT)) {
		return _env_handle_key_viewmode(k, &ins->pitch_env, &current_node_pitch, ENV_PITCH);
	}
	return instrument_list_pre_handle_key(k);
}
void instrument_list_pitch_load_page(struct page *page)
{
	static int midi_channel_selection_cursor_position = 0;

	_load_page_common(page, widgets_pitch);

	page->pre_handle_key = _fixup_mouse_instpage_pitch;
	page->draw_const = instrument_list_pitch_draw_const;
	page->predraw_hook = instrument_list_pitch_predraw_hook;
	page->total_widgets = 20;

	/* 5 = pitch envelope */
	widget_create_other(widgets_pitch + 5, 0, pitch_envelope_handle_key, NULL, pitch_envelope_draw);
	widgets_pitch[5].x = 32;
	widgets_pitch[5].y = 18;
	widgets_pitch[5].width = 45;
	widgets_pitch[5].height = 8;
	widgets_pitch[5].next.down = 6;

	/* 6-7 = envelope switches */
	widget_create_menutoggle(widgets_pitch + 6, 54, 28, 5, 7, 0, 0, 0,
		      instrument_list_pitch_update_values, pitch_envelope_states);
	widget_create_toggle(widgets_pitch + 7, 54, 29, 6, 8, 0, 0, 0,
		      instrument_list_pitch_update_values);

	/* 8-10 envelope loop settings */
	widget_create_toggle(widgets_pitch + 8, 54, 32, 7, 9, 0, 0, 0,
		      instrument_list_pitch_update_values);
	widget_create_numentry(widgets_pitch + 9, 54, 33, 3, 8, 10, 0,
			instrument_list_pitch_update_values, 0, 1,
			numentry_cursor_pos + 2);
	widget_create_numentry(widgets_pitch + 10, 54, 34, 3, 9, 11, 0,
			instrument_list_pitch_update_values, 0, 1,
			numentry_cursor_pos + 2);

	/* 11-13 = susloop settings */
	widget_create_toggle(widgets_pitch + 11, 54, 37, 10, 12, 0, 0, 0,
		      instrument_list_pitch_update_values);
	widget_create_numentry(widgets_pitch + 12, 54, 38, 3, 11, 13, 0,
			instrument_list_pitch_update_values, 0, 1,
			numentry_cursor_pos + 2);
	widget_create_numentry(widgets_pitch + 13, 54, 39, 3, 12, 14, 0,
			instrument_list_pitch_update_values, 0, 1,
			numentry_cursor_pos + 2);

	/* 14-15 = filter cutoff/resonance */
	widget_create_thumbbar(widgets_pitch + 14, 54, 42, 17, 13, 15, 0,
			instrument_list_pitch_update_values, -1, 127);
	widget_create_thumbbar(widgets_pitch + 15, 54, 43, 17, 14, 16, 0,
			instrument_list_pitch_update_values, -1, 127);
	widgets_pitch[14].d.thumbbar.text_at_min = "Off";
	widgets_pitch[15].d.thumbbar.text_at_min = "Off";

	/* 16-19 = midi crap */
	widget_create_bitset(widgets_pitch + 16, 54, 44, 17, 15, 17, 0,
			instrument_list_pitch_update_values,
			17,
			" 1 2 3 4 5 6 7 8 9P\0""111213141516M\0",
			".\0.\0.\0.\0.\0.\0.\0.\0.\0p\0.\0.\0.\0.\0.\0.\0m\0",
			&midi_channel_selection_cursor_position
			);
	widgets_pitch[16].d.bitset.activation_keys =
		"123456789pabcdefm";

	widget_create_thumbbar(widgets_pitch + 17, 54, 45, 17, 16, 18, 0,
			instrument_list_pitch_update_values, -1, 127);
	widget_create_thumbbar(widgets_pitch + 18, 54, 46, 17, 17, 19, 0,
			instrument_list_pitch_update_values, -1, 127);
	widget_create_thumbbar(widgets_pitch + 19, 54, 47, 17, 18, 19, 0,
			instrument_list_pitch_update_values, -1, 127);
	widgets_pitch[17].d.thumbbar.text_at_min = "Off";
	widgets_pitch[18].d.thumbbar.text_at_min = "Off";
	widgets_pitch[19].d.thumbbar.text_at_min = "Off";

	/* count how many formats there really are */
	num_save_formats = 0;
	while (instrument_save_formats[num_save_formats].label)
		num_save_formats++;
}

