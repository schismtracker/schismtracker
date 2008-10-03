/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
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

#include "clippy.h"

#include "song.h"
#include "dmoz.h"
#include "sample-edit.h"

#include <math.h>			/* for pow */

#include "video.h"

/* --------------------------------------------------------------------- */
/* static in my attic */
static struct vgamem_overlay sample_image = {
	55,26,76,29,
	0, 0, 0, 0,
};

static int dialog_f1_hack = 0;

static struct widget widgets_samplelist[20];
static const int vibrato_waveforms[] = { 15, 16, 17, 18, -1 };

static int top_sample = 1;
static int current_sample = 1;
static int need_retrigger = -1;
static int last_keyup = -1;

static int sample_list_cursor_pos = 25;	/* the "play" text */

static void sample_adlibconfig_dialog(UNUSED void *ign);

/* shared by all the numentry widgets */
static int sample_numentry_cursor_pos = 0;

/* for the loops */
static const char *const loop_states[] = { "Off", "On Forwards", "On Ping Pong", NULL };

/* playback */
static int last_note = 61;		/* C-5 */

/* woo */
static int _is_magic_sample(int no);
static void _fix_accept_text(void)
{
	if (_is_magic_sample(current_sample)) {
		widgets_samplelist[0].accept_text = (sample_list_cursor_pos == 23 ? 0 : 1);
	} else {
		widgets_samplelist[0].accept_text = (sample_list_cursor_pos == 25 ? 0 : 1);
	}
}
static int _last_vis_sample(void)
{
	int i, j, n;

	n = 99;
	j = 0;
	/* 65 is last visible sample on last page */
	for (i = 65; i <= SCHISM_MAX_SAMPLES; i++) {
		if (!song_sample_is_empty(i)) {
			j = i;
		}
	}
	while ((j + 34) > n) n += 34;
	if (n >= SCHISM_MAX_SAMPLES) n = SCHISM_MAX_SAMPLES;
	return n;
}
static int _is_magic_sample(int no)
{
	char *name;
	song_sample *sample;
	int pn;

	sample = song_get_sample(no, &name);
	if (sample && name && ((unsigned char)name[23]) == 0xFF) {
		pn = ((unsigned char)name[24]);
		if (pn < 200) return 1;
	}
	return 0;
}
/* --------------------------------------------------------------------- */

static void sample_list_reposition(void)
{
	if (dialog_f1_hack) {
		sample_adlibconfig_dialog(0);
		dialog_f1_hack = 0;

	} else if (current_sample < top_sample) {
		top_sample = current_sample;
		if (top_sample < 1)
			top_sample = 1;
	} else if (current_sample > top_sample + 34) {
		top_sample = current_sample - 34;
	}
}

/* --------------------------------------------------------------------- */

int sample_get_current(void)
{
	return current_sample;
}

void sample_set(int n)
{
	int new_sample = n;

	if (status.current_page == PAGE_SAMPLE_LIST)
		new_sample = CLAMP(n, 1, _last_vis_sample());
	else
		new_sample = CLAMP(n, 0, _last_vis_sample());

	if (current_sample == new_sample)
		return;

	current_sample = new_sample;
	sample_list_reposition();

	/* update_current_instrument(); */
	if (status.current_page == PAGE_SAMPLE_LIST)
		status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* draw the actual list */

static void sample_list_draw_list(void)
{
	int pos, n, nl, pn;
	char *name;
	song_sample *sample;
	int has_data, is_selected;
	char buf[64];
	int ss, cl = 0, cr = 0;
	int is_playing[100];

	if (clippy_owner(CLIPPY_SELECT) == widgets_samplelist) {
		cl = widgets_samplelist[0].clip_start % 25;
		cr = widgets_samplelist[0].clip_end % 25;
		if (cl > cr) {
			ss = cl;
			cl = cr;
			cr = ss;
		}
		ss = (widgets_samplelist[0].clip_start / 25);
	} else {
		ss = -1;
	}
	
	song_get_playing_samples(is_playing);

	/* list */
	for (pos = 0, n = top_sample; pos < 35; pos++, n++) {
		sample = song_get_sample(n, &name);
		is_selected = (n == current_sample);
		has_data = (sample->data != NULL);
		
		if (sample->played)
			draw_char(173, 1, 13 + pos, is_playing[n] ? 3 : 1, 2);
		
		draw_text(num99tostr(n, buf), 2, 13 + pos, 0, 2);

		pn = ((unsigned char)name[24]);
		if (((unsigned char)name[23]) == 0xFF && pn < 200) {
			nl = 23;
			draw_text(numtostr(3, (int)pn, buf), 32, 13 + pos, 0, 2);
			draw_char('P', 28, 13+pos, 3, 2);
			draw_char('a', 29, 13+pos, 3, 2);
			draw_char('t', 30, 13+pos, 3, 2);
			draw_char('.', 31, 13+pos, 3, 2);
		} else {
			nl = 25;
			draw_char(168, 30, 13 + pos, 2, (is_selected ? 14 : 0));
			draw_text("Play", 31, 13 + pos, (has_data ? 6 : 7), (is_selected ? 14 : 0));
		}

		draw_text_len(name, nl, 5, 13 + pos, 6, (is_selected ? 14 : 0));
		if (ss == n) {
			draw_text_len(name + cl, (cr-cl)+1, 5 + cl, 13 + pos, 3, 8);
		}
	}

	/* cursor */
	if (ACTIVE_PAGE.selected_widget == 0) {
		pos = current_sample - top_sample;
		sample = song_get_sample(current_sample, &name);
		has_data = (sample->data != NULL);

		if (pos < 0 || pos > 34) {
			/* err... */
		} else if (sample_list_cursor_pos == 25) {
			draw_text("Play", 31, 13 + pos, 0, (has_data ? 3 : 6));
		} else {
			//draw_char(((sample_list_cursor_pos > (signed) strlen(name))
			//	   ? 0 : name[sample_list_cursor_pos]),
			//	  sample_list_cursor_pos + 5, 13 + pos, 0, 3);
			draw_char(name[sample_list_cursor_pos],
				  sample_list_cursor_pos + 5, 13 + pos, 0, 3);
		}
	}

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void sample_list_predraw_hook(void)
{
	char buf[16];
	char *name;
	song_sample *sample;
	int has_data;

	sample = song_get_sample(current_sample, &name);
	has_data = (sample->data != NULL);

	/* set all the values to the current sample */

	/* default volume
	modplug hack here: sample volume has 4x the resolution...
	can't deal with this in song.cc (easily) without changing the actual volume of the sample. */
	widgets_samplelist[1].d.thumbbar.value = sample->volume / 4;

	/* global volume */
	widgets_samplelist[2].d.thumbbar.value = sample->global_volume;

	/* default pan (another modplug hack) */
	widgets_samplelist[3].d.toggle.state = (sample->flags & SAMP_PANNING);
	widgets_samplelist[4].d.thumbbar.value = sample->panning / 4;

	widgets_samplelist[5].d.thumbbar.value = sample->vib_speed;
	widgets_samplelist[6].d.thumbbar.value = sample->vib_depth;
	widgets_samplelist[7].d.textentry.text = sample->filename;
	widgets_samplelist[8].d.numentry.value = song_sample_get_c5speed(current_sample);

	widgets_samplelist[9].d.menutoggle.state =
		(sample->flags & SAMP_LOOP ? (sample->flags & SAMP_LOOP_PINGPONG ? 2 : 1) : 0);
	widgets_samplelist[10].d.numentry.value = sample->loop_start;
	widgets_samplelist[11].d.numentry.value = sample->loop_end;
	widgets_samplelist[12].d.menutoggle.state =
		(sample->flags & SAMP_SUSLOOP ? (sample->flags & SAMP_SUSLOOP_PINGPONG ? 2 : 1) : 0);
	widgets_samplelist[13].d.numentry.value = sample->sustain_start;
	widgets_samplelist[14].d.numentry.value = sample->sustain_end;

	switch (sample->vib_type) {
	case VIB_SINE:
		togglebutton_set(widgets_samplelist, 15, 0);
		break;
	case VIB_RAMP_UP:
	case VIB_RAMP_DOWN:
		togglebutton_set(widgets_samplelist, 16, 0);
		break;
	case VIB_SQUARE:
		togglebutton_set(widgets_samplelist, 17, 0);
		break;
	case VIB_RANDOM:
		togglebutton_set(widgets_samplelist, 18, 0);
		break;
	}

	widgets_samplelist[19].d.thumbbar.value = sample->vib_rate;

	if (sample->flags & SAMP_STEREO) {
		draw_text_len((has_data ? (sample->flags & SAMP_16_BIT ? "16 bit Stereo" : "8 bit Stereo") : "No sample"),
		      13, 64, 22, 2, 0);
	} else {
		draw_text_len((has_data ? (sample->flags & SAMP_16_BIT ? "16 bit" : "8 bit") : "No sample"),
		      13, 64, 22, 2, 0);
	}
	draw_text_len(numtostr(0, sample->length, buf), 13, 64, 23, 2, 0);

	draw_sample_data(&sample_image, sample, current_sample);

	if (need_retrigger > -1) {
		if (last_keyup > -1)
			song_keyup(current_sample, -1, last_keyup,
						KEYDOWN_CHAN_CURRENT, 0);
		song_keyup(current_sample, -1, need_retrigger,
						KEYDOWN_CHAN_CURRENT, 0);
		song_keydown(current_sample, -1, need_retrigger, 64,
						KEYDOWN_CHAN_CURRENT, 0);
		if (!song_is_multichannel_mode()) {
			last_keyup = need_retrigger;
		} else {
			last_keyup = -1;
		}
		song_update_playing_sample(current_sample);
		need_retrigger = -1;
	}
}

/* --------------------------------------------------------------------- */

static int sample_list_add_char(char c)
{
	char *name;

	if (c < 32)
		return 0;
	song_get_sample(current_sample, &name);
	text_add_char(name, c, &sample_list_cursor_pos, _is_magic_sample(current_sample)
							? 22 : 25);
	_fix_accept_text();

	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
	return 1;
}

static void sample_list_delete_char(void)
{
	char *name;

	song_get_sample(current_sample, &name);
	text_delete_char(name, &sample_list_cursor_pos, _is_magic_sample(current_sample)
							? 23 : 25);
	_fix_accept_text();

	status.flags |= SONG_NEEDS_SAVE;
	status.flags |= NEED_UPDATE;
}

static void sample_list_delete_next_char(void)
{
	char *name;

	song_get_sample(current_sample, &name);
	text_delete_next_char(name, &sample_list_cursor_pos, _is_magic_sample(current_sample)
							? 23 : 25);
	_fix_accept_text();

	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

static void clear_sample_text(void)
{
	char *name;

	memset(song_get_sample(current_sample, &name)->filename, 0, 14);
	if (_is_magic_sample(current_sample)) {
		memset(name, 0, 24);
	} else {
		memset(name, 0, 26);
	}
	sample_list_cursor_pos = 0;
	_fix_accept_text();

	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------- */

static struct widget swap_sample_widgets[6];
static char swap_sample_entry[4] = "";


static void do_swap_sample(UNUSED void *data)
{
	int n = atoi(swap_sample_entry);
	
	if (n < 1 || n > _last_vis_sample())
		return;
	song_swap_samples(current_sample, n);
}

static void swap_sample_draw_const(void)
{
	draw_text("Swap sample with:", 32, 25, 0, 2);
	draw_text("Sample", 35, 27, 0, 2);
	draw_box(41, 26, 45, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void swap_sample_dialog(void)
{
	struct dialog *dialog;
	
	swap_sample_entry[0] = 0;
	create_textentry(swap_sample_widgets + 0, 42, 27, 3, 1, 1, 1, NULL, swap_sample_entry, 2);
	create_button(swap_sample_widgets + 1, 36, 30, 6, 0, 0, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(26, 23, 29, 10, swap_sample_widgets, 2, 0, swap_sample_draw_const, NULL);
	dialog->action_yes = do_swap_sample;
}


static void do_exchange_sample(UNUSED void *data)
{
	int n = atoi(swap_sample_entry);
	
	if (n < 1 || n > _last_vis_sample())
		return;
	song_exchange_samples(current_sample, n);
}

static void exchange_sample_draw_const(void)
{
	draw_text("Exchange sample with:", 30, 25, 0, 2);
	draw_text("Sample", 35, 27, 0, 2);
	draw_box(41, 26, 45, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void exchange_sample_dialog(void)
{
	struct dialog *dialog;
	
	swap_sample_entry[0] = 0;
	create_textentry(swap_sample_widgets + 0, 42, 27, 3, 1, 1, 1, NULL, swap_sample_entry, 2);
	create_button(swap_sample_widgets + 1, 36, 30, 6, 0, 0, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(26, 23, 29, 10, swap_sample_widgets, 2, 0,
	                              exchange_sample_draw_const, NULL);
	dialog->action_yes = do_exchange_sample;
}

static void do_copy_sample(UNUSED void *data)
{
	int n = atoi(swap_sample_entry);
	song_sample *src;
	char *name;
	
	if (n < 1 || n > _last_vis_sample())
		return;

	name = 0;
	src = song_get_sample(n, &name);
	song_copy_sample(current_sample, src, name);
}

static void copy_sample_draw_const(void)
{
	draw_text("Copy sample:", 36, 25, 0, 2);
	draw_text("Sample", 35, 27, 0, 2);
	draw_box(41, 26, 45, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void copy_sample_dialog(void)
{
	struct dialog *dialog;
	
	swap_sample_entry[0] = 0;
	create_textentry(swap_sample_widgets + 0, 42, 27, 3, 1, 1, 1, NULL, swap_sample_entry, 2);
	create_button(swap_sample_widgets + 1, 36, 30, 6, 0, 0, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(26, 23, 29, 10, swap_sample_widgets, 2, 0,
	                              copy_sample_draw_const, NULL);
	dialog->action_yes = do_copy_sample;
}

/* --------------------------------------------------------------------- */

static int sample_list_handle_key_on_list(struct key_event * k)
{
	int new_sample = current_sample;
	int new_cursor_pos = sample_list_cursor_pos;
	char *name;

	if (k->mouse == MOUSE_CLICK && k->mouse_button == MOUSE_BUTTON_MIDDLE) {
		if (k->state) status.flags |= CLIPPY_PASTE_SELECTION;
		return 1;
	} else if (!k->state && k->mouse && k->x >= 5 && k->y >= 13 && k->y <= 47 && k->x <= 34) {
		if (k->mouse == MOUSE_SCROLL_UP) {
			top_sample--;
			if (top_sample < 1) top_sample = 1;
			status.flags |= NEED_UPDATE;
			return 1;
		} else if (k->mouse == MOUSE_SCROLL_DOWN) {
			top_sample++;
			if (top_sample > (_last_vis_sample()-34))
				top_sample = (_last_vis_sample()-34);
			status.flags |= NEED_UPDATE;
			return 1;
		} else {
			if (k->state || k->sy == k->y) {
				new_sample = (k->y - 13) + top_sample;
			}
			if (k->x <= 29) { /* and button1 */
				if (k->mouse == MOUSE_DBLCLICK) {
					set_page(PAGE_LOAD_SAMPLE);
					status.flags |= NEED_UPDATE;
					return 1;
				} else if (new_sample == current_sample
				|| sample_list_cursor_pos != 25) {
					new_cursor_pos = k->x - 5;
				}
			} else if (k->state || k->x == k->sx) {
				if (k->mouse == MOUSE_DBLCLICK
				|| (new_sample == current_sample
				&& sample_list_cursor_pos == 25)) {
					need_retrigger = 61;
					status.flags |= NEED_UPDATE;
				}
				new_cursor_pos = 25;
			}
		}
	} else {
		switch (k->sym) {
		case SDLK_LEFT:
			if (k->state) return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_cursor_pos--;
			break;
		case SDLK_RIGHT:
			if (k->state) return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_cursor_pos++;
			break;
		case SDLK_HOME:
			if (k->state) return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_cursor_pos = 0;
			break;
		case SDLK_END:
			if (k->state) return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_cursor_pos = 25;
			break;
		case SDLK_UP:
			if (k->state) return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_sample--;
			break;
		case SDLK_DOWN:
			if (k->state) return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_sample++;
			break;
		case SDLK_PAGEUP:
			if (k->state) return 0;
			if (k->mod & KMOD_CTRL) {
				new_sample = 1;
			} else {
				new_sample -= 16;
			}
			break;
		case SDLK_PAGEDOWN:
			if (k->state) return 0;
			if (k->mod & KMOD_CTRL) {
				new_sample = _last_vis_sample();
			} else {
				new_sample += 16;
			}
			break;
		case SDLK_RETURN:
			if (!k->state) return 0;
			set_page(PAGE_LOAD_SAMPLE);
			break;
		case SDLK_BACKSPACE:
			if (k->state) return 0;
			if ((k->mod & (KMOD_CTRL | KMOD_ALT)) == 0) {
				if (sample_list_cursor_pos < 25) {
					sample_list_delete_char();
				}
				return 1;
			} else if (k->mod & KMOD_CTRL) {
				/* just for compatibility with every weird thing
				 * Impulse Tracker does ^_^ */
				if (sample_list_cursor_pos < 25) {
					sample_list_add_char(127);
				}
				return 1;
			}
			return 0;
		case SDLK_DELETE:
			if (k->state) return 0;
			if ((k->mod & (KMOD_CTRL | KMOD_ALT)) == 0) {
				if (sample_list_cursor_pos < 25) {
					sample_list_delete_next_char();
				}
				return 1;
			}
			return 0;
		case SDLK_ESCAPE:
			if (k->mod & KMOD_SHIFT) {
				if (k->state) return 1;
				new_cursor_pos = 25;
				break;
			}
			return 0;
		default:
			if (k->mod & KMOD_ALT) {
				if (k->sym == SDLK_c) {
					clear_sample_text();
					return 1;
				}
			} else if ((k->mod & KMOD_CTRL) == 0 && sample_list_cursor_pos < 25) {
				if (!k->unicode) return 0;
				if (k->state) return 1;
				return sample_list_add_char(k->unicode);
			}
			return 0;
		}
	}
	
	new_sample = CLAMP(new_sample, 1, _last_vis_sample());
	new_cursor_pos = CLAMP(new_cursor_pos, 0, 25);
	clippy_select(0,0,0);

	if (new_sample != current_sample) {
		sample_set(new_sample);
		sample_list_reposition();
	} else if (new_cursor_pos != sample_list_cursor_pos) {
		sample_list_cursor_pos = new_cursor_pos;
		_fix_accept_text();
	}
	if (k->mouse && k->x != k->sx) {
		song_get_sample(current_sample, &name);
		widgets_samplelist[0].clip_start = (k->sx - 5) + (current_sample*25);
		widgets_samplelist[0].clip_end = (k->x - 5) + (current_sample*25);
		if (widgets_samplelist[0].clip_start < widgets_samplelist[0].clip_end) {
			clippy_select(widgets_samplelist, 
				name + (k->sx - 5),
				(k->x - k->sx));
		} else {
			clippy_select(widgets_samplelist, 
				name + (k->x - 5),
				(k->sx - k->x));
		}
	}

	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------- */
/* alt key dialog callbacks.
 * these don't need to do any actual redrawing, because the screen gets
 * redrawn anyway when the dialog is cleared. */

/* TODO | Deleting a sample doesn't require stopping the song because Modplug cleanly stops any playing copies
 * TODO | of the sample when it destroys it. It'd be nice if the other sample manipulations (quality convert,
 * TODO | loop cut, etc.) did this as well. */

static void do_sign_convert(UNUSED void *data)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	sample_sign_convert(sample);
}

static void do_quality_convert(UNUSED void *data)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	song_stop();
	sample_toggle_quality(sample, 1);
}

static void do_quality_toggle(UNUSED void *data)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	song_stop();
	sample_toggle_quality(sample, 0);
}

static void do_delete_sample(UNUSED void *data)
{
	song_clear_sample(current_sample);
}

static void do_post_loop_cut(UNUSED void *bweh) /* I'm already using 'data'. */
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	unsigned long pos = ((sample->flags & SAMP_SUSLOOP)
			     ? MAX(sample->loop_end, sample->sustain_end)
			     : sample->loop_end);

	if (pos == sample->length)
		return;

	song_stop();
	song_lock_audio();
	if (sample->loop_end > pos) sample->loop_end = pos;
	if (sample->sustain_end > pos) sample->sustain_end = pos;

	sample->length = pos;
	song_unlock_audio();
}

static void do_pre_loop_cut(UNUSED void *bweh)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	signed char *data;
	unsigned long pos = ((sample->flags & SAMP_SUSLOOP)
			     ? MIN(sample->loop_start, sample->sustain_start)
			     : sample->loop_start);
	unsigned long start_byte = pos * ((sample->flags & SAMP_16_BIT) ? 2 : 1)
				* ((sample->flags & SAMP_STEREO) ? 2 : 1);
	unsigned long  bytes = (sample->length - pos) * ((sample->flags & SAMP_16_BIT) ? 2 : 1)
				* ((sample->flags & SAMP_STEREO) ? 2 : 1);
	
	if (pos == 0)
		return;
	
	song_stop();
	
	song_lock_audio();
	data = song_sample_allocate(bytes);
	memcpy(data, sample->data + start_byte, bytes);
	song_sample_free(sample->data);
	sample->data = data;
	sample->length -= pos;
	
	if (sample->loop_start > pos)
		sample->loop_start -= pos;
	else
		sample->loop_start = 0;
	if (sample->sustain_start > pos)
		sample->sustain_start -= pos;
	else
		sample->sustain_start = 0;
	if (sample->loop_end > pos)
		sample->loop_end -= pos;
	else
		sample->loop_end = 0;
	if (sample->sustain_end > pos)
		sample->sustain_end -= pos;
	else
		sample->sustain_end = 0;
	song_unlock_audio();
}

static void do_centralise(UNUSED void *data)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	sample_centralise(sample);
}

/* --------------------------------------------------------------------- */

static struct widget sample_amplify_widgets[3];

static void do_amplify(UNUSED void *data)
{
	sample_amplify(song_get_sample(current_sample, NULL), sample_amplify_widgets[0].d.thumbbar.value);
}

static void sample_amplify_draw_const(void)
{
	draw_text("Sample Amplification %", 29, 27, 0, 2);
	draw_box(12, 29, 64, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void sample_amplify_dialog(void)
{
	struct dialog *dialog;
	int percent = sample_get_amplify_amount(song_get_sample(current_sample, NULL));

	percent = MIN(percent, 400);

	create_thumbbar(sample_amplify_widgets + 0, 13, 30, 51, 0, 1, 1, NULL, 0, 400);
	sample_amplify_widgets[0].d.thumbbar.value = percent;
	create_button(sample_amplify_widgets + 1, 31, 33, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
	create_button(sample_amplify_widgets + 2, 41, 33, 6, 0, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);

	dialog = dialog_create_custom(9, 25, 61, 11, sample_amplify_widgets,
	                              3, 0, sample_amplify_draw_const, NULL);
	dialog->action_yes = do_amplify;
}

/* --------------------------------------------------------------------- */

static struct widget sample_adlibconfig_widgets[28];

/* namespace adlibconfig { */

typedef enum adlibconfig_wtypes { N/*number*/,
					  B/*boolean/toggle*/
					  } adlibconfig_wtypes;
static const struct {
	int x,y; 
	adlibconfig_wtypes type;
	int byteno, firstbit, nbits;
} adlibconfig_widgets[] =
{
	{ 39,3, B, 10,0,1 }, // add. synth
	{ 39,4, N, 10,1,3 }, // mod. feedback
	
	{ 26,7, N, 5,4,4 },  // carrier attack
	{ 26,8, N, 5,0,4 },  // carrier decay
	{ 26,9, N, 7,4,-4 }, // carrier sustain (0=maximum, 15=minimum)
	{ 26,10,N, 7,0,4 },  // carrier release
	{ 26,11,B, 1,5,1 },  // carrier sustain flag
	{ 26,12,N, 3,0,-6 }, // carrier volume (0=maximum, 63=minimum)

	{ 30,7, N, 4,4,4 },  // modulator attack
	{ 30,8, N, 4,0,4 },  // modulator decay
	{ 30,9, N, 6,4,-4 }, // modulator sustain (0=maximum, 15=minimum)
	{ 30,10,N, 6,0,4 },  // modulator release
	{ 30,11,B, 0,5,1 },  // modulator sustain flag
	{ 30,12,N, 2,0,-6 }, // modulator volume (0=maximum, 63=minimum)
	
	{ 58,7, B, 1,4,1 },  // carrier scale envelope flag
	{ 58,8, N, 3,6,2 },  // carrier level scaling (This is actually reversed bits...)
	{ 58,9, N, 1,0,4 },  // carrier frequency multiplier
	{ 58,10,N, 9,0,2 },  // carrier wave select
	{ 58,11,B, 1,6,1 },  // carrier pitch vibrato
	{ 58,12,B, 1,7,1 },  // carrier volume vibrato
	
	{ 62,7, B, 0,4,1 },  // modulator scale envelope flag
	{ 62,8, N, 2,6,2 },  // modulator level scaling (This is actually reversed bits...)
	{ 62,9, N, 0,0,4 },  // modulator frequency multiplier
	{ 62,10,N, 8,0,2 },  // modulator wave select
	{ 62,11,B, 0,6,1 },  // modulator pitch vibrato
	{ 62,12,B, 0,7,1 },  // modulator volume vibrato
};
const int nadlibconfig_widgets = sizeof(adlibconfig_widgets)/sizeof(*adlibconfig_widgets);

/* } end namespace */

static void do_adlibconfig(UNUSED void *data)
{
	//page->help_index = HELP_SAMPLE_LIST;
	
	song_sample *sample = song_get_sample(current_sample, NULL);
	if (!sample->data) {
		sample->data = song_sample_allocate(1);
		sample->length = 1;
	}
	if (!(sample->flags & SAMP_ADLIB)) {
		sample->flags |= SAMP_ADLIB;
		status_text_flash("Created adlib sample");
	}
	sample->flags &= ~(SAMP_LOOP | SAMP_16_BIT | SAMP_LOOP_PINGPONG | SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG | SAMP_STEREO);
	sample->loop_start = 0;
	sample->loop_end   = 0;
}

static void adlibconfig_refresh(void)
{
    song_sample* sample = song_get_sample(current_sample, NULL);

	draw_sample_data(&sample_image, sample, current_sample);

    int a;
    for(a=0; a<nadlibconfig_widgets; ++a)
    {
        unsigned srcvalue = 0;
        unsigned maskvalue = 0xFFFF;

        unsigned nbits_real = adlibconfig_widgets[a].nbits < 0 ? -adlibconfig_widgets[a].nbits : adlibconfig_widgets[a].nbits;
        unsigned maxvalue = (1 << nbits_real)-1;

        switch(adlibconfig_widgets[a].type)
        {
            case B:
                srcvalue = sample_adlibconfig_widgets[a].d.toggle.state;
                break;
            case N:
                srcvalue = sample_adlibconfig_widgets[a].d.numentry.value;
                break;
        }

        if(adlibconfig_widgets[a].nbits < 0)
            srcvalue = maxvalue-srcvalue; // reverse the semantics

        srcvalue  &= maxvalue; srcvalue  <<= adlibconfig_widgets[a].firstbit;
        maskvalue &= maxvalue; maskvalue <<= adlibconfig_widgets[a].firstbit;

        sample->AdlibBytes[adlibconfig_widgets[a].byteno] =
         (sample->AdlibBytes[adlibconfig_widgets[a].byteno] &~ maskvalue) | srcvalue;
    }
}

static void sample_adlibconfig_draw_const(void)
{
    static const struct { int x,y; const char* label; } labels[] =
    { { 19, 1, "Adlib Melodic Instrument Parameters" },
      { 19, 3, "Additive Synthesis:" },
      { 18, 4, "Modulation Feedback:" },
      { 26, 6, "Car Mod" },
      { 19, 7, "Attack" },
      { 20, 8, "Decay" },
      { 18, 9, "Sustain" },
      { 18,10, "Release" },
      { 12,11, "Sustain Sound" },
      { 19,12, "Volume" },

      { 58, 6, "Car Mod" },
      { 43, 7, "Scale Envelope" },
      { 44, 8, "Level Scaling" },
      { 37, 9, "Frequency Multiplier" },
      { 46,10, "Wave Select" },
      { 44,11, "Pitch Vibrato" },
      { 43,12, "Volume Vibrato" }
    };

    unsigned int a;

    draw_fill_chars(25,6+30, 32,13+30, 0);
    draw_box(25,6+30, 28,13+30, BOX_THIN | BOX_INNER | BOX_INSET);
    draw_box(29,6+30, 32,13+30, BOX_THIN | BOX_INNER | BOX_INSET);

    draw_fill_chars(57,6+30, 64,13+30, 0);
    draw_box(57,6+30, 60,13+30, BOX_THIN | BOX_INNER | BOX_INSET);
    draw_box(61,6+30, 64,13+30, BOX_THIN | BOX_INNER | BOX_INSET);

    for(a=0; a<sizeof(labels)/sizeof(*labels); ++a)
    {
        int a1 = a?0:3, a2=2;
        draw_text(labels[a].label,
                  labels[a].x, labels[a].y+30, a1, a2);
    }

}

static int do_adlib_handlekey(struct key_event *kk)
{
	if (kk->sym == SDLK_F1) {
		if (!kk->state) return 1;
		status.current_help_index = HELP_ADLIB_SAMPLE;
		dialog_f1_hack = 1;
		dialog_destroy_all();
		set_page(PAGE_HELP);
		return 1;
	}
	return 0;
}

static void sample_adlibconfig_dialog(UNUSED void *ign)
{
    struct dialog* dialog;
    song_sample* sample = song_get_sample(current_sample, NULL);

    static int cursor_placeholders[sizeof(adlibconfig_widgets)/sizeof(*adlibconfig_widgets)] = {0};
    static const char* const bool_opts[3] = {"n","y", NULL};

	//page->help_index = HELP_ADLIB_SAMPLES;
	// Eh, what page? Where am I supposed to get a reference to page?
	// How do I make this work? -Bisqwit
	
    int a;
    for(a=0; a<nadlibconfig_widgets; ++a)
    {
        unsigned srcvalue = sample->AdlibBytes[adlibconfig_widgets[a].byteno];
        unsigned nbits_real = adlibconfig_widgets[a].nbits < 0 ? -adlibconfig_widgets[a].nbits : adlibconfig_widgets[a].nbits;
        unsigned minvalue = 0, maxvalue = (1 << nbits_real)-1;
        srcvalue >>= adlibconfig_widgets[a].firstbit;
        srcvalue &= maxvalue;
        if(adlibconfig_widgets[a].nbits < 0) srcvalue = maxvalue-srcvalue; // reverse the semantics

        switch(adlibconfig_widgets[a].type)
        {
            case B:
                create_menutoggle(sample_adlibconfig_widgets+a,
                                adlibconfig_widgets[a].x,
                                adlibconfig_widgets[a].y+30,
                                a>0 ? a-1 : 0,
                                a+1<nadlibconfig_widgets ? a+1 : a,
                                a,a,a,
                                adlibconfig_refresh, bool_opts);
                sample_adlibconfig_widgets[a].d.menutoggle.state = srcvalue;
                sample_adlibconfig_widgets[a].d.menutoggle.activation_keys = "ny";
                break;
            case N:
                create_numentry(sample_adlibconfig_widgets+a,
                                adlibconfig_widgets[a].x,
                                adlibconfig_widgets[a].y+30,
                                nbits_real<4 ? 1 : 2,
                                a>0 ? a-1 : 0,
                                a+1<nadlibconfig_widgets ? a+1 : a,
                                a,
                                adlibconfig_refresh,
                                minvalue, maxvalue,
                                &cursor_placeholders[a]);
                sample_adlibconfig_widgets[a].d.numentry.value = srcvalue;
                break;
        }
    }

    dialog = dialog_create_custom(9, 30, 61, 15, sample_adlibconfig_widgets,
                                  nadlibconfig_widgets, 0,
                                  sample_adlibconfig_draw_const, NULL);
    dialog->action_yes = do_adlibconfig;
    dialog->handle_key = do_adlib_handlekey;
}

/* --------------------------------------------------------------------- */

/* filename can be NULL, in which case the sample filename is used (quick save) */
struct sample_save_data {
	char *path;
	/* char *options? */
	int format_id;
};

static void save_sample_free_data(void *ptr)
{
	struct sample_save_data *data = (struct sample_save_data *) ptr;
	if (data->path)
		free(data->path);
	free(data);
}

static void do_save_sample(void *ptr)
{
	struct sample_save_data *data = (struct sample_save_data *) ptr;
	
	if (song_save_sample(current_sample, data->path, data->format_id)) {
		status_text_flash("%s sample saved (sample %d)",
				  sample_save_formats[data->format_id].name, current_sample);
	} else {
		status_text_flash("Error: Sample %d NOT saved! (No Filename?)", current_sample);
	}
	save_sample_free_data(ptr);
}

static void sample_save(const char *filename, int format_id)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	char *ptr, *q;
	struct sample_save_data *data;
	struct stat buf;
	int tmp;

	if (stat(cfg_dir_samples, &buf) == -1) {
		status_text_flash("Sample directory \"%s\" unreachable", filename);
		return;
	}

	data = mem_alloc(sizeof(struct sample_save_data));
	if (!S_ISDIR(buf.st_mode)) {
		/* directory browsing */
		q = strrchr(cfg_dir_samples, DIR_SEPARATOR);
		if (q) {
			tmp = q[1];
			q[1] = '\0';
		}
	} else {
		q = NULL;
	}

	ptr = dmoz_path_concat(cfg_dir_samples, filename ? : sample->filename);
	if (q) q[1] = tmp;
	
	data->path = ptr;
	data->format_id = format_id;

	if (filename && *filename && stat(ptr, &buf) == 0) {
		if (S_ISREG(buf.st_mode)) {
			dialog_create(DIALOG_OK_CANCEL, "Overwrite file?",
				      do_save_sample, save_sample_free_data, 1, data);
			/* callback will free it */
		} else if (S_ISDIR(buf.st_mode)) {
			status_text_flash("%s is a directory", filename);
			save_sample_free_data(data);
		} else {
			status_text_flash("%s is not a regular file", filename);
			save_sample_free_data(data);
		}
	} else {
		do_save_sample(data);
	}
}

/* export sample dialog */

static struct widget export_sample_widgets[6];
static char export_sample_filename[NAME_MAX + 1] = "";
static char export_sample_options[64] = "fnord";
static int export_sample_format = 0;

static void do_export_sample(UNUSED void *data)
{
	sample_save(export_sample_filename, export_sample_format);
}

static void export_sample_list_draw(void)
{
        int n, focused = (*selected_widget == 3);

	draw_fill_chars(53, 24, 56, 31, 0);
	for (n = 0; n < SSMP_SENTINEL; n++) {
		int fg = 6, bg = 0;
		if (focused && n == export_sample_format) {
			fg = 0;
			bg = 3;
		} else if (n == export_sample_format) {
			bg = 14;
		}
		draw_text_len(sample_save_formats[n].ext, 4, 53, 24 + n, fg, bg);
	}
}

static int export_sample_list_handle_key(struct key_event * k)
{
        int new_format = export_sample_format;

	if (k->state) return 0;
        switch (k->sym) {
        case SDLK_UP:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_format--;
                break;
        case SDLK_DOWN:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_format++;
                break;
        case SDLK_PAGEUP:
        case SDLK_HOME:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_format = 0;
                break;
        case SDLK_PAGEDOWN:
        case SDLK_END:
		if (!NO_MODIFIER(k->mod))
			return 0;
                new_format = SSMP_SENTINEL - 1;
                break;
        case SDLK_TAB:
		if (k->mod & KMOD_SHIFT) {
			change_focus_to(0);
			return 1;
		}
		/* fall through */
        case SDLK_LEFT:
        case SDLK_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
                change_focus_to(0); /* should focus 0/1/2 depending on what's closest */
                return 1;
        default:
                return 0;
        }

        new_format = CLAMP(new_format, 0, SSMP_SENTINEL - 1);
        if (new_format != export_sample_format) {
        	/* update the option string */
                export_sample_format = new_format;
                status.flags |= NEED_UPDATE;
        }

        return 1;
}

static void export_sample_draw_const(void)
{
	draw_text("Export Sample", 34, 21, 0, 2);

	draw_text("Filename", 24, 24, 0, 2);
	draw_box(32, 23, 51, 25, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_text("Options", 25, 27, 0, 2);
	draw_box(32, 26, 51, 28, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_box(52, 23, 57, 32, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void configure_options(void)
{
	dialog_create(DIALOG_OK, "This doesn't do anything yet. Stay tuned! :)", NULL, NULL, 0, NULL);
}

static void export_sample_dialog(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	struct dialog *dialog;

	create_textentry(export_sample_widgets + 0, 33, 24, 18, 0, 1, 3, NULL, export_sample_filename, NAME_MAX);
	create_textentry(export_sample_widgets + 1, 33, 27, 18, 0, 2, 3, NULL, export_sample_options, 256);
	create_button(export_sample_widgets + 2, 33, 30, 9, 1, 4, 3, 3, 3, configure_options, "Configure", 1);
	create_other(export_sample_widgets + 3, 0, export_sample_list_handle_key, export_sample_list_draw);
	create_button(export_sample_widgets + 4, 31, 35, 6, 2, 4, 5, 5, 5, dialog_yes_NULL, "OK", 3);
	create_button(export_sample_widgets + 5, 42, 35, 6, 3, 5, 4, 4, 4, dialog_cancel_NULL, "Cancel", 1);
	
	strncpy(export_sample_filename, sample->filename, NAME_MAX);
	export_sample_filename[NAME_MAX] = 0;
	
	dialog = dialog_create_custom(21, 20, 39, 18, export_sample_widgets, 6, 0, export_sample_draw_const, NULL);
	dialog->action_yes = do_export_sample;
}


/* resize sample dialog */
static struct widget resize_sample_widgets[2];
static int resize_sample_cursor;
static void resize_sample_cancel(UNUSED void *data)
{
	dialog_destroy();
}
static void do_resize_sample_aa(UNUSED void *data)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	unsigned int newlen = resize_sample_widgets[0].d.numentry.value;
	sample_resize(sample, newlen, 1);
}
static void do_resize_sample(UNUSED void *data)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	unsigned int newlen = resize_sample_widgets[0].d.numentry.value;
	sample_resize(sample, newlen, 0);
}
static void resize_sample_draw_const(void)
{
	draw_text("Resize Sample", 34, 24, 3, 2);
	draw_text("New Length", 31, 27, 0, 2);
	draw_box(41, 26, 49, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}
static void resize_sample_dialog(int aa)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	struct dialog *dialog;
	
	resize_sample_cursor = 0;
	create_numentry(resize_sample_widgets + 0, 42, 27, 7, 0, 1, 1, NULL, 0, 9999999, &resize_sample_cursor);
	resize_sample_widgets[0].d.numentry.value = sample->length;
	create_button(resize_sample_widgets + 1, 36, 30, 6, 0, 1, 1, 1, 1, (void *) resize_sample_cancel, "Cancel", 1);
	dialog = dialog_create_custom(26, 22, 29, 11, resize_sample_widgets, 2, 0, resize_sample_draw_const, NULL);
	if (aa) {
		dialog->action_yes = do_resize_sample_aa;
	} else {
		dialog->action_yes = do_resize_sample;
	}
}

/* --------------------------------------------------------------------- */

static void sample_list_handle_alt_key(struct key_event * k)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	if (k->state) return;
	switch (k->sym) {
	case SDLK_a:
		if (sample->data != NULL)
			dialog_create(DIALOG_OK_CANCEL, "Convert sample?", do_sign_convert, NULL, 0, NULL);
		return;
	case SDLK_b:
		/* this statement is too complicated :P */
		if (!(sample->data == NULL
		      || (sample->flags & SAMP_SUSLOOP && sample->loop_start == 0 && sample->sustain_start == 0)
		      || (sample->loop_start == 0)))
			dialog_create(DIALOG_OK_CANCEL, "Cut sample?", do_pre_loop_cut, NULL, 1, NULL);
		return;
	case SDLK_d:
		dialog_create(DIALOG_OK_CANCEL, "Delete sample?", do_delete_sample, NULL, 1, NULL);
		return;
	case SDLK_e:
		resize_sample_dialog(1);
		break;
	case SDLK_f:
		resize_sample_dialog(0);
		break;
	case SDLK_g:
		if (sample->data == NULL)
			return;
		sample_reverse(sample);
		break;
	case SDLK_h:
		if (sample->data != NULL)
			dialog_create(DIALOG_YES_NO, "Centralise sample?", do_centralise, NULL, 0, NULL);
		return;
	case SDLK_i:
		if (sample->data == NULL)
			return;
		sample_invert(sample);
		break;
	case SDLK_l:
		if (sample->data != NULL && (sample->loop_end != 0 || sample->sustain_end != 0))
			dialog_create(DIALOG_OK_CANCEL, "Cut sample?", do_post_loop_cut, NULL, 1, NULL);
		return;
	case SDLK_m:
		if (sample->data != NULL && !(sample->flags & SAMP_ADLIB))
			sample_amplify_dialog();
		return;
	case SDLK_n:
		song_toggle_multichannel_mode();
		return;
	case SDLK_q:
		if (sample->data != NULL) {
			if (sample->flags & SAMP_STEREO) {
				do_quality_convert(0);
			} else {
				dialog_create(DIALOG_YES_NO, "Convert sample?",
				      do_quality_convert, do_quality_toggle, 0, NULL);
			}
		}
		return;
	case SDLK_o:
		sample_save(NULL, SSMP_ITS);
		return;
	case SDLK_p:
		copy_sample_dialog();
		return;
	case SDLK_s:
		swap_sample_dialog();
		return;
	case SDLK_t:
		export_sample_dialog();
		return;
	case SDLK_w:
		sample_save(NULL, SSMP_RAW);
		return;
	case SDLK_x:
		exchange_sample_dialog();
		return;
	case SDLK_z:
		if (sample->data == NULL || (sample->flags & SAMP_ADLIB))
			sample_adlibconfig_dialog(NULL);
		else
			dialog_create(DIALOG_OK_CANCEL, "This will replace this sample",
				      sample_adlibconfig_dialog,
				      dialog_cancel, 1, NULL);
		return;
	case SDLK_INSERT:
		song_insert_sample_slot(current_sample);
		status.flags |= NEED_UPDATE;
		return;
	case SDLK_DELETE:
		song_remove_sample_slot(current_sample);
		status.flags |= NEED_UPDATE;
		return;
	default:
		return;
	}

	status.flags |= NEED_UPDATE;
}

static inline unsigned long calc_halftone(unsigned long hz, int rel)
{
	/* You wouldn't believe how long it took for me to figure this out. I had to calculate the
	logarithmic regression of the values that Impulse Tracker produced and figure out what the
	coefficients had to do with the number twelve... I don't imagine I'll forget this formula
	now. :)
	(FIXME: integer math and all that. Not that I exactly care, since this isn't at all
	performance-critical, but in principle it'd be a good idea.) */
	return pow(2, rel / 12.0) * hz + 0.5;
}

static void sample_list_handle_key(struct key_event * k)
{
	int new_sample = current_sample;
	unsigned int newspd;
	song_sample *sample = song_get_sample(current_sample, NULL);

	switch (k->sym) {
	case SDLK_SPACE:
		if (k->state) return;
		if (selected_widget && *selected_widget == 0) {
			need_retrigger = last_note;
			status.flags |= NEED_UPDATE;
		}
		return;
	case SDLK_PLUS:
		if (k->state) return;
		if (k->mod & KMOD_ALT) {
			newspd = sample->speed * 2;
			song_sample_set_c5speed(current_sample, newspd);
		} else if (k->mod & KMOD_CTRL) {
			newspd = calc_halftone(sample->speed, 1);
			song_sample_set_c5speed(current_sample, newspd);
		}
		status.flags |= NEED_UPDATE;
		return;
	case SDLK_MINUS:
		if (k->state) return;
		if (k->mod & KMOD_ALT) {
			newspd = sample->speed / 2;
			song_sample_set_c5speed(current_sample, newspd);
		} else if (k->mod & KMOD_CTRL) {
			newspd = calc_halftone(sample->speed, -1);
			song_sample_set_c5speed(current_sample, newspd);
		}
		status.flags |= NEED_UPDATE;
		return;

	case SDLK_COMMA:
	case SDLK_LESS:
		if (k->state) return;
		song_change_current_play_channel(-1, 0);
		return;
	case SDLK_PERIOD:
	case SDLK_GREATER:
		if (k->state) return;
		song_change_current_play_channel(1, 0);
		return;
	case SDLK_PAGEUP:
		if (k->state) return;
		new_sample--;
		break;
	case SDLK_PAGEDOWN:
		if (k->state) return;
		new_sample++;
		break;
	case SDLK_ESCAPE:
		if (k->mod & KMOD_SHIFT) {
			if (k->state) return;
			sample_list_cursor_pos = 25;
			_fix_accept_text();
			change_focus_to(0);
			status.flags |= NEED_UPDATE;
			return;
		}
		return;
	default:
		if (k->mod & KMOD_ALT) {
			if (k->state) return;
			sample_list_handle_alt_key(k);
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
				n = kbd_get_note(k);
				v = 64;
				if (n <= 0 || n > 120)
					return;
			}
			if (!k->state && !k->is_repeat) {
				last_note = need_retrigger = n;
				status.flags |= NEED_UPDATE;
			}
		}
		return;
	}

	new_sample = CLAMP(new_sample, 1, _last_vis_sample());

	if (new_sample != current_sample) {
		sample_set(new_sample);
		sample_list_reposition();
		status.flags |= NEED_UPDATE;
	}
}

/* --------------------------------------------------------------------- */
/* wheesh */

static void sample_list_draw_const(void)
{
	int n;

	draw_box(4, 12, 35, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(63, 12, 77, 24, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_box(36, 12, 53, 18, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(37, 15, 47, 17, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(36, 19, 53, 25, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(37, 22, 47, 24, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(36, 26, 53, 33, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(37, 29, 47, 32, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(36, 35, 53, 41, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(37, 38, 47, 40, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(36, 42, 53, 48, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(37, 45, 47, 47, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(54, 25, 77, 30, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(54, 31, 77, 41, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(54, 42, 77, 48, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(55, 45, 72, 47, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_fill_chars(41, 30, 46, 30, 0);
	draw_fill_chars(64, 13, 76, 23, 0);

	draw_text("Default Volume", 38, 14, 0, 2);
	draw_text("Global Volume", 38, 21, 0, 2);
	draw_text("Default Pan", 39, 28, 0, 2);
	draw_text("Vibrato Speed", 38, 37, 0, 2);
	draw_text("Vibrato Depth", 38, 44, 0, 2);
	draw_text("Filename", 55, 13, 0, 2);
	draw_text("Speed", 58, 14, 0, 2);
	draw_text("Loop", 59, 15, 0, 2);
	draw_text("LoopBeg", 56, 16, 0, 2);
	draw_text("LoopEnd", 56, 17, 0, 2);
	draw_text("SusLoop", 56, 18, 0, 2);
	draw_text("SusLBeg", 56, 19, 0, 2);
	draw_text("SusLEnd", 56, 20, 0, 2);
	draw_text("Quality", 56, 22, 0, 2);
	draw_text("Length", 57, 23, 0, 2);
	draw_text("Vibrato Waveform", 58, 33, 0, 2);
	draw_text("Vibrato Rate", 60, 44, 0, 2);

	for (n = 0; n < 13; n++)
		draw_char(154, 64 + n, 21, 3, 0);
}

/* --------------------------------------------------------------------- */
/* wow. this got ugly. */

/* callback for the loop menu toggles */
static void update_sample_loop_flags(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	/* these switch statements fall through */
	sample->flags &= ~(SAMP_LOOP | SAMP_LOOP_PINGPONG | SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG);
	switch (widgets_samplelist[9].d.menutoggle.state) {
	case 2: sample->flags |= SAMP_LOOP_PINGPONG;
	case 1: sample->flags |= SAMP_LOOP;
	}

	switch (widgets_samplelist[12].d.menutoggle.state) {
	case 2: sample->flags |= SAMP_SUSLOOP_PINGPONG;
	case 1: sample->flags |= SAMP_SUSLOOP;
	}

	if (sample->flags & SAMP_LOOP) {
		if (sample->loop_start == sample->length)
			sample->loop_start = 0;
		if (sample->loop_end <= sample->loop_start)
			sample->loop_end = sample->length;
	}

	if (sample->flags & SAMP_SUSLOOP) {
		if (sample->sustain_start == sample->length)
			sample->sustain_start = 0;
		if (sample->sustain_end <= sample->sustain_start)
			sample->sustain_end = sample->length;
	}

	/* update any samples currently playing */
	song_update_playing_sample(current_sample);

	status.flags |= NEED_UPDATE;
}

/* callback for the loop numentries */
static void update_sample_loop_points(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	int flags_changed = 0;

	/* 9 = loop toggle, 10 = loop start, 11 = loop end */
	if ((unsigned long) widgets_samplelist[10].d.numentry.value > sample->length - 1)
		widgets_samplelist[10].d.numentry.value = sample->length - 1;
	if (widgets_samplelist[11].d.numentry.value <= widgets_samplelist[10].d.numentry.value) {
		widgets_samplelist[9].d.menutoggle.state = 0;
		flags_changed = 1;
	} else if ((unsigned long) widgets_samplelist[11].d.numentry.value > sample->length) {
		widgets_samplelist[11].d.numentry.value = sample->length;
	}
	if (sample->loop_start != (unsigned long) widgets_samplelist[10].d.numentry.value
	|| sample->loop_end != (unsigned long) widgets_samplelist[11].d.numentry.value) {
		flags_changed = 1;
	}
	sample->loop_start = widgets_samplelist[10].d.numentry.value;
	sample->loop_end = widgets_samplelist[11].d.numentry.value;

	/* 12 = sus toggle, 13 = sus start, 14 = sus end */
	if ((unsigned long) widgets_samplelist[13].d.numentry.value > sample->length - 1)
		widgets_samplelist[13].d.numentry.value = sample->length - 1;
	if (widgets_samplelist[14].d.numentry.value <= widgets_samplelist[13].d.numentry.value) {
		widgets_samplelist[12].d.menutoggle.state = 0;
		flags_changed = 1;
	} else if ((unsigned long) widgets_samplelist[14].d.numentry.value > sample->length) {
		widgets_samplelist[14].d.numentry.value = sample->length;
	}
	if (sample->sustain_start != (unsigned long) widgets_samplelist[13].d.numentry.value
	|| sample->sustain_end != (unsigned long) widgets_samplelist[14].d.numentry.value) {
		flags_changed = 1;
	}
	sample->sustain_start = widgets_samplelist[13].d.numentry.value;
	sample->sustain_end = widgets_samplelist[14].d.numentry.value;

	if (flags_changed) {
		update_sample_loop_flags();
	}

	status.flags |= NEED_UPDATE;
	/* IT retriggers
	   ... wtf, no it doesn't -storlek
	if (!(sample->flags & (SAMP_LOOP|SAMP_SUSLOOP))) {
		need_retrigger = last_note;
		status.flags |= NEED_UPDATE;
	}
	*/
}

/* --------------------------------------------------------------------- */

static void update_values_in_song(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	/* a few more modplug hacks here... */
	sample->volume = widgets_samplelist[1].d.thumbbar.value * 4;
	sample->global_volume = widgets_samplelist[2].d.thumbbar.value;
	if (widgets_samplelist[3].d.toggle.state)
		sample->flags |= SAMP_PANNING;
	else
		sample->flags &= ~SAMP_PANNING;
	sample->vib_speed = widgets_samplelist[5].d.thumbbar.value;
	sample->vib_depth = widgets_samplelist[6].d.thumbbar.value;

	if (widgets_samplelist[15].d.togglebutton.state)
		sample->vib_type = VIB_SINE;
	else if (widgets_samplelist[16].d.togglebutton.state)
		sample->vib_type = VIB_RAMP_DOWN;
	else if (widgets_samplelist[17].d.togglebutton.state)
		sample->vib_type = VIB_SQUARE;
	else
		sample->vib_type = VIB_RANDOM;
	sample->vib_rate = widgets_samplelist[19].d.thumbbar.value;
}

static void update_sample_speed(void)
{
	song_sample_set_c5speed(current_sample,
			widgets_samplelist[8].d.numentry.value);
	need_retrigger = last_note;
	status.flags |= NEED_UPDATE;
}

static void update_panning(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	
	sample->flags |= SAMP_PANNING;
	sample->panning = widgets_samplelist[4].d.thumbbar.value * 4;
	
	widgets_samplelist[3].d.toggle.state = 1;
}

/* --------------------------------------------------------------------- */

int sample_is_used_by_instrument(int samp)
{
        song_instrument *ins;
	int i, j;
	if (samp < 1) return 0;
	for (i = 1; i <= SCHISM_MAX_INSTRUMENTS; i++) {
		ins = song_get_instrument(i,NULL);
		if (!ins) continue;
		for (j = 0; j < 120; j++) {
			if (ins->sample_map[j] == (unsigned int)samp)
				return 1;
		}
	}
	return 0;
}

void sample_synchronize_to_instrument(void)
{
        song_instrument *ins;
	int instnum = instrument_get_current();
        int pos, first;

        ins = song_get_instrument(instnum, NULL);
	first = 0;
	for (pos = 0; pos < 120; pos++) {
		if (first == 0) first = ins->sample_map[pos];
		if (ins->sample_map[pos] == (unsigned int)instnum) {
			sample_set(instnum);
			return;
		}
        }
	if (first > 0) {
		sample_set(first);
	} else {
		sample_set(instnum);
	}
}
void sample_list_load_page(struct page *page)
{
	vgamem_ovl_alloc(&sample_image);

	page->title = "Sample List (F3)";
	page->draw_const = sample_list_draw_const;
	page->predraw_hook = sample_list_predraw_hook;
	page->handle_key = sample_list_handle_key;
	page->set_page = sample_list_reposition;
	page->total_widgets = 20;
	page->widgets = widgets_samplelist;
	page->help_index = HELP_SAMPLE_LIST;

	/* 0 = sample list */
	create_other(widgets_samplelist + 0, 1, sample_list_handle_key_on_list, sample_list_draw_list);
	_fix_accept_text();

	widgets_samplelist[0].x = 5;
	widgets_samplelist[0].y = 13;
	widgets_samplelist[0].width = 30;
	widgets_samplelist[0].height = 35;

	/* 1 -> 6 = middle column */
	create_thumbbar(widgets_samplelist + 1, 38, 16, 9, 1, 2, 7, update_values_in_song, 0, 64);
	create_thumbbar(widgets_samplelist + 2, 38, 23, 9, 1, 3, 7, update_values_in_song, 0, 64);
	create_toggle(widgets_samplelist + 3, 38, 30, 2, 4, 0, 7, 7, update_values_in_song);
	create_thumbbar(widgets_samplelist + 4, 38, 31, 9, 3, 5, 7, update_panning, 0, 64);
	create_thumbbar(widgets_samplelist + 5, 38, 39, 9, 4, 6, 15, update_values_in_song, 0, 64);
	create_thumbbar(widgets_samplelist + 6, 38, 46, 9, 5, 6, 19, update_values_in_song, 0, 32);
	/* 7 -> 14 = top right box */
	create_textentry(widgets_samplelist + 7, 64, 13, 13, 7, 8, 0, NULL, NULL, 12);
	create_numentry(widgets_samplelist + 8, 64, 14, 7, 7, 9, 0,
			update_sample_speed, 0, 9999999, &sample_numentry_cursor_pos);
	create_menutoggle(widgets_samplelist + 9, 64, 15, 8, 10, 1, 0, 0, update_sample_loop_flags, loop_states);
	create_numentry(widgets_samplelist + 10, 64, 16, 7, 9, 11, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	create_numentry(widgets_samplelist + 11, 64, 17, 7, 10, 12, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	create_menutoggle(widgets_samplelist + 12, 64, 18, 11, 13, 1, 0, 0,
			  update_sample_loop_flags, loop_states);
	create_numentry(widgets_samplelist + 13, 64, 19, 7, 12, 14, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	create_numentry(widgets_samplelist + 14, 64, 20, 7, 13, 15, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	/* 15 -> 18 = vibrato waveforms */
	create_togglebutton(widgets_samplelist + 15, 57, 36, 6, 14, 17, 5,
			    16, 16, update_values_in_song, "\xb9\xba", 3, vibrato_waveforms);
	create_togglebutton(widgets_samplelist + 16, 67, 36, 6, 14, 18, 15,
			    0, 0, update_values_in_song, "\xbd\xbe", 3, vibrato_waveforms);
	create_togglebutton(widgets_samplelist + 17, 57, 39, 6, 15, 19, 5,
			    18, 18, update_values_in_song, "\xbb\xbc", 3, vibrato_waveforms);
	create_togglebutton(widgets_samplelist + 18, 67, 39, 6, 16, 19, 17,
			    0, 0, update_values_in_song, "Random", 1, vibrato_waveforms);
	/* 19 = vibrato rate */
	create_thumbbar(widgets_samplelist + 19, 56, 46, 16, 17, 19, 0, update_values_in_song, 0, 255);
}
