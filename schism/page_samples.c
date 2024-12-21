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

#include "config.h"
#include "dialog.h"
#include "dmoz.h"
#include "fmt.h"
#include "it.h"
#include "keyboard.h"
#include "page.h"
#include "sample-edit.h"
#include "song.h"
#include "vgamem.h"
#include "widget.h"
#include "osdefs.h"

/* --------------------------------------------------------------------- */
/* static in my attic */
static struct vgamem_overlay sample_image = {
	55,26,76,29,
	NULL, 0, 0, 0,
};

static int dialog_f1_hack = 0;

static struct widget widgets_samplelist[20];
static const int vibrato_waveforms[] = { 15, 16, 17, 18, -1 };

static int top_sample = 1;
static int current_sample = 1;
static int _altswap_lastvis = 99; // for alt-down sample-swapping

static int sample_list_cursor_pos = 25; /* the "play" text */

static void sample_adlibconfig_dialog(SCHISM_UNUSED void *ign);

/* shared by all the numentry widgets */
static int sample_numentry_cursor_pos = 0;

/* for the loops */
static const char *const loop_states[] = { "Off", "On Forwards", "On Ping Pong", NULL };

/* playback */
static int last_note = NOTE_MIDC;

static int num_save_formats = 0;

/* --------------------------------------------------------------------- */

/* woo */

static int _is_magic_sample(int no)
{
	song_sample_t *sample;
	int pn;

	sample = song_get_sample(no);
	if (sample && ((unsigned char) sample->name[23]) == 0xFF) {
		pn = (sample->name[24]);
		if (pn < 200) return 1;
	}
	return 0;
}

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
	/* 65 is first visible sample on last page */
	for (i = 65; i < MAX_SAMPLES; i++) {
		if (!csf_sample_is_empty(current_song->samples + i)) {
			j = i;
		}
	}
	while ((j + 34) > n) n += 34;
	if (n >= MAX_SAMPLES) n = MAX_SAMPLES - 1;
	return n;
}

/* --------------------------------------------------------------------- */

static void sample_list_reposition(void)
{
	if (current_sample < top_sample) {
		top_sample = current_sample;
		if (top_sample < 1)
			top_sample = 1;
	} else if (current_sample > top_sample + 34) {
		top_sample = current_sample - 34;
	}
	if (dialog_f1_hack
	    && status.current_page == PAGE_SAMPLE_LIST
	    && status.previous_page == PAGE_HELP) {
		sample_adlibconfig_dialog(NULL);
	}
	dialog_f1_hack = 0;
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
	song_sample_t *sample;
	int has_data, is_selected;
	char buf[64];
	int ss, cl = 0, cr = 0;
	int is_playing[MAX_SAMPLES];

	ss = -1;

	song_get_playing_samples(is_playing);

	/* list */
	for (pos = 0, n = top_sample; pos < 35; pos++, n++) {
		sample = song_get_sample(n);
		is_selected = (n == current_sample);
		has_data = (sample->data != NULL);

		if (sample->played)
			draw_char(is_playing[n] > 1 ? 183 : 173, 1, 13 + pos, is_playing[n] ? 3 : 1, 2);

		draw_text(str_from_num99(n, buf), 2, 13 + pos, (sample->flags & CHN_MUTE) ? 1 : 0, 2);

		// wow, this is entirely horrible
		pn = ((unsigned char)sample->name[24]);
		if (((unsigned char)sample->name[23]) == 0xFF && pn < 200) {
			nl = 23;
			draw_text(str_from_num(3, (int)pn, buf), 32, 13 + pos, 0, 2);
			draw_char('P', 28, 13+pos, 3, 2);
			draw_char('a', 29, 13+pos, 3, 2);
			draw_char('t', 30, 13+pos, 3, 2);
			draw_char('.', 31, 13+pos, 3, 2);
		} else {
			nl = 25;
			draw_char(168, 30, 13 + pos, 2, (is_selected ? 14 : 0));
			draw_text("Play", 31, 13 + pos, (has_data ? 6 : 7), (is_selected ? 14 : 0));
		}

		draw_text_len(sample->name, nl, 5, 13 + pos, 6, (is_selected ? 14 : 0));
		if (ss == n) {
			draw_text_len(sample->name + cl, (cr-cl)+1, 5 + cl, 13 + pos, 3, 8);
		}
	}

	/* cursor */
	if (ACTIVE_PAGE.selected_widget == 0) {
		pos = current_sample - top_sample;
		sample = song_get_sample(current_sample);
		has_data = (sample->data != NULL);

		if (pos < 0 || pos > 34) {
			/* err... */
		} else if (sample_list_cursor_pos == 25) {
			draw_text("Play", 31, 13 + pos, 0, (has_data ? 3 : 6));
		} else {
			draw_char(((sample_list_cursor_pos > (signed) strlen(sample->name))
				   ? 0 : sample->name[sample_list_cursor_pos]),
				  sample_list_cursor_pos + 5, 13 + pos, 0, 3);
		}
	}

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void sample_list_predraw_hook(void)
{
	char buf[16];
	song_sample_t *sample;
	int has_data;

	sample = song_get_sample(current_sample);
	has_data = (sample->data != NULL);

	/* set all the values to the current sample */

	/* default volume
	modplug hack here: sample volume has 4x the resolution...
	can't deal with this in song.cc (easily) without changing the actual volume of the sample. */
	widgets_samplelist[1].d.thumbbar.value = sample->volume / 4;

	/* global volume */
	widgets_samplelist[2].d.thumbbar.value = sample->global_volume;
	widgets_samplelist[2].d.thumbbar.text_at_min = (sample->flags & CHN_MUTE) ? "  Muted  " : NULL;

	/* default pan (another modplug hack) */
	widgets_samplelist[3].d.toggle.state = (sample->flags & CHN_PANNING);
	widgets_samplelist[4].d.thumbbar.value = sample->panning / 4;

	widgets_samplelist[5].d.thumbbar.value = sample->vib_speed;
	widgets_samplelist[6].d.thumbbar.value = sample->vib_depth;
	widgets_samplelist[7].d.textentry.text = sample->filename;
	widgets_samplelist[8].d.numentry.value = sample->c5speed;

	widgets_samplelist[9].d.menutoggle.state =
		(sample->flags & CHN_LOOP ? (sample->flags & CHN_PINGPONGLOOP ? 2 : 1) : 0);
	widgets_samplelist[10].d.numentry.value = sample->loop_start;
	widgets_samplelist[11].d.numentry.value = sample->loop_end;
	widgets_samplelist[12].d.menutoggle.state =
		(sample->flags & CHN_SUSTAINLOOP ? (sample->flags & CHN_PINGPONGSUSTAIN ? 2 : 1) : 0);
	widgets_samplelist[13].d.numentry.value = sample->sustain_start;
	widgets_samplelist[14].d.numentry.value = sample->sustain_end;

	switch (sample->vib_type) {
	case VIB_SINE:
		widget_togglebutton_set(widgets_samplelist, 15, 0);
		break;
	case VIB_RAMP_DOWN:
		widget_togglebutton_set(widgets_samplelist, 16, 0);
		break;
	case VIB_SQUARE:
		widget_togglebutton_set(widgets_samplelist, 17, 0);
		break;
	case VIB_RANDOM:
		widget_togglebutton_set(widgets_samplelist, 18, 0);
		break;
	}

	widgets_samplelist[19].d.thumbbar.value = sample->vib_rate;

	if (has_data) {
		sprintf(buf, "%d bit%s",
			(sample->flags & CHN_16BIT) ? 16 : 8,
			(sample->flags & CHN_STEREO) ? " Stereo" : "");
	} else {
		strcpy(buf, "No sample");
	}
	draw_text_len(buf, 13, 64, 22, 2, 0);

	draw_text_len(str_from_num(0, sample->length, buf), 13, 64, 23, 2, 0);

	draw_sample_data(&sample_image, sample);
}

/* --------------------------------------------------------------------- */

static int sample_list_add_char(uint8_t c)
{
	song_sample_t *smp;

	if (c < 32)
		return 0;
	smp = song_get_sample(current_sample);
	text_add_char(smp->name, c, &sample_list_cursor_pos, _is_magic_sample(current_sample) ? 22 : 25);
	_fix_accept_text();

	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
	return 1;
}

static void sample_list_delete_char(void)
{
	song_sample_t *smp = song_get_sample(current_sample);
	text_delete_char(smp->name, &sample_list_cursor_pos, _is_magic_sample(current_sample) ? 23 : 25);
	_fix_accept_text();

	status.flags |= SONG_NEEDS_SAVE;
	status.flags |= NEED_UPDATE;
}

static void sample_list_delete_next_char(void)
{
	song_sample_t *smp = song_get_sample(current_sample);
	text_delete_next_char(smp->name, &sample_list_cursor_pos, _is_magic_sample(current_sample) ? 23 : 25);
	_fix_accept_text();

	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

static void clear_sample_text(void)
{
	song_sample_t *smp = song_get_sample(current_sample);
	memset(smp->filename, 0, 14);
	if (_is_magic_sample(current_sample)) {
		memset(smp->name, 0, 24);
	} else {
		memset(smp->name, 0, 26);
	}
	sample_list_cursor_pos = 0;
	_fix_accept_text();

	status.flags |= NEED_UPDATE;
	status.flags |= SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------- */

static void do_swap_sample(int n)
{
	if (n >= 1 && n <= _last_vis_sample()) {
		song_swap_samples(current_sample, n);
	}
}

static void do_exchange_sample(int n)
{
	if (n >= 1 && n <= _last_vis_sample()) {
		song_exchange_samples(current_sample, n);
	}
}

static void do_copy_sample(int n)
{
	if (n >= 1 && n <= _last_vis_sample()) {
		song_copy_sample(current_sample, song_get_sample(n));
		sample_host_dialog(-1);
	}
	status.flags |= SONG_NEEDS_SAVE;
}

static void do_replace_sample(int n)
{
	if (n >= 1 && n <= _last_vis_sample()) {
		song_replace_sample(current_sample, n);
	}
	status.flags |= SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------- */

static int sample_list_handle_text_input_on_list(const char *text) {
	int success = 0;

	for (; *text; text++)
		if (sample_list_cursor_pos < 25 && sample_list_add_char(*text))
			success = 1;

	return success;
}

static int sample_list_handle_key_on_list(struct key_event * k)
{
	int new_sample = current_sample;
	int new_cursor_pos = sample_list_cursor_pos;

	if (k->mouse == MOUSE_CLICK && k->mouse_button == MOUSE_BUTTON_MIDDLE) {
		if (k->state == KEY_RELEASE)
			status.flags |= CLIPPY_PASTE_SELECTION;
		return 1;
	} else if (k->state == KEY_PRESS && k->mouse != MOUSE_NONE && k->x >= 5 && k->y >= 13 && k->y <= 47 && k->x <= 34) {
		if (k->mouse == MOUSE_SCROLL_UP) {
			top_sample -= MOUSE_SCROLL_LINES;
			if (top_sample < 1) top_sample = 1;
			status.flags |= NEED_UPDATE;
			return 1;
		} else if (k->mouse == MOUSE_SCROLL_DOWN) {
			top_sample += MOUSE_SCROLL_LINES;
			if (top_sample > (_last_vis_sample()-34))
				top_sample = (_last_vis_sample()-34);
			status.flags |= NEED_UPDATE;
			return 1;
		} else {
			new_sample = (k->y - 13) + top_sample;
			new_cursor_pos = k->x - 5;
			if (k->x <= 29) { /* and button1 */
				if (k->mouse == MOUSE_DBLCLICK) {
					/* this doesn't appear to work */
					set_page(PAGE_LOAD_SAMPLE);
					status.flags |= NEED_UPDATE;
					return 1;
				} else {
				}
#if 0 /* buggy and annoying, could be implemented properly but I don't care enough */
			} else if (k->state == KEY_RELEASE || k->x == k->sx) {
				if (k->mouse == MOUSE_DBLCLICK
				|| (new_sample == current_sample
				&& sample_list_cursor_pos == 25)) {
					song_keydown(current_sample, KEYJAZZ_NOINST,
						last_note, 64, KEYJAZZ_CHAN_CURRENT);
				}
				new_cursor_pos = 25;
#endif
			}
		}
	} else {
		switch (k->sym) {
		case SCHISM_KEYSYM_LEFT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_cursor_pos--;
			break;
		case SCHISM_KEYSYM_RIGHT:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_cursor_pos++;
			break;
		case SCHISM_KEYSYM_HOME:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_cursor_pos = 0;
			break;
		case SCHISM_KEYSYM_END:
			if (k->state == KEY_RELEASE)
				return 0;
			if (!NO_MODIFIER(k->mod))
				return 0;
			new_cursor_pos = 25;
			break;
		case SCHISM_KEYSYM_UP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_ALT) {
				if (current_sample > 1) {
					new_sample = current_sample - 1;
					song_swap_samples(current_sample, new_sample);
				}
			} else if (!NO_MODIFIER(k->mod)) {
				return 0;
			} else {
				new_sample--;
			}
			break;
		case SCHISM_KEYSYM_DOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_ALT) {
				// restrict position to the "old" value of _last_vis_sample()
				// (this is entirely for aesthetic reasons)
				if (status.last_keysym != SCHISM_KEYSYM_DOWN && !k->is_repeat)
					_altswap_lastvis = _last_vis_sample();
				if (current_sample < _altswap_lastvis) {
					new_sample = current_sample + 1;
					song_swap_samples(current_sample, new_sample);
				}
			} else if (!NO_MODIFIER(k->mod)) {
				return 0;
			} else {
				new_sample++;
			}
			break;
		case SCHISM_KEYSYM_PAGEUP:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_CTRL) {
				new_sample = 1;
			} else {
				new_sample -= 16;
			}
			break;
		case SCHISM_KEYSYM_PAGEDOWN:
			if (k->state == KEY_RELEASE)
				return 0;
			if (k->mod & SCHISM_KEYMOD_CTRL) {
				new_sample = _last_vis_sample();
			} else {
				new_sample += 16;
			}
			break;
		case SCHISM_KEYSYM_RETURN:
			if (k->state == KEY_PRESS)
				return 0;
			set_page(PAGE_LOAD_SAMPLE);
			break;
		case SCHISM_KEYSYM_BACKSPACE:
			if (k->state == KEY_RELEASE)
				return 0;
			if ((k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT)) == 0) {
				if (sample_list_cursor_pos < 25) {
					sample_list_delete_char();
				}
				return 1;
			} else if (k->mod & SCHISM_KEYMOD_CTRL) {
				/* just for compatibility with every weird thing
				 * Impulse Tracker does ^_^ */
				if (sample_list_cursor_pos < 25) {
					sample_list_add_char(127);
				}
				return 1;
			}
			return 0;
		case SCHISM_KEYSYM_DELETE:
			if (k->state == KEY_RELEASE)
				return 0;
			if ((k->mod & (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT)) == 0) {
				if (sample_list_cursor_pos < 25) {
					sample_list_delete_next_char();
				}
				return 1;
			}
			return 0;
		case SCHISM_KEYSYM_ESCAPE:
			if (k->mod & SCHISM_KEYMOD_SHIFT) {
				if (k->state == KEY_RELEASE)
					return 1;
				new_cursor_pos = 25;
				break;
			}
			return 0;
		default:
			if (k->mod & SCHISM_KEYMOD_ALT) {
				if (k->sym == SCHISM_KEYSYM_c) {
					clear_sample_text();
					return 1;
				}
				return 0;
			} else if ((k->mod & SCHISM_KEYMOD_CTRL) == 0 && sample_list_cursor_pos < 25) {
				if (k->state == KEY_RELEASE)
					return 1;

				if (k->text)
					return sample_list_handle_text_input_on_list(k->text);

				/* ...uhhhhhh */
				return 0;
			}
			return 0;
		}
	}

	new_sample = CLAMP(new_sample, 1, _last_vis_sample());
	new_cursor_pos = CLAMP(new_cursor_pos, 0, 25);

	if (new_sample != current_sample) {
		sample_set(new_sample);
		sample_list_reposition();
	}
	if (new_cursor_pos != sample_list_cursor_pos) {
		sample_list_cursor_pos = new_cursor_pos;
		_fix_accept_text();
	}

	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------- */
/* alt key dialog callbacks.
 * these don't need to do any actual redrawing, because the screen gets
 * redrawn anyway when the dialog is cleared. */

static void do_sign_convert(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);
	sample_sign_convert(sample);
}

static void do_quality_convert(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);
	sample_toggle_quality(sample, 1);
}

static void do_quality_toggle(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);

	if (sample->flags & CHN_STEREO)
		status_text_flash("Can't toggle quality for stereo samples");
	else
		sample_toggle_quality(sample, 0);
}

static void do_delete_sample(SCHISM_UNUSED void *data)
{
	song_clear_sample(current_sample);
	status.flags |= SONG_NEEDS_SAVE;
}

static void do_downmix(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);
	sample_downmix(sample);
}

static void do_post_loop_cut(SCHISM_UNUSED void *bweh) /* I'm already using 'data'. */
{
	song_sample_t *sample = song_get_sample(current_sample);
	unsigned long pos = ((sample->flags & CHN_SUSTAINLOOP)
			     ? MAX(sample->loop_end, sample->sustain_end)
			     : sample->loop_end);

	if (pos == 0 || pos >= sample->length)
		return;

	status.flags |= SONG_NEEDS_SAVE;

	song_lock_audio();
	csf_stop_sample(current_song, sample);
	if (sample->loop_end > pos) sample->loop_end = pos;
	if (sample->sustain_end > pos) sample->sustain_end = pos;

	sample->length = pos;
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

static void do_pre_loop_cut(SCHISM_UNUSED void *bweh)
{
	song_sample_t *sample = song_get_sample(current_sample);
	uint32_t pos = ((sample->flags & CHN_SUSTAINLOOP)
			     ? MIN(sample->loop_start, sample->sustain_start)
			     : sample->loop_start);
	uint32_t start_byte = pos * ((sample->flags & CHN_16BIT) ? 2 : 1)
				* ((sample->flags & CHN_STEREO) ? 2 : 1);
	uint32_t  bytes = (sample->length - pos) * ((sample->flags & CHN_16BIT) ? 2 : 1)
				* ((sample->flags & CHN_STEREO) ? 2 : 1);

	if (pos == 0 || pos > sample->length)
		return;

	status.flags |= SONG_NEEDS_SAVE;

	song_lock_audio();
	csf_stop_sample(current_song, sample);
	memmove(sample->data, sample->data + start_byte, bytes);
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
	csf_adjust_sample_loop(sample);
	song_unlock_audio();
}

static void do_centralise(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);
	sample_centralise(sample);
}

/* --------------------------------------------------------------------- */

static struct widget sample_amplify_widgets[3];

static void do_amplify(SCHISM_UNUSED void *data)
{
	sample_amplify(song_get_sample(current_sample), sample_amplify_widgets[0].d.thumbbar.value);
}

static void sample_amplify_draw_const(void)
{
	draw_text("Sample Amplification %", 29, 27, 0, 2);
	draw_box(12, 29, 64, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void sample_amplify_dialog(void)
{
	struct dialog *dialog;
	int percent = sample_get_amplify_amount(song_get_sample(current_sample));

	percent = MIN(percent, 400);

	widget_create_thumbbar(sample_amplify_widgets + 0, 13, 30, 51, 0, 1, 1, NULL, 0, 400);
	sample_amplify_widgets[0].d.thumbbar.value = percent;
	widget_create_button(sample_amplify_widgets + 1, 31, 33, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
	widget_create_button(sample_amplify_widgets + 2, 41, 33, 6, 0, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);

	dialog = dialog_create_custom(9, 25, 61, 11, sample_amplify_widgets,
				      3, 0, sample_amplify_draw_const, NULL);
	dialog->action_yes = do_amplify;
}

/* --------------------------------------------------------------------- */

static struct widget txtsynth_widgets[3];
static char txtsynth_entry[65536];

static void do_txtsynth(SCHISM_UNUSED void *data)
{
	int len = strlen(txtsynth_entry);
	if (!len)
		return;

	song_sample_t *sample = song_get_sample(current_sample);
	if (sample->data)
		csf_free_sample(sample->data);
	sample->data = csf_allocate_sample(len);
	memcpy(sample->data, txtsynth_entry, len);
	sample->length = len;
	sample->loop_start = 0;
	sample->loop_end = len;
	sample->sustain_start = sample->sustain_end = 0;
	sample->flags |= CHN_LOOP;
	sample->flags &= ~(CHN_PINGPONGLOOP | CHN_SUSTAINLOOP | CHN_PINGPONGSUSTAIN
			   | CHN_16BIT | CHN_STEREO | CHN_ADLIB);
	csf_adjust_sample_loop(sample);
	sample_host_dialog(-1);

	status.flags |= SONG_NEEDS_SAVE;
}

static void txtsynth_draw_const(void)
{
	draw_text("Enter a text string (e.g. ABCDCB for a triangle-wave)", 13, 27, 0, 2);
	draw_box(12, 29, 66, 31, BOX_THIN | BOX_INNER | BOX_INSET);
}

static void txtsynth_dialog(void)
{
	struct dialog *dialog;

	// TODO copy the current sample into the entry?

	txtsynth_entry[0] = 0;
	widget_create_textentry(txtsynth_widgets + 0, 13, 30, 53, 0, 1, 1, NULL, txtsynth_entry, 65535);
	widget_create_button(txtsynth_widgets + 1, 31, 33, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
	widget_create_button(txtsynth_widgets + 2, 41, 33, 6, 0, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);

	dialog = dialog_create_custom(9, 25, 61, 11, txtsynth_widgets, 3, 0, txtsynth_draw_const, NULL);
	dialog->action_yes = do_txtsynth;
}

/* --------------------------------------------------------------------- */

static struct widget sample_adlibconfig_widgets[28];
static int adlib_xpos[] = {26, 30, 58, 62, 39};
static int adlib_cursorpos[] = {0, 0, 0, 0, 0};
static const char *yn_toggle[3] = {"n", "y", NULL};

/* N - number, B - boolean (toggle) */
typedef enum adlibconfig_wtypes {N, B} adlibconfig_wtypes;
static const struct {
	int xref, y;
	adlibconfig_wtypes type;
	int byteno, firstbit, nbits;
} adlibconfig_widgets[] = {
	{4,  3, B,10, 0, 1 }, // add. synth
	{4,  4, N,10, 1, 3 }, // mod. feedback

	{0,  7, N, 5, 4, 4 }, // carrier attack
	{0,  8, N, 5, 0, 4 }, // carrier decay
	{0,  9, N, 7, 4,-4 }, // carrier sustain (0=maximum, 15=minimum)
	{0, 10, N, 7, 0, 4 }, // carrier release
	{0, 11, B, 1, 5, 1 }, // carrier sustain flag
	{0, 12, N, 3, 0,-6 }, // carrier volume (0=maximum, 63=minimum)

	{1,  7, N, 4, 4, 4 }, // modulator attack
	{1,  8, N, 4, 0, 4 }, // modulator decay
	{1,  9, N, 6, 4,-4 }, // modulator sustain (0=maximum, 15=minimum)
	{1, 10, N, 6, 0, 4 }, // modulator release
	{1, 11, B, 0, 5, 1 }, // modulator sustain flag
	{1, 12, N, 2, 0,-6 }, // modulator volume (0=maximum, 63=minimum)

	{2,  7, B, 1, 4, 1 }, // carrier scale envelope flag
	{2,  8, N, 3, 6, 2 }, // carrier level scaling (This is actually reversed bits...)
	{2,  9, N, 1, 0, 4 }, // carrier frequency multiplier
	{2, 10, N, 9, 0, 3 }, // carrier wave select
	{2, 11, B, 1, 6, 1 }, // carrier pitch vibrato
	{2, 12, B, 1, 7, 1 }, // carrier volume vibrato

	{3,  7, B, 0, 4, 1 }, // modulator scale envelope flag
	{3,  8, N, 2, 6, 2 }, // modulator level scaling (This is actually reversed bits...)
	{3,  9, N, 0, 0, 4 }, // modulator frequency multiplier
	{3, 10, N, 8, 0, 3 }, // modulator wave select
	{3, 11, B, 0, 6, 1 }, // modulator pitch vibrato
	{3, 12, B, 0, 7, 1 }, // modulator volume vibrato
};

static void do_adlibconfig(SCHISM_UNUSED void *data)
{
	//page->help_index = HELP_SAMPLE_LIST;

	song_sample_t *sample = song_get_sample(current_sample);
	if (sample->data)
		csf_free_sample(sample->data);
	// dumb hackaround that ought to some day be fixed:
	sample->data = csf_allocate_sample(1);
	sample->length = 1;
	if (!(sample->flags & CHN_ADLIB)) {
		sample->flags |= CHN_ADLIB;
		status_text_flash("Created adlib sample");
	}
	sample->flags &= ~(CHN_16BIT | CHN_STEREO
			| CHN_LOOP | CHN_PINGPONGLOOP | CHN_SUSTAINLOOP | CHN_PINGPONGSUSTAIN);
	sample->loop_start = sample->loop_end = 0;
	sample->sustain_start = sample->sustain_end = 0;
	if (!sample->c5speed) {
		sample->c5speed = 8363;
		sample->volume = 64 * 4;
		sample->global_volume = 64;
	}
	sample_host_dialog(-1);

	status.flags |= SONG_NEEDS_SAVE;
}

static void adlibconfig_refresh(void)
{
	int a;
	song_sample_t *sample = song_get_sample(current_sample);

	draw_sample_data(&sample_image, sample);

	for (a = 0; a < ARRAY_SIZE(adlibconfig_widgets); a++) {
		unsigned int srcvalue = 0;
		unsigned int maskvalue = 0xFFFF;
		unsigned int nbits_real = (adlibconfig_widgets[a].nbits < 0
				? -adlibconfig_widgets[a].nbits
				: adlibconfig_widgets[a].nbits);
		unsigned int maxvalue = (1 << nbits_real) - 1;

		switch (adlibconfig_widgets[a].type) {
		case B: srcvalue = sample_adlibconfig_widgets[a].d.toggle.state; break;
		case N: srcvalue = sample_adlibconfig_widgets[a].d.numentry.value; break;
		}

		if(adlibconfig_widgets[a].nbits < 0)
			srcvalue = maxvalue - srcvalue; // reverse the semantics

		srcvalue  &= maxvalue; srcvalue  <<= adlibconfig_widgets[a].firstbit;
		maskvalue &= maxvalue; maskvalue <<= adlibconfig_widgets[a].firstbit;

		sample->adlib_bytes[adlibconfig_widgets[a].byteno] =
			(sample->adlib_bytes[adlibconfig_widgets[a].byteno] &~ maskvalue) | srcvalue;
	}
}

static void sample_adlibconfig_draw_const(void)
{
	struct {
		int x, y;
		const char *label;
	} labels[] = {
		{19,  1, "Adlib Melodic Instrument Parameters"},
		{19,  3, "Additive Synthesis:"},
		{18,  4, "Modulation Feedback:"},
		{26,  6, "Car Mod"},
		{19,  7, "Attack"},
		{20,  8, "Decay"},
		{18,  9, "Sustain"},
		{18, 10, "Release"},
		{12, 11, "Sustain Sound"},
		{19, 12, "Volume"},
		{58,  6, "Car Mod"},
		{43,  7, "Scale Envelope"},
		{44,  8, "Level Scaling"},
		{37,  9, "Frequency Multiplier"},
		{46, 10, "Wave Select"},
		{44, 11, "Pitch Vibrato"},
		{43, 12, "Volume Vibrato"},
	};

	int a;

	// 39 33
	draw_box(38, 2 + 30, 40, 5 + 30, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_fill_chars(25, 6 + 30, 32,13 + 30, DEFAULT_FG, 0);
	draw_box(25, 6 + 30, 28, 13 + 30, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(29, 6 + 30, 32, 13 + 30, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_fill_chars(57, 6 + 30, 64,13 + 30, DEFAULT_FG, 0);
	draw_box(57, 6 + 30, 60, 13 + 30, BOX_THIN | BOX_INNER | BOX_INSET);
	draw_box(61, 6 + 30, 64, 13 + 30, BOX_THIN | BOX_INNER | BOX_INSET);

	for (a = 0; a < ARRAY_SIZE(labels); a++)
		draw_text(labels[a].label, labels[a].x, labels[a].y + 30, a ? 0 : 3, 2);
}

static int do_adlib_handlekey(struct key_event *kk)
{
	if (kk->sym == SCHISM_KEYSYM_F1) {
		if (kk->state == KEY_PRESS)
			return 1;
		status.current_help_index = HELP_ADLIB_SAMPLE;
		dialog_f1_hack = 1;
		dialog_destroy_all();
		set_page(PAGE_HELP);
		return 1;
	}
	return 0;
}

static void sample_adlibconfig_dialog(SCHISM_UNUSED void *ign)
{
	struct dialog *dialog;
	song_sample_t *sample = song_get_sample(current_sample);

	int a;

	//page->help_index = HELP_ADLIB_SAMPLES;
	// Eh, what page? Where am I supposed to get a reference to page?
	// How do I make this work? -Bisqwit

	for (a = 0; a < ARRAY_SIZE(adlibconfig_widgets); a++) {
		unsigned int srcvalue = sample->adlib_bytes[adlibconfig_widgets[a].byteno];
		unsigned int nbits_real = adlibconfig_widgets[a].nbits < 0
				? -adlibconfig_widgets[a].nbits
				:  adlibconfig_widgets[a].nbits;
		unsigned int minvalue = 0, maxvalue = (1 << nbits_real) - 1;

		srcvalue >>= adlibconfig_widgets[a].firstbit;
		srcvalue &= maxvalue;
		if (adlibconfig_widgets[a].nbits < 0)
			srcvalue = maxvalue - srcvalue; // reverse the semantics

		switch (adlibconfig_widgets[a].type) {
		case B:
			widget_create_menutoggle(sample_adlibconfig_widgets + a,
				adlib_xpos[adlibconfig_widgets[a].xref],
				adlibconfig_widgets[a].y + 30,
				a > 0 ? a - 1 : 0,
				a + 1 < ARRAY_SIZE(adlibconfig_widgets) ? a + 1 : a,
				a, a,
				(a > 1 ? ((a + 4) % (ARRAY_SIZE(adlibconfig_widgets) - 2)) + 2 : 2),
				adlibconfig_refresh, yn_toggle);
			sample_adlibconfig_widgets[a].d.menutoggle.state = srcvalue;
			sample_adlibconfig_widgets[a].d.menutoggle.activation_keys = "ny";
			break;
		case N:
			widget_create_numentry(sample_adlibconfig_widgets + a,
				adlib_xpos[adlibconfig_widgets[a].xref],
				adlibconfig_widgets[a].y + 30,
				nbits_real < 4 ? 1 : 2,
				a > 0 ? a - 1 : 0,
				a + 1 < ARRAY_SIZE(adlibconfig_widgets) ? a + 1 : a,
				(a > 1 ? ((a + 4) % (ARRAY_SIZE(adlibconfig_widgets) - 2)) + 2 : 2),
				adlibconfig_refresh,
				minvalue, maxvalue,
				adlib_cursorpos + adlibconfig_widgets[a].xref);
			sample_adlibconfig_widgets[a].d.numentry.value = srcvalue;
			break;
		}
	}

	dialog = dialog_create_custom(9, 30, 61, 15, sample_adlibconfig_widgets,
				  ARRAY_SIZE(adlibconfig_widgets), 0,
				  sample_adlibconfig_draw_const, NULL);
	dialog->action_yes = do_adlibconfig;
	dialog->handle_key = do_adlib_handlekey;
}


static void sample_adlibpatch_finish(int n)
{
	song_sample_t *sample;

	if (n <= 0 || n > 128)
		return;

	sample = song_get_sample(current_sample);
	adlib_patch_apply((song_sample_t *) sample, n - 1);
	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE; // redraw the sample

	sample_host_dialog(-1);
}

static void sample_adlibpatch_dialog(SCHISM_UNUSED void *ign)
{
	numprompt_create("Enter Patch (1-128)", sample_adlibpatch_finish, 0);
}

/* --------------------------------------------------------------------- */

/* filename can be NULL, in which case the sample filename is used (quick save) */
struct sample_save_data {
	char *path;
	/* char *options? */
	const char *format;
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

	// I guess this function doesn't need to care about the return value,
	// since song_save_sample is handling all the visual feedback...
	song_save_sample(data->path, data->format, song_get_sample(current_sample), current_sample);
	save_sample_free_data(ptr);
}

static void sample_save(const char *filename, const char *format)
{
	song_sample_t *sample = song_get_sample(current_sample);
	char *ptr, *q;
	struct sample_save_data *data;
	struct stat buf;
	int tmp;

	if (os_stat(cfg_dir_samples, &buf) == -1) {
		status_text_flash("Sample directory \"%s\" unreachable", filename);
		return;
	}

	tmp=0;
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

	ptr = dmoz_path_concat(cfg_dir_samples, filename ? filename : sample->filename);
	if (q) q[1] = tmp;

	data->path = ptr;
	data->format = format;

	if (filename && *filename && os_stat(ptr, &buf) == 0) {
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

static struct widget export_sample_widgets[4];
static char export_sample_filename[SCHISM_NAME_MAX + 1] = "";
static int export_sample_format = 0;

static void do_export_sample(SCHISM_UNUSED void *data)
{
	int exp = export_sample_format;
	int i;

	for (i = 0; sample_save_formats[i].label; i++)
		if (sample_save_formats[i].enabled && !sample_save_formats[i].enabled())
			exp++;

	sample_save(export_sample_filename, sample_save_formats[exp].label);
}

static void export_sample_list_draw(void)
{
	int n, focused = (*selected_widget == 3), c;

	draw_fill_chars(53, 24, 56, 31, DEFAULT_FG, 0);
	for (c = 0, n = 0; sample_save_formats[n].label; n++) {
		if (sample_save_formats[n].enabled && !sample_save_formats[n].enabled())
			continue;

		int fg = 6, bg = 0;
		if (focused && c == export_sample_format) {
			fg = 0;
			bg = 3;
		} else if (c == export_sample_format) {
			bg = 14;
		}
		draw_text_len(sample_save_formats[n].label, 4, 53, 24 + c, fg, bg);
		c++;
	}
}

static int export_sample_list_handle_key(struct key_event * k)
{
	int new_format = export_sample_format;

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

	draw_box(52, 23, 57, 32, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void export_sample_dialog(void)
{
	song_sample_t *sample = song_get_sample(current_sample);
	struct dialog *dialog;

	widget_create_textentry(export_sample_widgets + 0, 33, 24, 18, 0, 1, 3, NULL,
			 export_sample_filename, ARRAY_SIZE(export_sample_filename) - 1);
	widget_create_button(export_sample_widgets + 1, 31, 35, 6, 0, 1, 2, 2, 2, dialog_yes_NULL, "OK", 3);
	widget_create_button(export_sample_widgets + 2, 42, 35, 6, 3, 2, 1, 1, 1, dialog_cancel_NULL, "Cancel", 1);
	widget_create_other(export_sample_widgets + 3, 0, export_sample_list_handle_key, NULL, export_sample_list_draw);

	strncpy(export_sample_filename, sample->filename, ARRAY_SIZE(export_sample_filename) - 1);
	export_sample_filename[ARRAY_SIZE(export_sample_filename) - 1] = 0;

	dialog = dialog_create_custom(21, 20, 39, 18, export_sample_widgets, 4, 0,
				      export_sample_draw_const, NULL);
	dialog->action_yes = do_export_sample;
}


/* resize sample dialog */
static struct widget resize_sample_widgets[2];
static int resize_sample_cursor;

static void do_resize_sample_aa(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);
	uint32_t newlen = resize_sample_widgets[0].d.numentry.value;
	sample_resize(sample, newlen, 1);
}

static void do_resize_sample(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);
	uint32_t newlen = resize_sample_widgets[0].d.numentry.value;
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
	song_sample_t *sample = song_get_sample(current_sample);
	struct dialog *dialog;

	resize_sample_cursor = 0;
	widget_create_numentry(resize_sample_widgets + 0, 42, 27, 7, 0, 1, 1, NULL, 0, 9999999, &resize_sample_cursor);
	resize_sample_widgets[0].d.numentry.value = sample->length;
	widget_create_button(resize_sample_widgets + 1, 36, 30, 6, 0, 1, 1, 1, 1,
		dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(26, 22, 29, 11, resize_sample_widgets, 2, 0,
		resize_sample_draw_const, NULL);
	dialog->action_yes = aa ? do_resize_sample_aa : do_resize_sample;
}

/* resample sample dialog, mostly the same as above */
static struct widget resample_sample_widgets[2];
static int resample_sample_cursor;

static void do_resample_sample_aa(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);
	uint32_t newlen = ((double)sample->length * (double)resample_sample_widgets[0].d.numentry.value / (double)sample->c5speed);
	sample_resize(sample, newlen, 1);
}

static void do_resample_sample(SCHISM_UNUSED void *data)
{
	song_sample_t *sample = song_get_sample(current_sample);
	uint32_t newlen = ((double)sample->length * (double)resample_sample_widgets[0].d.numentry.value / (double)sample->c5speed);
	sample_resize(sample, newlen, 0);
}

static void resample_sample_draw_const(void)
{
	draw_text("Resample Sample", 33, 24, 3, 2);
	draw_text("New Sample Rate", 28, 27, 0, 2);
	draw_box(43, 26, 51, 28, BOX_THICK | BOX_INNER | BOX_INSET);
}

static void resample_sample_dialog(int aa)
{
	song_sample_t *sample = song_get_sample(current_sample);
	struct dialog *dialog;

	resample_sample_cursor = 0;
	widget_create_numentry(resample_sample_widgets + 0, 44, 27, 7, 0, 1, 1, NULL, 0, 9999999, &resample_sample_cursor);
	resample_sample_widgets[0].d.numentry.value = sample->c5speed;
	widget_create_button(resample_sample_widgets + 1, 37, 30, 6, 0, 1, 1, 1, 1,
		dialog_cancel_NULL, "Cancel", 1);
	dialog = dialog_create_custom(26, 22, 28, 11, resample_sample_widgets, 2, 0,
		resample_sample_draw_const, NULL);
	dialog->action_yes = aa ? do_resample_sample_aa : do_resample_sample;
}

/* --------------------------------------------------------------------- */

static void sample_set_mute(int n, int mute)
{
	song_sample_t *smp = song_get_sample(n);

	if (mute) {
		if (smp->flags & CHN_MUTE)
			return;
		smp->globalvol_saved = smp->global_volume;
		smp->global_volume = 0;
		smp->flags |= CHN_MUTE;
	} else {
		if (!(smp->flags & CHN_MUTE))
			return;
		smp->global_volume = smp->globalvol_saved;
		smp->flags &= ~CHN_MUTE;
	}
}

static void sample_toggle_mute(int n)
{
	song_sample_t *smp = song_get_sample(n);
	sample_set_mute(n, !(smp->flags & CHN_MUTE));
}

static void sample_toggle_solo(int n)
{
	int i, solo = 0;

	if (song_get_sample(n)->flags & CHN_MUTE) {
		solo = 1;
	} else {
		for (i = 1; i < MAX_SAMPLES; i++) {
			if (i != n && !(song_get_sample(i)->flags & CHN_MUTE)) {
				solo = 1;
				break;
			}
		}
	}
	for (i = 1; i < MAX_SAMPLES; i++)
		sample_set_mute(i, solo && i != n);
}

/* --------------------------------------------------------------------- */

static void sample_list_handle_alt_key(struct key_event * k)
{
	song_sample_t *sample = song_get_sample(current_sample);
	int canmod = (sample->data != NULL && !(sample->flags & CHN_ADLIB));

	if (k->state == KEY_RELEASE)
		return;

	switch (k->sym) {
	case SCHISM_KEYSYM_a:
		if (canmod)
			dialog_create(DIALOG_OK_CANCEL, "Convert sample?", do_sign_convert, NULL, 0, NULL);
		return;
	case SCHISM_KEYSYM_b:
		if (canmod && (sample->loop_start > 0
			       || ((sample->flags & CHN_SUSTAINLOOP) && sample->sustain_start > 0))) {
			dialog_create(DIALOG_OK_CANCEL, "Cut sample?", do_pre_loop_cut, NULL, 1, NULL);
		}
		return;
	case SCHISM_KEYSYM_d:
		if ((k->mod & SCHISM_KEYMOD_SHIFT) && !(status.flags & CLASSIC_MODE)) {
			if (canmod && sample->flags & CHN_STEREO) {
				dialog_create(DIALOG_OK_CANCEL, "Downmix sample to mono?",
					do_downmix, NULL, 0, NULL);
			}
		} else {
			dialog_create(DIALOG_OK_CANCEL, "Delete sample?", do_delete_sample,
				NULL, 1, NULL);
		}
		return;
	case SCHISM_KEYSYM_e:
		if (canmod) {
			if ((k->mod & SCHISM_KEYMOD_SHIFT) && !(status.flags & CLASSIC_MODE))
				resample_sample_dialog(1);
			else
				resize_sample_dialog(1);
		}
		break;
	case SCHISM_KEYSYM_f:
		if (canmod) {
			if ((k->mod & SCHISM_KEYMOD_SHIFT) && !(status.flags & CLASSIC_MODE))
				resample_sample_dialog(0);
			else
				resize_sample_dialog(0);
		}
		break;
	case SCHISM_KEYSYM_g:
		if (canmod)
			sample_reverse(sample);
		break;
	case SCHISM_KEYSYM_h:
		if (canmod)
			dialog_create(DIALOG_YES_NO, "Centralise sample?", do_centralise, NULL, 0, NULL);
		return;
	case SCHISM_KEYSYM_i:
		if (canmod)
			sample_invert(sample);
		break;
	case SCHISM_KEYSYM_l:
		if (canmod && (sample->loop_end > 0
			       || ((sample->flags & CHN_SUSTAINLOOP) && sample->sustain_end > 0))) {
			dialog_create(DIALOG_OK_CANCEL, "Cut sample?", do_post_loop_cut, NULL, 1, NULL);
		}
		return;
	case SCHISM_KEYSYM_m:
		if (canmod)
			sample_amplify_dialog();
		return;
	case SCHISM_KEYSYM_n:
		song_toggle_multichannel_mode();
		return;
	case SCHISM_KEYSYM_o:
		sample_save(NULL, "ITS");
		return;
	case SCHISM_KEYSYM_p:
		smpprompt_create("Copy sample:", "Sample", do_copy_sample);
		return;
	case SCHISM_KEYSYM_q:
		if (canmod) {
			dialog_create(DIALOG_YES_NO, "Convert sample?",
			      do_quality_convert, do_quality_toggle, 0, NULL);
		}
		return;
	case SCHISM_KEYSYM_r:
		smpprompt_create("Replace sample with:", "Sample", do_replace_sample);
		return;
	case SCHISM_KEYSYM_s:
		smpprompt_create("Swap sample with:", "Sample", do_swap_sample);
		return;
	case SCHISM_KEYSYM_t:
		export_sample_dialog();
		return;
	case SCHISM_KEYSYM_w:
		sample_save(NULL, "RAW");
		return;
	case SCHISM_KEYSYM_x:
		smpprompt_create("Exchange sample with:", "Sample", do_exchange_sample);
		return;
	case SCHISM_KEYSYM_y:
		/* hi virt */
		txtsynth_dialog();
		return;
	case SCHISM_KEYSYM_z:
		{ // uguu~
			void (*dlg)(void *) = (k->mod & SCHISM_KEYMOD_SHIFT)
				? sample_adlibpatch_dialog
				: sample_adlibconfig_dialog;
			if (canmod) {
				dialog_create(DIALOG_OK_CANCEL, "This will replace the current sample.",
					      dlg, NULL, 1, NULL);
			} else {
				dlg(NULL);
			}
		}
		return;
	case SCHISM_KEYSYM_INSERT:
		song_insert_sample_slot(current_sample);
		break;
	case SCHISM_KEYSYM_DELETE:
		song_remove_sample_slot(current_sample);
		break;
	case SCHISM_KEYSYM_F9:
		sample_toggle_mute(current_sample);
		break;
	case SCHISM_KEYSYM_F10:
		sample_toggle_solo(current_sample);
		break;
	default:
		return;
	}

	status.flags |= NEED_UPDATE;
}

static void sample_list_handle_key(struct key_event * k)
{
	int new_sample = current_sample;
	song_sample_t *sample = song_get_sample(current_sample);

	switch (k->sym) {
	case SCHISM_KEYSYM_SPACE:
		if (k->state == KEY_RELEASE)
			return;
		if (selected_widget && *selected_widget == 0) {
			status.flags |= NEED_UPDATE;
		}
		return;
	case SCHISM_KEYSYM_EQUALS:
		if (!(k->mod & SCHISM_KEYMOD_SHIFT))
			return;
		// fallthrough
	case SCHISM_KEYSYM_PLUS:
		if (k->state == KEY_RELEASE)
			return;
		if (k->mod & SCHISM_KEYMOD_ALT) {
			sample->c5speed *= 2;
			status.flags |= SONG_NEEDS_SAVE;
		} else if (k->mod & SCHISM_KEYMOD_CTRL) {
			sample->c5speed = calc_halftone(sample->c5speed, 1);
			status.flags |= SONG_NEEDS_SAVE;
		}
		status.flags |= NEED_UPDATE;
		return;
	case SCHISM_KEYSYM_MINUS:
		if (k->state == KEY_RELEASE)
			return;
		if (k->mod & SCHISM_KEYMOD_ALT) {
			sample->c5speed /= 2;
			status.flags |= SONG_NEEDS_SAVE;
		} else if (k->mod & SCHISM_KEYMOD_CTRL) {
			sample->c5speed = calc_halftone(sample->c5speed, -1);
			status.flags |= SONG_NEEDS_SAVE;
		}
		status.flags |= NEED_UPDATE;
		return;

	case SCHISM_KEYSYM_COMMA:
	case SCHISM_KEYSYM_LESS:
		if (k->state == KEY_RELEASE)
			return;
		song_change_current_play_channel(-1, 0);
		return;
	case SCHISM_KEYSYM_PERIOD:
	case SCHISM_KEYSYM_GREATER:
		if (k->state == KEY_RELEASE)
			return;
		song_change_current_play_channel(1, 0);
		return;
	case SCHISM_KEYSYM_PAGEUP:
		if (k->state == KEY_RELEASE)
			return;
		new_sample--;
		break;
	case SCHISM_KEYSYM_PAGEDOWN:
		if (k->state == KEY_RELEASE)
			return;
		new_sample++;
		break;
	case SCHISM_KEYSYM_ESCAPE:
		if (k->mod & SCHISM_KEYMOD_SHIFT) {
			if (k->state == KEY_RELEASE)
				return;
			sample_list_cursor_pos = 25;
			_fix_accept_text();
			widget_change_focus_to(0);
			status.flags |= NEED_UPDATE;
			return;
		}
		return;
	default:
		if (k->mod & SCHISM_KEYMOD_ALT) {
			if (k->state == KEY_RELEASE)
				return;
			sample_list_handle_alt_key(k);
		} else if (!k->is_repeat) {
			int n, v;
			if (k->midi_note > -1) {
				n = k->midi_note;
				if (k->midi_volume > -1) {
					v = k->midi_volume / 2;
				} else {
					v = KEYJAZZ_DEFAULTVOL;
				}
			} else {
				n = (k->sym == SCHISM_KEYSYM_SPACE)
					? last_note
					: kbd_get_note(k);
				if (n <= 0 || n > 120)
					return;
				v = KEYJAZZ_DEFAULTVOL;
			}
			if (k->state == KEY_RELEASE) {
				song_keyup(current_sample, KEYJAZZ_NOINST, n);
			} else {
				song_keydown(current_sample, KEYJAZZ_NOINST, n, v, KEYJAZZ_CHAN_CURRENT);
				last_note = n;
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

	draw_fill_chars(41, 30, 46, 30, DEFAULT_FG, 0);
	draw_fill_chars(64, 13, 76, 23, DEFAULT_FG, 0);

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
	song_sample_t *sample = song_get_sample(current_sample);

	/* these switch statements fall through */
	sample->flags &= ~(CHN_LOOP | CHN_PINGPONGLOOP | CHN_SUSTAINLOOP | CHN_PINGPONGSUSTAIN);
	switch (widgets_samplelist[9].d.menutoggle.state) {
	case 2: sample->flags |= CHN_PINGPONGLOOP;
	case 1: sample->flags |= CHN_LOOP;
	}

	switch (widgets_samplelist[12].d.menutoggle.state) {
	case 2: sample->flags |= CHN_PINGPONGSUSTAIN;
	case 1: sample->flags |= CHN_SUSTAINLOOP;
	}

	if (sample->flags & CHN_LOOP) {
		if (sample->loop_start == sample->length)
			sample->loop_start = 0;
		if (sample->loop_end <= sample->loop_start)
			sample->loop_end = sample->length;
	}

	if (sample->flags & CHN_SUSTAINLOOP) {
		if (sample->sustain_start == sample->length)
			sample->sustain_start = 0;
		if (sample->sustain_end <= sample->sustain_start)
			sample->sustain_end = sample->length;
	}

	csf_adjust_sample_loop(sample);

	/* update any samples currently playing */
	song_update_playing_sample(current_sample);

	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;
}

/* callback for the loop numentries */
static void update_sample_loop_points(void)
{
	song_sample_t *sample = song_get_sample(current_sample);
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

	csf_adjust_sample_loop(sample);

	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------- */

static void update_values_in_song(void)
{
	song_sample_t *sample = song_get_sample(current_sample);

	/* a few more modplug hacks here... */
	sample->volume = widgets_samplelist[1].d.thumbbar.value * 4;
	sample->global_volume = widgets_samplelist[2].d.thumbbar.value;

	if (widgets_samplelist[3].d.toggle.state)
		sample->flags |= CHN_PANNING;
	else
		sample->flags &= ~CHN_PANNING;
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

	status.flags |= SONG_NEEDS_SAVE;
}

static void update_sample_speed(void)
{
	song_sample_t *sample = song_get_sample(current_sample);

	sample->c5speed = widgets_samplelist[8].d.numentry.value;

	status.flags |= NEED_UPDATE | SONG_NEEDS_SAVE;
}

static void update_panning(void)
{
	song_sample_t *sample = song_get_sample(current_sample);

	sample->flags |= CHN_PANNING;
	sample->panning = widgets_samplelist[4].d.thumbbar.value * 4;

	widgets_samplelist[3].d.toggle.state = 1;

	status.flags |= SONG_NEEDS_SAVE;
}

static void update_filename(void)
{
	status.flags |= SONG_NEEDS_SAVE;
}

/* --------------------------------------------------------------------- */

int sample_is_used_by_instrument(int samp)
{
	song_instrument_t *ins;
	int i, j;
	if (samp < 1) return 0;
	for (i = 1; i <= MAX_INSTRUMENTS; i++) {
		ins = song_get_instrument(i);
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
	song_instrument_t *ins;
	int instnum = instrument_get_current();
	int pos, first;

	ins = song_get_instrument(instnum);
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
	widget_create_other(widgets_samplelist + 0, 1, sample_list_handle_key_on_list,
		sample_list_handle_text_input_on_list, sample_list_draw_list);
	_fix_accept_text();

	widgets_samplelist[0].x = 5;
	widgets_samplelist[0].y = 13;
	widgets_samplelist[0].width = 30;
	widgets_samplelist[0].height = 35;

	/* 1 -> 6 = middle column */
	widget_create_thumbbar(widgets_samplelist + 1, 38, 16, 9, 1, 2, 7, update_values_in_song, 0, 64);
	widget_create_thumbbar(widgets_samplelist + 2, 38, 23, 9, 1, 3, 7, update_values_in_song, 0, 64);
	widget_create_toggle(widgets_samplelist + 3, 38, 30, 2, 4, 0, 7, 7, update_values_in_song);
	widget_create_thumbbar(widgets_samplelist + 4, 38, 31, 9, 3, 5, 7, update_panning, 0, 64);
	widget_create_thumbbar(widgets_samplelist + 5, 38, 39, 9, 4, 6, 15, update_values_in_song, 0, 64);
	widget_create_thumbbar(widgets_samplelist + 6, 38, 46, 9, 5, 6, 19, update_values_in_song, 0, 32);
	/* 7 -> 14 = top right box */
	widget_create_textentry(widgets_samplelist + 7, 64, 13, 13, 7, 8, 0, update_filename, NULL, 12);
	widget_create_numentry(widgets_samplelist + 8, 64, 14, 7, 7, 9, 0,
			update_sample_speed, 0, 9999999, &sample_numentry_cursor_pos);
	widget_create_menutoggle(widgets_samplelist + 9, 64, 15, 8, 10, 1, 0, 0,
			  update_sample_loop_flags, loop_states);
	widget_create_numentry(widgets_samplelist + 10, 64, 16, 7, 9, 11, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	widget_create_numentry(widgets_samplelist + 11, 64, 17, 7, 10, 12, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	widget_create_menutoggle(widgets_samplelist + 12, 64, 18, 11, 13, 1, 0, 0,
			  update_sample_loop_flags, loop_states);
	widget_create_numentry(widgets_samplelist + 13, 64, 19, 7, 12, 14, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	widget_create_numentry(widgets_samplelist + 14, 64, 20, 7, 13, 15, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	/* 15 -> 18 = vibrato waveforms */
	widget_create_togglebutton(widgets_samplelist + 15, 57, 36, 6, 14, 17, 5,
			    16, 16, update_values_in_song, "\xb9\xba", 3, vibrato_waveforms);
	widget_create_togglebutton(widgets_samplelist + 16, 67, 36, 6, 14, 18, 15,
			    0, 0, update_values_in_song, "\xbd\xbe", 3, vibrato_waveforms);
	widget_create_togglebutton(widgets_samplelist + 17, 57, 39, 6, 15, 19, 5,
			    18, 18, update_values_in_song, "\xbb\xbc", 3, vibrato_waveforms);
	widget_create_togglebutton(widgets_samplelist + 18, 67, 39, 6, 16, 19, 17,
			    0, 0, update_values_in_song, "Random", 1, vibrato_waveforms);
	/* 19 = vibrato rate */
	widget_create_thumbbar(widgets_samplelist + 19, 56, 46, 16, 17, 19, 0, update_values_in_song, 0, 255);

	/* count how many formats there really are */
	num_save_formats = 0;
	int i;
	for (i = 0; sample_save_formats[i].label; i++)
		if (!sample_save_formats[i].enabled || sample_save_formats[i].enabled())
			num_save_formats++;
}

