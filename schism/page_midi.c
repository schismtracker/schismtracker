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

#define NEED_TIME
#include "headers.h"

#include "it.h"
#include "page.h"
#include "midi.h"

#include "song.h"

/* --------------------------------------------------------------------- */

static int top_midi_port = 0;
static int current_port = 0;
static struct widget widgets_midi[17];
static time_t last_midi_poll = 0;

/* --------------------------------------------------------------------- */

static void midi_output_config(void)
{
        set_page(PAGE_MIDI_OUTPUT);
}


static void update_ip_ports(void)
{
        if (widgets_midi[12].d.thumbbar.value > 0 && (status.flags & NO_NETWORK)) {
                status_text_flash("Networking is disabled");
                widgets_midi[12].d.thumbbar.value = 0;
        } else {
                ip_midi_setports(widgets_midi[12].d.thumbbar.value);
        }

        last_midi_poll = 0;
        status.flags |= NEED_UPDATE;
}

static void update_midi_values(void)
{
        midi_flags = 0
        |       (widgets_midi[1].d.toggle.state ? MIDI_TICK_QUANTIZE : 0)
        |       (widgets_midi[2].d.toggle.state ? MIDI_BASE_PROGRAM1 : 0)
        |       (widgets_midi[3].d.toggle.state ? MIDI_RECORD_NOTEOFF : 0)
        |       (widgets_midi[4].d.toggle.state ? MIDI_RECORD_VELOCITY : 0)
        |       (widgets_midi[5].d.toggle.state ? MIDI_RECORD_AFTERTOUCH : 0)
        |       (widgets_midi[6].d.toggle.state ? MIDI_CUT_NOTE_OFF : 0)
        |       (widgets_midi[9].d.toggle.state ? MIDI_PITCHBEND : 0)
        ;
        if (widgets_midi[11].d.toggle.state)
                current_song->flags |= SONG_EMBEDMIDICFG;
        else
                current_song->flags &= ~SONG_EMBEDMIDICFG;

        midi_amplification = widgets_midi[7].d.thumbbar.value;
        midi_c5note = widgets_midi[8].d.thumbbar.value;
        midi_pitch_depth = widgets_midi[10].d.thumbbar.value;
}

static void get_midi_config(void)
{
        widgets_midi[1].d.toggle.state = !!(midi_flags & MIDI_TICK_QUANTIZE);
        widgets_midi[2].d.toggle.state = !!(midi_flags & MIDI_BASE_PROGRAM1);
        widgets_midi[3].d.toggle.state = !!(midi_flags & MIDI_RECORD_NOTEOFF);
        widgets_midi[4].d.toggle.state = !!(midi_flags & MIDI_RECORD_VELOCITY);
        widgets_midi[5].d.toggle.state = !!(midi_flags & MIDI_RECORD_AFTERTOUCH);
        widgets_midi[6].d.toggle.state = !!(midi_flags & MIDI_CUT_NOTE_OFF);
        widgets_midi[9].d.toggle.state = !!(midi_flags & MIDI_PITCHBEND);
        widgets_midi[11].d.toggle.state = !!(current_song->flags & SONG_EMBEDMIDICFG);

        widgets_midi[7].d.thumbbar.value = midi_amplification;
        widgets_midi[8].d.thumbbar.value = midi_c5note;
        widgets_midi[10].d.thumbbar.value = midi_pitch_depth;
        widgets_midi[12].d.thumbbar.value = ip_midi_getports();
}

static void toggle_port(void)
{
        struct midi_port *p;
        p = midi_engine_port(current_port, NULL);
        if (p) {
                status.flags |= NEED_UPDATE;

                if (p->disable) if (!p->disable(p)) return;
                switch (p->io) {
                case 0:
                        if (p->iocap & MIDI_INPUT) p->io = MIDI_INPUT;
                        else if (p->iocap & MIDI_OUTPUT) p->io = MIDI_OUTPUT;
                        break;
                case MIDI_INPUT:
                        if (p->iocap & MIDI_OUTPUT) p->io = MIDI_OUTPUT;
                        else p->io = 0;
                        break;
                case MIDI_OUTPUT:
                        if (p->iocap & MIDI_INPUT) p->io |= MIDI_INPUT;
                        else p->io = 0;
                        break;
                case MIDI_INPUT|MIDI_OUTPUT:
                        p->io = 0;
                        break;
                };

                if (p->enable) {
                        if (!p->enable(p)) {
                                p->io = 0;
                                return;
                        }
                }
        }
}

static int midi_page_handle_key(struct key_event * k)
{
        int new_port = current_port;
        int pos;

        if (k->mouse == MOUSE_SCROLL_UP) {
                new_port -= MOUSE_SCROLL_LINES;
        } else if (k->mouse == MOUSE_SCROLL_DOWN) {
                new_port += MOUSE_SCROLL_LINES;
        } else if (k->mouse) {
                if (k->x >= 3 && k->x <= 11 && k->y >= 15 && k->y <= 27) {
                        if (k->mouse == MOUSE_DBLCLICK) {
                                if (k->state == KEY_PRESS)
                                        return 0;
                                toggle_port();
                                return 1;
                        }
                        new_port = top_midi_port + (k->y - 15);
                } else {
                        return 0;
                }
        }

        switch (k->sym) {
        case SDLK_SPACE:
                if (k->state == KEY_PRESS)
                        return 1;
                toggle_port();
                return 1;
        case SDLK_PAGEUP:
                new_port -= 13;
                if (new_port < 0) new_port = 0;
                break;
        case SDLK_PAGEDOWN:
                new_port += 13;
                if (new_port >= midi_engine_port_count()) {
                        new_port = midi_engine_port_count() - 1;
                }
                break;
        case SDLK_HOME:
                new_port = 0;
                break;
        case SDLK_END:
                new_port = midi_engine_port_count() - 1;
                break;
        case SDLK_UP:
                new_port--;
                break;
        case SDLK_DOWN:
                new_port++;
                break;
        case SDLK_TAB:
                if (k->state == KEY_RELEASE)
                        return 1;
                change_focus_to(1);
                status.flags |= NEED_UPDATE;
                return 1;
        default:
                if (!k->mouse) return 0;
                break;
        };
        if (k->state == KEY_RELEASE)
                return 0;

        if (new_port != current_port) {
                if (new_port < 0 || new_port >= midi_engine_port_count()) {
                        new_port = current_port;
                        if (k->sym == SDLK_DOWN) {
                                change_focus_to(1);
                        }
                }

                current_port = new_port;
                if (current_port < top_midi_port)
                        top_midi_port = current_port;

                pos = current_port - top_midi_port;
                if (pos > 12) top_midi_port = current_port - 12;
                if (top_midi_port < 0) top_midi_port = 0;

                status.flags |= NEED_UPDATE;
        }

        return 1;
}

static void midi_page_redraw(void)
{
        draw_text(       "Tick quantize", 6, 30, 0, 2);
        draw_text(      "Base Program 1", 5, 31, 0, 2);
        draw_text(     "Record Note-Off", 4, 32, 0, 2);
        draw_text(     "Record Velocity", 4, 33, 0, 2);
        draw_text(   "Record Aftertouch", 2, 34, 0, 2);
        draw_text(        "Cut note off", 7, 35, 0, 2);

        draw_fill_chars(23, 30, 24, 35, 0);
        draw_box(19,29,25,36, BOX_THIN|BOX_INNER|BOX_INSET);

        draw_box(52,29,73,32, BOX_THIN|BOX_INNER|BOX_INSET);

        draw_fill_chars(56, 34, 72, 34, 0);
        draw_box(52,33,73,36, BOX_THIN|BOX_INNER|BOX_INSET);

        draw_fill_chars(56, 38, 72, 38, 0);
        draw_box(52,37,73,39, BOX_THIN|BOX_INNER|BOX_INSET);

        draw_text(    "Amplification", 39, 30, 0, 2);
        draw_text(   "C-5 Note-value", 38, 31, 0, 2);
        draw_text("Output MIDI pitch", 35, 34, 0, 2);
        draw_text("Pitch wheel depth", 35, 35, 0, 2);
        draw_text(  "Embed MIDI data", 37, 38, 0, 2);

        draw_text(    "IP MIDI ports", 39, 41, 0, 2);
        draw_box(52,40,73,42, BOX_THIN|BOX_INNER|BOX_INSET);
}

static void midi_page_draw_portlist(void)
{
        struct midi_port *p;
        const char *name, *state;
        char buffer[64];
        int i, n, ct, fg, bg;
        unsigned long j;
        time_t now;

        draw_fill_chars(3, 15, 76, 28, 0);
        draw_text("Midi ports:", 2, 13, 0, 2);
        draw_box(2,14,77,28, BOX_THIN|BOX_INNER|BOX_INSET);

        time(&now);
        if ((now - last_midi_poll) > 10) {
                last_midi_poll = now;
                midi_engine_poll_ports();
        }

        ct = midi_engine_port_count();
        for (i = 0; i < 13; i++) {
                draw_char(168, 12, i + 15, 2, 0);

                if (top_midi_port + i >= ct)
                        continue; /* err */

                p = midi_engine_port(top_midi_port + i, &name);
                if (current_port == top_midi_port + i
                    && ACTIVE_WIDGET.type == WIDGET_OTHER) {
                        fg = 0;
                        bg = 3;
                } else {
                        fg = 5;
                        bg = 0;
                }
                draw_text_len(name, 64, 13, 15+i, 5, 0);

                /* portability: should use difftime */
                if (status.flags & MIDI_EVENT_CHANGED
                    && (time(NULL) - status.last_midi_time) < 3
                    && ((!status.last_midi_port && p->io & MIDI_OUTPUT)
                    || p == (struct midi_port *) status.last_midi_port)) {
                        for (j = n = 0; j < 21 && j < status.last_midi_len; j++) { /* 21 is approx 64/3 */
                                sprintf(buffer + n, "%02X ", status.last_midi_event[j]);
                                n += 3;
                        }
                        draw_text(buffer, 77 - strlen(buffer), 15+i,
                                status.last_midi_port ? 4 : 10, 0);
                }

                switch (p->io) {
                        case 0:                         state = "Disabled "; break;
                        case MIDI_INPUT:                state = "   Input "; break;
                        case MIDI_OUTPUT:               state = "  Output "; break;
                        case MIDI_INPUT | MIDI_OUTPUT:  state = "  Duplex "; break;
                        default:                        state = " Enabled?"; break;
                }
                draw_text(state, 3, 15 + i, fg, bg);
        }
}

/* --------------------------------------------------------------------- */

void midi_load_page(struct page *page)
{
        page->title = "Midi Screen (Shift-F1)";
        page->draw_const = midi_page_redraw;
        page->song_changed_cb = NULL;
        page->predraw_hook = NULL;
        page->playback_update = NULL;
        page->handle_key = NULL;
        page->set_page = get_midi_config;
        page->total_widgets = 15;
        page->widgets = widgets_midi;
        page->help_index = HELP_GLOBAL;

        create_other(widgets_midi + 0, 0, midi_page_handle_key, midi_page_draw_portlist);
        widgets_midi[0].x = 2;
        widgets_midi[0].y = 14;
        widgets_midi[0].width = 75;
        widgets_midi[0].height = 15;

        create_toggle(widgets_midi + 1, 20, 30, 0, 2, 7, 7, 7, update_midi_values);
        create_toggle(widgets_midi + 2, 20, 31, 1, 3, 8, 8, 8, update_midi_values);
        create_toggle(widgets_midi + 3, 20, 32, 2, 4, 8, 8, 8, update_midi_values);
        create_toggle(widgets_midi + 4, 20, 33, 3, 5, 9, 9, 9, update_midi_values);
        create_toggle(widgets_midi + 5, 20, 34, 4, 6, 9, 9, 9, update_midi_values);
        create_toggle(widgets_midi + 6, 20, 35, 5, 13, 10, 10, 10, update_midi_values);
        create_thumbbar(widgets_midi + 7, 53, 30, 20, 0, 8, 1, update_midi_values, 0, 200);
        create_thumbbar(widgets_midi + 8, 53, 31, 20, 7, 9, 2, update_midi_values, 0, 127);
        create_toggle(widgets_midi + 9, 53, 34, 8, 10, 5, 5, 5, update_midi_values);
        create_thumbbar(widgets_midi + 10, 53, 35, 20, 9, 11, 6, update_midi_values, 0, 48);
        create_toggle(widgets_midi + 11, 53, 38, 10, 12, 13, 13, 13, update_midi_values);
        create_thumbbar(widgets_midi + 12, 53, 41, 20, 11, 12, 13, update_ip_ports, 0, 128);
        create_button(widgets_midi + 13, 2, 41, 27, 6, 14, 12, 12, 12,
                midi_output_config, "MIDI Output Configuration", 2);
        create_button(widgets_midi + 14, 2, 44, 27, 13, 14, 12, 12, 12,
                cfg_midipage_save, "Save Output Configuration", 2);
}

