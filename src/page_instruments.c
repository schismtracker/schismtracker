/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#include <SDL.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */
/* just one global variable... */

int instrument_list_subpage = PAGE_INSTRUMENT_LIST_GENERAL;

/* --------------------------------------------------------------------- */
/* ... but tons o' ugly statics */

static struct item items_general[18];
static struct item items_volume[17];
static struct item items_panning[19];
static struct item items_pitch[18];

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
	
	current_node_vol = ins->vol_env_nodes ? CLAMP(current_node_vol, 0, ins->vol_env_nodes - 1) : 0;
	current_node_pan = ins->pan_env_nodes ? CLAMP(current_node_vol, 0, ins->pan_env_nodes - 1) : 0;
	current_node_pitch = ins->pitch_env_nodes ? CLAMP(current_node_vol, 0, ins->pan_env_nodes - 1) : 0;
	
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

static void instrument_list_draw_list(void)
{
        int pos, n;
        song_instrument *ins;
        int selected = (ACTIVE_PAGE.selected_item == 0);
        int is_current;
        byte buf[4];

        for (pos = 0, n = top_instrument; pos < 35; pos++, n++) {
                ins = song_get_instrument(n, NULL);
                is_current = (n == current_instrument);

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
                new_ins--;
                break;
        case SDLK_DOWN:
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
                if (instrument_cursor_pos < 25) {
                        instrument_cursor_pos = 0;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_END:
                if (instrument_cursor_pos < 24) {
                        instrument_cursor_pos = 24;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_LEFT:
                if (instrument_cursor_pos < 25
                    && instrument_cursor_pos > 0) {
                        instrument_cursor_pos--;
                        status.flags |= NEED_UPDATE;
                }
                return 1;
        case SDLK_RIGHT:
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
                        printf("TODO: load-instrument page\n");
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
        case SDLK_DELETE:
                if (instrument_cursor_pos == 25)
                        return 0;
                if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0)
                        instrument_list_delete_next_char();
                return 1;
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
        int is_selected = (ACTIVE_PAGE.selected_item == 5);
        int bg, sel_bg = (is_selected ? 14 : 0);
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
        byte buf[4];

        SDL_LockSurface(screen);

        for (pos = 0, n = note_trans_top_line; pos < 32; pos++, n++) {
                bg = ((n == note_trans_sel_line) ? sel_bg : 0);

                /* invalid notes are translated to themselves
                 * (and yes, this edits the actual instrument) */
                if (ins->note_map[n] < 1 || ins->note_map[n] > 120)
                        ins->note_map[n] = n + 1;

                draw_text_unlocked(get_note_string(n + 1, buf), 32,
                                   16 + pos, 2, bg);
                draw_char_unlocked(168, 35, 16 + pos, 2, bg);
                draw_text_unlocked(get_note_string(ins->note_map[n], buf),
                                   36, 16 + pos, 2, bg);
                if (is_selected && n == note_trans_sel_line) {
                        if (note_trans_cursor_pos == 0)
                                draw_char_unlocked(buf[0], 36, 16 + pos, 0, 3);
                        else if (note_trans_cursor_pos == 1)
                                draw_char_unlocked(buf[2], 38, 16 + pos, 0, 3);
                }
                draw_char_unlocked(0, 39, 16 + pos, 2, bg);
                if (ins->sample_map[n]) {
                        numtostr(2, ins->sample_map[n], buf);
                } else {
                        buf[0] = buf[1] = 173;
                        buf[2] = 0;
                }
                draw_text_unlocked(buf, 40, 16 + pos, 2, bg);
                if (is_selected && n == note_trans_sel_line) {
                        if (note_trans_cursor_pos == 2)
                                draw_char_unlocked(buf[0], 40, 16 + pos, 0, 3);
                        else if (note_trans_cursor_pos == 3)
                                draw_char_unlocked(buf[1], 41, 16 + pos, 0, 3);
                }
        }

        SDL_UnlockSurface(screen);
}

static int note_trans_handle_key(SDL_keysym * k)
{
        int prev_line = note_trans_sel_line;
        int new_line = prev_line;
        int prev_pos = note_trans_cursor_pos;
        int new_pos = prev_pos;
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
        char c = unicode_to_ascii(k->unicode);
        int n;

        switch (k->sym) {
        case SDLK_UP:
                if (--new_line < 0) {
                        change_focus_to(1);
                        return 1;
                }
                break;
        case SDLK_DOWN:
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
                new_line = 0;
                break;
        case SDLK_END:
                new_line = 119;
                break;
        case SDLK_LEFT:
                new_pos--;
                break;
        case SDLK_RIGHT:
                new_pos++;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                sample_set(ins->sample_map[note_trans_sel_line]);
                return 1;
        default:
                switch (note_trans_cursor_pos) {
                case 0:        /* note */
                        n = kbd_get_note(c);
                        if (n <= 0 || n > 120)
                                return 0;
                        ins->note_map[note_trans_sel_line] = n;
                        ins->sample_map[note_trans_sel_line] =
                                sample_get_current();
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
                putpixel_screen(259, 144 + n, c);
        for (n = 0; n < 256; n += 2)
                putpixel_screen(257 + n, y, c);
}

static void _env_draw_node(int x, int y, int on)
{
	/* FIXME: the lines draw over the nodes. This doesn't matter unless the color is different. */
#if 1
	int c = (status.flags & CLASSIC_MODE) ? 12 : 5;
#else
	int c = 12;
#endif
	
	putpixel_screen(x - 1, y - 1, c);
	putpixel_screen(x - 1, y, c);
	putpixel_screen(x - 1, y + 1, c);

	putpixel_screen(x, y - 1, c);
	putpixel_screen(x, y, c);
	putpixel_screen(x, y + 1, c);

	putpixel_screen(x + 1, y - 1, c);
	putpixel_screen(x + 1, y, c);
	putpixel_screen(x + 1, y + 1, c);

	if (on) {
		putpixel_screen(x - 3, y - 1, c);
		putpixel_screen(x - 3, y, c);
		putpixel_screen(x - 3, y + 1, c);

		putpixel_screen(x + 3, y - 1, c);
		putpixel_screen(x + 3, y, c);
		putpixel_screen(x + 3, y + 1, c);
	}
}

static void _env_draw_loop(int xs, int xe, int sustain)
{
	int y = 144;
	int c = (status.flags & CLASSIC_MODE) ? 12 : 3;
	
	if (sustain) {
		while (y < 206) {
			/* unrolled once */
			putpixel_screen(xs, y, c); putpixel_screen(xe, y, c); y++;
			putpixel_screen(xs, y, 0); putpixel_screen(xe, y, 0); y++;
			putpixel_screen(xs, y, c); putpixel_screen(xe, y, c); y++;
			putpixel_screen(xs, y, 0); putpixel_screen(xe, y, 0); y++;
		}
	} else {
		while (y < 206) {
			putpixel_screen(xs, y, 0); putpixel_screen(xe, y, 0); y++;
			putpixel_screen(xs, y, c); putpixel_screen(xe, y, c); y++;			
			putpixel_screen(xs, y, c); putpixel_screen(xe, y, c); y++;
			putpixel_screen(xs, y, 0); putpixel_screen(xe, y, 0); y++;
		}
	}
}

static void _env_draw(int middle, int current_node, byte nodes, unsigned short ticks[], byte values[],
		      int loop_on, byte loop_start, byte loop_end,
		      int sustain_on, byte sustain_start, byte sustain_end)
{
	SDL_Rect envelope_rect = { 256, 144, 360, 64 };
	byte buf[16];
	int x, y, n;
	int last_x = 0, last_y;
	int max_ticks = 50;
	
	while (ticks[nodes - 1] >= max_ticks)
		max_ticks *= 2;
	
        draw_fill_rect(&envelope_rect, 0);
	
        SDL_LockSurface(screen);
	
	/* draw the axis lines */
	_env_draw_axes(middle);

	for (n = 0; n < nodes; n++) {
		x = 259 + ticks[n] * 256 / max_ticks;
		
		/* 65 values are being crammed into 62 pixels => have to lose three pixels somewhere.
		 * This is where IT compromises -- I don't quite get how the lines are drawn, though,
		 * because it changes for each value... (apart from drawing 63 and 64 the same way) */
		y = values[n];
		if (y > 63) y--;
		if (y > 42) y--;
		if (y > 21) y--;
		y = 206 - y;
		
		_env_draw_node(x, y, n == current_node);
		
		if (last_x)
			draw_line_screen(last_x, last_y, x, y, 12);
		
		last_x = x;
		last_y = y;
	}
	
	if (sustain_on)
		_env_draw_loop(259 + ticks[sustain_start] * 256 / max_ticks,
			       259 + ticks[sustain_end] * 256 / max_ticks, 1);
	if (loop_on)
		_env_draw_loop(259 + ticks[loop_start] * 256 / max_ticks,
			       259 + ticks[loop_end] * 256 / max_ticks, 0);
	
        sprintf(buf, "Node %d/%d", current_node, nodes);
        draw_text_unlocked(buf, 66, 19, 2, 0);
        sprintf(buf, "Tick %d", ticks[current_node]);
        draw_text_unlocked(buf, 66, 21, 2, 0);
        sprintf(buf, "Value %d", values[current_node] - (middle ? 32 : 0));
        draw_text_unlocked(buf, 66, 23, 2, 0);
	
        SDL_UnlockSurface(screen);
}

static void _env_node_add(byte *nodes, int *current_node, unsigned short ticks[], byte values[])
{
	int newtick, newvalue;
	
	/* is 24 the right number here, or 25? */
	if (*nodes > 24 || *current_node == *nodes - 1)
		return;
	
	newtick = (ticks[*current_node] + ticks[*current_node + 1]) / 2;
	newvalue = (values[*current_node] + values[*current_node + 1]) / 2;
	if (newtick == ticks[*current_node] || newtick == ticks[*current_node + 1]) {
		/* If the current node is at (for example) tick 30, and the next node is at tick 32,
		 * is there any chance of a rounding error that would make newtick 30 instead of 31?
		 * ("Is there a chance the track could bend?") */
		printf("Not enough room!\n");
		return;
	}
	
	(*nodes)++;
	memmove(ticks + *current_node + 1, ticks + *current_node,
		(*nodes - *current_node - 1) * sizeof(ticks[0]));
	memmove(values + *current_node + 1, values + *current_node,
		(*nodes - *current_node - 1) * sizeof(values[0]));
	ticks[*current_node + 1] = newtick;
	values[*current_node + 1] = newvalue;
}

static void _env_node_remove(byte *nodes, int *current_node, unsigned short ticks[], byte values[])
{
	if (*current_node == 0 || *nodes < 3)
		return;
	
	memmove(ticks + *current_node, ticks + *current_node + 1,
		(*nodes - *current_node - 1) * sizeof(ticks[0]));
	memmove(values + *current_node, values + *current_node + 1,
		(*nodes - *current_node - 1) * sizeof(values[0]));
	(*nodes)--;
	if (*current_node >= *nodes)
		*current_node = *nodes - 1;
}

static int _env_handle_key_viewmode(SDL_keysym *k, byte *nodes, int *current_node,
				    unsigned short ticks[], byte values[])
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
		new_node--;
                break;
        case SDLK_RIGHT:
		new_node++;
                break;
	case SDLK_INSERT:
		_env_node_add(nodes, current_node, ticks, values);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DELETE:
		_env_node_remove(nodes, current_node, ticks, values);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_SPACE:
		song_play_note(current_instrument, last_note, 0, 1);
		return 1;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		envelope_edit_mode = 1;
		status.flags |= NEED_UPDATE;
		return 1;
        default:
                return 0;
        }
	
	new_node = CLAMP(new_node, 0, *nodes - 1);
	if (*current_node != new_node) {
		*current_node = new_node;
		status.flags |= NEED_UPDATE;
	}
	
	return 1;
}

static int _env_handle_key_editmode(SDL_keysym * k, byte *nodes, int *current_node, unsigned short ticks[],
				    byte values[], UNUSED byte *loop_start, UNUSED byte *loop_end,
				    UNUSED byte *sustain_start, UNUSED byte *sustain_end)
{
	int new_node = *current_node, new_tick = ticks[*current_node], new_value = values[*current_node];
	
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
		new_value += 16;
		break;
	case SDLK_PAGEDOWN:
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
		new_tick = 0;
		break;
	case SDLK_END:
		new_tick = 10000;
		break;
	case SDLK_INSERT:
		_env_node_add(nodes, current_node, ticks, values);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_DELETE:
		_env_node_remove(nodes, current_node, ticks, values);
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_SPACE:
		song_play_note(current_instrument, last_note, 0, 1);
		return 1;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		envelope_edit_mode = 0;
		break;
        default:
                return 0;
        }
	
	new_node = CLAMP(new_node, 0, *nodes - 1);
	new_tick = (new_node == 0) ? 0 : CLAMP
		(new_tick, ticks[*current_node - 1] + 1,
		 ((*current_node == *nodes - 1) ? 10000 : ticks[*current_node + 1]) - 1);
	new_value = CLAMP(new_value, 0, 64);
	
	if (new_node == *current_node && new_value == values[new_node] && new_tick == ticks[new_node]) {
		/* there's no need to update the screen -- everything's the same as it was */
		return 1;
	}
	*current_node = new_node;
	values[*current_node] = new_value;
	ticks[*current_node] = new_tick;
	
	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */
/* envelope stuff (draw()'s and handle_key()'s) */

static void _draw_env_label(const char *env_name, int is_selected)
{
	int pos = 33;
	
	SDL_LockSurface(screen);
	pos += draw_text_unlocked(env_name, pos, 16, is_selected ? 3 : 0, 2);
	pos += draw_text_unlocked(" Envelope", pos, 16, is_selected ? 3 : 0, 2);
	if (envelope_edit_mode)
		draw_text_unlocked(" (Edit)", pos, 16, is_selected ? 3 : 0, 2);
	SDL_UnlockSurface(screen);
}

static void volume_envelope_draw(void)
{
        int is_selected = (ACTIVE_PAGE.selected_item == 5);
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	_draw_env_label("Volume", is_selected);
	_env_draw(0, current_node_vol, ins->vol_env_nodes, ins->vol_env_ticks, ins->vol_env_values,
		  ins->flags & ENV_VOLLOOP, ins->vol_loop_start, ins->vol_loop_end,
		  ins->flags & ENV_VOLSUSTAIN, ins->vol_sustain_start, ins->vol_sustain_end);
}

static void panning_envelope_draw(void)
{
	int is_selected = (ACTIVE_PAGE.selected_item == 5);
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	_draw_env_label("Panning", is_selected);
	_env_draw(1, current_node_pan, ins->pan_env_nodes, ins->pan_env_ticks, ins->pan_env_values,
		  ins->flags & ENV_PANLOOP, ins->pan_loop_start, ins->pan_loop_end,
		  ins->flags & ENV_PANSUSTAIN, ins->pan_sustain_start, ins->pan_sustain_end);
}

static void pitch_envelope_draw(void)
{
	int is_selected = (ACTIVE_PAGE.selected_item == 5);
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	_draw_env_label("Frequency", is_selected);
	_env_draw((ins->flags & ENV_FILTER) ? 0 : 1, current_node_pitch, ins->pitch_env_nodes,
		  ins->pitch_env_ticks, ins->pitch_env_values, ins->flags & ENV_PITCHLOOP,
		  ins->pitch_loop_start, ins->pitch_loop_end, ins->flags & ENV_PITCHSUSTAIN,
		  ins->pitch_sustain_start, ins->pitch_sustain_end);
}

static int volume_envelope_handle_key(SDL_keysym * k)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);

	if (envelope_edit_mode) {
		return _env_handle_key_editmode(k, &ins->vol_env_nodes, &current_node_vol,
						ins->vol_env_ticks, ins->vol_env_values,
						&ins->vol_loop_start, &ins->vol_loop_end,
						&ins->vol_sustain_start, &ins->vol_sustain_end);
	} else {
		return _env_handle_key_viewmode(k, &ins->vol_env_nodes, &current_node_vol,
						ins->vol_env_ticks, ins->vol_env_values);
	}
}

static int panning_envelope_handle_key(SDL_keysym * k)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);

	if (envelope_edit_mode) {
		return _env_handle_key_editmode(k, &ins->pan_env_nodes, &current_node_pan,
						ins->pan_env_ticks, ins->pan_env_values,
						&ins->pan_loop_start, &ins->pan_loop_end,
						&ins->pan_sustain_start, &ins->pan_sustain_end);
	} else {
		return _env_handle_key_viewmode(k, &ins->pan_env_nodes, &current_node_pan,
						ins->pan_env_ticks, ins->pan_env_values);
	}
}

static int pitch_envelope_handle_key(SDL_keysym * k)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);

	if (envelope_edit_mode) {
		return _env_handle_key_editmode(k, &ins->pitch_env_nodes, &current_node_pitch,
						ins->pitch_env_ticks, ins->pitch_env_values,
						&ins->pitch_loop_start, &ins->pitch_loop_end,
						&ins->pitch_sustain_start, &ins->pitch_sustain_end);
	} else {
		return _env_handle_key_viewmode(k, &ins->pitch_env_nodes, &current_node_pitch,
						ins->pitch_env_ticks, ins->pitch_env_values);
	}
}

/* --------------------------------------------------------------------------------------------------------- */
/* pitch-pan center */

static int pitch_pan_center_handle_key(SDL_keysym *k)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	int ppc = ins->pitch_pan_center;
	
	switch (k->sym) {
	case SDLK_LEFT:
		ppc--;
		break;
	case SDLK_RIGHT:
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
        int selected = (ACTIVE_PAGE.selected_item == 16);
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	draw_text(get_note_string(ins->pitch_pan_center + 1, buf), 54, 45, selected ? 3 : 2, 0);
}

/* --------------------------------------------------------------------------------------------------------- */
/* default key handler (for instrument changing on pgup/pgdn) */

static inline void instrument_list_handle_alt_key(SDL_keysym *k)
{
	//song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	switch (k->sym) {
	case SDLK_n:
		song_toggle_multichannel_mode();
		return;
	default:
		return;
	}
	
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
        int item = ACTIVE_PAGE.selected_item;
        int page = status.current_page;

        switch (item) {
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
                pages[page].selected_item = item;
		togglebutton_set(pages[page].items, item, 0);
                set_page(page);
                instrument_list_subpage = page;
        }
}

/* --------------------------------------------------------------------- */
/* predraw hooks... */

static void instrument_list_general_predraw_hook(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	togglebutton_set(items_general, 6 + ins->nna, 0);
	togglebutton_set(items_general, 10 + ins->dct, 0);
	togglebutton_set(items_general, 14 + ins->dca, 0);
	
        items_general[17].textentry.text = ins->filename;
}

static void instrument_list_volume_predraw_hook(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);

        items_volume[6].toggle.state = !!(ins->flags & ENV_VOLUME);
        items_volume[7].toggle.state = !!(ins->flags & ENV_VOLCARRY);
        items_volume[8].toggle.state = !!(ins->flags & ENV_VOLLOOP);
        items_volume[11].toggle.state = !!(ins->flags & ENV_VOLSUSTAIN);

        items_volume[9].numentry.value = ins->vol_loop_start;
        items_volume[10].numentry.value = ins->vol_loop_end;
        items_volume[12].numentry.value = ins->vol_sustain_start;
        items_volume[13].numentry.value = ins->vol_sustain_end;

        /* mp hack: shifting values all over the place here, ugh */
        items_volume[14].thumbbar.value = ins->global_volume << 1;
        items_volume[15].thumbbar.value = ins->fadeout >> 5;
        items_volume[16].thumbbar.value = ins->volume_swing;
}

static void instrument_list_panning_predraw_hook(void)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	items_panning[6].toggle.state = !!(ins->flags & ENV_PANNING);
	items_panning[7].toggle.state = !!(ins->flags & ENV_PANCARRY);
	items_panning[8].toggle.state = !!(ins->flags & ENV_PANLOOP);
	items_panning[11].toggle.state = !!(ins->flags & ENV_PANSUSTAIN);
	
	items_panning[9].numentry.value = ins->pan_loop_start;
	items_panning[10].numentry.value = ins->pan_loop_end;
	items_panning[12].numentry.value = ins->pan_sustain_start;
	items_panning[13].numentry.value = ins->pan_sustain_end;
	
	items_panning[14].toggle.state = !!(ins->flags & ENV_SETPANNING);
	items_panning[15].thumbbar.value = ins->panning >> 2;
	// (items_panning[16] is the pitch-pan center)
	items_panning[17].thumbbar.value = ins->pitch_pan_separation;
	items_panning[18].thumbbar.value = ins->pan_swing;
}

static void instrument_list_pitch_predraw_hook(void)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	items_pitch[6].menutoggle.state = ((ins->flags & ENV_PITCH)
					   ? ((ins->flags & ENV_FILTER)
					      ? 2 : 1) : 0);
	items_pitch[7].toggle.state = !!(ins->flags & ENV_PITCHCARRY);
	items_pitch[8].toggle.state = !!(ins->flags & ENV_PITCHLOOP);
	items_pitch[11].toggle.state = !!(ins->flags & ENV_PITCHSUSTAIN);

	items_pitch[9].numentry.value = ins->pitch_loop_start;
	items_pitch[10].numentry.value = ins->pitch_loop_end;
	items_pitch[12].numentry.value = ins->pitch_sustain_start;
	items_pitch[13].numentry.value = ins->pitch_sustain_end;
	
	//printf("ins%02d: ch%04d pgm%04d bank%06d drum%04d\n", current_instrument,
	//       ins->midi_channel, ins->midi_program, ins->midi_bank, ins->midi_drum_key);
	items_pitch[14].thumbbar.value = ins->midi_channel;
	items_pitch[15].thumbbar.value = (signed char) ins->midi_program;
	items_pitch[16].thumbbar.value = (signed char) (ins->midi_bank & 0xff);
	items_pitch[17].thumbbar.value = (signed char) (ins->midi_bank >> 8);
	// what is midi_drum_key for?
}

/* --------------------------------------------------------------------- */
/* update values in song */

static void instrument_list_general_update_values(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);

	for (ins->nna = 4; ins->nna--;)
                if (items_general[ins->nna + 6].togglebutton.state)
                        break;
	for (ins->dct = 4; ins->dct--;)
                if (items_general[ins->dct + 10].togglebutton.state)
                        break;
	for (ins->dca = 3; ins->dca--;)
                if (items_general[ins->dca + 14].togglebutton.state)
                        break;
}

static void instrument_list_volume_update_values(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);

	ins->flags &= ~(ENV_VOLUME | ENV_VOLCARRY | ENV_VOLLOOP | ENV_VOLSUSTAIN);
        if (items_volume[6].toggle.state)
                ins->flags |= ENV_VOLUME;
        if (items_volume[7].toggle.state)
                ins->flags |= ENV_VOLCARRY;
        if (items_volume[8].toggle.state)
                ins->flags |= ENV_VOLLOOP;
        if (items_volume[11].toggle.state)
                ins->flags |= ENV_VOLSUSTAIN;

        ins->vol_loop_start = items_volume[9].numentry.value;
        ins->vol_loop_end = items_volume[10].numentry.value;
        ins->vol_sustain_start = items_volume[12].numentry.value;
        ins->vol_sustain_end = items_volume[13].numentry.value;

        /* more ugly shifts */
        ins->global_volume = items_volume[14].thumbbar.value >> 1;
        ins->fadeout = items_volume[15].thumbbar.value << 5;
        ins->volume_swing = items_volume[16].thumbbar.value;
}

static void instrument_list_panning_update_values(void)
{
        song_instrument *ins = song_get_instrument(current_instrument, NULL);
	
	ins->flags &= ~(ENV_PANNING | ENV_PANCARRY | ENV_PANLOOP | ENV_PANSUSTAIN | ENV_SETPANNING);
        if (items_panning[6].toggle.state)
                ins->flags |= ENV_PANNING;
        if (items_panning[7].toggle.state)
                ins->flags |= ENV_PANCARRY;
        if (items_panning[8].toggle.state)
                ins->flags |= ENV_PANLOOP;
        if (items_panning[11].toggle.state)
                ins->flags |= ENV_PANSUSTAIN;
	if (items_panning[14].toggle.state)
		ins->flags |= ENV_SETPANNING;
	
        ins->pan_loop_start = items_panning[9].numentry.value;
        ins->pan_loop_end = items_panning[10].numentry.value;
        ins->pan_sustain_start = items_panning[12].numentry.value;
        ins->pan_sustain_end = items_panning[13].numentry.value;
	
	ins->panning = items_panning[15].thumbbar.value << 2;
	// (items_panning[16] is the pitch-pan center)
        ins->pitch_pan_separation = items_panning[17].thumbbar.value;
        ins->pan_swing = items_panning[18].thumbbar.value;
}

static void instrument_list_pitch_update_values(void)
{
	song_instrument *ins = song_get_instrument(current_instrument, NULL);

	ins->flags &= ~(ENV_PITCH | ENV_PITCHCARRY | ENV_PITCHLOOP | ENV_PITCHSUSTAIN | ENV_FILTER);
	
	switch (items_pitch[6].menutoggle.state) {
	case 2: ins->flags |= ENV_FILTER;
	case 1: ins->flags |= ENV_PITCH;
	}
	
	if (items_pitch[6].menutoggle.state)
		ins->flags |= ENV_PITCH;
	if (items_pitch[7].toggle.state)
		ins->flags |= ENV_PITCHCARRY;
	if (items_pitch[8].toggle.state)
		ins->flags |= ENV_PITCHLOOP;
	if (items_pitch[11].toggle.state)
		ins->flags |= ENV_PITCHSUSTAIN;
	
	ins->pitch_loop_start = items_pitch[9].numentry.value;
	ins->pitch_loop_end= items_pitch[10].numentry.value;
	ins->pitch_sustain_start = items_pitch[12].numentry.value;
	ins->pitch_sustain_end = items_pitch[13].numentry.value;
	
	ins->midi_channel = items_pitch[14].thumbbar.value;
	ins->midi_program = items_pitch[15].thumbbar.value;
	ins->midi_bank = ((items_pitch[17].thumbbar.value << 8)
			  | (items_pitch[16].thumbbar.value & 0xff));
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

        SDL_LockSurface(screen);

        draw_box_unlocked(31, 15, 42, 48, BOX_THICK | BOX_INNER | BOX_INSET);

        /* Kind of a hack, and not really useful, but... :) */
        if (status.flags & CLASSIC_MODE) {
                draw_box_unlocked(55, 46, 73, 48, BOX_THICK | BOX_INNER | BOX_INSET);
                draw_text_unlocked("    ", 69, 47, 1, 0);
        } else {
                draw_box_unlocked(55, 46, 69, 48, BOX_THICK | BOX_INNER | BOX_INSET);
        }

        draw_text_unlocked("New Note Action", 54, 17, 0, 2);
        draw_text_unlocked("Duplicate Check Type & Action", 47, 32, 0, 2);
        draw_text_unlocked("Filename", 47, 47, 0, 2);

        for (n = 0; n < 35; n++) {
                draw_char_unlocked(134, 44 + n, 15, 0, 2);
                draw_char_unlocked(134, 44 + n, 30, 0, 2);
                draw_char_unlocked(154, 44 + n, 45, 0, 2);
        }

        SDL_UnlockSurface(screen);
}

static void instrument_list_volume_draw_const(void)
{
        instrument_list_draw_const();

        draw_fill_chars(57, 28, 62, 29, 0);
        draw_fill_chars(57, 32, 62, 34, 0);
        draw_fill_chars(57, 37, 62, 39, 0);

        SDL_LockSurface(screen);

        draw_box_unlocked(31, 17, 77, 26, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 27, 63, 30, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 31, 63, 35, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 36, 63, 40, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 41, 71, 44, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box_unlocked(53, 45, 71, 47, BOX_THICK | BOX_INNER | BOX_INSET);

        draw_text_unlocked("Volume Envelope", 38, 28, 0, 2);
        draw_text_unlocked("Carry", 48, 29, 0, 2);
        draw_text_unlocked("Envelope Loop", 40, 32, 0, 2);
        draw_text_unlocked("Loop Begin", 43, 33, 0, 2);
        draw_text_unlocked("Loop End", 45, 34, 0, 2);
        draw_text_unlocked("Sustain Loop", 41, 37, 0, 2);
        draw_text_unlocked("SusLoop Begin", 40, 38, 0, 2);
        draw_text_unlocked("SusLoop End", 42, 39, 0, 2);
        draw_text_unlocked("Global Volume", 40, 42, 0, 2);
        draw_text_unlocked("Fadeout", 46, 43, 0, 2);
        draw_text_unlocked("Volume Swing %", 39, 46, 0, 2);

        SDL_UnlockSurface(screen);
}

static void instrument_list_panning_draw_const(void)
{
        instrument_list_draw_const();

        draw_fill_chars(57, 28, 62, 29, 0);
        draw_fill_chars(57, 32, 62, 34, 0);
        draw_fill_chars(57, 37, 62, 39, 0);
        draw_fill_chars(57, 42, 62, 45, 0);

        SDL_LockSurface(screen);

        draw_box_unlocked(31, 17, 77, 26, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 27, 63, 30, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 31, 63, 35, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 36, 63, 40, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 41, 63, 48, BOX_THICK | BOX_INNER | BOX_INSET);

        draw_text_unlocked("Panning Envelope", 37, 28, 0, 2);
        draw_text_unlocked("Carry", 48, 29, 0, 2);
        draw_text_unlocked("Envelope Loop", 40, 32, 0, 2);
        draw_text_unlocked("Loop Begin", 43, 33, 0, 2);
        draw_text_unlocked("Loop End", 45, 34, 0, 2);
        draw_text_unlocked("Sustain Loop", 41, 37, 0, 2);
        draw_text_unlocked("SusLoop Begin", 40, 38, 0, 2);
        draw_text_unlocked("SusLoop End", 42, 39, 0, 2);
        draw_text_unlocked("Default Pan", 42, 42, 0, 2);
        draw_text_unlocked("Pan Value", 44, 43, 0, 2);
	draw_text_unlocked("Pitch-Pan Center", 37, 45, 0, 2);
	draw_text_unlocked("Pitch-Pan Separation", 33, 46, 0, 2);
	/* Hmm. The 's' in swing isn't capitalised. ;) */
	draw_text_unlocked("Pan swing", 44, 47, 0, 2);

	draw_text_unlocked("\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a", 54, 44, 2, 0);
	
        SDL_UnlockSurface(screen);
}

static void instrument_list_pitch_draw_const(void)
{
	instrument_list_draw_const();

        draw_fill_chars(57, 28, 62, 29, 0);
        draw_fill_chars(57, 32, 62, 34, 0);
        draw_fill_chars(57, 37, 62, 39, 0);
	
	SDL_LockSurface(screen);
	
        draw_box_unlocked(31, 17, 77, 26, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 27, 63, 30, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 31, 63, 35, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 36, 63, 40, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(53, 41, 71, 46, BOX_THICK | BOX_INNER | BOX_INSET);
	
	draw_text_unlocked("Frequency Envelope", 35, 28, 0, 2);
	draw_text_unlocked("Carry", 48, 29, 0, 2);
        draw_text_unlocked("Envelope Loop", 40, 32, 0, 2);
        draw_text_unlocked("Loop Begin", 43, 33, 0, 2);
        draw_text_unlocked("Loop End", 45, 34, 0, 2);
        draw_text_unlocked("Sustain Loop", 41, 37, 0, 2);
        draw_text_unlocked("SusLoop Begin", 40, 38, 0, 2);
        draw_text_unlocked("SusLoop End", 42, 39, 0, 2);
	draw_text_unlocked("MIDI Channel", 36, 42, 0, 2);
	draw_text_unlocked("MIDI Program", 36, 43, 0, 2);
	draw_text_unlocked("MIDI Bank Low", 36, 44, 0, 2);
	draw_text_unlocked("MIDI Bank High", 36, 45, 0, 2);
	
	SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */
/* load_page functions */

static void _load_page_common(struct page *page, struct item *page_items)
{
	page->title = "Instrument List (F4)";
	page->handle_key = instrument_list_handle_key;
	page->items = page_items;
	page->help_index = HELP_INSTRUMENT_LIST;
	
	/* the first five items are the same for all four pages. */
	
	/* 0 = instrument list */
	create_other(page_items + 0, 1, instrument_list_handle_key_on_list, instrument_list_draw_list);

	/* 1-4 = subpage switches */
	create_togglebutton(page_items + 1, 32, 13, 7, 1, 5, 0, 2, 2, change_subpage, "General",
			    1, subpage_switches_group);
	create_togglebutton(page_items + 2, 44, 13, 7, 2, 5, 1, 3, 3, change_subpage, "Volume",
			    1, subpage_switches_group);
	create_togglebutton(page_items + 3, 56, 13, 7, 3, 5, 2, 4, 4, change_subpage, "Panning",
			    1, subpage_switches_group);
	create_togglebutton(page_items + 4, 68, 13, 7, 4, 5, 3, 0, 0, change_subpage, "Pitch",
			    2, subpage_switches_group);
}


void instrument_list_general_load_page(struct page *page)
{
	_load_page_common(page, items_general);
	
        page->draw_const = instrument_list_general_draw_const;
        page->predraw_hook = instrument_list_general_predraw_hook;
        page->total_items = 18;
	
	/* special case stuff */
        items_general[1].togglebutton.state = 1;
	items_general[2].next.down = items_general[3].next.down = items_general[4].next.down = 6;
	
        /* 5 = note trans table */
	create_other(items_general + 5, 6, note_trans_handle_key, note_trans_draw);

        /* 6-9 = nna toggles */
        create_togglebutton(items_general + 6, 46, 19, 29, 2, 7, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Cut", 2, nna_group);
        create_togglebutton(items_general + 7, 46, 22, 29, 6, 8, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Continue", 2, nna_group);
        create_togglebutton(items_general + 8, 46, 25, 29, 7, 9, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Off", 2, nna_group);
        create_togglebutton(items_general + 9, 46, 28, 29, 8, 10, 5, 0, 0,
                            instrument_list_general_update_values,
                            "Note Fade", 2, nna_group);

        /* 10-13 = dct toggles */
        create_togglebutton(items_general + 10, 46, 34, 12, 9, 11, 5, 14,
                            14, instrument_list_general_update_values,
                            "Disabled", 2, dct_group);
        create_togglebutton(items_general + 11, 46, 37, 12, 10, 12, 5, 15,
                            15, instrument_list_general_update_values,
                            "Note", 2, dct_group);
        create_togglebutton(items_general + 12, 46, 40, 12, 11, 13, 5, 16,
                            16, instrument_list_general_update_values,
                            "Sample", 2, dct_group);
        create_togglebutton(items_general + 13, 46, 43, 12, 12, 17, 5, 13,
                            13, instrument_list_general_update_values,
                            "Instrument", 2, dct_group);
        /* 14-16 = dca toggles */
        create_togglebutton(items_general + 14, 62, 34, 13, 9, 15, 10, 0,
                            0, instrument_list_general_update_values,
                            "Note Cut", 2, dca_group);
        create_togglebutton(items_general + 15, 62, 37, 13, 14, 16, 11, 0,
                            0, instrument_list_general_update_values,
                            "Note Off", 2, dca_group);
        create_togglebutton(items_general + 16, 62, 40, 13, 15, 17, 12, 0,
                            0, instrument_list_general_update_values,
                            "Note Fade", 2, dca_group);
        /* 17 = filename */
        /* impulse tracker has a 17-char-wide box for the filename for
         * some reason, though it still limits the actual text to 12
         * characters. go figure... */
        create_textentry(items_general + 17, 56, 47, 13, 13, 17, 0, NULL,
                         NULL, 12);
}

void instrument_list_volume_load_page(struct page *page)
{
	_load_page_common(page, items_volume);
	
        page->draw_const = instrument_list_volume_draw_const;
        page->predraw_hook = instrument_list_volume_predraw_hook;
        page->total_items = 17;

        /* 5 = volume envelope */
	create_other(items_volume + 5, 0, volume_envelope_handle_key, volume_envelope_draw);

        /* 6-7 = envelope switches */
        create_toggle(items_volume + 6, 54, 28, 5, 7, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_toggle(items_volume + 7, 54, 29, 6, 8, 0, 0, 0,
                      instrument_list_volume_update_values);

        /* 8-10 envelope loop settings */
        create_toggle(items_volume + 8, 54, 32, 7, 9, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_numentry(items_volume + 9, 54, 33, 3, 8, 10, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);
        create_numentry(items_volume + 10, 54, 34, 3, 9, 11, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);

        /* 11-13 = susloop settings */
        create_toggle(items_volume + 11, 54, 37, 10, 12, 0, 0, 0,
                      instrument_list_volume_update_values);
        create_numentry(items_volume + 12, 54, 38, 3, 11, 13, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);
        create_numentry(items_volume + 13, 54, 39, 3, 12, 14, 0,
                        instrument_list_volume_update_values, 0, 1,
                        numentry_cursor_pos + 0);

        /* 14-16 = volume thumbbars */
        create_thumbbar(items_volume + 14, 54, 42, 17, 13, 15, 0,
                        instrument_list_volume_update_values, 0, 128);
        create_thumbbar(items_volume + 15, 54, 43, 17, 14, 16, 0,
                        instrument_list_volume_update_values, 0, 128);
        create_thumbbar(items_volume + 16, 54, 46, 17, 15, 16, 0,
                        instrument_list_volume_update_values, 0, 100);
}

void instrument_list_panning_load_page(struct page *page)
{
	_load_page_common(page, items_panning);
	
        page->draw_const = instrument_list_panning_draw_const;
        page->predraw_hook = instrument_list_panning_predraw_hook;
        page->total_items = 19;
	
        /* 5 = panning envelope */
	create_other(items_panning + 5, 0, panning_envelope_handle_key, panning_envelope_draw);

        /* 6-7 = envelope switches */
        create_toggle(items_panning + 6, 54, 28, 5, 7, 0, 0, 0,
                      instrument_list_panning_update_values);
        create_toggle(items_panning + 7, 54, 29, 6, 8, 0, 0, 0,
                      instrument_list_panning_update_values);

        /* 8-10 envelope loop settings */
        create_toggle(items_panning + 8, 54, 32, 7, 9, 0, 0, 0,
                      instrument_list_panning_update_values);
        create_numentry(items_panning + 9, 54, 33, 3, 8, 10, 0,
                        instrument_list_panning_update_values, 0, 1,
                        numentry_cursor_pos + 1);
        create_numentry(items_panning + 10, 54, 34, 3, 9, 11, 0,
                        instrument_list_panning_update_values, 0, 1,
                        numentry_cursor_pos + 1);

        /* 11-13 = susloop settings */
        create_toggle(items_panning + 11, 54, 37, 10, 12, 0, 0, 0,
                      instrument_list_panning_update_values);
        create_numentry(items_panning + 12, 54, 38, 3, 11, 13, 0,
                        instrument_list_panning_update_values, 0, 1,
                        numentry_cursor_pos + 1);
        create_numentry(items_panning + 13, 54, 39, 3, 12, 14, 0,
                        instrument_list_panning_update_values, 0, 1,
                        numentry_cursor_pos + 1);

	/* 14-15 = default panning */
        create_toggle(items_panning + 14, 54, 42, 13, 15, 0, 0, 0,
		      instrument_list_panning_update_values);
        create_thumbbar(items_panning + 15, 54, 43, 9, 14, 16, 0,
                        instrument_list_panning_update_values, 0, 64);
	
	/* 16 = pitch-pan center */
	create_other(items_panning + 16, 0, pitch_pan_center_handle_key, pitch_pan_center_draw);
        items_panning[16].next.up = 15;
        items_panning[16].next.down = 17;
	
	/* 17-18 = other panning stuff */
	create_thumbbar(items_panning + 17, 54, 46, 9, 16, 18, 0,
			instrument_list_panning_update_values, -32, 32);
	create_thumbbar(items_panning + 18, 54, 47, 9, 17, 18, 0,
			instrument_list_panning_update_values, 0, 64);
}

void instrument_list_pitch_load_page(struct page *page)
{
	_load_page_common(page, items_pitch);
	
        page->draw_const = instrument_list_pitch_draw_const;
        page->predraw_hook = instrument_list_pitch_predraw_hook;
        page->total_items = 18;
	
        /* 5 = pitch envelope */
	create_other(items_pitch + 5, 0, pitch_envelope_handle_key, pitch_envelope_draw);
	
        /* 6-7 = envelope switches */
        create_menutoggle(items_pitch + 6, 54, 28, 5, 7, 0, 0, 0,
                      instrument_list_pitch_update_values, pitch_envelope_states);
        create_toggle(items_pitch + 7, 54, 29, 6, 8, 0, 0, 0,
                      instrument_list_pitch_update_values);

        /* 8-10 envelope loop settings */
        create_toggle(items_pitch + 8, 54, 32, 7, 9, 0, 0, 0,
                      instrument_list_pitch_update_values);
        create_numentry(items_pitch + 9, 54, 33, 3, 8, 10, 0,
                        instrument_list_pitch_update_values, 0, 1,
                        numentry_cursor_pos + 2);
        create_numentry(items_pitch + 10, 54, 34, 3, 9, 11, 0,
                        instrument_list_pitch_update_values, 0, 1,
                        numentry_cursor_pos + 2);

        /* 11-13 = susloop settings */
        create_toggle(items_pitch + 11, 54, 37, 10, 12, 0, 0, 0,
                      instrument_list_pitch_update_values);
        create_numentry(items_pitch + 12, 54, 38, 3, 11, 13, 0,
                        instrument_list_pitch_update_values, 0, 1,
                        numentry_cursor_pos + 2);
        create_numentry(items_pitch + 13, 54, 39, 3, 12, 14, 0,
                        instrument_list_pitch_update_values, 0, 1,
                        numentry_cursor_pos + 2);
	
	/* 14-17 = midi crap */
	create_thumbbar(items_pitch + 14, 54, 42, 17, 13, 15, 0,
			instrument_list_pitch_update_values, 0, 17);
	create_thumbbar(items_pitch + 15, 54, 43, 17, 14, 16, 0,
			instrument_list_pitch_update_values, -1, 127);
	create_thumbbar(items_pitch + 16, 54, 44, 17, 15, 17, 0,
			instrument_list_pitch_update_values, -1, 127);
	create_thumbbar(items_pitch + 17, 54, 45, 17, 16, 17, 0,
			instrument_list_pitch_update_values, -1, 127);
	items_pitch[14].thumbbar.text_at_min = "Off";
	items_pitch[14].thumbbar.text_at_max = "Mapped";
	items_pitch[15].thumbbar.text_at_min = "Off";
	items_pitch[16].thumbbar.text_at_min = "Off";
	items_pitch[17].thumbbar.text_at_min = "Off";
}
