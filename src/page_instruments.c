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

/* This is getting almost as disturbing as the pattern editor. */

#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"

#include <SDL.h>

/* --------------------------------------------------------------------- */
/* just one global variable... */

int instrument_list_subpage = PAGE_INSTRUMENT_LIST_GENERAL;

/* --------------------------------------------------------------------- */
/* ... but tons o' ugly statics */

static struct widget widgets_general[18];
static struct widget widgets_volume[17];
static struct widget widgets_panning[19];
static struct widget widgets_pitch[20];

static int subpage_switches_group[5] = { 1, 2, 3, 4, -1 };
static int nna_group[5] = { 6, 7, 8, 9, -1 };
static int dct_group[5] = { 10, 11, 12, 13, -1 };
static int dca_group[4] = { 14, 15, 16, -1 };

static const char *pitch_envelope_states[] = { "Off", "On Pitch", "On Filter", NULL };

static int top_instrument = 1;
static int current_instrument = 1;
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

/* playback */
static int last_note = 61;		/* C-5 */

/* --------------------------------------------------------------------------------------------------------- */

static void instrument_list_draw_list(void);

/* --------------------------------------------------------------------------------------------------------- */
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
	song_instrument *ins;

        if (page_is_instrument_list(status.current_page)) {
                new_ins = CLAMP(n, 1, 99);
        } else {
                new_ins = CLAMP(n, 0, 99);
        }

        if (current_instrument == new_ins)
                return;

        status.flags = (status.flags & ~SAMPLE_CHANGED) | INSTRUMENT_CHANGED;
        current_instrument = new_ins;
        instrument_list_reposition();

	ins = song_get_instrument(current_instrument, NULL);
	
	current_node_vol = ins->vol_env.nodes ? CLAMP(current_node_vol, 0, ins->vol_env.nodes - 1) : 0;
	current_node_pan = ins->pan_env.nodes ? CLAMP(current_node_vol, 0, ins->pan_env.nodes - 1) : 0;
	current_node_pitch = ins->pitch_env.nodes ? CLAMP(current_node_vol, 0, ins->pan_env.nodes - 1) : 0;
	
        status.flags |= NEED_UPDATE;
}

void instrument_synchronize_to_sample(void)
{
        song_instrument *ins;
        int sample = sample_get_current();
        int n, pos;

        /* 1. if the instrument with the same number as the current sample
         * has the sample in its sample_map, change to that instrument. */
        ins = song_get_instrument(sample, NULL);
        for (pos = 0; pos < 120; pos++) {
                if ((ins->sample_map[pos]) == sample) {
                        instrument_set(sample);
                        return;
                }
        }

        /* 2. look through the instrument list for the first instrument
         * that uses the selected sample. */
        for (n = 1; n < 100; n++) {
                if (n == sample)
                        continue;
                ins = song_get_instrument(n, NULL);
                for (pos = 0; pos < 120; pos++) {
                        if ((ins->sample_map[pos]) == sample) {
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

static int instrument_list_add_char(char c)
{
        char *name;

        if (c < 32)
                return 0;
        song_get_instrument(current_instrument, &name);
        text_add_char(name, c, &instrument_cursor_pos, 25);
        if (instrument_cursor_pos == 25)
                instrument_cursor_pos--;

        status.flags |= NEED_UPDATE;
        return 1;
}

static void instrument_list_delete_char(void)
{
        char *name;
        song_get_instrument(current_instrument, &name);
        text_delete_char(name, &instrument_cursor_pos, 25);

        status.flags |= NEED_UPDATE;
}

static void instrument_list_delete_next_char(void)
{
        char *name;
        song_get_instrument(current_instrument, &name);
        text_delete_next_char(name, &instrument_cursor_pos, 25);

        status.flags |= NEED_UPDATE;
}

static void clear_instrument_text(void)
{
        char *name;

        memset(song_get_instrument(current_instrument, &name)->filename, 0, 14);
        memset(name, 0, 26);
        if (instrument_cursor_pos != 25)
                instrument_cursor_pos = 0;

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static struct widget swap_instrument_widgets[6];
static char swap_instrument_entry[4] = "";


static void do_swap_instrument(UNUSED void *data)
{
	int n = atoi(swap_instrument_entry);
	
	if (n < 1 || n > 99)
		return;
	song_swap_instruments(current_instrument, n);
}

static void swap_instrument_draw_const(void)
{
	draw_text("Swap instrument with:", 29, 25, 0, 2);
	draw_text("Instrument", 31, 27, 0, 2);
	draw_box(41, 26, 45, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void swap_instrument_dialog(void)
{
	struct dialog *dialog;
	
	swap_instrument_entry[0] = 0;
	create_textentry(swap_instrument_widgets + 0, 42, 27, 3, 1, 1, 1, NULL, swap_instrument_entry, 2);
	create_button(swap_instrument_widgets + 1, 36, 30, 6, 0, 0, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(26, 23, 29, 10, swap_instrument_widgets, 2, 0,
	                              swap_instrument_draw_const, NULL);
	dialog->action_yes = do_swap_instrument;
}


static void do_exchange_instrument(UNUSED void *data)
{
	int n = atoi(swap_instrument_entry);
	
	if (n < 1 || n > 99)
		return;
	song_exchange_instruments(current_instrument, n);
}

static void exchange_instrument_draw_const(void)
{
	draw_text("Exchange instrument with:", 28, 25, 0, 2);
	draw_text("Instrument", 31, 27, 0, 2);
	draw_box(41, 26, 45, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void exchange_instrument_dialog(void)
{
	struct dialog *dialog;
	
	swap_instrument_entry[0] = 0;
	create_textentry(swap_instrument_widgets + 0, 42, 27, 3, 1, 1, 1, NULL, swap_instrument_entry, 2);
	create_button(swap_instrument_widgets + 1, 36, 30, 6, 0, 0, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(26, 23, 29, 10, swap_instrument_widgets, 2, 0,
	                              exchange_instrument_draw_const, NULL);
	dialog->action_yes = do_exchange_instrument;
}

/* --------------------------------------------------------------------- */

static void instrument_list_draw_list(void)
{
        int pos, n;
        song_instrument *ins;
        int selected = (ACTIVE_PAGE.selected_widget == 0);
        int is_current;
        int is_playing[100];
        byte buf[4];
	
	song_get_playing_instruments(is_playing);
	
        for (pos = 0, n = top_instrument; pos < 35; pos++, n++) {
                ins = song_get_instrument(n, NULL);
                is_current = (n == current_instrument);

                if (ins->played)
                	draw_char(173, 1, 13 + pos, is_playing[n] ? 3 : 1, 2);

                draw_text(numtostr(2, n, buf), 2, 13 + pos, 0, 2);
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
        }
}

static int instrument_list_handle_key_on_list(SDL_keysym * k)
{
        int new_ins = current_instrument;

        switch (k->sym) {
        case SDLK_UP:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_ins--;
                break;
        case SDLK_DOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_ins++;
                break;
        case SDLK_PAGEUP:
                if (k->mod & KMOD_CTRL)
                        new_ins = 1;
                else
                        new_ins -= 16;
                break;
        case SDLK_PAGEDOWN:
                if (k->mod & KMOD_CTRL)
                        new_ins = 99;
                else
                        new_ins += 16;
                break;
        case SDLK_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
                if (instrument_cursor_pos < 25) {
                        instrument_cursor_pos = 0;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
                if (instrument_cursor_pos < 24) {
                        instrument_cursor_pos = 24;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
                if (instrument_cursor_pos < 25 && instrument_cursor_pos > 0) {
                        instrument_cursor_pos--;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
                if (instrument_cursor_pos == 25) {
                        change_focus_to(1);
                } else if (instrument_cursor_pos < 24) {
                        instrument_cursor_pos++;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                if (instrument_cursor_pos < 25) {
                        instrument_cursor_pos = 25;
                        status.flags |= NEED_UPDATE;
                } else {
			status_text_flash("TODO: load-instrument page");
                }
                return 1;
        case SDLK_ESCAPE:
                if (instrument_cursor_pos < 25) {
                        instrument_cursor_pos = 25;
                        status.flags |= NEED_UPDATE;
                        return 1;
                }
                return 0;
        case SDLK_BACKSPACE:
                if (instrument_cursor_pos == 25)
                        return 0;
                if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0)
                        instrument_list_delete_char();
                else if (k->mod & KMOD_CTRL)
                        instrument_list_add_char(127);
                return 1;
	case SDLK_INSERT:
		if (k->mod & (KMOD_ALT | KMOD_META)) {
			song_insert_instrument_slot(current_instrument);
			status.flags |= NEED_UPDATE;
			return 1;
		}
		return 0;
        case SDLK_DELETE:
        	if (k->mod & (KMOD_ALT | KMOD_META)) {
        		song_remove_instrument_slot(current_instrument);
        		status.flags |= NEED_UPDATE;
        		return 1;
        	} else if ((k->mod & KMOD_CTRL) == 0) {
        		if (instrument_cursor_pos == 25)
        			return 0;
       	                instrument_list_delete_next_char();
        	        return 1;
        	}
        	return 0;
        default:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        if (k->sym == SDLK_c) {
                                clear_instrument_text();
                                return 1;
                        }
                } else if ((k->mod & KMOD_CTRL) == 0) {
                        char c = unicode_to_ascii(k->unicode);
                        if (c == 0)
                                return 0;

                        if (instrument_cursor_pos < 25) {
                                return instrument_list_add_char(c);
                        } else if (k->sym == SDLK_SPACE) {
                                instrument_cursor_pos = 0;
                                status.flags |= NEED_UPDATE;
                                return 1;
			}
                }
                return 0;
        }

        new_ins = CLAMP(new_ins, 1, 99);
        if (new_ins != current_instrument) {
                instrument_set(new_ins);
                status.flags |= NEED_UPDATE;
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
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
        byte buf[4];

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
                        numtostr(2, ins->sample_map[n], buf);
                } else {
                        buf[0] = buf[1] = 173;
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
}

static int note_trans_handle_alt_key(SDL_keysym * k)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
	int n, s;
	
	switch (k->sym) {
	case SDLK_a:
		s = sample_get_current();
		for (n = 0; n < 120; n++)
			ins->sample_map[n] = s;
		break;
	default:
		return 0;
	}
	
	status.flags |= NEED_UPDATE;
	return 1;
}

static int note_trans_handle_key(SDL_keysym * k)
{
        int prev_line = note_trans_sel_line;
        int new_line = prev_line;
        int prev_pos = note_trans_cursor_pos;
        int new_pos = prev_pos;
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
        char c;
        int n;

	if (k->mod & (KMOD_ALT | KMOD_META))
		return note_trans_handle_alt_key(k);
		
       	switch (k->sym) {
        case SDLK_UP:
		if (!NO_MODIFIER(k->mod))
			return 0;
                if (--new_line < 0) {
                        change_focus_to(1);
                        return 1;
                }
                break;
        case SDLK_DOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_line++;
                break;
        case SDLK_PAGEUP:
                if (k->mod & KMOD_CTRL) {
                        instrument_set(current_instrument - 1);
                        return 1;
                }
                new_line -= 16;
                break;
        case SDLK_PAGEDOWN:
                if (k->mod & KMOD_CTRL) {
                        instrument_set(current_instrument + 1);
                        return 1;
                }
                new_line += 16;
                break;
        case SDLK_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_line = 0;
                break;
        case SDLK_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_line = 119;
                break;
        case SDLK_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_pos--;
                break;
        case SDLK_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_pos++;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
		if (!NO_MODIFIER(k->mod))
			return 0;
                sample_set(ins->sample_map[note_trans_sel_line]);
                return 1;
        default:
		c = unicode_to_ascii(k->unicode);
		
		if (c == '<') {
	        	sample_set(sample_get_current() - 1);
	        	return 1;
	        } else if (c == '>') {
	        	sample_set(sample_get_current() + 1);
	        	return 1;
	        }
	        
                switch (note_trans_cursor_pos) {
                case 0:        /* note */
                        n = kbd_get_note(c);
                        if (n <= 0 || n > 120)
                                return 0;
                        ins->note_map[note_trans_sel_line] = n;
                        ins->sample_map[note_trans_sel_line] = sample_get_current();
                        new_line++;
                        break;
                case 1:        /* octave */
                        if (c < '0' || c > '9')
                                return 0;
                        n = ins->note_map[note_trans_sel_line];
                        n = ((n - 1) % 12) + (12 * (c - '0')) + 1;
                        ins->note_map[note_trans_sel_line] = n;
                        new_line++;
                        break;
                case 2:        /* instrument, first digit */
                case 3:        /* instrument, second digit */
                        if (c == ' ') {
                                ins->sample_map[note_trans_sel_line] =
                                        sample_get_current();
                                new_line++;
                                break;
                        }
                        if (c == note_trans[NOTE_TRANS_CLEAR]) {
                                ins->sample_map[note_trans_sel_line] = 0;
                                sample_set(0);
                                new_line++;
                                break;
                        }
                        if (c < '0' || c > '9')
                                return 0;
                        c -= '0';
                        n = ins->sample_map[note_trans_sel_line];
                        if (note_trans_cursor_pos == 2) {
                                n = (c * 10) + (n % 10);
                                new_pos++;
                        } else {
                                n = ((n / 10) * 10) + c;
                                new_pos--;
                                new_line++;
                        }
                        ins->sample_map[note_trans_sel_line] = n;
                        sample_set(n);
                        break;
                }
                break;
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
	int n, y = middle ? 175 : 206, c = 12;
        for (n = 0; n < 64; n += 2)
                putpixel(screen, 259, 144 + n, c);
        for (n = 0; n < 256; n += 2)
                putpixel(screen, 257 + n, y, c);
}

static void _env_draw_node(int x, int y, int on)
{
	/* FIXME: the lines draw over the nodes. This doesn't matter unless the color is different. */
#if 1
	int c = (status.flags & CLASSIC_MODE) ? 12 : 5;
#else
	int c = 12;
#endif
	
	putpixel(screen, x - 1, y - 1, c);
	putpixel(screen, x - 1, y, c);
	putpixel(screen, x - 1, y + 1, c);

	putpixel(screen, x, y - 1, c);
	putpixel(screen, x, y, c);
	putpixel(screen, x, y + 1, c);

	putpixel(screen, x + 1, y - 1, c);
	putpixel(screen, x + 1, y, c);
	putpixel(screen, x + 1, y + 1, c);

	if (on) {
		putpixel(screen, x - 3, y - 1, c);
		putpixel(screen, x - 3, y, c);
		putpixel(screen, x - 3, y + 1, c);

		putpixel(screen, x + 3, y - 1, c);
		putpixel(screen, x + 3, y, c);
		putpixel(screen, x + 3, y + 1, c);
	}
}

static void _env_draw_loop(int xs, int xe, int sustain)
{
	int y = 144;
	int c = (status.flags & CLASSIC_MODE) ? 12 : 3;
	
	if (sustain) {
		while (y < 206) {
			/* unrolled once */
			putpixel(screen, xs, y, c); putpixel(screen, xe, y, c); y++;
			putpixel(screen, xs, y, 0); putpixel(screen, xe, y, 0); y++;
			putpixel(screen, xs, y, c); putpixel(screen, xe, y, c); y++;
			putpixel(screen, xs, y, 0); putpixel(screen, xe, y, 0); y++;
		}
	} else {
		while (y < 206) {
			putpixel(screen, xs, y, 0); putpixel(screen, xe, y, 0); y++;
			putpixel(screen, xs, y, c); putpixel(screen, xe, y, c); y++;			
			putpixel(screen, xs, y, c); putpixel(screen, xe, y, c); y++;
			putpixel(screen, xs, y, 0); putpixel(screen, xe, y, 0); y++;
		}
	}
}

static void _env_draw(const song_envelope *env, int middle, int current_node, int loop_on, int sustain_on)
{
	SDL_Rect envelope_rect = { 256, 144, 360, 64 };
	byte buf[16];
	int x, y, n;
	int last_x = 0, last_y = 0;
	int max_ticks = 50;
	
	while (env->ticks[env->nodes - 1] >= max_ticks)
		max_ticks *= 2;
	
        draw_fill_rect(screen, &envelope_rect, 0);
	
        SDL_LockSurface(screen);
	
	/* draw the axis lines */
	_env_draw_axes(middle);

	for (n = 0; n < env->nodes; n++) {
		x = 259 + env->ticks[n] * 256 / max_ticks;
		
		/* 65 values are being crammed into 62 pixels => have to lose three pixels somewhere.
		 * This is where IT compromises -- I don't quite get how the lines are drawn, though,
		 * because it changes for each value... (apart from drawing 63 and 64 the same way) */
		y = env->values[n];
		if (y > 63) y--;
		if (y > 42) y--;
		if (y > 21) y--;
		y = 206 - y;
		
		_env_draw_node(x, y, n == current_node);
		
		if (last_x)
			draw_line(screen, last_x, last_y, x, y, 12);
		
		last_x = x;
		last_y = y;
	}
	
	if (sustain_on)
		_env_draw_loop(259 + env->ticks[env->sustain_start] * 256 / max_ticks,
			       259 + env->ticks[env->sustain_end] * 256 / max_ticks, 1);
	if (loop_on)
		_env_draw_loop(259 + env->ticks[env->loop_start] * 256 / max_ticks,
			       259 + env->ticks[env->loop_end] * 256 / max_ticks, 0);
	
        SDL_UnlockSurface(screen);
	
        sprintf(buf, "Node %d/%d", current_node, env->nodes);
        draw_text(buf, 66, 19, 2, 0);
        sprintf(buf, "Tick %d", env->ticks[current_node]);
        draw_text(buf, 66, 21, 2, 0);
        sprintf(buf, "Value %d", env->values[current_node] - (middle ? 32 : 0));
        draw_text(buf, 66, 23, 2, 0);
}

/* return: the new current node */
static int _env_node_add(song_envelope *env, int current_node)
{
	int newtick, newvalue;
	
	/* is 24 the right number here, or 25? */
	if (env->nodes > 24 || current_node == env->nodes - 1)
		return current_node;
	
	newtick = (env->ticks[current_node] + env->ticks[current_node + 1]) / 2;
	newvalue = (env->values[current_node] + env->values[current_node + 1]) / 2;
	if (newtick == env->ticks[current_node] || newtick == env->ticks[current_node + 1]) {
		/* If the current node is at (for example) tick 30, and the next node is at tick 32,
		 * is there any chance of a rounding error that would make newtick 30 instead of 31?
		 * ("Is there a chance the track could bend?") */
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
	
	return current_node;
}

/* return: the new current node */
static int _env_node_remove(song_envelope *env, int current_node)
{
	if (current_node == 0 || env->nodes < 3)
		return current_node;
	
	memmove(env->ticks + current_node, env->ticks + current_node + 1,
		(env->nodes - current_node - 1) * sizeof(env->ticks[0]));
	memmove(env->values + current_node, env->values + current_node + 1,
		(env->nodes - current_node - 1) * sizeof(env->values[0]));
	env->nodes--;
	if (current_node >= env->nodes)
		current_node = env->nodes - 1;
	
	return current_node;
}

/* the return value here is actually a bitmask:
r & 1 => the key was handled
r & 2 => the envelope changed (i.e., it should be enabled) */
static int _env_handle_key_viewmode(SDL_keysym *k, song_envelope *env, int *current_node)
{
	int new_node = *current_node;
	
        switch (k->sym) {
        case SDLK_UP:
                change_focus_to(1);
                return 1;
        case SDLK_DOWN:
                change_focus_to(6);
                return 1;
        case SDLK_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_node--;
                break;
        case SDLK_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_node++;
                break;
	case SDLK_INSERT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		*current_node = _env_node_add(env, *current_node);
		status.flags |= NEED_UPDATE;
		return 1 | 2;
	case SDLK_DELETE:
		if (!NO_MODIFIER(k->mod))
			return 0;
		*current_node = _env_node_remove(env, *current_node);
		status.flags |= NEED_UPDATE;
		return 1 | 2;
	case SDLK_SPACE:
		if (!NO_MODIFIER(k->mod))
			return 0;
		song_play_note(current_instrument, last_note, 0, 1);
		return 1;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (!NO_MODIFIER(k->mod))
			return 0;
		envelope_edit_mode = 1;
		status.flags |= NEED_UPDATE;
		return 1 | 2;
        default:
                return 0;
        }
	
	new_node = CLAMP(new_node, 0, env->nodes - 1);
	if (*current_node != new_node) {
		*current_node = new_node;
		status.flags |= NEED_UPDATE;
	}
	
	return 1;
}

/* - this function is only ever called when the envelope is in edit mode
   - envelope_edit_mode is only ever assigned a true value once, in _env_handle_key_viewmode.
   - when _env_handle_key_viewmode enables envelope_edit_mode, it indicates in its return value
     that the envelope should be enabled.
   - therefore, the envelope will always be enabled when this function is called, so there is
     no reason to indicate a change in the envelope here. */
static int _env_handle_key_editmode(SDL_keysym *k, song_envelope *env, int *current_node)
{
	int new_node = *current_node, new_tick = env->ticks[*current_node],
		new_value = env->values[*current_node];
	
	/* TODO: when does adding/removing a node alter loop points? */
	
        switch (k->sym) {
        case SDLK_UP:
		if (k->mod & (KMOD_ALT | KMOD_META))
			new_value += 16;
		else
			new_value++;
                break;
        case SDLK_DOWN:
		if (k->mod & (KMOD_ALT | KMOD_META))
			new_value -= 16;
		else
			new_value--;
                break;
	case SDLK_PAGEUP:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_value += 16;
		break;
	case SDLK_PAGEDOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_value -= 16;
		break;
        case SDLK_LEFT:
		if (k->mod & KMOD_CTRL)
			new_node--;
		else if (k->mod & (KMOD_ALT | KMOD_META))
			new_tick -= 16;
		else
			new_tick--;
                break;
        case SDLK_RIGHT:
		if (k->mod & KMOD_CTRL)
			new_node++;
		else if (k->mod & (KMOD_ALT | KMOD_META))
			new_tick += 16;
		else
			new_tick++;
                break;
	case SDLK_TAB:
		if (k->mod & KMOD_SHIFT)
			new_tick -= 16;
		else
			new_tick += 16;
		break;
	case SDLK_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_tick = 0;
		break;
	case SDLK_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
		new_tick = 10000;
		break;
	case SDLK_INSERT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		*current_node = _env_node_add(env, *current_node);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DELETE:
		if (!NO_MODIFIER(k->mod))
			return 0;
		*current_node = _env_node_remove(env, *current_node);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_SPACE:
		if (!NO_MODIFIER(k->mod))
			return 0;
		song_play_note(current_instrument, last_note, 0, 1);
		return 1;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		if (!NO_MODIFIER(k->mod))
			return 0;
		envelope_edit_mode = 0;
		status.flags |= NEED_UPDATE;
		break;
        default:
                return 0;
        }
	
	new_node = CLAMP(new_node, 0, env->nodes - 1);
	new_tick = (new_node == 0) ? 0 : CLAMP(new_tick, env->ticks[*current_node - 1] + 1,
					       ((*current_node == env->nodes - 1)
						? 10000 : env->ticks[*current_node + 1]) - 1);
	new_value = CLAMP(new_value, 0, 64);
	
	if (new_node == *current_node
	    && new_value == env->values[new_node]
	    && new_tick == env->ticks[new_node]) {
		/* there's no need to update the screen -- everything's the same as it was */
		return 1;
	}
	*current_node = new_node;
	env->values[*current_node] = new_value;
	env->ticks[*current_node] = new_tick;
	
	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */
/* envelope stuff (draw()'s and handle_key()'s) */

static void _draw_env_label(const char *env_name, int is_selected)
{
	int pos = 33;
	
	pos += draw_text(env_name, pos, 16, is_selected ? 3 : 0, 2);
	pos += draw_text(" Envelope", pos, 16, is_selected ? 3 : 0, 2);
	if (envelope_edit_mode)
		draw_text(" (Edit)", pos, 16, is_selected ? 3 : 0, 2);
}

static void volume_envelope_draw(void)
{
        int is_selected = (ACTIVE_PAGE.selected_widget == 5);
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	_draw_env_label("Volume", is_selected);
	_env_draw(&ins->vol_env, 0, current_node_vol,
		  ins->flags & ENV_VOLLOOP, ins->flags & ENV_VOLSUSTAIN);
}

static void panning_envelope_draw(void)
{
	int is_selected = (ACTIVE_PAGE.selected_widget == 5);
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	_draw_env_label("Panning", is_selected);
	_env_draw(&ins->pan_env, 1, current_node_pan,
		  ins->flags & ENV_PANLOOP, ins->flags & ENV_PANSUSTAIN);
}

static void pitch_envelope_draw(void)
{
	int is_selected = (ACTIVE_PAGE.selected_widget == 5);
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	_draw_env_label("Frequency", is_selected);
	_env_draw(&ins->pitch_env, (ins->flags & ENV_FILTER) ? 0 : 1, current_node_pitch,
		  ins->flags & ENV_PITCHLOOP, ins->flags & ENV_PITCHSUSTAIN);
}

static int volume_envelope_handle_key(SDL_keysym * k)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	int r;

	if (envelope_edit_mode)
		r = _env_handle_key_editmode(k, &ins->vol_env, &current_node_vol);
	else
		r = _env_handle_key_viewmode(k, &ins->vol_env, &current_node_vol);
	if (r & 2) {
		r ^= 2;
		ins->flags |= ENV_VOLUME;
	}
	return r;
}

static int panning_envelope_handle_key(SDL_keysym * k)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	int r;

	if (envelope_edit_mode)
		r = _env_handle_key_editmode(k, &ins->pan_env, &current_node_pan);
	else
		r = _env_handle_key_viewmode(k, &ins->pan_env, &current_node_pan);
	if (r & 2) {
		r ^= 2;
		ins->flags |= ENV_PANNING;
	}
	return r;
}

static int pitch_envelope_handle_key(SDL_keysym * k)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	int r;

	if (envelope_edit_mode)
		r = _env_handle_key_editmode(k, &ins->pitch_env, &current_node_pitch);
	else
		r = _env_handle_key_viewmode(k, &ins->pitch_env, &current_node_pitch);
	if (r & 2) {
		r ^= 2;
		ins->flags |= ENV_PITCH;
	}
	return r;
}

/* --------------------------------------------------------------------------------------------------------- */
/* pitch-pan center */

static int pitch_pan_center_handle_key(SDL_keysym *k)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	int ppc = ins->pitch_pan_center;
	
	switch (k->sym) {
	case SDLK_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		ppc--;
		break;
	case SDLK_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		ppc++;
		break;
	default:
		if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0) {
			char c = unicode_to_ascii(k->unicode);
			if (c == 0)
				return 0;
			ppc = kbd_get_note(c);
			if (ppc < 1 || ppc > 120)
				return 0;
			ppc--;
			break;
		}
		return 0;
	}
	if (ppc != ins->pitch_pan_center && ppc >= 0 && ppc < 120) {
		ins->pitch_pan_center = ppc;
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

static void pitch_pan_center_draw(void)
{
	char buf[4];
        int selected = (ACTIVE_PAGE.selected_widget == 16);
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	draw_text(get_note_string(ins->pitch_pan_center + 1, buf), 54, 45, selected ? 3 : 2, 0);
}

/* --------------------------------------------------------------------------------------------------------- */
/* default key handler (for instrument changing on pgup/pgdn) */

static void instrument_save(void)
{
#if 0
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	char *ptr = dmoz_path_concat(cfg_dir_instruments, ins->filename);
	
	/* TODO: ask before overwriting the file */
	if (song_save_instrument(current_instrument, ptr))
		status_text_flash("Instrument saved (instrument %d)", current_instrument);
	else
		status_text_flash("Error: Instrument %d NOT saved! (No Filename?)", current_instrument);
	free(ptr);
#else
	status_text_flash("TODO: save instrument");
#endif
}

static void instrument_list_handle_alt_key(SDL_keysym *k)
{
	/* song_instrument *ins = song_get_instrument(current_instrument, NULL); */
	
	switch (k->sym) {
	case SDLK_n:
		song_toggle_multichannel_mode();
		return;
	case SDLK_o:
		instrument_save();
		return;
	case SDLK_s:
		swap_instrument_dialog();
		return;
	case SDLK_x:
		exchange_instrument_dialog();
		return;
	default:
		return;
	}
	
	/* Don't pay any attention to this -- since EVERY case in the switch
	ends with a return statement, this line is never reached. */
	status.flags |= NEED_UPDATE;
}

static void instrument_list_handle_key(SDL_keysym * k)
{
        switch (k->sym) {
	case SDLK_COMMA:
	case SDLK_LESS:
		song_change_current_play_channel(-1, 0);
		return;
	case SDLK_PERIOD:
	case SDLK_GREATER:
		song_change_current_play_channel(1, 0);
		return;
		
        case SDLK_PAGEUP:
                instrument_set(current_instrument - 1);
                break;
        case SDLK_PAGEDOWN:
                instrument_set(current_instrument + 1);
                break;
        default:
		if (k->mod & (KMOD_ALT | KMOD_META)) {
			instrument_list_handle_alt_key(k);
		} else {
			int n = kbd_get_note(unicode_to_ascii(k->unicode));
			if (n <= 0 || n > 120)
				return;
			song_play_note(current_instrument, n, 0, 1);
			last_note = n;
		}
		return;
	}
}

/* --------------------------------------------------------------------- */

static void change_subpage(void)
{
        int widget = ACTIVE_PAGE.selected_widget;
        int page = status.current_page;

        switch (widget) {
        case 1:
                page = PAGE_INSTRUMENT_LIST_GENERAL;
                break;
        case 2:
                page = PAGE_INSTRUMENT_LIST_VOLUME;
                break;
        case 3:
                page = PAGE_INSTRUMENT_LIST_PANNING;
                break;
        case 4:
		page = PAGE_INSTRUMENT_LIST_PITCH;
                break;
#ifndef NDEBUG
        default:
		fprintf(stderr, "change_subpage: wtf, how did I get here?\n");
		abort();
                return;
#endif
        }

        if (page != status.current_page) {
                pages[page].selected_widget = widget;
		togglebutton_set(pages[page].widgets, widget, 0);
                set_page(page);
                instrument_list_subpage = page;
        }
}

/* --------------------------------------------------------------------- */
/* predraw hooks... */

static void instrument_list_general_predraw_hook(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	togglebutton_set(widgets_general, 6 + ins->nna, 0);
	togglebutton_set(widgets_general, 10 + ins->dct, 0);
	togglebutton_set(widgets_general, 14 + ins->dca, 0);
	
        widgets_general[17].textentry.text = ins->filename;
}

static void instrument_list_volume_predraw_hook(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);

        widgets_volume[6].toggle.state = !!(ins->flags & ENV_VOLUME);
        widgets_volume[7].toggle.state = !!(ins->flags & ENV_VOLCARRY);
        widgets_volume[8].toggle.state = !!(ins->flags & ENV_VOLLOOP);
        widgets_volume[11].toggle.state = !!(ins->flags & ENV_VOLSUSTAIN);
	
	/* FIXME: this is the wrong place for this.
	... and it's probably not even right -- how does Impulse Tracker handle loop constraints?
	See below for panning/pitch envelopes; same deal there. */
	if (ins->vol_env.loop_start > ins->vol_env.loop_end)
		ins->vol_env.loop_end = ins->vol_env.loop_start;
	if (ins->vol_env.sustain_start > ins->vol_env.sustain_end)
		ins->vol_env.sustain_end = ins->vol_env.sustain_start;

	widgets_volume[9].numentry.max = ins->vol_env.nodes - 1;
	widgets_volume[10].numentry.max = ins->vol_env.nodes - 1;
	widgets_volume[12].numentry.max = ins->vol_env.nodes - 1;
	widgets_volume[13].numentry.max = ins->vol_env.nodes - 1;

        widgets_volume[9].numentry.value = ins->vol_env.loop_start;
        widgets_volume[10].numentry.value = ins->vol_env.loop_end;
        widgets_volume[12].numentry.value = ins->vol_env.sustain_start;
        widgets_volume[13].numentry.value = ins->vol_env.sustain_end;

        /* mp hack: shifting values all over the place here, ugh */
        widgets_volume[14].thumbbar.value = ins->global_volume << 1;
        widgets_volume[15].thumbbar.value = ins->fadeout >> 5;
        widgets_volume[16].thumbbar.value = ins->volume_swing;
}

static void instrument_list_panning_predraw_hook(void)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	widgets_panning[6].toggle.state = !!(ins->flags & ENV_PANNING);
	widgets_panning[7].toggle.state = !!(ins->flags & ENV_PANCARRY);
	widgets_panning[8].toggle.state = !!(ins->flags & ENV_PANLOOP);
	widgets_panning[11].toggle.state = !!(ins->flags & ENV_PANSUSTAIN);
	
	if (ins->pan_env.loop_start > ins->pan_env.loop_end)
		ins->pan_env.loop_end = ins->pan_env.loop_start;
	if (ins->pan_env.sustain_start > ins->pan_env.sustain_end)
		ins->pan_env.sustain_end = ins->pan_env.sustain_start;

	widgets_panning[9].numentry.max = ins->pan_env.nodes - 1;
	widgets_panning[10].numentry.max = ins->pan_env.nodes - 1;
	widgets_panning[12].numentry.max = ins->pan_env.nodes - 1;
	widgets_panning[13].numentry.max = ins->pan_env.nodes - 1;
	
	widgets_panning[9].numentry.value = ins->pan_env.loop_start;
	widgets_panning[10].numentry.value = ins->pan_env.loop_end;
	widgets_panning[12].numentry.value = ins->pan_env.sustain_start;
	widgets_panning[13].numentry.value = ins->pan_env.sustain_end;
	
	widgets_panning[14].toggle.state = !!(ins->flags & ENV_SETPANNING);
	widgets_panning[15].thumbbar.value = ins->panning >> 2;
	/* (widgets_panning[16] is the pitch-pan center) */
	widgets_panning[17].thumbbar.value = ins->pitch_pan_separation;
	widgets_panning[18].thumbbar.value = ins->pan_swing;
}

static void instrument_list_pitch_predraw_hook(void)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	widgets_pitch[6].menutoggle.state = ((ins->flags & ENV_PITCH)
					     ? ((ins->flags & ENV_FILTER)
						? 2 : 1) : 0);
	widgets_pitch[7].toggle.state = !!(ins->flags & ENV_PITCHCARRY);
	widgets_pitch[8].toggle.state = !!(ins->flags & ENV_PITCHLOOP);
	widgets_pitch[11].toggle.state = !!(ins->flags & ENV_PITCHSUSTAIN);
	
	if (ins->pitch_env.loop_start > ins->pitch_env.loop_end)
		ins->pitch_env.loop_end = ins->pitch_env.loop_start;
	if (ins->pitch_env.sustain_start > ins->pitch_env.sustain_end)
		ins->pitch_env.sustain_end = ins->pitch_env.sustain_start;

	widgets_pitch[9].numentry.max = ins->pitch_env.nodes - 1;
	widgets_pitch[10].numentry.max = ins->pitch_env.nodes - 1;
	widgets_pitch[12].numentry.max = ins->pitch_env.nodes - 1;
	widgets_pitch[13].numentry.max = ins->pitch_env.nodes - 1;

	widgets_pitch[9].numentry.value = ins->pitch_env.loop_start;
	widgets_pitch[10].numentry.value = ins->pitch_env.loop_end;
	widgets_pitch[12].numentry.value = ins->pitch_env.sustain_start;
	widgets_pitch[13].numentry.value = ins->pitch_env.sustain_end;
	
	widgets_pitch[14].thumbbar.value = ins->filter_cutoff & 0x7f;
	widgets_pitch[15].thumbbar.value = ins->filter_resonance & 0x7f;
	
	/* printf("ins%02d: ch%04d pgm%04d bank%06d drum%04d\n", current_instrument,
		ins->midi_channel, ins->midi_program, ins->midi_bank, ins->midi_drum_key); */
	widgets_pitch[16].thumbbar.value = ins->midi_channel;
	widgets_pitch[17].thumbbar.value = (signed char) ins->midi_program;
	widgets_pitch[18].thumbbar.value = (signed char) (ins->midi_bank & 0xff);
	widgets_pitch[19].thumbbar.value = (signed char) (ins->midi_bank >> 8);
	/* what is midi_drum_key for? */
}

/* --------------------------------------------------------------------- */
/* update values in song */

static void instrument_list_general_update_values(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);

	for (ins->nna = 4; ins->nna--;)
                if (widgets_general[ins->nna + 6].togglebutton.state)
                        break;
	for (ins->dct = 4; ins->dct--;)
                if (widgets_general[ins->dct + 10].togglebutton.state)
                        break;
	for (ins->dca = 3; ins->dca--;)
                if (widgets_general[ins->dca + 14].togglebutton.state)
                        break;
}

#define CHECK_SET(a,b,c) if (a != b) { a = b; c; }

static void instrument_list_volume_update_values(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);

	ins->flags &= ~(ENV_VOLUME | ENV_VOLCARRY | ENV_VOLLOOP | ENV_VOLSUSTAIN);
        if (widgets_volume[6].toggle.state)
                ins->flags |= ENV_VOLUME;
        if (widgets_volume[7].toggle.state)
                ins->flags |= ENV_VOLCARRY;
        if (widgets_volume[8].toggle.state)
                ins->flags |= ENV_VOLLOOP;
        if (widgets_volume[11].toggle.state)
                ins->flags |= ENV_VOLSUSTAIN;

        CHECK_SET(ins->vol_env.loop_start, widgets_volume[9].numentry.value,
		  ins->flags |= ENV_VOLLOOP);
        CHECK_SET(ins->vol_env.loop_end, widgets_volume[10].numentry.value,
                  ins->flags |= ENV_VOLLOOP);
        CHECK_SET(ins->vol_env.sustain_start, widgets_volume[12].numentry.value,
                  ins->flags |= ENV_VOLSUSTAIN);
        CHECK_SET(ins->vol_env.sustain_end, widgets_volume[13].numentry.value,
                  ins->flags |= ENV_VOLSUSTAIN);

        /* more ugly shifts */
        ins->global_volume = widgets_volume[14].thumbbar.value >> 1;
        ins->fadeout = widgets_volume[15].thumbbar.value << 5;
        ins->volume_swing = widgets_volume[16].thumbbar.value;
}

static void instrument_list_panning_update_values(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
	int n;
	
	ins->flags &= ~(ENV_PANNING | ENV_PANCARRY | ENV_PANLOOP | ENV_PANSUSTAIN | ENV_SETPANNING);
        if (widgets_panning[6].toggle.state)
                ins->flags |= ENV_PANNING;
        if (widgets_panning[7].toggle.state)
                ins->flags |= ENV_PANCARRY;
        if (widgets_panning[8].toggle.state)
                ins->flags |= ENV_PANLOOP;
        if (widgets_panning[11].toggle.state)
                ins->flags |= ENV_PANSUSTAIN;
        if (widgets_panning[14].toggle.state)
                ins->flags |= ENV_SETPANNING;

        CHECK_SET(ins->pan_env.loop_start, widgets_panning[9].numentry.value,
                  ins->flags |= ENV_PANLOOP);
        CHECK_SET(ins->pan_env.loop_end, widgets_panning[10].numentry.value,
                  ins->flags |= ENV_PANLOOP);
        CHECK_SET(ins->pan_env.sustain_start, widgets_panning[12].numentry.value,
                  ins->flags |= ENV_PANSUSTAIN);
        CHECK_SET(ins->pan_env.sustain_end, widgets_panning[13].numentry.value,
                  ins->flags |= ENV_PANSUSTAIN);

	n = widgets_panning[15].thumbbar.value << 2;
	if (ins->panning != n) {
		ins->panning = n;
		ins->flags |= ENV_SETPANNING;
	}
	/* (widgets_panning[16] is the pitch-pan center) */
        ins->pitch_pan_separation = widgets_panning[17].thumbbar.value;
        ins->pan_swing = widgets_panning[18].thumbbar.value;
}

static void instrument_list_pitch_update_values(void)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);

	ins->flags &= ~(ENV_PITCH | ENV_PITCHCARRY | ENV_PITCHLOOP | ENV_PITCHSUSTAIN | ENV_FILTER);
	
	switch (widgets_pitch[6].menutoggle.state) {
	case 2: ins->flags |= ENV_FILTER;
	case 1: ins->flags |= ENV_PITCH;
	}
	
	if (widgets_pitch[6].menutoggle.state)
		ins->flags |= ENV_PITCH;
	if (widgets_pitch[7].toggle.state)
		ins->flags |= ENV_PITCHCARRY;
	if (widgets_pitch[8].toggle.state)
		ins->flags |= ENV_PITCHLOOP;
	if (widgets_pitch[11].toggle.state)
		ins->flags |= ENV_PITCHSUSTAIN;
        
        CHECK_SET(ins->pitch_env.loop_start, widgets_pitch[9].numentry.value,
                  ins->flags |= ENV_PITCHLOOP);
        CHECK_SET(ins->pitch_env.loop_end, widgets_pitch[10].numentry.value,
                  ins->flags |= ENV_PITCHLOOP);
        CHECK_SET(ins->pitch_env.sustain_start, widgets_pitch[12].numentry.value,
                  ins->flags |= ENV_PITCHSUSTAIN);
        CHECK_SET(ins->pitch_env.sustain_end, widgets_pitch[13].numentry.value,
                  ins->flags |= ENV_PITCHSUSTAIN);
        ins->filter_cutoff = widgets_pitch[14].thumbbar.value | 0x80;
	ins->filter_resonance = widgets_pitch[15].thumbbar.value | 0x80;
	ins->midi_channel = widgets_pitch[16].thumbbar.value;
	ins->midi_program = widgets_pitch[17].thumbbar.value;
	ins->midi_bank = ((widgets_pitch[19].thumbbar.value << 8)
			  | (widgets_pitch[18].thumbbar.value & 0xff));
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

        draw_fill_chars(57, 28, 62, 29, 0);
        draw_fill_chars(57, 32, 62, 34, 0);
        draw_fill_chars(57, 37, 62, 39, 0);

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

        draw_fill_chars(57, 28, 62, 29, 0);
        draw_fill_chars(57, 32, 62, 34, 0);
        draw_fill_chars(57, 37, 62, 39, 0);
        draw_fill_chars(57, 42, 62, 45, 0);

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
	draw_text("Pan swing", 44, 47, 0, 2); /* Hmm. The 's' in swing isn't capitalised. ;) */

	draw_text("\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a", 54, 44, 2, 0);
}

static void instrument_list_pitch_draw_const(void)
{
	instrument_list_draw_const();

        draw_fill_chars(57, 28, 62, 29, 0);
        draw_fill_chars(57, 32, 62, 34, 0);
        draw_fill_chars(57, 37, 62, 39, 0);
	
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
	draw_text("MIDI Channel", 36, 44, 0, 2);
	draw_text("MIDI Program", 36, 45, 0, 2);
	draw_text("MIDI Bank Low", 36, 46, 0, 2);
	draw_text("MIDI Bank High", 36, 47, 0, 2);
}

/* --------------------------------------------------------------------- */
/* load_page functions */

static void _load_page_common(struct page *page, struct widget *page_widgets)
{
	page->title = "Instrument List (F4)";
	page->handle_key = instrument_list_handle_key;
	page->widgets = page_widgets;
	page->help_index = HELP_INSTRUMENT_LIST;
	
	/* the first five widgets are the same for all four pages. */
	
	/* 0 = instrument list */
	create_other(page_widgets + 0, 1, instrument_list_handle_key_on_list, instrument_list_draw_list);

	/* 1-4 = subpage switches */
	create_togglebutton(page_widgets + 1, 32, 13, 7, 1, 5, 0, 2, 2, change_subpage, "General",
			    1, subpage_switches_group);
	create_togglebutton(page_widgets + 2, 44, 13, 7, 2, 5, 1, 3, 3, change_subpage, "Volume",
			    1, subpage_switches_group);
	create_togglebutton(page_widgets + 3, 56, 13, 7, 3, 5, 2, 4, 4, change_subpage, "Panning",
			    1, subpage_switches_group);
	create_togglebutton(page_widgets + 4, 68, 13, 7, 4, 5, 3, 0, 0, change_subpage, "Pitch",
			    2, subpage_switches_group);
}


void instrument_list_general_load_page(struct page *page)
{
	_load_page_common(page, widgets_general);
	
        page->draw_const = instrument_list_general_draw_const;
        page->predraw_hook = instrument_list_general_predraw_hook;
        page->total_widgets = 18;
	
	/* special case stuff */
        widgets_general[1].togglebutton.state = 1;
	widgets_general[2].next.down = widgets_general[3].next.down = widgets_general[4].next.down = 6;
	
        /* 5 = note trans table */
	create_other(widgets_general + 5, 6, note_trans_handle_key, note_trans_draw);

        /* 6-9 = nna toggles */
        create_togglebutton(widgets_general + 6, 46, 19, 29, 2, 7, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Cut", 2, nna_group);
        create_togglebutton(widgets_general + 7, 46, 22, 29, 6, 8, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Continue", 2, nna_group);
        create_togglebutton(widgets_general + 8, 46, 25, 29, 7, 9, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Off", 2, nna_group);
        create_togglebutton(widgets_general + 9, 46, 28, 29, 8, 10, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Fade", 2, nna_group);

        /* 10-13 = dct toggles */
        create_togglebutton(widgets_general + 10, 46, 34, 12, 9, 11, 5, 14,
                            14, instrument_list_general_update_values,
                            "Disabled", 2, dct_group);
        create_togglebutton(widgets_general + 11, 46, 37, 12, 10, 12, 5, 15,
                            15, instrument_list_general_update_values,
                            "Note", 2, dct_group);
        create_togglebutton(widgets_general + 12, 46, 40, 12, 11, 13, 5, 16,
                            16, instrument_list_general_update_values,
                            "Sample", 2, dct_group);
        create_togglebutton(widgets_general + 13, 46, 43, 12, 12, 17, 5, 13,
                            13, instrument_list_general_update_values,
                            "Instrument", 2, dct_group);
        /* 14-16 = dca toggles */
        create_togglebutton(widgets_general + 14, 62, 34, 13, 9, 15, 10, 0,
                            0, instrument_list_general_update_values,
                            "Note Cut", 2, dca_group);
        create_togglebutton(widgets_general + 15, 62, 37, 13, 14, 16, 11, 0,
                            0, instrument_list_general_update_values,
                            "Note Off", 2, dca_group);
        create_togglebutton(widgets_general + 16, 62, 40, 13, 15, 17, 12, 0,
                            0, instrument_list_general_update_values,
                            "Note Fade", 2, dca_group);
        /* 17 = filename */
        /* impulse tracker has a 17-char-wide box for the filename for
         * some reason, though it still limits the actual text to 12
         * characters. go figure... */
        create_textentry(widgets_general + 17, 56, 47, 13, 13, 17, 0, NULL,
                         NULL, 12);
}

void instrument_list_volume_load_page(struct page *page)
{
	_load_page_common(page, widgets_volume);
	
        page->draw_const = instrument_list_volume_draw_const;
        page->predraw_hook = instrument_list_volume_predraw_hook;
        page->total_widgets = 17;

        /* 5 = volume envelope */
	create_other(widgets_volume + 5, 0, volume_envelope_handle_key, volume_envelope_draw);

        /* 6-7 = envelope switches */
        create_toggle(widgets_volume + 6, 54, 28, 5, 7, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_toggle(widgets_volume + 7, 54, 29, 6, 8, 0, 0, 0,
                      instrument_list_volume_update_values);

        /* 8-10 envelope loop settings */
        create_toggle(widgets_volume + 8, 54, 32, 7, 9, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_numentry(widgets_volume + 9, 54, 33, 3, 8, 10, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);
        create_numentry(widgets_volume + 10, 54, 34, 3, 9, 11, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);

        /* 11-13 = susloop settings */
        create_toggle(widgets_volume + 11, 54, 37, 10, 12, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_numentry(widgets_volume + 12, 54, 38, 3, 11, 13, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);
        create_numentry(widgets_volume + 13, 54, 39, 3, 12, 14, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);

        /* 14-16 = volume thumbbars */
        create_thumbbar(widgets_volume + 14, 54, 42, 17, 13, 15, 0,
                        instrument_list_volume_update_values, 0, 128);
        create_thumbbar(widgets_volume + 15, 54, 43, 17, 14, 16, 0,
                        instrument_list_volume_update_values, 0, 128);
        create_thumbbar(widgets_volume + 16, 54, 46, 17, 15, 16, 0,
                        instrument_list_volume_update_values, 0, 100);
}

void instrument_list_panning_load_page(struct page *page)
{
	_load_page_common(page, widgets_panning);
	
        page->draw_const = instrument_list_panning_draw_const;
        page->predraw_hook = instrument_list_panning_predraw_hook;
        page->total_widgets = 19;
	
        /* 5 = panning envelope */
	create_other(widgets_panning + 5, 0, panning_envelope_handle_key, panning_envelope_draw);

        /* 6-7 = envelope switches */
        create_toggle(widgets_panning + 6, 54, 28, 5, 7, 0, 0, 0,
                      instrument_list_panning_update_values);
        create_toggle(widgets_panning + 7, 54, 29, 6, 8, 0, 0, 0,
                      instrument_list_panning_update_values);

        /* 8-10 envelope loop settings */
        create_toggle(widgets_panning + 8, 54, 32, 7, 9, 0, 0, 0,
                      instrument_list_panning_update_values);
        create_numentry(widgets_panning + 9, 54, 33, 3, 8, 10, 0,
                        instrument_list_panning_update_values, 0, 1,
                        numentry_cursor_pos + 1);
        create_numentry(widgets_panning + 10, 54, 34, 3, 9, 11, 0,
                        instrument_list_panning_update_values, 0, 1,
                        numentry_cursor_pos + 1);

        /* 11-13 = susloop settings */
        create_toggle(widgets_panning + 11, 54, 37, 10, 12, 0, 0, 0,
                      instrument_list_panning_update_values);
        create_numentry(widgets_panning + 12, 54, 38, 3, 11, 13, 0,
                        instrument_list_panning_update_values, 0, 1,
                        numentry_cursor_pos + 1);
        create_numentry(widgets_panning + 13, 54, 39, 3, 12, 14, 0,
                        instrument_list_panning_update_values, 0, 1,
                        numentry_cursor_pos + 1);

	/* 14-15 = default panning */
        create_toggle(widgets_panning + 14, 54, 42, 13, 15, 0, 0, 0,
		      instrument_list_panning_update_values);
        create_thumbbar(widgets_panning + 15, 54, 43, 9, 14, 16, 0,
                        instrument_list_panning_update_values, 0, 64);
	
	/* 16 = pitch-pan center */
	create_other(widgets_panning + 16, 0, pitch_pan_center_handle_key, pitch_pan_center_draw);
        widgets_panning[16].next.up = 15;
        widgets_panning[16].next.down = 17;
	
	/* 17-18 = other panning stuff */
	create_thumbbar(widgets_panning + 17, 54, 46, 9, 16, 18, 0,
			instrument_list_panning_update_values, -32, 32);
	create_thumbbar(widgets_panning + 18, 54, 47, 9, 17, 18, 0,
			instrument_list_panning_update_values, 0, 64);
}

void instrument_list_pitch_load_page(struct page *page)
{
	_load_page_common(page, widgets_pitch);
	
        page->draw_const = instrument_list_pitch_draw_const;
        page->predraw_hook = instrument_list_pitch_predraw_hook;
        page->total_widgets = 20;
	
        /* 5 = pitch envelope */
	create_other(widgets_pitch + 5, 0, pitch_envelope_handle_key, pitch_envelope_draw);
	
        /* 6-7 = envelope switches */
        create_menutoggle(widgets_pitch + 6, 54, 28, 5, 7, 0, 0, 0,
                      instrument_list_pitch_update_values, pitch_envelope_states);
        create_toggle(widgets_pitch + 7, 54, 29, 6, 8, 0, 0, 0,
                      instrument_list_pitch_update_values);

        /* 8-10 envelope loop settings */
        create_toggle(widgets_pitch + 8, 54, 32, 7, 9, 0, 0, 0,
                      instrument_list_pitch_update_values);
        create_numentry(widgets_pitch + 9, 54, 33, 3, 8, 10, 0,
                        instrument_list_pitch_update_values, 0, 1,
                        numentry_cursor_pos + 2);
        create_numentry(widgets_pitch + 10, 54, 34, 3, 9, 11, 0,
                        instrument_list_pitch_update_values, 0, 1,
                        numentry_cursor_pos + 2);

        /* 11-13 = susloop settings */
        create_toggle(widgets_pitch + 11, 54, 37, 10, 12, 0, 0, 0,
                      instrument_list_pitch_update_values);
        create_numentry(widgets_pitch + 12, 54, 38, 3, 11, 13, 0,
                        instrument_list_pitch_update_values, 0, 1,
                        numentry_cursor_pos + 2);
        create_numentry(widgets_pitch + 13, 54, 39, 3, 12, 14, 0,
                        instrument_list_pitch_update_values, 0, 1,
                        numentry_cursor_pos + 2);
	
	/* 14-15 = filter cutoff/resonance */
        create_thumbbar(widgets_pitch + 14, 54, 42, 17, 13, 15, 0,
			instrument_list_pitch_update_values, 0, 127);
	create_thumbbar(widgets_pitch + 15, 54, 43, 17, 14, 16, 0,
			instrument_list_pitch_update_values, 0, 127);
	
	/* 16-19 = midi crap */
	create_thumbbar(widgets_pitch + 16, 54, 44, 17, 15, 17, 0,
			instrument_list_pitch_update_values, 0, 17);
	create_thumbbar(widgets_pitch + 17, 54, 45, 17, 16, 18, 0,
			instrument_list_pitch_update_values, -1, 127);
	create_thumbbar(widgets_pitch + 18, 54, 46, 17, 17, 19, 0,
			instrument_list_pitch_update_values, -1, 127);
	create_thumbbar(widgets_pitch + 19, 54, 47, 17, 18, 19, 0,
			instrument_list_pitch_update_values, -1, 127);
	widgets_pitch[16].thumbbar.text_at_min = "Off";
	widgets_pitch[16].thumbbar.text_at_max = "Mapped";
	widgets_pitch[17].thumbbar.text_at_min = "Off";
	widgets_pitch[18].thumbbar.text_at_min = "Off";
	widgets_pitch[19].thumbbar.text_at_min = "Off";
}
