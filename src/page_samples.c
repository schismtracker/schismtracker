/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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
#include "sample-edit.h"

#include <SDL.h>
#include <math.h>			/* for pow */

/* --------------------------------------------------------------------- */
/* static in my attic */

static struct item items_samplelist[20];
static int vibrato_waveforms[] = { 15, 16, 17, 18, -1 };

static int top_sample = 1;
static int current_sample = 1;

static int sample_list_cursor_pos = 25;	/* the "play" text */

static const SDL_Rect sample_waveform_area = { 440, 208, 176, 32 };

/* shared by all the numentry items */
static int sample_numentry_cursor_pos = 0;

/* for the loops */
static const char *loop_states[] = { "Off", "On Forwards", "On Ping Pong", NULL };

/* playback */
static int last_note = 61;		/* C-5 */

/* --------------------------------------------------------------------- */

/*
 * 0 = hasn't been played (no dot)
 * 1 = initial attack ('big' dot, color 3)
 * 2 = currently active ('little' dot, color 3)
 * 3 = was playing at some point ('little' dot, color 1)
 */

#if 0
static byte samples_played[99] = 0;

void update_played_samples(void)
{
blah...}
#endif

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
		new_sample = CLAMP(n, 1, 99);
	else
		new_sample = CLAMP(n, 0, 99);

	if (current_sample == new_sample)
		return;

	status.flags = (status.flags & ~INSTRUMENT_CHANGED) | SAMPLE_CHANGED;
	current_sample = new_sample;
	sample_list_reposition();

	/* update_current_instrument(); */
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */
/* draw the actual list */

static void sample_list_draw_list(void)
{
	int pos, n;
	char *name;
	song_sample *sample;
	int has_data, is_selected;
	char buf[64];

	/* list */
	for (pos = 0, n = top_sample; pos < 35; pos++, n++) {
		sample = song_get_sample(n, &name);
		is_selected = (n == current_sample);
		has_data = (sample->data != NULL);

		draw_text(numtostr(2, n, buf), 2, 13 + pos, 0, 2);
		draw_text_len(name, 25, 5, 13 + pos, 6, (is_selected ? 14 : 0));
		draw_char(168, 30, 13 + pos, 2, (is_selected ? 14 : 0));
		draw_text("Play", 31, 13 + pos, (has_data ? 6 : 7), (is_selected ? 14 : 0));
	}

	/* cursor */
	if (ACTIVE_PAGE.selected_item == 0) {
		pos = current_sample - top_sample;
		sample = song_get_sample(current_sample, &name);
		has_data = (sample->data != NULL);

		if (sample_list_cursor_pos == 25) {
			draw_text("Play", 31, 13 + pos, 0, (has_data ? 3 : 6));
		} else {
			draw_char((sample_list_cursor_pos > (signed) strlen(name)
				   ? 0 : name[sample_list_cursor_pos]),
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
	 * modplug hack here: sample volume has 4x the resolution...
	 * can't deal with this in song.cc (easily) without changing the
	 * actual volume of the sample. */
	items_samplelist[1].thumbbar.value = sample->volume / 4;

	/* global volume */
	items_samplelist[2].thumbbar.value = sample->global_volume;

	/* default pan (another modplug hack) */
	items_samplelist[3].toggle.state = (sample->flags & SAMP_PANNING);
	items_samplelist[4].thumbbar.value = sample->panning / 4;

	items_samplelist[5].thumbbar.value = sample->vib_speed;
	items_samplelist[6].thumbbar.value = sample->vib_depth;
	items_samplelist[7].textentry.text = sample->filename;
	items_samplelist[8].numentry.value = sample->speed;

	items_samplelist[9].menutoggle.state =
		(sample->flags & SAMP_LOOP ? (sample->flags & SAMP_LOOP_PINGPONG ? 2 : 1) : 0);
	items_samplelist[10].numentry.value = sample->loop_start;
	items_samplelist[11].numentry.value = sample->loop_end;
	items_samplelist[12].menutoggle.state =
		(sample->flags & SAMP_SUSLOOP ? (sample->flags & SAMP_SUSLOOP_PINGPONG ? 2 : 1) : 0);
	items_samplelist[13].numentry.value = sample->sustain_start;
	items_samplelist[14].numentry.value = sample->sustain_end;

	switch (sample->vib_type) {
	case VIB_SINE:
		togglebutton_set(items_samplelist, 15, 0);
		break;
	case VIB_RAMP_UP:
	case VIB_RAMP_DOWN:
		togglebutton_set(items_samplelist, 16, 0);
		break;
	case VIB_SQUARE:
		togglebutton_set(items_samplelist, 17, 0);
		break;
	case VIB_RANDOM:
		togglebutton_set(items_samplelist, 18, 0);
		break;
	}

	items_samplelist[19].thumbbar.value = ((sample->vib_rate < 64)
					       ? sample->vib_rate * 4 : 255);

	draw_text_len((has_data ? (sample->flags & SAMP_16_BIT ? "16 bits" : "8 bits")
		       : "No sample"), 13, 64, 22, 2, 0);
	draw_text_len(numtostr(0, sample->length, buf), 13, 64, 23, 2, 0);

	{
		SDL_Rect rect = sample_waveform_area;
		draw_sample_data(&rect, sample, current_sample);
	}
}

/* --------------------------------------------------------------------- */

static int sample_list_add_char(char c)
{
	char *name;

	if (c < 32)
		return 0;
	song_get_sample(current_sample, &name);
	text_add_char(name, c, &sample_list_cursor_pos, 25);

	status.flags |= NEED_UPDATE;
	return 1;
}

static void sample_list_delete_char(void)
{
	char *name;

	song_get_sample(current_sample, &name);
	text_delete_char(name, &sample_list_cursor_pos, 25);

	status.flags |= NEED_UPDATE;
}

static void sample_list_delete_next_char(void)
{
	char *name;

	song_get_sample(current_sample, &name);
	text_delete_next_char(name, &sample_list_cursor_pos, 25);

	status.flags |= NEED_UPDATE;
}

static void clear_sample_text(void)
{
	char *name;

	memset(song_get_sample(current_sample, &name)->filename, 0, 14);
	memset(name, 0, 26);
	sample_list_cursor_pos = 0;

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static int sample_list_handle_key_on_list(SDL_keysym * k)
{
	int new_sample = current_sample;
	int new_cursor_pos = sample_list_cursor_pos;

	switch (k->sym) {
	case SDLK_LEFT:
		new_cursor_pos--;
		break;
	case SDLK_RIGHT:
		new_cursor_pos++;
		break;
	case SDLK_HOME:
		new_cursor_pos = 0;
		break;
	case SDLK_END:
		new_cursor_pos = 25;
		break;
	case SDLK_UP:
		new_sample--;
		break;
	case SDLK_DOWN:
		new_sample++;
		break;
	case SDLK_PAGEUP:
		if (k->mod & KMOD_CTRL) {
			new_sample = 1;
		} else {
			new_sample -= 16;
		}
		break;
	case SDLK_PAGEDOWN:
		if (k->mod & KMOD_CTRL) {
			new_sample = 99;
		} else {
			new_sample += 16;
		}
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		set_page(PAGE_LOAD_SAMPLE);
		break;
	case SDLK_BACKSPACE:
		if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0) {
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
		if ((k->mod & (KMOD_CTRL | KMOD_ALT | KMOD_META)) == 0) {
			if (sample_list_cursor_pos < 25) {
				sample_list_delete_next_char();
			}
			return 1;
		}
		return 0;
	default:
		if (k->mod & (KMOD_ALT | KMOD_META)) {
			if (k->sym == SDLK_c) {
				clear_sample_text();
				return 1;
			}
		} else if ((k->mod & KMOD_CTRL) == 0 && sample_list_cursor_pos < 25) {
			int c = unicode_to_ascii(k->unicode);
			if (c == 0)
				return 0;
			return sample_list_add_char(c);
		}
		return 0;
	}

	new_sample = CLAMP(new_sample, 1, 99);
	new_cursor_pos = CLAMP(new_cursor_pos, 0, 25);

	if (new_sample != current_sample) {
		sample_set(new_sample);
		sample_list_reposition();
	} else if (new_cursor_pos != sample_list_cursor_pos) {
		sample_list_cursor_pos = new_cursor_pos;
	} else {
		return 0;
	}

	status.flags |= NEED_UPDATE;
	return 1;
}

/* --------------------------------------------------------------------- */
/* alt key dialog callbacks.
 * these don't need to do any actual redrawing, because the screen gets
 * redrawn anyway when the dialog is cleared. */

static void do_sign_convert(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	sample_sign_convert(sample);
	clear_cached_waveform(current_sample);
}

static void do_quality_convert(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	sample_toggle_quality(sample, 1);
	clear_cached_waveform(current_sample);
}

static void do_quality_toggle(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	sample_toggle_quality(sample, 0);
	clear_cached_waveform(current_sample);
}

static void do_delete_sample(void)
{
	/* song_stop(); ??? */
	song_clear_sample(current_sample);
	clear_cached_waveform(current_sample);
}

static void do_post_loop_cut(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	signed char *data;
	unsigned long pos = MAX(sample->loop_end, sample->sustain_end);
	int bytes = pos * ((sample->flags & SAMP_16_BIT) ? 2 : 1);

	if (pos == sample->length)
		return;

	song_stop();

	data = song_sample_allocate(bytes);
	memcpy(data, sample->data, bytes);
	song_sample_free(sample->data);
	sample->data = data;
	sample->length = pos;

	clear_cached_waveform(current_sample);
}

static void do_pre_loop_cut(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	signed char *data;
	unsigned long pos = ((sample->flags & SAMP_SUSLOOP)
			     ? MIN(sample->loop_start, sample->sustain_start)
			     : sample->loop_start);
	int start_byte = pos * ((sample->flags & SAMP_16_BIT) ? 2 : 1);
	int bytes = (sample->length - pos) * ((sample->flags & SAMP_16_BIT) ? 2 : 1);
	
	if (pos == 0)
		return;
	
	song_stop();
	
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

	clear_cached_waveform(current_sample);
}

static void do_centralise(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	sample_centralise(sample);
	clear_cached_waveform(current_sample);
}

/* --------------------------------------------------------------------- */

static struct item sample_amplify_items[3];

static void do_amplify(void)
{
	sample_amplify(song_get_sample(current_sample, NULL), sample_amplify_items[0].thumbbar.value);
	clear_cached_waveform(current_sample);
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

	create_thumbbar(sample_amplify_items + 0, 13, 30, 51, 0, 1, 1, NULL, 0, 400);
	sample_amplify_items[0].thumbbar.value = percent;
	create_button(sample_amplify_items + 1, 31, 33, 6, 0, 1, 2, 2, 2, dialog_yes, "OK", 3);
	create_button(sample_amplify_items + 2, 41, 33, 6, 0, 2, 1, 1, 1, dialog_cancel, "Cancel", 1);

	dialog = dialog_create_custom(9, 25, 61, 11, sample_amplify_items, 3, 0, sample_amplify_draw_const);
	dialog->action_yes = do_amplify;
}

/* --------------------------------------------------------------------- */

static void sample_save_its(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	char *ptr;
	asprintf(&ptr, "%s/%s", cfg_dir_samples, sample->filename);

	if (song_save_sample_its(current_sample, ptr))
		status_text_flash("Impulse Tracker sample saved (sample %d)", current_sample);
	else
		status_text_flash("Error: Sample %d NOT saved! (No Filename?)", current_sample);

	free(ptr);
}

static void sample_save_s3i(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	char *ptr;
	asprintf(&ptr, "%s/%s", cfg_dir_samples, sample->filename);

	if (song_save_sample_s3i(current_sample, ptr))
		status_text_flash("Scream Tracker sample saved (sample %d)", current_sample);
	else
		status_text_flash("Error: Sample %d NOT saved! (No Filename?)", current_sample);

	free(ptr);
}

static void sample_save_raw(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	char *ptr;
	asprintf(&ptr, "%s/%s", cfg_dir_samples, sample->filename);

	if (song_save_sample_raw(current_sample, ptr))
		status_text_flash("RAW Sample saved (sample %d)", current_sample);
	else
		status_text_flash("Error: Sample %d NOT saved! (No Filename?)", current_sample);

	free(ptr);
}

/* --------------------------------------------------------------------- */

static inline void sample_list_handle_alt_key(SDL_keysym * k)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	switch (k->sym) {
	case SDLK_a:
		if (sample->data != NULL)
			dialog_create(DIALOG_OK_CANCEL, "Convert sample?", do_sign_convert, NULL, 0);
		return;
	case SDLK_b:
		/* this statement is too complicated :P */
		if (!(sample->data == NULL
		      || (sample->flags & SAMP_SUSLOOP && sample->loop_start == 0 && sample->sustain_start == 0)
		      || (sample->loop_start == 0)))
			dialog_create(DIALOG_OK_CANCEL, "Cut sample?", do_pre_loop_cut, NULL, 1);
		return;
	case SDLK_d:
		dialog_create(DIALOG_OK_CANCEL, "Delete sample?", do_delete_sample, NULL, 1);
		return;
	case SDLK_g:
		if (sample->data == NULL)
			return;
		sample_reverse(sample);
		break;
	case SDLK_h:
		if (sample->data != NULL)
			dialog_create(DIALOG_YES_NO, "Centralise sample?", do_centralise, NULL, 0);
		return;
	case SDLK_l:
		if (sample->data != NULL && (sample->loop_end != 0 || sample->sustain_end != 0))
			dialog_create(DIALOG_OK_CANCEL, "Cut sample?", do_post_loop_cut, NULL, 1);
		return;
	case SDLK_m:
		if (sample->data != NULL)
			sample_amplify_dialog();
		return;
	case SDLK_n:
		song_toggle_multichannel_mode();
		return;
	case SDLK_q:
		if (sample->data != NULL)
			dialog_create(DIALOG_YES_NO, "Convert sample?",
				      do_quality_convert, do_quality_toggle, 0);
		return;
	case SDLK_o:
		/* if (file exists) prompt for overwrite; else */
		sample_save_its();
		return;
	case SDLK_t:
		/* if (file exists) prompt for overwrite; else */
		sample_save_s3i();
	case SDLK_w:
		/* if (file exists) prompt for overwrite; else */
		sample_save_raw();
		return;
	default:
		return;
	}

	clear_cached_waveform(current_sample);
	status.flags |= NEED_UPDATE;
}

static inline unsigned long calc_halftone(unsigned long hz, int rel)
{
	/* You wouldn't believe how long it took for me to figure this out.
	 * I had to calculate the logarithmic regression of the values that
	 * Impulse Tracker produced and figure out what the coefficients
	 * had to do with the number twelve... I don't imagine I'll forget
	 * this formula now. :)
	 * (FIXME: integer math and all that. Not that I exactly care, since
	 * this isn't at all performance-critical, but in principle it'd be
	 * a good idea.) */
	return pow(2, rel / 12.0) * hz + 0.5;
}

static void sample_list_handle_key(SDL_keysym * k)
{
	int new_sample = current_sample;
	song_sample *sample = song_get_sample(current_sample, NULL);

	switch (k->sym) {
	case SDLK_KP_PLUS:
		if (k->mod & (KMOD_ALT | KMOD_META)) {
			sample->speed *= 2;
		} else if (k->mod & KMOD_CTRL) {
			sample->speed = calc_halftone(sample->speed, 1);
		}
		status.flags |= NEED_UPDATE;
		return;
	case SDLK_KP_MINUS:
		if (k->mod & (KMOD_ALT | KMOD_META)) {
			sample->speed /= 2;
		} else if (k->mod & KMOD_CTRL) {
			sample->speed = calc_halftone(sample->speed, -1);
		}
		status.flags |= NEED_UPDATE;
		return;

	case SDLK_COMMA:
	case SDLK_LESS:
		song_change_current_play_channel(-1, 0);
		return;
	case SDLK_PERIOD:
	case SDLK_GREATER:
		song_change_current_play_channel(1, 0);
		return;
	case SDLK_PAGEUP:
		new_sample--;
		break;
	case SDLK_PAGEDOWN:
		new_sample++;
		break;
	default:
		if (k->mod & (KMOD_ALT | KMOD_META)) {
			sample_list_handle_alt_key(k);
		} else {
			int n = kbd_get_note(unicode_to_ascii(k->unicode));
			if (n <= 0 || n > 120)
				return;
			song_play_note(current_sample, n, 0, 0);
			last_note = n;
		}
		return;
	}

	new_sample = CLAMP(new_sample, 1, 99);

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

	SDL_LockSurface(screen);

	draw_text_unlocked("Default Volume", 38, 14, 0, 2);
	draw_text_unlocked("Global Volume", 38, 21, 0, 2);
	draw_text_unlocked("Default Pan", 39, 28, 0, 2);
	draw_text_unlocked("Vibrato Speed", 38, 37, 0, 2);
	draw_text_unlocked("Vibrato Depth", 38, 44, 0, 2);
	draw_text_unlocked("Filename", 55, 13, 0, 2);
	draw_text_unlocked("Speed", 58, 14, 0, 2);
	draw_text_unlocked("Loop", 59, 15, 0, 2);
	draw_text_unlocked("LoopBeg", 56, 16, 0, 2);
	draw_text_unlocked("LoopEnd", 56, 17, 0, 2);
	draw_text_unlocked("SusLoop", 56, 18, 0, 2);
	draw_text_unlocked("SusLBeg", 56, 19, 0, 2);
	draw_text_unlocked("SusLEnd", 56, 20, 0, 2);
	draw_text_unlocked("Quality", 56, 22, 0, 2);
	draw_text_unlocked("Length", 57, 23, 0, 2);
	draw_text_unlocked("Vibrato Waveform", 58, 33, 0, 2);
	draw_text_unlocked("Vibrato Rate", 60, 44, 0, 2);

	for (n = 0; n < 13; n++)
		draw_char_unlocked(154, 64 + n, 21, 3, 0);

	SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------- */
/* wow. this got ugly. */

/* callback for the loop menu toggles */
static void update_sample_loop_flags(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	/* these switch statements fall through */
	sample->flags &= ~(SAMP_LOOP | SAMP_LOOP_PINGPONG | SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG);
	switch (items_samplelist[9].menutoggle.state) {
	case 2: sample->flags |= SAMP_LOOP_PINGPONG;
	case 1: sample->flags |= SAMP_LOOP;
	}

	switch (items_samplelist[12].menutoggle.state) {
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

	status.flags |= NEED_UPDATE;
}

/* callback for the loop numentries */
static void update_sample_loop_points(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	int flags_changed = 0;

	/* 9 = loop toggle, 10 = loop start, 11 = loop end */
	if ((unsigned long) items_samplelist[10].numentry.value > sample->length - 1)
		items_samplelist[10].numentry.value = sample->length - 1;
	if (items_samplelist[11].numentry.value <= items_samplelist[10].numentry.value) {
		items_samplelist[9].menutoggle.state = 0;
		flags_changed = 1;
	} else if ((unsigned long) items_samplelist[11].numentry.value > sample->length) {
		items_samplelist[11].numentry.value = sample->length;
	}
	sample->loop_start = items_samplelist[10].numentry.value;
	sample->loop_end = items_samplelist[11].numentry.value;

	/* 12 = sus toggle, 13 = sus start, 14 = sus end */
	if ((unsigned long) items_samplelist[13].numentry.value > sample->length - 1)
		items_samplelist[13].numentry.value = sample->length - 1;
	if (items_samplelist[14].numentry.value <= items_samplelist[13].numentry.value) {
		items_samplelist[12].menutoggle.state = 0;
		flags_changed = 1;
	} else if ((unsigned long) items_samplelist[14].numentry.value > sample->length) {
		items_samplelist[14].numentry.value = sample->length;
	}
	sample->sustain_start = items_samplelist[13].numentry.value;
	sample->sustain_end = items_samplelist[14].numentry.value;

	if (flags_changed)
		update_sample_loop_flags();

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void update_values_in_song(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);

	/* a few more modplug hacks here... */
	sample->volume = items_samplelist[1].thumbbar.value * 4;
	sample->global_volume = items_samplelist[2].thumbbar.value;
	if (items_samplelist[3].toggle.state)
		sample->flags |= SAMP_PANNING;
	else
		sample->flags &= ~SAMP_PANNING;
	sample->vib_speed = items_samplelist[5].thumbbar.value;
	sample->vib_depth = items_samplelist[6].thumbbar.value;

	if (items_samplelist[15].togglebutton.state)
		sample->vib_type = VIB_SINE;
	else if (items_samplelist[16].togglebutton.state)
		sample->vib_type = VIB_RAMP_DOWN;
	else if (items_samplelist[17].togglebutton.state)
		sample->vib_type = VIB_SQUARE;
	else
		sample->vib_type = VIB_RANDOM;
	sample->vib_rate = (items_samplelist[19].thumbbar.value + 3) / 4;
}

static void update_sample_speed(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	
	sample->speed = items_samplelist[8].numentry.value;
	
	song_play_note(current_sample, last_note, 0, 0);
}

static void update_panning(void)
{
	song_sample *sample = song_get_sample(current_sample, NULL);
	
	sample->flags |= SAMP_PANNING;
	sample->panning = items_samplelist[4].thumbbar.value * 4;
	
	items_samplelist[3].toggle.state = 1;
}

/* --------------------------------------------------------------------- */

void sample_list_load_page(struct page *page)
{
	page->title = "Sample List (F3)";
	page->draw_const = sample_list_draw_const;
	page->predraw_hook = sample_list_predraw_hook;
	page->handle_key = sample_list_handle_key;
	page->total_items = 20;
	page->items = items_samplelist;
	page->help_index = HELP_SAMPLE_LIST;

	/* 0 = sample list */
	create_other(items_samplelist + 0, 1, sample_list_handle_key_on_list, sample_list_draw_list);
	/* 1 -> 6 = middle column */
	create_thumbbar(items_samplelist + 1, 38, 16, 9, 1, 2, 7, update_values_in_song, 0, 64);
	create_thumbbar(items_samplelist + 2, 38, 23, 9, 1, 3, 7, update_values_in_song, 0, 64);
	create_toggle(items_samplelist + 3, 38, 30, 2, 4, 0, 7, 7, update_values_in_song);
	create_thumbbar(items_samplelist + 4, 38, 31, 9, 3, 5, 7, update_panning, 0, 64);
	create_thumbbar(items_samplelist + 5, 38, 39, 9, 4, 6, 15, update_values_in_song, 0, 64);
	create_thumbbar(items_samplelist + 6, 38, 46, 9, 5, 6, 19, update_values_in_song, 0, 32);
	/* 7 -> 14 = top right box */
	create_textentry(items_samplelist + 7, 64, 13, 13, 7, 8, 0, NULL, NULL, 12);
	create_numentry(items_samplelist + 8, 64, 14, 7, 7, 9, 0,
			update_sample_speed, 0, 9999999, &sample_numentry_cursor_pos);
	create_menutoggle(items_samplelist + 9, 64, 15, 8, 10, 1, 0, 0, update_sample_loop_flags, loop_states);
	create_numentry(items_samplelist + 10, 64, 16, 7, 9, 11, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	create_numentry(items_samplelist + 11, 64, 17, 7, 10, 12, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	create_menutoggle(items_samplelist + 12, 64, 18, 11, 13, 1, 0, 0,
			  update_sample_loop_flags, loop_states);
	create_numentry(items_samplelist + 13, 64, 19, 7, 12, 14, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	create_numentry(items_samplelist + 14, 64, 20, 7, 13, 15, 0,
			update_sample_loop_points, 0, 9999999, &sample_numentry_cursor_pos);
	/* 15 -> 18 = vibrato waveforms */
	create_togglebutton(items_samplelist + 15, 57, 36, 6, 14, 17, 5,
			    16, 16, update_values_in_song, "\xb9\xba", 3, vibrato_waveforms);
	create_togglebutton(items_samplelist + 16, 67, 36, 6, 14, 18, 15,
			    0, 0, update_values_in_song, "\xbd\xbe", 3, vibrato_waveforms);
	create_togglebutton(items_samplelist + 17, 57, 39, 6, 15, 19, 5,
			    18, 18, update_values_in_song, "\xbb\xbc", 3, vibrato_waveforms);
	create_togglebutton(items_samplelist + 18, 67, 39, 6, 16, 19, 17,
			    0, 0, update_values_in_song, "Random", 1, vibrato_waveforms);
	/* 19 = vibrato rate */
	create_thumbbar(items_samplelist + 19, 56, 46, 16, 17, 19, 0, update_values_in_song, 0, 255);
}
