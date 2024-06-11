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
#include "song.h"
#include "page.h"
#include "charset.h"
#include "util.h"
#include "midi.h"
#include "version.h"
#include "video.h"

#include "sdlmain.h"

#include <assert.h>
#include <math.h>

/* --------------------------------------------------------------------- */
/* globals */

struct tracker_status status = {
	.current_page = PAGE_BLANK,
	.previous_page = PAGE_BLANK,
	.current_help_index = HELP_GLOBAL,
	.dialog_type = DIALOG_NONE,
	.flags = IS_FOCUSED | IS_VISIBLE,
	.time_display = TIME_PLAY_ELAPSED,
	.vis_style = VIS_VU_METER,
	.last_midi_event = "",
	// everything else set to 0/NULL/etc.
};

struct page pages[PAGE_MAX] = {0};

struct widget *widgets = NULL;
int *selected_widget = NULL;
int *total_widgets = NULL;

static int fontedit_return_page = PAGE_PATTERN_EDITOR;

/* --------------------------------------------------------------------- */

static struct {
	int h, m, s;
} current_time = {0, 0, 0};

extern int playback_tracing;    /* scroll lock */
extern int midi_playback_tracing;

/* return 1 -> the time changed; need to redraw */
static int check_time(void)
{
	static int last_o = -1, last_r = -1, last_timep = -1;

	time_t timep = 0;
	int h, m, s;
	enum tracker_time_display td = status.time_display;
	int is_playing = song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP);

	int row, order;

	switch (td) {
	case TIME_PLAY_ELAPSED:
		td = (is_playing ? TIME_PLAYBACK : TIME_ELAPSED);
		break;
	case TIME_PLAY_CLOCK:
		td = (is_playing ? TIME_PLAYBACK : TIME_CLOCK);
		break;
	case TIME_PLAY_OFF:
		td = (is_playing ? TIME_PLAYBACK : TIME_OFF);
		break;
	default:
		break;
	}

	switch (td) {
	case TIME_OFF:
		h = m = s = 0;
		break;
	case TIME_PLAYBACK:
		h = (m = (s = song_get_current_time()) / 60) / 60;
		break;
	case TIME_ELAPSED:
		h = (m = (s = SDL_GetTicks() / 1000) / 60) / 60;
		break;
	case TIME_ABSOLUTE:
		/* absolute time shows the time of the current cursor
		position in the pattern editor :) */
		if (status.current_page == PAGE_PATTERN_EDITOR) {
			row = get_current_row();
			order = song_next_order_for_pattern(get_current_pattern());
		} else {
			order = get_current_order();
			row = 0;
		}
		if (order < 0) {
			s = m = h = 0;
		} else {
			if (last_o == order && last_r == row) {
				timep = last_timep;
			} else {
				last_timep = timep = song_get_length_to(order, row);
				last_o = order;
				last_r = row;
			}
			s = timep % 60;
			m = (timep / 60) % 60;
			h = (timep / 3600);
		}
		break;
	default:
		/* this will never happen */
	case TIME_CLOCK:
		/* Impulse Tracker doesn't have this, but I always wanted it, so here 'tis. */
		h = status.tmnow.tm_hour;
		m = status.tmnow.tm_min;
		s = status.tmnow.tm_sec;
		break;
	}

	if (h == current_time.h && m == current_time.m && s == current_time.s) {
		return 0;
	}

	current_time.h = h;
	current_time.m = m;
	current_time.s = s;
	return 1;
}

static inline void draw_time(void)
{
	char buf[16];
	int is_playing = song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP);

	if (status.time_display == TIME_OFF || (status.time_display == TIME_PLAY_OFF && !is_playing))
		return;

	/* this allows for 999 hours... that's like... 41 days...
	 * who on earth leaves a tracker running for 41 days? */
	sprintf(buf, "%3d:%02d:%02d", current_time.h % 1000,
		current_time.m % 60, current_time.s % 60);
	draw_text(buf, 69, 9, 0, 2);
}

/* --------------------------------------------------------------------- */

static void draw_page_title(void)
{
	int x, tpos, tlen = strlen(ACTIVE_PAGE.title);

	if (tlen > 0) {
		tpos = 41 - ((tlen + 1) / 2);

		for (x = 1; x < tpos - 1; x++)
			draw_char(154, x, 11, 1, 2);
		draw_char(0, tpos - 1, 11, 1, 2);
		draw_text(ACTIVE_PAGE.title, tpos, 11, 0, 2);
		draw_char(0, tpos + tlen, 11, 1, 2);
		for (x = tpos + tlen + 1; x < 79; x++)
			draw_char(154, x, 11, 1, 2);
	} else {
		for (x = 1; x < 79; x++)
			draw_char(154, x, 11, 1, 2);
	}
}

/* --------------------------------------------------------------------- */
/* Not that happy with the way this function developed, but well, it still
 * works. Maybe someday I'll make it suck less. */

static void draw_page(void)
{
	int n = ACTIVE_PAGE.total_widgets;

	if (ACTIVE_PAGE.draw_full) {
		ACTIVE_PAGE.draw_full();
	} else {

		draw_page_title();
		if (ACTIVE_PAGE.draw_const) ACTIVE_PAGE.draw_const();
		if (ACTIVE_PAGE.predraw_hook) ACTIVE_PAGE.predraw_hook();
	}

	/* this doesn't use widgets[] because it needs to draw the page's
	 * widgets whether or not a dialog is active */
	while (n--)
		draw_widget(ACTIVE_PAGE.widgets + n, n == ACTIVE_PAGE.selected_widget);

	/* redraw the area over the menu if there is one */
	if (status.dialog_type & DIALOG_MENU)
		menu_draw();
	else if (status.dialog_type & DIALOG_BOX)
		dialog_draw();
}

/* --------------------------------------------------------------------- */

inline int page_is_instrument_list(int page)
{
	switch (page) {
	case PAGE_INSTRUMENT_LIST_GENERAL:
	case PAGE_INSTRUMENT_LIST_VOLUME:
	case PAGE_INSTRUMENT_LIST_PANNING:
	case PAGE_INSTRUMENT_LIST_PITCH:
		return 1;
	default:
		return 0;
	}
}

/* --------------------------------------------------------------------------------------------------------- */

static struct widget new_song_widgets[10] = {0};
static const int new_song_groups[4][3] = { {0, 1, -1}, {2, 3, -1}, {4, 5, -1}, {6, 7, -1} };

static void new_song_ok(UNUSED void *data)
{
	int flags = 0;
	if (new_song_widgets[0].d.togglebutton.state)
		flags |= KEEP_PATTERNS;
	if (new_song_widgets[2].d.togglebutton.state)
		flags |= KEEP_SAMPLES;
	if (new_song_widgets[4].d.togglebutton.state)
		flags |= KEEP_INSTRUMENTS;
	if (new_song_widgets[6].d.togglebutton.state)
		flags |= KEEP_ORDERLIST;
	song_new(flags);
}

static void new_song_draw_const(void)
{
	draw_text("New Song", 36, 21, 3, 2);
	draw_text("Patterns", 26, 24, 0, 2);
	draw_text("Samples", 27, 27, 0, 2);
	draw_text("Instruments", 23, 30, 0, 2);
	draw_text("Order List", 24, 33, 0, 2);
}

void new_song_dialog(void)
{
	struct dialog *dialog;

	/* only create everything if it hasn't been set up already */
	if (new_song_widgets[0].width == 0) {
		create_togglebutton(new_song_widgets + 0, 35, 24, 6, 0, 2, 1, 1, 1, NULL, "Keep",
				    2, new_song_groups[0]);
		create_togglebutton(new_song_widgets + 1, 45, 24, 7, 1, 3, 0, 0, 0, NULL, "Clear",
				    2, new_song_groups[0]);
		create_togglebutton(new_song_widgets + 2, 35, 27, 6, 0, 4, 3, 3, 3, NULL, "Keep",
				    2, new_song_groups[1]);
		create_togglebutton(new_song_widgets + 3, 45, 27, 7, 1, 5, 2, 2, 2, NULL, "Clear",
				    2, new_song_groups[1]);
		create_togglebutton(new_song_widgets + 4, 35, 30, 6, 2, 6, 5, 5, 5, NULL, "Keep",
				    2, new_song_groups[2]);
		create_togglebutton(new_song_widgets + 5, 45, 30, 7, 3, 7, 4, 4, 4, NULL, "Clear",
				    2, new_song_groups[2]);
		create_togglebutton(new_song_widgets + 6, 35, 33, 6, 4, 8, 7, 7, 7, NULL, "Keep",
				    2, new_song_groups[3]);
		create_togglebutton(new_song_widgets + 7, 45, 33, 7, 5, 9, 6, 6, 6, NULL, "Clear",
				    2, new_song_groups[3]);
		create_button(new_song_widgets + 8, 28, 36, 8, 6, 8, 9, 9, 9, dialog_yes_NULL, "OK", 4);
		create_button(new_song_widgets + 9, 41, 36, 8, 6, 9, 8, 8, 8, dialog_cancel_NULL, "Cancel", 2);
		togglebutton_set(new_song_widgets, 1, 0);
		togglebutton_set(new_song_widgets, 3, 0);
		togglebutton_set(new_song_widgets, 5, 0);
		togglebutton_set(new_song_widgets, 7, 0);
	}

	dialog = dialog_create_custom(21, 20, 38, 19, new_song_widgets, 10, 8, new_song_draw_const, NULL);
	dialog->action_yes = new_song_ok;
}

/* --------------------------------------------------------------------------------------------------------- */
/* This is an ugly monster. */
/* Jesus, you're right. WTF is all this? I'm lost. :/ -storlek */

static int _mp_active = 0;
static struct widget _mpw[1];
static void (*_mp_setv)(int v) = NULL;
static void (*_mp_setv_noplay)(int v) = NULL;
static const char *_mp_text = "";
static int _mp_text_x, _mp_text_y;

static void _mp_draw(void)
{
	const char *name = NULL;
	int n, i;

	if (_mp_text[0] == '!') {
		/* inst */
		n = instrument_get_current();
		if (n)
			name = song_get_instrument(n)->name;
		else
			name = "(No Instrument)";
	} else if (_mp_text[0] == '@') {
		/* samp */
		n = sample_get_current();
		if (n > 0)
			name = song_get_sample(n)->name;
		else
			name = "(No Sample)";
	} else {
		name = _mp_text;
	}
	i = strlen(name);
	draw_fill_chars(_mp_text_x, _mp_text_y, _mp_text_x + 17, _mp_text_y, 2);
	draw_text_len( name, 17, _mp_text_x, _mp_text_y, 0, 2);
	if (i < 17 && name == _mp_text) {
		draw_char(':', _mp_text_x + i, _mp_text_y, 0, 2);
	}
	draw_box(_mp_text_x, _mp_text_y + 1, _mp_text_x + 14, _mp_text_y + 3,
		 BOX_THIN | BOX_INNER | BOX_INSET);
}

static void _mp_change(void)
{
	if (_mp_setv) _mp_setv(_mpw[0].d.thumbbar.value);
	if (!(song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP))) {
		if (_mp_setv_noplay)
			_mp_setv_noplay(_mpw[0].d.thumbbar.value);
	}
	_mp_active = 2;
}

static void _mp_finish(UNUSED void *ign)
{
	if (_mp_active) {
		dialog_destroy_all();
		_mp_active = 0;
	}
}

static void minipop_slide(int cv, const char *name, int min, int max,
	void (*setv)(int v), void (*setv_noplay)(int v), int midx, int midy)
{
	if (_mp_active == 1) {
		_mp_active = 2;
		return;
	}
	_mp_text = name;
	_mp_text_x = midx - 9;
	_mp_text_y = midy - 2;
	_mp_setv = setv;
	_mp_setv_noplay = setv_noplay;
	create_thumbbar(_mpw, midx - 8, midy, 13, 0, 0, 0, _mp_change, min, max);
	_mpw[0].d.thumbbar.value = CLAMP(cv, min, max);
	_mpw[0].depressed = 1; /* maybe it just needs some zoloft? */
	dialog_create_custom(midx - 10, midy - 3,  20, 6, _mpw, 1, 0, _mp_draw, NULL);
	/* warp mouse to position of slider knob */
	if (max == 0) max = 1; /* prevent division by zero */
	SDL_WarpMouseInWindow(
		video_window(),
		video_width()*((midx - 8)*8 + (cv - min)*96.0/(max - min) + 1)/640,
		video_height()*midy*8/400.0 + 4);

	_mp_active = 1;
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------------------------------------------- */
/* text input handler */

void handle_text_input(const uint8_t* text_input) {
	if (widget_handle_text_input(text_input)) return;

	if (!(status.dialog_type & DIALOG_BOX) && ACTIVE_PAGE.handle_text_input)
		ACTIVE_PAGE.handle_text_input(text_input);
}

/* --------------------------------------------------------------------------------------------------------- */

/* returns 1 if the key was handled */
static int handle_key_global(struct key_event * k)
{
	int i, ins_mode;

	if (_mp_active == 2 && (k->mouse == MOUSE_CLICK && k->state == KEY_RELEASE)) {
		status.flags |= NEED_UPDATE;
		dialog_destroy_all();
		_mp_active = 0;
		// eat it...
		return 1;
	}
	if ((!_mp_active) && k->state == KEY_PRESS && k->mouse == MOUSE_CLICK) {
		if (k->x >= 63 && k->x <= 77 && k->y >= 6 && k->y <= 7) {
			status.vis_style++;
			status.vis_style %= VIS_SENTINEL;
			status.flags |= NEED_UPDATE;
			return 1;
		} else if (k->y == 5 && k->x == 50) {
			minipop_slide(kbd_get_current_octave(), "Octave", 0, 8,
				kbd_set_current_octave, NULL, 50, 5);
			return 1;
		} else if (k->y == 4 && k->x >= 50 && k->x <= 52) {
			minipop_slide(song_get_current_speed(), "Speed", 1, 255,
				song_set_current_speed, song_set_initial_speed, 51, 4);
			return 1;
		} else if (k->y == 4 && k->x >= 54 && k->x <= 56) {
			minipop_slide(song_get_current_tempo(), "Tempo", 32, 255,
				song_set_current_tempo, song_set_initial_tempo, 55, 4);
			return 1;
		} else if (k->y == 3 && k->x >= 50 && k-> x <= 77) {
			if (page_is_instrument_list(status.current_page)
			    || status.current_page == PAGE_SAMPLE_LIST
			    || (!(status.flags & CLASSIC_MODE)
				&& (status.current_page == PAGE_ORDERLIST_PANNING
				    || status.current_page == PAGE_ORDERLIST_VOLUMES)))
				ins_mode = 0;
			else
				ins_mode = song_is_instrument_mode();
			if (ins_mode) {
				minipop_slide(instrument_get_current(), "Instrument",
					status.current_page == PAGE_INSTRUMENT_LIST ? 1 : 0,
					99 /* FIXME */, instrument_set, NULL, 58, 3);
			} else {
				minipop_slide(sample_get_current(), "Sample",
					status.current_page == PAGE_SAMPLE_LIST ? 1 : 0,
					99 /* FIXME */, sample_set, NULL, 58, 3);
			}

		} else if (k->x >= 12 && k->x <= 18) {
			if (k->y == 7) {
				minipop_slide(get_current_row(), "Row",
					0, song_get_rows_in_pattern(get_current_pattern()),
					set_current_row, NULL, 14, 7);
				return 1;
			} else if (k->y == 6) {
				minipop_slide(get_current_pattern(), "Pattern",
					0, csf_get_num_patterns(current_song),
					set_current_pattern, NULL, 14, 6);
				return 1;
			} else if (k->y == 5) {
				minipop_slide(get_current_order(), "Order",
					0, csf_get_num_orders(current_song),
					set_current_order, NULL, 14, 5);
				return 1;
			}
		}
	} else if ((!_mp_active) && k->mouse == MOUSE_DBLCLICK) {
		if (k->y == 4 && k->x >= 11 && k->x <= 28) {
			set_page(PAGE_SAVE_MODULE);
			return 1;
		} else if (k->y == 3 && k->x >= 11 && k->x <= 35) {
			set_page(PAGE_SONG_VARIABLES);
			return 1;
		}
	}

	/* shortcut */
	if (k->mouse != MOUSE_NONE) {
		return 0;
	}

	/* first, check the truly global keys (the ones that still work if
	 * a dialog's open) */
	switch (k->sym) {
	case SDLK_RETURN:
		if ((k->mod & KMOD_CTRL) && k->mod & KMOD_ALT) {
			if (k->state == KEY_PRESS)
				return 1;
			toggle_display_fullscreen();
			return 1;
		}
		break;
	case SDLK_m:
		if (k->mod & KMOD_CTRL) {
			if (k->state == KEY_RELEASE)
				return 1;
			video_mousecursor(MOUSE_CYCLE_STATE);
			return 1;
		}
		break;

	case SDLK_d:
		if (k->mod & KMOD_CTRL) {
			if (k->state == KEY_RELEASE)
				return 1; /* argh */
			const SDL_bool grabbed = !SDL_GetWindowGrab(video_window());
			SDL_SetWindowGrab(video_window(), grabbed);
			status_text_flash(grabbed
				? "Mouse and keyboard grabbed, press Ctrl+D to release"
				: "Mouse and keyboard released");
			return 1;
		}
		break;

	case SDLK_i:
		/* reset audio stuff? */
		if (k->mod & KMOD_CTRL) {
			if (k->state == KEY_RELEASE)
				return 1;
			audio_reinit(NULL);
			return 1;
		}
		break;
	case SDLK_e:
		/* This should reset everything display-related. */
		if (k->mod & KMOD_CTRL) {
			if (k->state == KEY_RELEASE)
				return 1;
			font_init();
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	case SDLK_HOME:
		if (!(k->mod & KMOD_ALT)) break;
		if (status.flags & DISKWRITER_ACTIVE) break;
		if (k->state == KEY_RELEASE)
			return 0;
		kbd_set_current_octave(kbd_get_current_octave() - 1);
		return 1;
	case SDLK_END:
		if (!(k->mod & KMOD_ALT)) break;
		if (status.flags & DISKWRITER_ACTIVE) break;
		if (k->state == KEY_RELEASE)
			return 0;
		kbd_set_current_octave(kbd_get_current_octave() + 1);
		return 1;
	default:
		break;
	}

	/* next, if there's no dialog, check the rest of the keys */
	if (status.flags & DISKWRITER_ACTIVE) return 0;

	switch (k->sym) {
	case SDLK_q:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS) {
				if (k->mod & KMOD_SHIFT)
					schism_exit(0);
				show_exit_prompt();
			}
			return 1;
		}
		break;
	case SDLK_n:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				new_song_dialog();
			return 1;
		}
		break;
	case SDLK_g:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				show_song_timejump();
			return 1;
		}
		break;
	case SDLK_p:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				show_song_length();
			return 1;
		}
		break;
	case SDLK_F1:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_CONFIG);
		} else if (k->mod & KMOD_SHIFT) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(status.current_page == PAGE_MIDI ? PAGE_MIDI_OUTPUT : PAGE_MIDI);
		} else if (NO_MODIFIER(k->mod)) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_HELP);
		} else {
			break;
		}
		return 1;
	case SDLK_F2:
		if (k->mod & KMOD_CTRL) {
			if (status.current_page == PAGE_PATTERN_EDITOR) {
				_mp_finish(NULL);
				if (k->state == KEY_PRESS && status.dialog_type == DIALOG_NONE) {
					pattern_editor_length_edit();
				}
				return 1;
			}
			if (status.dialog_type != DIALOG_NONE)
				return 0;
		} else if (NO_MODIFIER(k->mod)) {
			if (status.current_page == PAGE_PATTERN_EDITOR) {
				if (k->state == KEY_PRESS) {
					if (status.dialog_type & DIALOG_MENU) {
						return 0;
					} else if (status.dialog_type != DIALOG_NONE) {
						dialog_yes_NULL();
						status.flags |= NEED_UPDATE;
					} else {
						_mp_finish(NULL);
						pattern_editor_display_options();
					}
				}
			} else {
				if (status.dialog_type != DIALOG_NONE)
					return 0;
				_mp_finish(NULL);
				if (k->state == KEY_PRESS)
					set_page(PAGE_PATTERN_EDITOR);
			}
			return 1;
		}
		break;
	case SDLK_F3:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (NO_MODIFIER(k->mod)) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_SAMPLE_LIST);
		} else {
			_mp_finish(NULL);
			if (k->mod & KMOD_CTRL) set_page(PAGE_LIBRARY_SAMPLE);
			break;
		}
		return 1;
	case SDLK_F4:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (NO_MODIFIER(k->mod)) {
			if (status.current_page == PAGE_INSTRUMENT_LIST) return 0;
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_INSTRUMENT_LIST);
		} else {
			if (k->mod & KMOD_SHIFT) return 0;
			_mp_finish(NULL);
			if (k->mod & KMOD_CTRL) set_page(PAGE_LIBRARY_INSTRUMENT);
			break;
		}
		return 1;
	case SDLK_F5:
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				song_start();
		} else if (k->mod & KMOD_SHIFT) {
			if (status.dialog_type != DIALOG_NONE)
				return 0;
			_mp_finish(NULL);
			if (k->state == KEY_RELEASE)
				set_page(PAGE_PREFERENCES);
		} else if (NO_MODIFIER(k->mod)) {
			if (song_get_mode() == MODE_STOPPED
			|| (song_get_mode() == MODE_SINGLE_STEP && status.current_page == PAGE_INFO)) {
				_mp_finish(NULL);
				if (k->state == KEY_PRESS)
					song_start();
			}
			if (k->state == KEY_PRESS) {
				if (status.dialog_type != DIALOG_NONE)
					return 0;
				_mp_finish(NULL);
				set_page(PAGE_INFO);
			}
		} else {
			break;
		}
		return 1;
	case SDLK_F6:
		if (k->mod & KMOD_SHIFT) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				song_start_at_order(get_current_order(), 0);
		} else if (NO_MODIFIER(k->mod)) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				song_loop_pattern(get_current_pattern(), 0);
		} else {
			break;
		}
		return 1;
	case SDLK_F7:
		if (NO_MODIFIER(k->mod)) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				play_song_from_mark();
		} else {
			break;
		}
		return 1;
	case SDLK_F8:
		if (k->mod & KMOD_SHIFT) {
			if (k->state == KEY_PRESS)
				song_pause();
		} else if (NO_MODIFIER(k->mod)) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				song_stop();
			status.flags |= NEED_UPDATE;
		} else {
			break;
		}
		return 1;
	case SDLK_F9:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_SHIFT) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_MESSAGE);
		} else if (NO_MODIFIER(k->mod)) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_LOAD_MODULE);
		} else {
			break;
		}
		return 1;
	case SDLK_l:
	case SDLK_r:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_RELEASE)
				set_page(PAGE_LOAD_MODULE);
		} else {
			break;
		}
		return 1;
	case SDLK_s:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_RELEASE)
				save_song_or_save_as();
		} else {
			break;
		}
		return 1;
	case SDLK_w:
		/* Ctrl-W _IS_ in IT, and hands don't leave home row :) */
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_RELEASE)
				set_page(PAGE_SAVE_MODULE);
		} else {
			break;
		}
		return 1;
	case SDLK_F10:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (k->mod & KMOD_ALT) break;
		if (k->mod & KMOD_CTRL) break;

		_mp_finish(NULL);
		if (k->mod & KMOD_SHIFT) {
			if (k->state == KEY_PRESS)
				set_page(PAGE_EXPORT_MODULE);
		} else {
			if (k->state == KEY_PRESS)
				set_page(PAGE_SAVE_MODULE);
		}
		return 1;
	case SDLK_F11:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if (NO_MODIFIER(k->mod)) {
			_mp_finish(NULL);
			if (status.current_page == PAGE_ORDERLIST_PANNING) {
				if (k->state == KEY_PRESS)
					set_page(PAGE_ORDERLIST_VOLUMES);
			} else {
				if (k->state == KEY_PRESS)
					set_page(PAGE_ORDERLIST_PANNING);
			}
		} else if (k->mod & KMOD_CTRL) {
			if (k->state == KEY_PRESS) {
				_mp_finish(NULL);
				if (status.current_page == PAGE_LOG) {
					show_about();
				} else {
					set_page(PAGE_LOG);
				}
			}
		} else if (k->state == KEY_PRESS && (k->mod & KMOD_ALT)) {
			_mp_finish(NULL);
			if (song_toggle_orderlist_locked())
				status_text_flash("Order list locked");
			else
				status_text_flash("Order list unlocked");
		} else {
			break;
		}
		return 1;
	case SDLK_F12:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		if ((k->mod & KMOD_ALT) && status.current_page == PAGE_INFO) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_WATERFALL);
		} else if (k->mod & KMOD_CTRL) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_PALETTE_EDITOR);
		} else if (k->mod & KMOD_SHIFT) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS) {
				fontedit_return_page = status.current_page;
				set_page(PAGE_FONT_EDIT);
			}

		} else if (NO_MODIFIER(k->mod)) {
			_mp_finish(NULL);
			if (k->state == KEY_PRESS)
				set_page(PAGE_SONG_VARIABLES);
		} else {
			break;
		}
		return 1;
	/* hack alert */
	case SDLK_f:
		if (!(k->mod & KMOD_CTRL))
			return 0;
		/* fall through */
	case SDLK_SCROLLLOCK:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		_mp_finish(NULL);
		if (k->mod & KMOD_ALT) {
			if (k->state == KEY_PRESS) {
				midi_flags ^= (MIDI_DISABLE_RECORD);
				status_text_flash("MIDI Input %s",
					(midi_flags & MIDI_DISABLE_RECORD)
					? "Disabled" : "Enabled");
			}
			return 1;
		} else {
			/* os x steals plain scroll lock for brightness,
			 * so catch ctrl+scroll lock here as well */
			if (k->state == KEY_PRESS) {
				midi_playback_tracing = (playback_tracing = !playback_tracing);
				status_text_flash("Playback tracing %s",
						  (playback_tracing ? "enabled" : "disabled"));
			}
			return 1;
		}
	default:
		if (status.dialog_type != DIALOG_NONE)
			return 0;
		break;
	}

	/* got a bit ugly here, sorry */
	i = k->sym;
	if (k->mod & KMOD_ALT) {
		switch (i) {
		case SDLK_F1: i = 0; break;
		case SDLK_F2: i = 1; break;
		case SDLK_F3: i = 2; break;
		case SDLK_F4: i = 3; break;
		case SDLK_F5: i = 4; break;
		case SDLK_F6: i = 5; break;
		case SDLK_F7: i = 6; break;
		case SDLK_F8: i = 7; break;
		default:
			return 0;
		};
		if (k->state == KEY_RELEASE)
			return 1;

		song_toggle_channel_mute(i);
		status.flags |= NEED_UPDATE;
		return 1;
	}

	/* oh well */
	return 0;
}

static int _handle_ime(struct key_event *k)
{
	int c, m;
	static int alt_numpad = 0;
	static int alt_numpad_c = 0;
	static int digraph_n = 0;
	static int digraph_c = 0;
	static int cs_unicode = 0;
	static int cs_unicode_c = 0;

	if (ACTIVE_PAGE.selected_widget > -1 && ACTIVE_PAGE.selected_widget < ACTIVE_PAGE.total_widgets
	    && ACTIVE_PAGE.widgets[ACTIVE_PAGE.selected_widget].accept_text) {
		if (digraph_n == -1 && k->state == KEY_RELEASE) {
			digraph_n = 0;

		} else if (!(status.flags & CLASSIC_MODE) && (k->sym == SDLK_LCTRL || k->sym == SDLK_RCTRL)) {
			if (k->state == KEY_RELEASE && digraph_n >= 0) {
				digraph_n++;
				if (digraph_n >= 2)
					status_text_flash_bios("Enter digraph:");
			}
		} else if (k->sym == SDLK_LSHIFT || k->sym == SDLK_RSHIFT) {
			/* do nothing */
		} else if (!NO_MODIFIER((k->mod&~KMOD_SHIFT)) || (c=(k->text) ? *k->text : k->sym) == 0 || digraph_n < 2) {
			if (k->state == KEY_PRESS && k->mouse == MOUSE_NONE) {
				if (digraph_n > 0) status_text_flash(" ");
				digraph_n = -1;
			}
		} else if (digraph_n >= 2) {
			if (k->state == KEY_RELEASE)
				return 1;
			if (!digraph_c) {
				digraph_c = c;
				status_text_flash_bios("Enter digraph: %c", c);
			} else {
				uint8_t digraph_input[2] = {char_digraph(digraph_c, c), '\0'};
				if (digraph_input[0]) {
					status_text_flash_bios("Enter digraph: %c%c -> %c",
							       digraph_c, c, digraph_input[0]);
				} else {
					status_text_flash_bios("Enter digraph: %c%c -> INVALID", digraph_c, c);
				}
				digraph_n = digraph_c = 0;
				if (digraph_input[0]) {
					handle_text_input((const uint8_t*)digraph_input);
				}
			}
			return 1;
		} else {
			if (digraph_n > 0) status_text_flash(" ");
			digraph_n = 0;
		}

		/* ctrl+shift -> unicode character */
		if (k->sym==SDLK_LCTRL || k->sym==SDLK_RCTRL || k->sym==SDLK_LSHIFT || k->sym==SDLK_RSHIFT) {
			if (k->state == KEY_RELEASE) {
				if (cs_unicode_c > 0) {
					uint8_t unicode[2] = {(uint8_t)(char_unicode_to_cp437(cs_unicode)), '\0'};

					if (unicode[0] >= 32) {
						status_text_flash_bios("Enter Unicode: U+%04X -> %c",
									   cs_unicode, unicode[0]);
						SDL_SetModState(0);
						handle_text_input((const uint8_t*)unicode);
					} else {
						status_text_flash_bios("Enter Unicode: U+%04X -> INVALID", cs_unicode);
					}
					cs_unicode = cs_unicode_c = 0;
					alt_numpad = alt_numpad_c = 0;
					digraph_n = digraph_c = 0;
				}
				return 1;
			}
		} else if (!(status.flags & CLASSIC_MODE) && (k->mod & KMOD_CTRL) && (k->mod & KMOD_SHIFT)) {
			if (cs_unicode_c >= 0) {
				/* bleh... */
				SDL_Keycode sym = k->sym;
				m = k->mod;

				k->sym = k->orig_sym;
				k->mod = 0;

				c = kbd_char_to_hex(k);

				k->sym = sym;
				k->mod = m;

				if (c == -1) {
					cs_unicode = cs_unicode_c = -1;
				} else {
					if (k->state == KEY_PRESS) return 1;
					cs_unicode *= 16;
					cs_unicode += c;
					cs_unicode_c++;
					digraph_n = digraph_c = 0;
					status_text_flash_bios("Enter Unicode: U+%04X", cs_unicode);
					return 1;
				}
			}
		} else {
			if (k->sym==SDLK_LCTRL || k->sym==SDLK_RCTRL || k->sym==SDLK_LSHIFT || k->sym==SDLK_RSHIFT) {
				return 1;
			}
			cs_unicode = cs_unicode_c = 0;
		}

		/* alt+numpad -> char number */
		if (k->sym == SDLK_LALT || k->sym == SDLK_RALT
			|| k->sym == SDLK_LGUI || k->sym == SDLK_RGUI) {
			if (k->state == KEY_RELEASE && alt_numpad_c > 0 && (alt_numpad & 255) > 0) {\
				if (alt_numpad < 32)
					return 0;
				uint8_t unicode[2] = {(uint8_t)(alt_numpad & 255), '\0'};
				if (!(status.flags & CLASSIC_MODE))
					status_text_flash_bios("Enter DOS/ASCII: %d -> %c",
							       (int)unicode[0], (int)unicode[0]);
				handle_text_input((const uint8_t*)unicode);
				alt_numpad = alt_numpad_c = 0;
				digraph_n = digraph_c = 0;
				cs_unicode = cs_unicode_c = 0;
				return 1;
			}
		} else if (k->mod & KMOD_ALT && !(k->mod & (KMOD_CTRL|KMOD_SHIFT))) {
			if (alt_numpad_c >= 0) {
				m = k->mod;
				k->mod = 0;
				c = numeric_key_event(k, 1); /* kp only */
				k->mod = m;
				if (c == -1 || c > 9) {
					alt_numpad = alt_numpad_c = -1;
				} else {
					if (k->state == KEY_PRESS) return 1;
					alt_numpad *= 10;
					alt_numpad += c;
					alt_numpad_c++;
					if (!(status.flags & CLASSIC_MODE))
						status_text_flash_bios("Enter DOS/ASCII: %d", (int)alt_numpad);
					return 1;
				}
			}
		} else {
			alt_numpad = alt_numpad_c = 0;
		}
	} else {
		cs_unicode = cs_unicode_c = 0;
		alt_numpad = alt_numpad_c = 0;
		digraph_n = digraph_c = 0;
	}

	return 0;
}

/* this is the important one */
void handle_key(struct key_event *k)
{
	if (_handle_ime(k))
		return;

	/* okay... */
	if (!(status.flags & DISKWRITER_ACTIVE) && ACTIVE_PAGE.pre_handle_key) {
		if (ACTIVE_PAGE.pre_handle_key(k)) return;
	}

	if (handle_key_global(k)) return;
	if (!(status.flags & DISKWRITER_ACTIVE) && menu_handle_key(k)) return;
	if (widget_handle_key(k)) return;

	/* now check a couple other keys. */
	switch (k->sym) {
	case SDLK_LEFT:
		if (k->state == KEY_RELEASE) return;
		if (status.flags & DISKWRITER_ACTIVE) return;
		if ((k->mod & KMOD_CTRL) && status.current_page != PAGE_PATTERN_EDITOR) {
			_mp_finish(NULL);
			if (song_get_mode() == MODE_PLAYING)
				song_set_current_order(song_get_current_order() - 1);
			return;
		}
		break;
	case SDLK_RIGHT:
		if (k->state == KEY_RELEASE) return;
		if (status.flags & DISKWRITER_ACTIVE) return;
		if ((k->mod & KMOD_CTRL) && status.current_page != PAGE_PATTERN_EDITOR) {
			_mp_finish(NULL);
			if (song_get_mode() == MODE_PLAYING)
				song_set_current_order(song_get_current_order() + 1);
			return;
		}
		break;
	case SDLK_ESCAPE:
		/* TODO | Page key handlers should return true/false depending on if the key was handled
		   TODO | (same as with other handlers), and the escape key check should go *after* the
		   TODO | page gets a chance to grab it. This way, the load sample page can switch back
		   TODO | to the sample list on escape like it's supposed to. (The status.current_page
		   TODO | checks above won't be necessary, either.) */
		if (NO_MODIFIER(k->mod) && status.dialog_type == DIALOG_NONE
		    && status.current_page != PAGE_LOAD_SAMPLE
		    && status.current_page != PAGE_LOAD_INSTRUMENT) {
			if (k->state == KEY_RELEASE) return;
			if (_mp_active) {
				_mp_finish(NULL);
				return;
			}
			menu_show();
			return;
		}
		break;
	case SDLK_SLASH:
		if (k->state == KEY_RELEASE) return;
		if (status.flags & DISKWRITER_ACTIVE) return;
		if (k->orig_sym == SDLK_KP_DIVIDE) {
			kbd_set_current_octave(kbd_get_current_octave() - 1);
		}
		return;
	case SDLK_ASTERISK:
		if (k->state == KEY_RELEASE) return;
		if (status.flags & DISKWRITER_ACTIVE) return;
		if (k->orig_sym == SDLK_KP_MULTIPLY) {
			kbd_set_current_octave(kbd_get_current_octave() + 1);
		}
		return;
	case SDLK_LEFTBRACKET:
		if (k->state == KEY_RELEASE) break;
		if (status.flags & DISKWRITER_ACTIVE) return;
		if (k->mod & KMOD_SHIFT) {
			song_set_current_speed(song_get_current_speed() - 1);
			status_text_flash("Speed set to %d frames per row", song_get_current_speed());
			if (!(song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP))) {
				song_set_initial_speed(song_get_current_speed());
			}
		} else if ((k->mod & KMOD_CTRL) && !(status.flags & CLASSIC_MODE)) {
			song_set_current_tempo(song_get_current_tempo() - 1);
			status_text_flash("Tempo set to %d frames per row", song_get_current_tempo());
			if (!(song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP))) {
				song_set_initial_tempo(song_get_current_tempo());
			}
		} else if (NO_MODIFIER(k->mod)) {
			song_set_current_global_volume(song_get_current_global_volume() - 1);
			status_text_flash("Global volume set to %d", song_get_current_global_volume());
			if (!(song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP))) {
				song_set_initial_global_volume(song_get_current_global_volume());
			}
		}
		return;
	case SDLK_RIGHTBRACKET:
		if (k->state == KEY_RELEASE) break;
		if (status.flags & DISKWRITER_ACTIVE) return;
		if (k->mod & KMOD_SHIFT) {
			song_set_current_speed(song_get_current_speed() + 1);
			status_text_flash("Speed set to %d frames per row", song_get_current_speed());
			if (!(song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP))) {
				song_set_initial_speed(song_get_current_speed());
			}
		} else if ((k->mod & KMOD_CTRL) && !(status.flags & CLASSIC_MODE)) {
			song_set_current_tempo(song_get_current_tempo() + 1);
			status_text_flash("Tempo set to %d frames per row", song_get_current_tempo());
			if (!(song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP))) {
				song_set_initial_tempo(song_get_current_tempo());
			}
		} else if (NO_MODIFIER(k->mod)) {
			song_set_current_global_volume(song_get_current_global_volume() + 1);
			status_text_flash("Global volume set to %d", song_get_current_global_volume());
			if (!(song_get_mode() & (MODE_PLAYING | MODE_PATTERN_LOOP))) {
				song_set_initial_global_volume(song_get_current_global_volume());
			}
		}
		return;

	default:
		break;
	}

	/* and if we STILL didn't handle the key, pass it to the page.
	 * (or dialog, if one's active) */
	if (status.dialog_type & DIALOG_BOX) {
		dialog_handle_key(k);
	} else {
		if (status.flags & DISKWRITER_ACTIVE) return;
		if (ACTIVE_PAGE.handle_key) ACTIVE_PAGE.handle_key(k);
	}
}

/* --------------------------------------------------------------------- */
static void draw_top_info_const(void)
{
	int n, tl, br;

	if (status.flags & INVERTED_PALETTE) {
		tl = 3;
		br = 1;
	} else {
		tl = 1;
		br = 3;
	}

	draw_text(schism_banner(status.flags & CLASSIC_MODE),
		(80 - strlen(schism_banner(status.flags & CLASSIC_MODE))) / 2, 1, 0, 2);
	draw_text("Song Name", 2, 3, 0, 2);
	draw_text("File Name", 2, 4, 0, 2);
	draw_text("Order", 6, 5, 0, 2);
	draw_text("Pattern", 4, 6, 0, 2);
	draw_text("Row", 8, 7, 0, 2);

	draw_text("Speed/Tempo", 38, 4, 0, 2);
	draw_text("Octave", 43, 5, 0, 2);

	draw_text("F1...Help       F9.....Load", 21, 6, 0, 2);
	draw_text("ESC..Main Menu  F5/F8..Play / Stop", 21, 7, 0, 2);

	/* the neat-looking (but incredibly ugly to draw) borders */
	draw_char(128, 30, 4, br, 2);
	draw_char(128, 57, 4, br, 2);
	draw_char(128, 19, 5, br, 2);
	draw_char(128, 51, 5, br, 2);
	draw_char(129, 36, 4, br, 2);
	draw_char(129, 50, 6, br, 2);
	draw_char(129, 17, 8, br, 2);
	draw_char(129, 18, 8, br, 2);
	draw_char(131, 37, 3, br, 2);
	draw_char(131, 78, 3, br, 2);
	draw_char(131, 19, 6, br, 2);
	draw_char(131, 19, 7, br, 2);
	draw_char(132, 49, 3, tl, 2);
	draw_char(132, 49, 4, tl, 2);
	draw_char(132, 49, 5, tl, 2);
	draw_char(134, 75, 2, tl, 2);
	draw_char(134, 76, 2, tl, 2);
	draw_char(134, 77, 2, tl, 2);
	draw_char(136, 37, 4, br, 2);
	draw_char(136, 78, 4, br, 2);
	draw_char(136, 30, 5, br, 2);
	draw_char(136, 57, 5, br, 2);
	draw_char(136, 51, 6, br, 2);
	draw_char(136, 19, 8, br, 2);
	draw_char(137, 49, 6, br, 2);
	draw_char(137, 11, 8, br, 2);
	draw_char(138, 37, 2, tl, 2);
	draw_char(138, 78, 2, tl, 2);
	draw_char(139, 11, 2, tl, 2);
	draw_char(139, 49, 2, tl, 2);

	for (n = 0; n < 5; n++) {
		draw_char(132, 11, 3 + n, tl, 2);
		draw_char(129, 12 + n, 8, br, 2);
		draw_char(134, 12 + n, 2, tl, 2);
		draw_char(129, 20 + n, 5, br, 2);
		draw_char(129, 31 + n, 4, br, 2);
		draw_char(134, 32 + n, 2, tl, 2);
		draw_char(134, 50 + n, 2, tl, 2);
		draw_char(129, 52 + n, 5, br, 2);
		draw_char(129, 58 + n, 4, br, 2);
		draw_char(134, 70 + n, 2, tl, 2);
	}
	for (; n < 10; n++) {
		draw_char(134, 12 + n, 2, tl, 2);
		draw_char(129, 20 + n, 5, br, 2);
		draw_char(134, 50 + n, 2, tl, 2);
		draw_char(129, 58 + n, 4, br, 2);
	}
	for (; n < 20; n++) {
		draw_char(134, 12 + n, 2, tl, 2);
		draw_char(134, 50 + n, 2, tl, 2);
		draw_char(129, 58 + n, 4, br, 2);
	}

	draw_text("Time", 63, 9, 0, 2);
	draw_char('/', 15, 5, 1, 0);
	draw_char('/', 15, 6, 1, 0);
	draw_char('/', 15, 7, 1, 0);
	draw_char('/', 53, 4, 1, 0);
	draw_char(':', 52, 3, 7, 0);
}

/* --------------------------------------------------------------------- */

void update_current_instrument(void)
{
	int ins_mode, n;
	char *name = NULL;
	char buf[4];

	if (page_is_instrument_list(status.current_page)
	|| status.current_page == PAGE_SAMPLE_LIST
	|| status.current_page == PAGE_LOAD_SAMPLE
	|| status.current_page == PAGE_LIBRARY_SAMPLE
	|| (!(status.flags & CLASSIC_MODE)
		&& (status.current_page == PAGE_ORDERLIST_PANNING
			|| status.current_page == PAGE_ORDERLIST_VOLUMES)))
		ins_mode = 0;
	else
		ins_mode = song_is_instrument_mode();

	if (ins_mode) {
		draw_text("Instrument", 39, 3, 0, 2);
		n = instrument_get_current();
		if (n > 0)
			name = song_get_instrument(n)->name;
	} else {
		draw_text("    Sample", 39, 3, 0, 2);
		n = sample_get_current();
		if (n > 0)
			name = song_get_sample(n)->name;
	}

	if (n > 0) {
		draw_text(num99tostr(n, buf), 50, 3, 5, 0);
		draw_text_len(name, 25, 53, 3, 5, 0);
	} else {
		draw_text("..", 50, 3, 5, 0);
		draw_text(".........................", 53, 3, 5, 0);
	}
}

static void redraw_top_info(void)
{
	char buf[8];

	update_current_instrument();

	draw_text_len(song_get_basename(), 18, 12, 4, 5, 0);
	draw_text_len(current_song->title, 25, 12, 3, 5, 0);

	if ((status.flags & (CLASSIC_MODE | SONG_NEEDS_SAVE)) == SONG_NEEDS_SAVE)
		draw_char('+', 29, 4, 4, 0);

	update_current_order();
	update_current_pattern();
	update_current_row();

	draw_text(numtostr(3, song_get_current_speed(), buf), 50, 4, 5, 0);
	draw_text(numtostr(3, song_get_current_tempo(), buf), 54, 4, 5, 0);
	draw_char('0' + kbd_get_current_octave(), 50, 5, 5, 0);
}

static void _draw_vis_box(void)
{
	draw_box(62, 5, 78, 8, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_fill_chars(63, 6, 77, 7, 0);
}

static int _vis_virgin = 1;
static struct vgamem_overlay vis_overlay = {
	63, 6, 77, 7,
	NULL, 0, 0, 0,
};

extern short current_fft_data[2][1024];
extern short fftlog[256];
/* convert the fft bands to columns of the vis box
out and d have a range of 0 to 128 */
static inline void _get_columns_from_fft(unsigned char *out, short d[2][1024])
{
	int i, j, jbis, t, a;
	/*this assumes out of size 120. */
	for (i = 0, t= 0 , a=0; i < 120; i++, t+=2)  {
		float afloat = fftlog[t];
		float floora = floor(afloat);
		if (afloat + 1.0f > fftlog[t+1]) {
			a = (int)floora;
			j = d[0][a] + (d[0][a+1]-d[0][a])*(afloat-floora);
			jbis = d[1][a] + (d[1][a+1]-d[1][a])*(afloat-floora);
			j = MAX(j,jbis);
			a = floor(afloat+0.5f);
		}
		else {
			j=d[0][a];
			j = MAX(j,d[1][a]);
			while(a<=afloat){
				j = MAX(j,d[0][a]);
				j = MAX(j,d[1][a]);
				a++;
			}
		}
		*out = j; out++;
	}
}
static void vis_fft(void)
{
	int i, y;
	/*this is the size of vis_overlay.width*/
	unsigned char outfft[120];

	if (_vis_virgin) {
		vgamem_ovl_alloc(&vis_overlay);
		_vis_virgin = 0;
	}
	_draw_vis_box();
	song_lock_audio();

	vgamem_ovl_clear(&vis_overlay,0);
	_get_columns_from_fft(outfft,current_fft_data);
	for (i = 0; i < 120; i++) {
		y = outfft[i];
		/*reduce range */
		y >>= 3;
		if (y > 15) y = 15;
		if (y > 0) {
		       vgamem_ovl_drawline(&vis_overlay,i,15-y,i,15,5);
		}
	}
	vgamem_ovl_apply(&vis_overlay);

	song_unlock_audio();
}
static void vis_oscilloscope(void)
{
	if (_vis_virgin) {
		vgamem_ovl_alloc(&vis_overlay);
		_vis_virgin = 0;
	}
	_draw_vis_box();
	song_lock_audio();
	if (status.vis_style == VIS_MONOSCOPE) {
		if (audio_output_bits == 16) {
			draw_sample_data_rect_16(&vis_overlay,audio_buffer,
					audio_buffer_samples,
					audio_output_channels,1);
		} else {
			draw_sample_data_rect_8(&vis_overlay,(void*)audio_buffer,
					audio_buffer_samples,
					audio_output_channels,1);
		}
	} else if (audio_output_bits == 16) {
		draw_sample_data_rect_16(&vis_overlay,audio_buffer,audio_buffer_samples,
					audio_output_channels,audio_output_channels);
	} else {
		draw_sample_data_rect_8(&vis_overlay,(void *)audio_buffer,audio_buffer_samples,
					audio_output_channels,audio_output_channels);
	}
	song_unlock_audio();
}

static void vis_vu_meter(void)
{
	int left, right;

	song_get_vu_meter(&left, &right);
	left >>= 1;
	right >>= 1;

	_draw_vis_box();
	draw_vu_meter(63, 6, 15, left, 5, 4);
	draw_vu_meter(63, 7, 15, right, 5, 4);
}

static void vis_fakemem(void)
{
	char buf[32];
	unsigned int conv;
	unsigned int ems;

	if (status.flags & CLASSIC_MODE) {
		ems = memused_ems();
		if (ems > 67108864) ems = 0;
		else ems = 67108864 - ems;

		conv = memused_lowmem();
		if (conv > 524288) conv = 0;
		else conv = 524288 - conv;

		conv >>= 10;
		ems >>= 10;

		sprintf(buf, "FreeMem %uk", conv);
		draw_text(buf, 63, 6, 0, 2);
		sprintf(buf, "FreeEMS %uk", ems);
		draw_text(buf, 63, 7, 0, 2);
	} else {
		sprintf(buf, "   Song %uk",
				(unsigned)(
					(memused_patterns()
					 +memused_instruments()
					 +memused_songmessage()) >> 10));
		draw_text(buf, 63, 6, 0, 2);
		sprintf(buf, "Samples %uk", (unsigned)(memused_samples() >> 10));
		draw_text(buf, 63, 7, 0, 2);
	}
}

static inline void draw_vis(void)
{
	if (status.flags & CLASSIC_MODE) {
		/* classic mode requires fakemem display */
		vis_fakemem();
		return;
	}
	switch (status.vis_style) {
	case VIS_FAKEMEM:
		vis_fakemem();
		break;
	case VIS_OSCILLOSCOPE:
	case VIS_MONOSCOPE:
		vis_oscilloscope();
		break;
	case VIS_VU_METER:
		vis_vu_meter();
		break;
	case VIS_FFT:
		vis_fft();
		break;
	default:
	case VIS_OFF:
		break;
	}
}

/* this completely redraws everything. */
void redraw_screen(void)
{
	int n;
	char buf[4];

	if (!ACTIVE_PAGE.draw_full) {
		draw_fill_chars(0,0,79,49,2);

		/* border around the whole screen */
		draw_char(128, 0, 0, 3, 2);
		for (n = 79; n > 49; n--)
			draw_char(129, n, 0, 3, 2);
		do {
			draw_char(129, n, 0, 3, 2);
			draw_char(131, 0, n, 3, 2);
		} while (--n);

		draw_top_info_const();
		redraw_top_info();
	}

	if (!ACTIVE_PAGE.draw_full) {
		draw_vis();
		draw_time();
		draw_text(numtostr(3, song_get_current_speed(), buf), 50, 4, 5, 0);
		draw_text(numtostr(3, song_get_current_tempo(), buf), 54, 4, 5, 0);

		status_text_redraw();
	}

	draw_page();

}

/* important :) */
void playback_update(void)
{
	/* the order here is significant -- check_time has side effects */
	if (check_time() || song_get_mode())
		status.flags |= NEED_UPDATE;

	if (ACTIVE_PAGE.playback_update) ACTIVE_PAGE.playback_update();
}

/* --------------------------------------------------------------------- */

static void _set_from_f3(void)
{
	switch (status.previous_page) {
	case PAGE_ORDERLIST_PANNING:
	case PAGE_ORDERLIST_VOLUMES:
		if (status.flags & CLASSIC_MODE) return;
	case PAGE_SAMPLE_LIST:
		if (song_is_instrument_mode())
			instrument_synchronize_to_sample();
		else
			instrument_set(sample_get_current());
	};
}

static void _set_from_f4(void)
{
	switch (status.previous_page) {
	case PAGE_ORDERLIST_PANNING:
	case PAGE_ORDERLIST_VOLUMES:
		if (status.flags & CLASSIC_MODE) break;
	case PAGE_SAMPLE_LIST:
	case PAGE_LOAD_SAMPLE:
	case PAGE_LIBRARY_SAMPLE:
		return;
/*
 * storlek says pattern editor syncs...
	case PAGE_PATTERN_EDITOR:
*/
	};

	if (song_is_instrument_mode()) {
		sample_synchronize_to_instrument();
	}
}
void set_page(int new_page)
{
	int prev_page = status.current_page;


	if (new_page != prev_page)
		status.previous_page = prev_page;
	status.current_page = new_page;

	_set_from_f3();
	_set_from_f4();

	if (new_page != PAGE_HELP)
		status.current_help_index = ACTIVE_PAGE.help_index;

	if (status.dialog_type & DIALOG_MENU) {
		menu_hide();
	} else if (status.dialog_type != DIALOG_NONE) {
		return;
	}

	/* update the pointers */
	widgets = ACTIVE_PAGE.widgets;
	selected_widget = &(ACTIVE_PAGE.selected_widget);
	total_widgets = &(ACTIVE_PAGE.total_widgets);

	if (ACTIVE_PAGE.set_page) ACTIVE_PAGE.set_page();
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

void load_pages(void)
{
	blank_load_page(pages + PAGE_BLANK);
	help_load_page(pages + PAGE_HELP);
	pattern_editor_load_page(pages + PAGE_PATTERN_EDITOR);
	sample_list_load_page(pages + PAGE_SAMPLE_LIST);
	instrument_list_general_load_page(pages + PAGE_INSTRUMENT_LIST_GENERAL);
	instrument_list_volume_load_page(pages + PAGE_INSTRUMENT_LIST_VOLUME);
	instrument_list_panning_load_page(pages + PAGE_INSTRUMENT_LIST_PANNING);
	instrument_list_pitch_load_page(pages + PAGE_INSTRUMENT_LIST_PITCH);
	info_load_page(pages + PAGE_INFO);
	preferences_load_page(pages + PAGE_PREFERENCES);
	midi_load_page(pages + PAGE_MIDI);
	midiout_load_page(pages + PAGE_MIDI_OUTPUT);
	fontedit_load_page(pages + PAGE_FONT_EDIT);
	load_module_load_page(pages + PAGE_LOAD_MODULE);
	save_module_load_page(pages + PAGE_SAVE_MODULE, 0);
	orderpan_load_page(pages + PAGE_ORDERLIST_PANNING);
	ordervol_load_page(pages + PAGE_ORDERLIST_VOLUMES);
	song_vars_load_page(pages + PAGE_SONG_VARIABLES);
	palette_load_page(pages + PAGE_PALETTE_EDITOR);
	message_load_page(pages + PAGE_MESSAGE);
	log_load_page(pages + PAGE_LOG);
	load_sample_load_page(pages + PAGE_LOAD_SAMPLE);
	library_sample_load_page(pages + PAGE_LIBRARY_SAMPLE);
	load_instrument_load_page(pages + PAGE_LOAD_INSTRUMENT);
	library_instrument_load_page(pages + PAGE_LIBRARY_INSTRUMENT);
	waterfall_load_page(pages + PAGE_WATERFALL);
	about_load_page(pages+PAGE_ABOUT);
	config_load_page(pages + PAGE_CONFIG);
	save_module_load_page(pages + PAGE_EXPORT_MODULE, 1);

	widgets = pages[PAGE_BLANK].widgets;
	selected_widget = &(pages[PAGE_BLANK].selected_widget);
	total_widgets = &(pages[PAGE_BLANK].total_widgets);
}

/* --------------------------------------------------------------------- */
/* this function's name sucks, but I don't know what else to call it. */
void main_song_changed_cb(void)
{
	int n;

	/* perhaps this should be in page_patedit.c? */
	set_current_order(0);
	n = current_song->orderlist[0];
	if (n > 199)
		n = 0;
	set_current_pattern(n);
	set_current_row(0);
	song_save_channel_states();

	for (n = ARRAY_SIZE(pages) - 1; n >= 0; n--) {
		if (pages[n].song_changed_cb)
			pages[n].song_changed_cb();
	}

	/* TODO | print some message like "new song created" if there's
	 * TODO | no filename, and thus no file. (but DON'T print it the
	 * TODO | very first time this is called) */

	status.flags |= NEED_UPDATE;
	memused_songchanged();
}

/* --------------------------------------------------------------------- */
/* not sure where else to toss this crap */

static void savecheck(void (*ok)(void *data), void (*cancel)(void *data), void *data)
{
	if (status.flags & SONG_NEEDS_SAVE) {
		dialog_create(DIALOG_OK_CANCEL, "Current module not saved. Proceed?", ok, cancel, 1, data);
	} else {
		ok(data);
	}
}

static void exit_ok_confirm(UNUSED void *data)
{
	schism_exit(0);
}

static void exit_ok(UNUSED void *data)
{
	savecheck(exit_ok_confirm, NULL, NULL);
}

static void real_load_ok(void *filename)
{
	if (song_load_unchecked(filename)) {
		set_page((song_get_mode() == MODE_PLAYING) ? PAGE_INFO : PAGE_LOG);
	} else {
		set_page(PAGE_LOG);
	}
	free(filename);
}

void song_load(const char *filename)
{
	savecheck(real_load_ok, free, str_dup(filename));
}

void show_exit_prompt(void)
{
	/* This used to kill all open dialogs, but that doesn't seem to be necessary.
	Do keep in mind though, a dialog *might* exist when this function is called
	(for example, if the WM sends a close request). */

	if (status.current_page == PAGE_ABOUT) {
		/* haven't even started up yet; don't bother confirming */
		schism_exit(0);
	} else if (status.current_page == PAGE_FONT_EDIT) {
		if (status.flags & STARTUP_FONTEDIT) {
			dialog_create(DIALOG_OK_CANCEL, "Exit Font Editor?",
				      exit_ok_confirm, NULL, 0, NULL);
		} else {
			/* don't ask, just go away */
			dialog_destroy_all();
			set_page(fontedit_return_page);
		}
	} else {
		dialog_create(DIALOG_OK_CANCEL,
			      ((status.flags & CLASSIC_MODE)
			       ? "Exit Impulse Tracker?"
			       : "Exit Schism Tracker?"),
			      exit_ok, NULL, 0, NULL);
	}
}

static struct widget _timejump_widgets[4];
static int _tj_num1 = 0, _tj_num2 = 0;

static int _timejump_keyh(struct key_event *k)
{
	if (k->sym == SDLK_BACKSPACE) {
		if (*selected_widget == 1 && _timejump_widgets[1].d.numentry.value == 0) {
			if (k->state == KEY_RELEASE) change_focus_to(0);
			return 1;
		}
	}
	if (k->sym == SDLK_COLON || k->sym == SDLK_SEMICOLON) {
		if (k->state == KEY_RELEASE) {
			if (*selected_widget == 0) {
				change_focus_to(1);
			}
		}
		return 1;
	}
	return 0;
}

static void _timejump_draw(void)
{
	draw_text("Jump to time:", 30, 26, 0, 2);

	draw_char(':', 46, 26, 3, 0);
	draw_box(43, 25, 49, 27, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void _timejump_ok(UNUSED void *ign)
{
	unsigned long sec;
	int no, np, nr;
	sec = (_timejump_widgets[0].d.numentry.value * 60)
		+ _timejump_widgets[1].d.numentry.value;
	song_get_at_time(sec, &no, &nr);
	set_current_order(no);
	np = current_song->orderlist[no];
	if (np < 200) {
		set_current_pattern(np);
		set_current_row(nr);
		set_page(PAGE_PATTERN_EDITOR);
	}
}

void show_song_timejump(void)
{
	struct dialog *d;
	_tj_num1 = _tj_num2 = 0;
	create_numentry(_timejump_widgets+0, 44, 26, 2, 0, 2, 1, NULL, 0, 21, &_tj_num1);
	create_numentry(_timejump_widgets+1, 47, 26, 2, 1, 2, 2, NULL, 0, 59, &_tj_num2);
	_timejump_widgets[0].d.numentry.handle_unknown_key = _timejump_keyh;
	_timejump_widgets[0].d.numentry.reverse = 1;
	_timejump_widgets[1].d.numentry.reverse = 1;
	create_button(_timejump_widgets+2, 30, 29, 8, 0, 2, 2, 3, 3, (void(*)(void))_timejump_ok, "OK", 4);
	create_button(_timejump_widgets+3, 42, 29, 8, 1, 3, 3, 3, 0, dialog_cancel_NULL, "Cancel", 2);
	d = dialog_create_custom(26, 24, 30, 8, _timejump_widgets, 4, 0, _timejump_draw, NULL);
	d->handle_key = _timejump_keyh;
	d->action_yes = _timejump_ok;
}

void show_length_dialog(const char *label, unsigned int length)
{
	char *buf;

	if (asprintf(&buf, "%s: %3u:%02u:%02u", label, length / 3600, (length / 60) % 60, length % 60) == -1) {
		perror("asprintf");
		return;
	}
	dialog_create(DIALOG_OK, buf, free, free, 0, buf);
}

void show_song_length(void)
{
	show_length_dialog("Total song time", csf_get_length(current_song));
}

/* FIXME this is an illogical place to put this but whatever, i just want
to get the frigging thing to build */
void set_previous_instrument(void)
{
	if (song_is_instrument_mode())
		instrument_set(instrument_get_current() - 1);
	else
		sample_set(sample_get_current() - 1);
}

void set_next_instrument(void)
{
	if (song_is_instrument_mode())
		instrument_set(instrument_get_current() + 1);
	else
		sample_set(sample_get_current() + 1);
}

