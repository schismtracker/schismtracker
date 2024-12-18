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
#include "config.h"
#include "charset.h"
#include "song.h"
#include "page.h"
#include "dmoz.h"
#include "sample-edit.h"
#include "keyboard.h"
#include "fakemem.h"
#include "log.h"
#include "widget.h"
#include "dialog.h"
#include "vgamem.h"
#include "osdefs.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

/* --------------------------------------------------------------------------------------------------------- */
/* the locals */
static struct vgamem_overlay sample_image = {
	52,25,76,28,
	NULL, 0, 0, 0,
};

static char current_filename[22];
static int sample_speed_pos = 0;
static int sample_loop_beg = 0;
static int sample_loop_end = 0;
static int sample_susloop_beg = 0;
static int sample_susloop_end = 0;


static int _library_mode = 0;
static struct widget widgets_loadsample[15];
static int fake_slot_changed = 0;
static int will_move_to = -1;
static int fake_slot = KEYJAZZ_NOINST;
static const char *const loop_states[] = {
		"Off", "On Forwards", "On Ping Pong", NULL };

static char samp_cwd[SCHISM_PATH_MAX] = {0};

static void handle_preload(void);

/* --------------------------------------------------------------------------------------------------------- */

/* files:
	file type       color   displayed title                 notes
	---------       -----   ---------------                 -----
	unchecked       4       <the filename>                  IT uses color 6 for these
	directory       5       "........Directory........"     dots are char 154 (same for libraries)
	sample          3       <the sample name>
	libraries       6       ".........Library........."     IT uses color 3. maybe use module name here?
	unknown         2       <the filename>                  any regular file that's not recognized
*/

static int top_file = 0;
static time_t directory_mtime;
static dmoz_filelist_t flist;
#define current_file (flist.selected)

static int search_pos = -1;
static char search_str[SCHISM_PATH_MAX];

/* get a color index from a dmoz_file_t 'type' field */
static inline int get_type_color(int type)
{
	if (type == TYPE_DIRECTORY)
		return 5;
	if (!(type & TYPE_EXT_DATA_MASK))
		return 4; /* unchecked */
	if (type & TYPE_BROWSABLE_MASK)
		return 6; /* library */
	if (type == TYPE_UNKNOWN)
		return 2;
	return 3; /* sample */
}

static void clear_directory(void)
{
	dmoz_free(&flist, NULL);
	fake_slot = KEYJAZZ_NOINST;
	fake_slot_changed = 0;
}

static void file_list_reposition(void)
{
	dmoz_file_t *f;

	current_file = CLAMP(current_file, 0, flist.num_files - 1);
	// XXX use CLAMP() here too, I can't brain
	if (current_file < top_file)
		top_file = current_file;
	else if (current_file > top_file + 34)
		top_file = current_file - 34;
	if (current_file >= 0 && current_file < flist.num_files) {
		f = flist.files[current_file];

		if (f && f->smp_filename) {
			strncpy(current_filename, f->smp_filename, ARRAY_SIZE(current_filename) - 1);
		} else if (f && f->base) {
			CHARSET_EASY_MODE(f->base, CHARSET_CHAR, CHARSET_CP437, {
				strncpy(current_filename, out, ARRAY_SIZE(current_filename) - 1);
			});
		} else {
			current_filename[0] = '\0';
		}
		widgets_loadsample[1].d.textentry.firstchar = 0;
		widgets_loadsample[1].d.textentry.cursor_pos =
				strlen(current_filename);

		widgets_loadsample[2].d.numentry.value = f ? f->smp_speed : 0;

		if (f && f->smp_flags & CHN_PINGPONGLOOP) {
			widgets_loadsample[3].d.menutoggle.state = 2;
		} else if (f && f->smp_flags & CHN_LOOP) {
			widgets_loadsample[3].d.menutoggle.state = 1;
		} else {
			widgets_loadsample[3].d.menutoggle.state = 0;
		}

		widgets_loadsample[4].d.numentry.value = f ? f->smp_loop_start : 0;
		widgets_loadsample[5].d.numentry.value = f ? f->smp_loop_end : 0;

		if (f && f->smp_flags & CHN_PINGPONGSUSTAIN) {
			widgets_loadsample[6].d.menutoggle.state = 2;
		} else if (f && f->smp_flags & CHN_SUSTAINLOOP) {
			widgets_loadsample[6].d.menutoggle.state = 1;
		} else {
			widgets_loadsample[6].d.menutoggle.state = 0;
		}

		widgets_loadsample[7].d.numentry.value = f ? f->smp_sustain_start : 0;
		widgets_loadsample[8].d.numentry.value = f ? f->smp_sustain_end : 0;
		widgets_loadsample[9].d.thumbbar.value = f ? f->smp_defvol : 64;
		widgets_loadsample[10].d.thumbbar.value = f ? f->smp_gblvol : 64;
		widgets_loadsample[11].d.thumbbar.value = f ? f->smp_vibrato_speed : 0;
		widgets_loadsample[12].d.thumbbar.value = f ? f->smp_vibrato_depth : 0;
		widgets_loadsample[13].d.thumbbar.value = f ? f->smp_vibrato_rate : 0;
		if (f) {
			/* autoload some files */
			if (TYPE_SAMPLE_EXTD == (f->type & TYPE_SAMPLE_EXTD)
			    && f->filesize < 0x4000000 && f->smp_length < 0x1000000)
				handle_preload();
		}
	}
}


static void read_directory(void)
{
	struct stat st;

	clear_directory();
	if (os_stat(samp_cwd, &st) < 0)
		directory_mtime = 0;
	else
		directory_mtime = st.st_mtime;
	/* if the stat call failed, this will probably break as well, but
	at the very least, it'll add an entry for the root directory. */
	if (dmoz_read(samp_cwd, &flist, NULL, dmoz_read_sample_library) < 0)
		log_perror(samp_cwd);

	dmoz_filter_filelist(&flist, dmoz_fill_ext_data, &current_file, file_list_reposition);
	dmoz_cache_lookup(samp_cwd, &flist, NULL);
	file_list_reposition();
}

/* return: 1 = success, 0 = failure
TODO: provide some sort of feedback if something went wrong. */
static int change_dir(const char *dir)
{
	char *ptr = dmoz_path_normal(dir);
	struct stat buf;

	if (!ptr)
		return 0;

	dmoz_cache_update(cfg_dir_samples, &flist, NULL);

	if (!os_stat(ptr, &buf) && S_ISDIR(buf.st_mode)) {
		strncpy(cfg_dir_samples, ptr, ARRAY_SIZE(cfg_dir_samples) - 1);
		// paranoia
		cfg_dir_samples[ARRAY_SIZE(cfg_dir_samples) - 1] = '\0';
	}
	strncpy(samp_cwd, ptr, ARRAY_SIZE(samp_cwd) - 1);
	samp_cwd[ARRAY_SIZE(samp_cwd) - 1] = '\0';
	free(ptr);

	read_directory();
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static void load_sample_draw_const(void)
{
	dmoz_file_t *f;
	song_sample_t *s;
	char sbuf[64];

	draw_box(5, 12, 50, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(6, 13, 49, 47, DEFAULT_FG, 0);

	draw_fill_chars(64, 13, 77, 22, DEFAULT_FG, 0);
	draw_box(62, 32, 72, 35, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_box(62, 36, 72, 40, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_box(63, 12, 77, 23, BOX_THICK | BOX_INNER | BOX_INSET);

	draw_box(51, 24, 77, 29, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(52, 25, 76, 28, DEFAULT_FG, 0);

	draw_box(51, 30, 77, 42, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_fill_chars(59, 44, 76, 47, DEFAULT_FG, 0);
	draw_box(58, 43, 77, 48, BOX_THICK | BOX_INNER | BOX_INSET);

	f = NULL;
	if (current_file >= 0 && current_file < flist.num_files && flist.files[current_file]) {
		f = flist.files[current_file];

		sprintf(sbuf, "%07u", f->smp_length);
		draw_text_len(sbuf, 13, 64, 22, 2, 0);

		if (!f->smp_length && !f->smp_filename && !f->smp_flags) {
			draw_text_len("No sample",13, 64, 21, 2, 0);
		} else if (f->smp_flags & CHN_STEREO) {
			draw_text_len(
				(f->smp_flags & CHN_16BIT
				? "16 bit Stereo" : "8 bit Stereo"),
			13, 64, 21, 2, 0);
		} else {
			draw_text_len(
				(f->smp_flags & CHN_16BIT
				? "16 bit" : "8 bit"),
			13, 64, 21, 2, 0);
		}
		if (f->description) {
			draw_text_len(f->description,
					18,
					59, 44, 5, 0);
		} else {
			switch (f->type) {
			case TYPE_DIRECTORY:
				draw_text("Directory",
						59, 44, 5, 0);
				break;
			default:
				draw_text("Unknown format",
						59, 44, 5, 0);
				break;
			};
		}
		sprintf(sbuf, "%07ld", (long)f->filesize);
		draw_text(sbuf, 59, 45, 5,0);
		str_from_date(f->timestamp, sbuf);
		draw_text(sbuf, 59, 46, 5,0);
		str_from_time(f->timestamp, sbuf);
		draw_text(sbuf, 59, 47, 5,0);
	}

	/* these are exactly the same as in page_samples.c, apart from
	 * 'quality' and 'length' being one line higher */
	draw_text("Filename", 55, 13, 0, 2);
	draw_text("Speed", 58, 14, 0, 2);
	draw_text("Loop", 59, 15, 0, 2);
	draw_text("LoopBeg", 56, 16, 0, 2);
	draw_text("LoopEnd", 56, 17, 0, 2);
	draw_text("SusLoop", 56, 18, 0, 2);
	draw_text("SusLBeg", 56, 19, 0, 2);
	draw_text("SusLEnd", 56, 20, 0, 2);
	draw_text("Quality", 56, 21, 0, 2);
	draw_text("Length", 57, 22, 0, 2);

	/* these abbreviations are sucky and lame. any suggestions? */
	draw_text("Def. Vol.", 53, 33, 0, 2);
	draw_text("Glb. Vol.", 53, 34, 0, 2);
	draw_text("Vib.Speed", 53, 37, 0, 2);
	draw_text("Vib.Depth", 53, 38, 0, 2);
	draw_text("Vib. Rate", 53, 39, 0, 2);

	draw_text("Format", 52, 44, 0, 2);
	draw_text("Size", 54, 45, 0, 2);
	draw_text("Date", 54, 46, 0, 2);
	draw_text("Time", 54, 47, 0, 2);

	if (fake_slot != KEYJAZZ_NOINST) {
		s = song_get_sample(fake_slot);
		vgamem_ovl_clear(&sample_image, 0);
		if (s)
			draw_sample_data(&sample_image, s);
		else
			vgamem_ovl_apply(&sample_image);
	}
}

/* --------------------------------------------------------------------------------------------------------- */

static void _common_set_page(void)
{
	struct stat st;

	if (!*samp_cwd) {
		strncpy(samp_cwd, cfg_dir_samples, ARRAY_SIZE(samp_cwd) - 1);
		samp_cwd[ARRAY_SIZE(samp_cwd) - 1] = '\0';
	}

	/* if we have a list, the directory didn't change, and the mtime is the same, we're set */
	if (flist.num_files > 0
	    && (status.flags & DIR_SAMPLES_CHANGED) == 0
	    && os_stat(samp_cwd, &st) == 0
	    && st.st_mtime == directory_mtime) {
		return;
	}

	change_dir(samp_cwd);

	status.flags &= ~DIR_SAMPLES_CHANGED;
	fake_slot = KEYJAZZ_NOINST;
	fake_slot_changed = 0;

	*selected_widget = 0;
	search_pos = -1;
}

static void load_sample_set_page(void)
{
	_library_mode = 0;
	_common_set_page();
}

static void library_sample_set_page(void)
{
	_library_mode = 1;
	_common_set_page();
}

/* --------------------------------------------------------------------------------------------------------- */

static void file_list_draw(void)
{
	int n, pos, fg, bg;
	char buf[8];
	dmoz_file_t *file;

	/* there's no need to have if (files) { ... } like in the load-module page,
	   because there will always be at least "/" in the list */
	if (top_file < 0) top_file = 0;
	if (current_file < 0) current_file = 0;
	for (n = top_file, pos = 13; n < flist.num_files && pos < 48; n++, pos++) {
		file = flist.files[n];

		if (n == current_file && ACTIVE_PAGE.selected_widget == 0) {
			fg = 0;
			bg = 3;
		} else {
			fg = get_type_color(file->type);
			bg = 0;
		}
		draw_text(str_from_num(3, n+1, buf), 2, pos, 0, 2);
		draw_text_len(file->title ? file->title : "", 25, 6, pos, fg, bg);
		draw_char(168, 31, pos, 2, bg);
		draw_text_utf8_len(file->base ? file->base : "", 18, 32, pos, fg, bg);

		/* this is stupid */
		if (file->base && search_pos > -1) {
			if (charset_strncasecmp(file->base, CHARSET_CHAR,
					search_str, CHARSET_CP437, search_pos) == 0) {
				size_t len = charset_strncasecmplen(file->base, CHARSET_CHAR,
					search_str, CHARSET_CP437, search_pos);

				draw_text_utf8_len(file->base, MIN(len, 18), 32, pos, 3, 1);
			}
		}
	}

	/* draw the info for the current file (or directory...) */

	while (pos < 48)
		draw_char(168, 31, pos++, 2, 0);
}

/* --------------------------------------------------------------------------------------------------------- */
/* Nasty mess to load a sample and prompt for stereo convert / create host instrument as necessary. */

static struct widget stereo_cvt_widgets[4];

static void _create_host_ok(void *vpage)
{
	intptr_t page = (intptr_t) vpage;
	song_create_host_instrument(sample_get_current());
	if (page >= 0)
		set_page(page);
}

static void _create_host_cancel(void *vpage)
{
	intptr_t page = (intptr_t) vpage;
	if (page >= 0)
		set_page(page);
}

int sample_host_dialog(int newpage)
{
	/* Actually IT defaults to No when the sample slot already had a sample in it, rather than checking if
	it was assigned to an instrument. Maybe this is better, though?
	(Not to mention, passing around the extra state that'd be required to do it that way would be kind of
	messy...)

	also the double pointer cast sucks.

	also also, IT says Ok/No here instead of Yes/No... but do I care? */

	if (song_is_instrument_mode()) {
		int used = sample_is_used_by_instrument(sample_get_current());
		dialog_create(DIALOG_YES_NO, "Create host instrument?",
			_create_host_ok, _create_host_cancel, used ? 1 : 0, (void *) (intptr_t) newpage);
		return 1;
	}
	if (newpage >= 0)
		set_page(newpage);
	return 0;
}

static void finish_load(int cur);
static void stereo_cvt_complete_left(void)
{
	int cur = sample_get_current();
	song_sample_t *smp;
	smp = song_get_sample(cur);
	sample_mono_left(smp);
	dialog_destroy();
	finish_load(cur);
}

static void stereo_cvt_complete_right(void)
{
	int cur = sample_get_current();
	song_sample_t *smp;
	smp = song_get_sample(cur);
	sample_mono_right(smp);
	dialog_destroy();
	finish_load(cur);
}

static void stereo_cvt_complete_both(void)
{
	memused_songchanged();
	dialog_destroy();
	sample_host_dialog(PAGE_SAMPLE_LIST);
}

static void stereo_cvt_dialog(void)
{
	draw_text("Loading Stereo Sample", 30, 27, 0, 2);
}

static int stereo_cvt_hk(struct key_event *k)
{
	if (!NO_MODIFIER(k->mod))
		return 0;

	/* trap the default dialog keys - we don't want to escape this dialog without running something */
	switch (k->sym) {
	case SCHISM_KEYSYM_RETURN:
		printf("why am I here\n");
	case SCHISM_KEYSYM_ESCAPE: case SCHISM_KEYSYM_o: case SCHISM_KEYSYM_c:
		return 1;
	case SCHISM_KEYSYM_l:
		if (k->state == KEY_RELEASE)
			stereo_cvt_complete_left();
		return 1;
	case SCHISM_KEYSYM_r:
		if (k->state == KEY_RELEASE)
			stereo_cvt_complete_right();
		return 1;
	case SCHISM_KEYSYM_s:
	case SCHISM_KEYSYM_b:
		if (k->state == KEY_RELEASE)
			stereo_cvt_complete_both();
		return 1;
	default:
		return 0;
	}
}

static void finish_load(int cur)
{
	song_sample_t *smp;

	status.flags |= SONG_NEEDS_SAVE;
	memused_songchanged();
	smp = song_get_sample(cur);
	if (smp->flags & CHN_STEREO) {
		struct dialog *dd;
		widget_create_button(stereo_cvt_widgets+0, 27, 30, 6,
				0, 0, 2, 1, 1,
				stereo_cvt_complete_left, "Left", 2);

		widget_create_button(stereo_cvt_widgets+1, 37, 30, 6,
				1, 1, 0, 2, 2,
				stereo_cvt_complete_both, "Both", 2);

		widget_create_button(stereo_cvt_widgets+2, 47, 30, 6,
				2, 2, 1, 0, 0,
				stereo_cvt_complete_right, "Right", 1);

		dd = dialog_create_custom(24, 25, 33, 8,
				stereo_cvt_widgets, 3,
				1,
				stereo_cvt_dialog, NULL);
		dd->handle_key = stereo_cvt_hk;
		return;
	}
	sample_host_dialog(PAGE_SAMPLE_LIST);
}

static void reposition_at_slash_search(void)
{
	dmoz_file_t *f;
	int i, j, b, bl;

	if (search_pos < 0) return;
	bl = b = -1;
	for (i = 0; i < flist.num_files; i++) {
		f = flist.files[i];
		if (!f || !f->base) continue;

		j = charset_strncasecmplen(f->base, CHARSET_CHAR, search_str, CHARSET_CP437, search_pos);
		if (bl < j) {
			bl = j;
			b = i;
		}
	}
	if (bl > 0) {
		current_file = b;
		file_list_reposition();
	}
}

/* on the file list, that is */
static void handle_enter_key(void)
{
	dmoz_file_t *file;
	song_sample_t *smp;
	int cur = sample_get_current();

	if (current_file < 0 || current_file >= flist.num_files) return;

	file = flist.files[current_file];
	dmoz_cache_update(cfg_dir_samples, &flist, NULL);
	dmoz_fill_ext_data(file);

	if ((file->type & (TYPE_BROWSABLE_MASK|TYPE_INST_MASK))
	&& !(file->type & TYPE_SAMPLE_MASK)) {
		change_dir(file->path);
		status.flags |= NEED_UPDATE;
	} else if (_library_mode) {
		return;
	} else if (file->sample) {
		/* it's already been loaded, so copy it */
		smp = song_get_sample(cur);
		song_copy_sample(cur, file->sample);
		strncpy(smp->name, file->title, ARRAY_SIZE(smp->name));
		smp->name[25] = 0;
		CHARSET_EASY_MODE(file->base, CHARSET_CHAR, CHARSET_CP437, {
			strncpy(smp->filename, out, ARRAY_SIZE(smp->filename));
		});
		smp->filename[12] = 0;
		finish_load(cur);
		memused_songchanged();
	} else if (file->type & TYPE_SAMPLE_MASK) {
		/* load the sample */
		song_load_sample(cur, file->path);
		finish_load(cur);
		memused_songchanged();
	}
}

static void do_discard_changes_and_move(SCHISM_UNUSED void *gn)
{
	fake_slot = KEYJAZZ_NOINST;
	fake_slot_changed = 0;
	search_pos = -1;
	current_file = will_move_to;
	file_list_reposition();
	status.flags |= NEED_UPDATE;
}

static void do_delete_file(SCHISM_UNUSED void *data)
{
	int old_top_file, old_current_file;
	char *ptr;

	if (current_file < 0 || current_file >= flist.num_files)
		return;

	ptr = flist.files[current_file]->path;

	/* would be neat to send it to the trash can if there is one */
	unlink(ptr);

	/* remember the list positions */
	old_top_file = top_file;
	old_current_file = current_file;

	read_directory();

	/* put the list positions back */
	top_file = old_top_file;
	current_file = old_current_file;
	/* edge case: if this was the last file, move the cursor up */
	if (current_file >= flist.num_files)
		current_file = flist.num_files - 1;
	file_list_reposition();
}

static int file_list_handle_text_input(const char *text)
{
	dmoz_file_t* f = flist.files[current_file];
	for (; *text; text++) {
		if (*text >= 32 && (search_pos > -1 || (f && (f->type & TYPE_DIRECTORY)))) {
			if (search_pos < 0) search_pos = 0;
			if (search_pos + 1 < ARRAY_SIZE(search_str)) {
				search_str[search_pos++] = *text;
				reposition_at_slash_search();
				status.flags |= NEED_UPDATE;
			}
			return 1;
		}
	}
	return 0;
}

static int file_list_handle_key(struct key_event * k)
{
	int new_file = current_file;

	new_file = CLAMP(new_file, 0, flist.num_files - 1);

	if (!(status.flags & CLASSIC_MODE) && k->sym == SCHISM_KEYSYM_n && (k->mod & SCHISM_KEYMOD_ALT)) {
		if (k->state == KEY_RELEASE)
			song_toggle_multichannel_mode();
		return 1;
	}

	if (k->mouse) {
		if (k->x >= 6 && k->x <= 49 && k->y >= 13 && k->y <= 47) {
			search_pos = -1;
			if (k->mouse == MOUSE_SCROLL_UP) {
				new_file -= MOUSE_SCROLL_LINES;
			} else if (k->mouse == MOUSE_SCROLL_DOWN) {
				new_file += MOUSE_SCROLL_LINES;
			} else {
				new_file = top_file + (k->y - 13);
			}
		}
	}
	switch (k->sym) {
	case SCHISM_KEYSYM_UP:           new_file--; search_pos = -1; break;
	case SCHISM_KEYSYM_DOWN:         new_file++; search_pos = -1; break;
	case SCHISM_KEYSYM_PAGEUP:       new_file -= 35; search_pos = -1; break;
	case SCHISM_KEYSYM_PAGEDOWN:     new_file += 35; search_pos = -1; break;
	case SCHISM_KEYSYM_HOME:         new_file = 0; search_pos = -1; break;
	case SCHISM_KEYSYM_END:          new_file = flist.num_files - 1; search_pos = -1; break;

	case SCHISM_KEYSYM_ESCAPE:
		if (search_pos < 0) {
			if (k->state == KEY_RELEASE && NO_MODIFIER(k->mod))
				set_page(PAGE_SAMPLE_LIST);
			return 1;
		} /* else fall through */
	case SCHISM_KEYSYM_RETURN:
		if (search_pos < 0) {
			if (k->state == KEY_PRESS)
				return 0;
			handle_enter_key();
			search_pos = -1;
		} else {
			if (k->state == KEY_PRESS)
				return 1;
			search_pos = -1;
			status.flags |= NEED_UPDATE;
			return 1;
		}
		return 1;
	case SCHISM_KEYSYM_DELETE:
		if (k->state == KEY_RELEASE)
			return 1;
		search_pos = -1;
		if (flist.num_files > 0)
			dialog_create(DIALOG_OK_CANCEL, "Delete file?", do_delete_file, NULL, 1, NULL);
		return 1;
	case SCHISM_KEYSYM_BACKSPACE:
		if (search_pos > -1) {
			if (k->state == KEY_RELEASE)
				return 1;
			search_pos--;
			status.flags |= NEED_UPDATE;
			reposition_at_slash_search();
			return 1;
		}
	case SCHISM_KEYSYM_SLASH:
		if (search_pos < 0) {
			if (k->state == KEY_PRESS)
				return 0;
			search_pos = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		} /* else fall through */
	default:
		if (k->text)
			file_list_handle_text_input(k->text);

		if (!k->mouse) return 0;
	}

	if (k->mouse == MOUSE_CLICK) {
		if (k->state == KEY_PRESS)
			return 0;
	} else if (k->mouse == MOUSE_DBLCLICK) {
		handle_enter_key();
		return 1;
	} else {
		/* prevent moving the cursor twice from a single key press */
		if (k->state == KEY_RELEASE)
			return 1;
	}

	new_file = CLAMP(new_file, 0, flist.num_files - 1);
	if (new_file != current_file) {
		if (fake_slot != KEYJAZZ_NOINST && fake_slot_changed) {
			will_move_to = new_file;
			dialog_create(DIALOG_YES_NO,
				"Discard Changes?",
				do_discard_changes_and_move,
				NULL,
				0, NULL);
			return 1;
			/* support saving? XXX */
			/*"Save Sample?" OK Cancel*/
			/*"Discard Changes?" OK Cancel*/
		}
		fake_slot = KEYJAZZ_NOINST;
		fake_slot_changed = 0;
		search_pos = -1;
		current_file = new_file;
		file_list_reposition();
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

static void load_sample_handle_key(struct key_event * k)
{
	int n, v;

	if (k->state == KEY_PRESS && k->sym == SCHISM_KEYSYM_ESCAPE && NO_MODIFIER(k->mod)) {
		set_page(PAGE_SAMPLE_LIST);
		return;
	}
	if (!NO_MODIFIER(k->mod)) return;

	if (k->midi_note > -1) {
		n = k->midi_note;
		if (k->midi_volume > -1) {
			v = k->midi_volume / 2;
		} else {
			v = KEYJAZZ_DEFAULTVOL;
		}
	} else if (k->is_repeat) {
		return;
	} else {
		n = kbd_get_note(k);
		v = KEYJAZZ_DEFAULTVOL;
		if (n <= 0 || n > 120)
			return;
	}

	handle_preload();
	if (fake_slot != KEYJAZZ_NOINST) {
		if (k->state == KEY_PRESS)
			song_keydown(KEYJAZZ_INST_FAKE, KEYJAZZ_NOINST, n, v, KEYJAZZ_CHAN_CURRENT);
		else
			song_keyup(KEYJAZZ_INST_FAKE, KEYJAZZ_NOINST, n);
	}
}

/* --------------------------------------------------------------------------------------------------------- */
static void handle_preload(void)
{
	dmoz_file_t *file;

	if (fake_slot == KEYJAZZ_NOINST && current_file >= 0 && current_file < flist.num_files) {
		file = flist.files[current_file];
		if (file && (file->type & TYPE_SAMPLE_MASK)) {
			fake_slot_changed = 0;
			fake_slot = song_preload_sample(file); // either 0 or KEYJAZZ_NOTINST
		}
	}
}

static void handle_rename_op(void)
{
	handle_preload();
}

static void handle_load_copy_uint(uint32_t s, uint32_t *d)
{
	if (s != *d) {
		*d = s;
		fake_slot_changed = 1;
	}
}

static void handle_load_copy(song_sample_t *s)
{
	handle_load_copy_uint(widgets_loadsample[2].d.numentry.value, &s->c5speed);
	handle_load_copy_uint(widgets_loadsample[4].d.numentry.value, &s->loop_start);
	handle_load_copy_uint(widgets_loadsample[5].d.numentry.value, &s->loop_end);
	handle_load_copy_uint(widgets_loadsample[7].d.numentry.value, &s->sustain_start);
	handle_load_copy_uint(widgets_loadsample[8].d.numentry.value, &s->sustain_end);
	handle_load_copy_uint(widgets_loadsample[9].d.thumbbar.value, &s->volume);
	if ((unsigned int)widgets_loadsample[9].d.thumbbar.value == (s->volume>>2)) {
		s->volume = (widgets_loadsample[9].d.thumbbar.value << 2);
		fake_slot_changed=1;
	}
	handle_load_copy_uint(widgets_loadsample[10].d.thumbbar.value, &s->global_volume);
	handle_load_copy_uint(widgets_loadsample[11].d.thumbbar.value, &s->vib_rate);
	handle_load_copy_uint(widgets_loadsample[12].d.thumbbar.value, &s->vib_depth);
	handle_load_copy_uint(widgets_loadsample[13].d.thumbbar.value, &s->vib_speed);
	switch (widgets_loadsample[3].d.menutoggle.state) {
	case 0:
		if (s->flags & (CHN_LOOP|CHN_PINGPONGLOOP)) {
			s->flags &= ~(CHN_LOOP|CHN_PINGPONGLOOP);
			fake_slot_changed=1;
		}
		break;
	case 1:
		if ((s->flags & (CHN_LOOP|CHN_PINGPONGLOOP)) == CHN_LOOP) {
			s->flags &= ~(CHN_LOOP|CHN_PINGPONGLOOP);
			s->flags |= (CHN_LOOP);
			fake_slot_changed=1;
		}
		break;
	case 2:
		if ((s->flags & (CHN_LOOP|CHN_PINGPONGLOOP)) == CHN_PINGPONGLOOP) {
			s->flags &= ~(CHN_LOOP|CHN_PINGPONGLOOP);
			s->flags |= (CHN_PINGPONGLOOP);
			fake_slot_changed=1;
		}
		break;
	};
	switch (widgets_loadsample[6].d.menutoggle.state) {
	case 0:
		if (s->flags & (CHN_SUSTAINLOOP|CHN_PINGPONGSUSTAIN)) {
			s->flags &= ~(CHN_SUSTAINLOOP|CHN_PINGPONGSUSTAIN);
			fake_slot_changed=1;
		}
		break;
	case 1:
		if ((s->flags & (CHN_SUSTAINLOOP|CHN_PINGPONGSUSTAIN)) == CHN_SUSTAINLOOP) {
			s->flags &= ~(CHN_SUSTAINLOOP|CHN_PINGPONGSUSTAIN);
			s->flags |= (CHN_SUSTAINLOOP);
			fake_slot_changed=1;
		}
		break;
	case 2:
		if ((s->flags & (CHN_SUSTAINLOOP|CHN_PINGPONGSUSTAIN)) == CHN_PINGPONGSUSTAIN) {
			s->flags &= ~(CHN_SUSTAINLOOP|CHN_PINGPONGSUSTAIN);
			s->flags |= (CHN_PINGPONGSUSTAIN);
			fake_slot_changed=1;
		}
		break;
	};
}

static void handle_load_update(void)
{
	song_sample_t *s;
	handle_preload();
	if (fake_slot != KEYJAZZ_NOINST) {
		s = song_get_sample(fake_slot);
		if (s) {
			handle_load_copy(s);
			song_update_playing_sample(fake_slot);
		}
	}
}


void load_sample_load_page(struct page *page)
{
	vgamem_ovl_alloc(&sample_image);
	clear_directory();


	widget_create_other(widgets_loadsample + 0, 0,
				file_list_handle_key,
				file_list_handle_text_input,
				file_list_draw);
	widgets_loadsample[0].accept_text = 1;
	widgets_loadsample[0].next.tab = 1;

	widget_create_textentry(widgets_loadsample+1,
			64, 13,
			13,
				1,2, 9, handle_rename_op,
				current_filename, sizeof(current_filename)-1);
	sample_speed_pos = 0;
	widget_create_numentry(widgets_loadsample+2,
			64, 14,
			7,
			1,3, 9, handle_load_update,
			0, 9999999,
			&sample_speed_pos);

	widget_create_menutoggle(widgets_loadsample+3,
			64, 15,
			2, 4,  0,  9,9, handle_load_update,
			loop_states);

	sample_loop_beg = 0;
	widget_create_numentry(widgets_loadsample+4,
			64, 16,
			7,
			3,5, 9, handle_load_update,
			0, 9999999,
			&sample_loop_beg);
	sample_loop_end = 0;
	widget_create_numentry(widgets_loadsample+5,
			64, 17,
			7,
			4,6, 9, handle_load_update,
			0, 9999999,
			&sample_loop_end);

	widget_create_menutoggle(widgets_loadsample+6,
			64, 18,
			5, 7,  0,  9,9, handle_load_update,
			loop_states);

	sample_susloop_beg = 0;
	widget_create_numentry(widgets_loadsample+7,
			64, 19,
			7,
			6,8, 9, handle_load_update,
			0, 9999999,
			&sample_susloop_beg);
	sample_susloop_end = 0;
	widget_create_numentry(widgets_loadsample+8,
			64, 20,
			7,
			7,9, 9, handle_load_update,
			0, 9999999,
			&sample_susloop_end);

	widget_create_thumbbar(widgets_loadsample+9,
			63, 33,
			9,
			8, 10, 0, handle_load_update,
			0,64);
	widget_create_thumbbar(widgets_loadsample+10,
			63, 34,
			9,
			9, 11, 0, handle_load_update,
			0,64);

	widget_create_thumbbar(widgets_loadsample+11,
			63, 37,
			9,
			10, 12, 0, handle_load_update,
			0,64);
	widget_create_thumbbar(widgets_loadsample+12,
			63, 38,
			9,
			11, 13, 0, handle_load_update,
			0,32);
	widget_create_thumbbar(widgets_loadsample+13,
			63, 39,
			9,
			12, 13, 0, handle_load_update,
			0,255);


	page->title = "Load Sample";
	page->draw_const = load_sample_draw_const;
	page->set_page = load_sample_set_page;
	page->handle_key = load_sample_handle_key;
	page->total_widgets = 14;
	page->widgets = widgets_loadsample;
	page->help_index = HELP_GLOBAL;
}

void library_sample_load_page(struct page *page)
{
	/* this shares all the widgets from load_sample */

	page->title = "Sample Library (Ctrl-F3)";
	page->draw_const = load_sample_draw_const;
	page->set_page = library_sample_set_page;
	page->handle_key = load_sample_handle_key;
	page->total_widgets = 14;
	page->widgets = widgets_loadsample;
	page->help_index = HELP_GLOBAL;
}

