/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
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

#include "sdlmain.h"

#include <time.h>

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
	if (widgets_midi[15].d.thumbbar.value > 0 && (status.flags & NO_NETWORK)) {
		status_text_flash("Networking is disabled");
		widgets_midi[15].d.thumbbar.value = 0;
	} else {
		ip_midi_setports(widgets_midi[15].d.thumbbar.value);
	}

	last_midi_poll = 0;
	status.flags |= NEED_UPDATE;
}
static void update_midi_values(void)
{
	midi_flags =
		(widgets_midi[1].d.toggle.state ? MIDI_TICK_QUANTIZE : 0)
	|	(widgets_midi[2].d.toggle.state ? MIDI_BASE_PROGRAM1 : 0)
	|	(widgets_midi[3].d.toggle.state ? MIDI_RECORD_NOTEOFF : 0)
	|	(widgets_midi[4].d.toggle.state ? MIDI_RECORD_VELOCITY : 0)
	|	(widgets_midi[5].d.toggle.state ? MIDI_RECORD_AFTERTOUCH : 0)
	|	(widgets_midi[6].d.toggle.state ? MIDI_CUT_NOTE_OFF : 0)
	|	(widgets_midi[9].d.toggle.state ? MIDI_PITCH_BEND : 0)
	|	(widgets_midi[11].d.toggle.state? MIDI_EMBED_DATA : 0)
	|	(widgets_midi[14].d.toggle.state? MIDI_RECORD_SDX : 0);

	midi_amplification = widgets_midi[7].d.thumbbar.value;
	midi_c5note = widgets_midi[8].d.thumbbar.value;
	midi_pitch_depth = widgets_midi[10].d.thumbbar.value;
	if (midi_flags & MIDI_EMBED_DATA) {
		status.flags |= SONG_NEEDS_SAVE;
	}
}
static void save_midi_config(void)
{
	cfg_midipage_save();
	status_text_flash("Configuration saved");
}
static void toggle_port(void)
{
	struct midi_port *p;
	p = midi_engine_port(current_port, 0);
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
		new_port--;
	} else if (k->mouse == MOUSE_SCROLL_DOWN) {
		new_port++;
	} else if (k->mouse) {
		if (k->x >= 3 && k->x <= 11 && k->y >= 15 && k->y <= 27) {
			if (k->mouse == MOUSE_DBLCLICK) {
				if (k->state) return 0;
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
		if (!k->state) return 1;
		toggle_port();
		return 1;
	case SDLK_PAGEUP:
		new_port -= 13;
		if (new_port < 0) new_port = 0;
		break;
	case SDLK_PAGEDOWN:
		new_port += 13;
		if (new_port >= midi_engine_port_count()) {
			new_port = midi_engine_port_count()-1;
		}
		break;
	case SDLK_HOME:
		new_port = 0;
		break;
	case SDLK_END:
		new_port = midi_engine_port_count()-1;
		break;
	case SDLK_UP:
		new_port--;
		break;
	case SDLK_DOWN:
		new_port++;
		break;
	case SDLK_TAB:
		if (k->state) return 1;
		change_focus_to(1);
		status.flags |= NEED_UPDATE;
		return 1;
	default:
		if (!k->mouse) return 0;
		break;
	};
	if (k->state) return 0;

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
	struct midi_port *p;
	const char *name;
	char buffer[64];
	int i, n, ct, fg, bg;
	unsigned long j;
	time_t now;

	draw_fill_chars(3, 15, 76, 28, 0);
	draw_text((unsigned char *) "Midi ports:", 2, 13, 0, 2);
	draw_box(2,14,77,28, BOX_THIN|BOX_INNER|BOX_INSET);

	draw_text((unsigned char *)        "Tick quantize", 6, 30, 0, 2);
	draw_text((unsigned char *)       "Base Program 1", 5, 31, 0, 2);
	draw_text((unsigned char *)      "Record Note-Off", 4, 32, 0, 2);
	draw_text((unsigned char *)      "Record Velocity", 4, 33, 0, 2);
	draw_text((unsigned char *)    "Record Aftertouch", 2, 34, 0, 2);
	draw_text((unsigned char *)     "Record using SDx", 3, 35, 0, 2);
	draw_text((unsigned char *)         "Cut note off", 7, 36, 0, 2);

	draw_fill_chars(23, 30, 24, 36, 0);
	draw_box(19,29,25,37, BOX_THIN|BOX_INNER|BOX_INSET);

	draw_box(52,29,73,32, BOX_THIN|BOX_INNER|BOX_INSET);

	draw_fill_chars(56, 34, 72, 34, 0);
	draw_box(52,33,73,36, BOX_THIN|BOX_INNER|BOX_INSET);

	draw_fill_chars(56, 38, 72, 38, 0);
	draw_box(52,37,73,39, BOX_THIN|BOX_INNER|BOX_INSET);

	draw_text((unsigned char *)     "Amplification", 39, 30, 0, 2);
	draw_text((unsigned char *)    "C-5 Note-value", 38, 31, 0, 2);
	draw_text((unsigned char *) "Output MIDI pitch", 35, 34, 0, 2);
	draw_text((unsigned char *) "Pitch wheel depth", 35, 35, 0, 2);
	draw_text((unsigned char *)   "Embed MIDI data", 37, 38, 0, 2);

	draw_text((unsigned char *)     "IP MIDI ports", 39, 41, 0, 2);
	draw_box(52,40,73,42, BOX_THIN|BOX_INNER|BOX_INSET);

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
		draw_text_len((unsigned char *) name, 64, 13, 15+i, 5, 0);

		/* portability: should use difftime */
		if (status.flags & MIDI_EVENT_CHANGED
		    && (time(NULL) - status.last_midi_time) < 3
		    && ((!status.last_midi_port && p->io & MIDI_OUTPUT)
		    || p == (struct midi_port *) status.last_midi_port)) {
			for (j = n = 0; j < 21 && j < status.last_midi_len; j++) { /* 21 is approx 64/3 */
				sprintf(buffer + n, "%02x ", status.last_midi_event[j]);
				n += 3;
			}
			draw_text((unsigned char *) buffer, 77 - strlen(buffer), 15+i,
				status.last_midi_port ? 4 : 10, 0);
		}

		switch (p->io) {
		case 0:
			draw_text((unsigned char *) "Disabled ", 3, 15+i, fg, bg);
			break;
		case MIDI_INPUT:
			draw_text((unsigned char *) "   Input ", 3, 15+i, fg, bg);
			break;
		case MIDI_OUTPUT:
			draw_text((unsigned char *) "  Output ", 3, 15+i, fg, bg);
			break;
		case MIDI_INPUT|MIDI_OUTPUT:
			draw_text((unsigned char *) "  Duplex ", 3, 15+i, fg, bg);
			break;
		default:
			draw_text((unsigned char *) " Enabled ", 3, 15+i, fg, bg);
			break;
		};
	}
}

/* --------------------------------------------------------------------- */

void midi_load_page(struct page *page)
{
        page->title = "Midi Screen (Shift-F1)";
        page->draw_const = NULL;
        page->song_changed_cb = NULL;
        page->predraw_hook = NULL;
        page->playback_update = NULL;
        page->handle_key = NULL;
        page->set_page = NULL;
        page->total_widgets = 16;
        page->widgets = widgets_midi;
        page->help_index = HELP_GLOBAL;

	create_other(widgets_midi+0, 0, midi_page_handle_key, midi_page_redraw);
	widgets_midi[0].x = 2;
	widgets_midi[0].y = 14;
	widgets_midi[0].width = 75;
	widgets_midi[0].height = 15;

	create_toggle(widgets_midi+1, 20, 30,
			0, 2, 1, 7, 2, update_midi_values); /* on */
	widgets_midi[1].d.toggle.state = midi_flags & MIDI_TICK_QUANTIZE;
	create_toggle(widgets_midi+2, 20, 31,
			1, 3, 2, 8, 3, update_midi_values); /* off */
	widgets_midi[2].d.toggle.state = midi_flags & MIDI_BASE_PROGRAM1;
	create_toggle(widgets_midi+3, 20, 32,
			2, 4, 3, 3, 4, update_midi_values);
	widgets_midi[3].d.toggle.state = midi_flags & MIDI_RECORD_NOTEOFF;
	create_toggle(widgets_midi+4, 20, 33,
			3, 5, 4, 9, 5, update_midi_values);
	widgets_midi[4].d.toggle.state = midi_flags & MIDI_RECORD_VELOCITY;
	create_toggle(widgets_midi+5, 20, 34,
			4, 14, 5, 10, 14, update_midi_values);
	widgets_midi[5].d.toggle.state = midi_flags & MIDI_RECORD_AFTERTOUCH;

	create_toggle(widgets_midi+14, 20, 35,
			5, 6, 6, 6, 6, update_midi_values); /* off */
	widgets_midi[14].d.toggle.state = midi_flags & MIDI_RECORD_SDX;

	create_toggle(widgets_midi+6, 20, 36,
			14, 12, 6, 6, 7, update_midi_values); /* off */
	widgets_midi[6].d.toggle.state = midi_flags & MIDI_CUT_NOTE_OFF;



	create_thumbbar(widgets_midi+7, 53, 30, 20,
			0, 8, 8, update_midi_values, 0, 200); /* d. 100 */
	widgets_midi[7].d.thumbbar.value = midi_amplification;
	create_thumbbar(widgets_midi+8, 53, 31, 20,
			7, 9, 9, update_midi_values, 0, 127); /* d. 60 */
	widgets_midi[8].d.thumbbar.value = midi_c5note;


	create_toggle(widgets_midi+9, 53, 34,
			8, 10, 4, 9, 10, update_midi_values); /* off */
	widgets_midi[9].d.toggle.state = midi_flags & MIDI_PITCH_BEND;
	create_thumbbar(widgets_midi+10, 53, 35, 20,
			9, 11, 11, update_midi_values, 0, 48); /* d. 0 */
	widgets_midi[10].d.thumbbar.value = midi_pitch_depth;

	create_toggle(widgets_midi+11, 53, 38,
			10, 15, 6, 11, 15, update_midi_values); /* off */
	widgets_midi[11].d.toggle.state = midi_flags & MIDI_EMBED_DATA;

	create_thumbbar(widgets_midi+15, 53, 41, 20,
			11, 15, 12, update_ip_ports, 0, 128);
	widgets_midi[15].d.thumbbar.value = ip_midi_getports();

	create_button(widgets_midi+12, 2, 41,
		27,
		6, 13, 12, 12, 13,
		midi_output_config,
		"MIDI Output Configuration",
		2);
	create_button(widgets_midi+13, 2, 44,
		27,
		12, 13, 13, 13, 0,
		save_midi_config,
		"Save Output Configuration",
		2);
}
