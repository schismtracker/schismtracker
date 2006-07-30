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

#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"
#include "dmoz.h"

#include <sys/types.h>
#include <sys/stat.h>

#include "sdlmain.h"

#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

/* --------------------------------------------------------------------------------------------------------- */
/* the locals */

static int _library_mode = 0;
static struct widget widgets_loadsample[1];
static int fake_slot = -1;
static int need_trigger = -1;
static int need_keyoff = -1;

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
#define current_file flist.selected

static int slash_search_mode = -1;
static char slash_search_str[PATH_MAX];

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

static int sampgrep(dmoz_file_t *f)
{
	if ((f->type & TYPE_EXT_DATA_MASK) == 0)
		dmoz_fill_ext_data(f);
	return 1;
}

static void clear_directory(void)
{
	dmoz_free(&flist, NULL);
	fake_slot = -1;
}
static void file_list_reposition(void)
{
	if (current_file >= flist.num_files)
		current_file = flist.num_files-1;
	if (current_file < 0) current_file = 0;
	if (current_file < top_file)
		top_file = current_file;
	else if (current_file > top_file + 34)
		top_file = current_file - 34;
}


static void read_directory(void)
{
	struct stat st;

	clear_directory();
	
	if (stat(cfg_dir_samples, &st) < 0)
		directory_mtime = 0;
	else
		directory_mtime = st.st_mtime;
	/* if the stat call failed, this will probably break as well, but
	at the very least, it'll add an entry for the root directory. */
	if (dmoz_read(cfg_dir_samples, &flist, NULL) < 0)
		perror(cfg_dir_samples);

	dmoz_filter_filelist(&flist, sampgrep, &current_file,
				file_list_reposition);
        dmoz_cache_lookup(cfg_dir_samples, &flist, 0);
	file_list_reposition();
}

/* return: 1 = success, 0 = failure
TODO: provide some sort of feedback if something went wrong. */
static int change_dir(const char *dir)
{
	char *ptr = dmoz_path_normal(dir);

	if (!ptr)
		return 0;

        dmoz_cache_update(cfg_dir_samples, &flist, 0);

	/* FIXME: need to make sure it exists, and that it's a directory */
	strncpy(cfg_dir_samples, ptr, PATH_MAX);
	cfg_dir_samples[PATH_MAX] = 0;
	free(ptr);
	
	read_directory();
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static void load_sample_draw_const(void)
{
	dmoz_file_t *f;
	char sbuf[64];
	int filled;

	draw_box(5, 12, 50, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(6, 13, 49, 47, 0);
	
	draw_box(63, 12, 77, 23, BOX_THICK | BOX_INNER | BOX_INSET);
	filled = 0;
	f = 0;
	if (current_file >= 0
	&& current_file < flist.num_files && flist.files[current_file]) {
		f = flist.files[current_file];
		draw_text_len((unsigned char *) (f->smp_filename ? f->smp_filename : ""), 13, 64,13, 2, 0);

		sprintf(sbuf, "%07d", f->smp_speed);
		draw_text_len((unsigned char *) sbuf, 13, 64, 14, 2, 0);

		sprintf(sbuf, "%07d", f->smp_loop_start);
		draw_text_len((unsigned char *) sbuf, 13, 64, 16, 2, 0);
		sprintf(sbuf, "%07d", f->smp_loop_end);
		draw_text_len((unsigned char *) sbuf, 13, 64, 17, 2, 0);

		sprintf(sbuf, "%07d", f->smp_sustain_start);
		draw_text_len((unsigned char *) sbuf, 13, 64, 19, 2, 0);
		sprintf(sbuf, "%07d", f->smp_sustain_end);
		draw_text_len((unsigned char *) sbuf, 13, 64, 20, 2, 0);

		sprintf(sbuf, "%07d", f->smp_length);
		draw_text_len((unsigned char *) sbuf, 13, 64, 22, 2, 0);

		if (!f->smp_length && !f->smp_filename && !f->smp_flags) {
			draw_text_len((unsigned char *) "No sample",13, 64, 21, 2, 0);
		} else if (f->smp_flags & SAMP_STEREO) {
			draw_text_len((unsigned char *)
				(f->smp_flags & SAMP_16_BIT
				? "16 bits Stereo" : "8 bits Stereo"),
			13, 64, 21, 2, 0);
		} else {
			draw_text_len((unsigned char *)
				(f->smp_flags & SAMP_16_BIT
				? "16 bits" : "8 bits"),
			13, 64, 21, 2, 0);
		}

		draw_text_len((unsigned char *)
			(f->smp_flags & SAMP_SUSLOOP
			? "On" : "Off"),
		13, 64, 18, 2, 0);

		if (f->smp_flags & SAMP_LOOP_PINGPONG) {
			draw_text_len((unsigned char *) "On Ping Pong",
				13, 64, 15, 2, 0);
		} else if (f->smp_flags & SAMP_LOOP) {
			draw_text_len((unsigned char *) "On Forwards",
				13, 64, 15, 2, 0);
		} else {
			draw_text_len((unsigned char *) "Off",
				13, 64, 15, 2, 0);
		}
	}

	/* these are exactly the same as in page_samples.c, apart from
	 * 'quality' and 'length' being one line higher */
	draw_text((unsigned char *) "Filename", 55, 13, 0, 2);
	draw_text((unsigned char *) "Speed", 58, 14, 0, 2);
	draw_text((unsigned char *) "Loop", 59, 15, 0, 2);
	draw_text((unsigned char *) "LoopBeg", 56, 16, 0, 2);
	draw_text((unsigned char *) "LoopEnd", 56, 17, 0, 2);
	draw_text((unsigned char *) "SusLoop", 56, 18, 0, 2);
	draw_text((unsigned char *) "SusLBeg", 56, 19, 0, 2);
	draw_text((unsigned char *) "SusLEnd", 56, 20, 0, 2);
	draw_text((unsigned char *) "Quality", 56, 21, 0, 2);
	draw_text((unsigned char *) "Length", 57, 22, 0, 2);

	draw_box(51, 24, 77, 29, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(52, 25, 76, 28, 0);

	draw_box(51, 30, 77, 42, BOX_THIN | BOX_INNER | BOX_INSET);

	/* these abbreviations are sucky and lame. any suggestions? */
	draw_text((unsigned char *) "Def. Vol.", 53, 33, 0, 2);
	draw_text((unsigned char *) "Glb. Vol.", 53, 34, 0, 2);
	draw_text((unsigned char *) "Vib.Speed", 53, 37, 0, 2);
	draw_text((unsigned char *) "Vib.Depth", 53, 38, 0, 2);
	draw_text((unsigned char *) "Vib. Rate", 53, 39, 0, 2);

	draw_box(52, 43, 77, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(53, 44, 76, 47, 0);


	if (need_trigger > -1) {
		if (fake_slot > -1) {
			if (need_keyoff > -1) song_keyup(fake_slot, -1, need_keyoff, -1, 0);
			song_keydown(fake_slot, -1, need_keyoff = need_trigger, 64, -1, 0);
		}
		need_trigger = -1;
	}
}

/* --------------------------------------------------------------------------------------------------------- */

static void _common_set_page(void)
{
	struct stat st;
	
	/* if we have a list, the directory didn't change, and the mtime is the same, we're set */
	if (flist.num_files > 0
	    && (status.flags & DIR_SAMPLES_CHANGED) == 0
	    && stat(cfg_dir_samples, &st) == 0
	    && st.st_mtime == directory_mtime) {
		return;
	}

	change_dir(cfg_dir_samples);

	status.flags &= ~DIR_SAMPLES_CHANGED;
	fake_slot = -1;

	*selected_widget = 0;
	slash_search_mode = -1;
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
	int n, i, pos, fg, bg;
	char buf[8];
	dmoz_file_t *file;
	
	/* there's no need to have if (files) { ... } like in the load-module page,
	   because there will always be at least "/" in the list */
	if (top_file < 0) top_file = 0;
	if (current_file < 0) current_file = 0;
	for (n = top_file, pos = 13; n < flist.num_files && pos < 48; n++, pos++) {
		file = flist.files[n];
		if ((file->type & TYPE_EXT_DATA_MASK) == 0)
			dmoz_fill_ext_data(file);
		
		if (n == current_file && ACTIVE_PAGE.selected_widget == 0) {
			fg = 0;
			bg = 3;
		} else {
			fg = get_type_color(file->type);
			bg = 0;
		}
		draw_text(numtostr(3, n+1, (unsigned char *) buf), 2, pos, 0, 2);
		draw_text_len((unsigned char *) (file->title ? file->title : ""),
					25, 6, pos, fg, bg);
		draw_char(168, 31, pos, 2, bg);
		draw_text_len((unsigned char *) (file->base ? file->base : ""),
					18, 32, pos, fg, bg);
		if (file->base && slash_search_mode > -1) {
			if (strncasecmp(file->base,slash_search_str,slash_search_mode) == 0) {
				for (i = 0 ; i < slash_search_mode; i++) {
					if (tolower(((unsigned)file->base[i]))
					!= tolower(((unsigned)slash_search_str[i]))) break;
					draw_char(file->base[i], 32+i, pos, 3,1);
				}
			}
		}
	}
	
	/* draw the info for the current file (or directory...) */
	
	while (pos < 48)
		draw_char(168, 31, pos++, 2, 0);
}

static void do_create_host(UNUSED void *gn)
{
	int cur = sample_get_current();
	int n;

	if (song_instrument_is_empty(cur)) {
		song_init_instrument_from_sample(cur, cur);
	} else if (!(status.flags & CLASSIC_MODE)
	&& song_instrument_is_empty((n=instrument_get_current()))) {
		song_init_instrument_from_sample(n, cur);
	} else {
		n = song_first_unused_instrument();
		if (n) {
			song_init_instrument_from_sample(n, cur);
		} else {
			status_text_flash("Out of instruments");
		}
	}
	set_page(PAGE_SAMPLE_LIST);
}
static void dont_create_host(UNUSED void *gn)
{
	set_page(PAGE_SAMPLE_LIST);
}

static void finish_load(int cur);
static void stereo_cvt_complete_left(void)
{
	int cur = sample_get_current();
	song_sample *smp;
	smp = song_get_sample(cur, NULL);
	sample_mono_left(smp);
	dialog_destroy_all();
	finish_load(cur);
}
static void stereo_cvt_complete_right(void)
{
	int cur = sample_get_current();
	song_sample *smp;
	smp = song_get_sample(cur, NULL);
	sample_mono_right(smp);
	dialog_destroy_all();
	finish_load(cur);
}
static void stereo_cvt_complete_both(void)
{
	int cur = sample_get_current();
	dialog_destroy_all();
	memused_songchanged();
	if (!sample_is_used_by_instrument(cur) && song_is_instrument_mode()) {
		dialog_create(DIALOG_YES_NO, "Create host instrument?",
			do_create_host, dont_create_host, 0, NULL);
	} else {
		set_page(PAGE_SAMPLE_LIST);
	}
}
static void stereo_cvt_dialog(void)
{
	draw_text((unsigned char *) "Loading Stereo Sample", 30, 27, 0, 2);
}
static int stereo_cvt_hk(struct key_event *k)
{
	if (k->sym == SDLK_l) {
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state) stereo_cvt_complete_left();
		return 1;
	}
	if (k->sym == SDLK_r) {
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state) stereo_cvt_complete_right();
		return 1;
	}
	if (k->sym == SDLK_s || k->sym == SDLK_b) {
		if (!NO_MODIFIER(k->mod)) return 0;
		if (k->state) stereo_cvt_complete_both();
		return 1;
	}
	return 0;
}

static void finish_load(int cur)
{
	song_sample *smp;

	memused_songchanged();
	smp = song_get_sample(cur, NULL);
	if (smp->flags & SAMP_STEREO) {
/* Loading Stereo Sample
Left  Both  Right
*/
		struct dialog *dd;
		static struct widget stereo_cvt_widgets[4];
		create_button(stereo_cvt_widgets+0, 27, 30, 6,
				0, 0, 0,    (status.flags & CLASSIC_MODE) ? 2 : 1,
					(status.flags & CLASSIC_MODE) ? 2 : 1,
				stereo_cvt_complete_left, "Left", 2);

		create_button(stereo_cvt_widgets+1, 37, 30, 6,
				1, 1, 0, 2, 2,
				stereo_cvt_complete_both, "Both", 2);

		create_button(stereo_cvt_widgets+2, 47, 30, 6,
				1, 1, (status.flags & CLASSIC_MODE) ? 0 : 1, 2, 0,
				stereo_cvt_complete_right, "Right", 1);

		dd = dialog_create_custom(24, 25, 33, 8,
				stereo_cvt_widgets, 3,
				1,
				stereo_cvt_dialog, NULL);
		dd->action_cancel = (void *) stereo_cvt_complete_both;
		dd->handle_key = stereo_cvt_hk;
		return;
	}

	if (!sample_is_used_by_instrument(cur) && song_is_instrument_mode()) {
		dialog_create(DIALOG_YES_NO, "Create host instrument?",
			do_create_host, dont_create_host, 0, NULL);
	} else {
		set_page(PAGE_SAMPLE_LIST);
	}
}

static void reposition_at_slash_search(void)
{
	dmoz_file_t *f;
	int i, j, b, bl;

	if (slash_search_mode < 0) return;
	bl = b = -1;
	for (i = 0; i < flist.num_files; i++) {
		f = flist.files[i];
		if (!f || !f->base) continue;
		for (j = 0; j < slash_search_mode; j++) {
			if (tolower(((unsigned)f->base[j]))
			!= tolower(((unsigned)slash_search_str[j]))) break;
		}
		if (bl < j) {
			bl = j;
			b = i;
		}
	}
	if (bl > -1) {
		current_file = b;
		file_list_reposition();
	}
}

/* on the file list, that is */
static void handle_enter_key(void)
{
	dmoz_file_t *file;
	song_sample *smp;
	int cur = sample_get_current();
	
	if (current_file < 0 || current_file >= flist.num_files) return;

	file = flist.files[current_file];
        dmoz_cache_update(cfg_dir_samples, &flist, 0);

	if (file->type & (TYPE_BROWSABLE_MASK|TYPE_INST_MASK)) {
		change_dir(file->path);
		status.flags |= NEED_UPDATE;
	} else if (file->sample) {
		if (_library_mode) return;
		/* it's already been loaded, so copy it */
		smp = song_get_sample(cur, NULL);
		song_copy_sample(cur, file->sample, file->title);
		strncpy(smp->filename, file->base, 12);
		smp->filename[12] = 0;
		finish_load(cur);
		memused_songchanged();
	} else if (file->type & TYPE_SAMPLE_MASK) {
		if (_library_mode) return;
		/* load the sample */
		song_load_sample(cur, file->path);
		finish_load(cur);
		memused_songchanged();
	}

}

static int file_list_handle_key(struct key_event * k)
{
	int new_file = current_file;

	new_file = CLAMP(new_file, 0, flist.num_files - 1);

	if (!(status.flags & CLASSIC_MODE) && k->sym == SDLK_n && (k->mod & KMOD_ALT)) {
		song_toggle_multichannel_mode();
		return 1;
	}

	if (k->mouse) {
		if (k->x >= 6 && k->x <= 49 && k->y >= 13 && k->y <= 47) {
			if (k->mouse == MOUSE_SCROLL_UP) {
				new_file--;
			} else if (k->mouse == MOUSE_SCROLL_DOWN) {
				new_file++;
			} else {
				new_file = top_file + (k->y - 13);
			}
		}
	} else if (slash_search_mode > -1) {
		int c = unicode_to_ascii(k->unicode);
		if (k->sym == SDLK_RETURN || k->sym == SDLK_ESCAPE) {
			if (!k->state) return 1;
			slash_search_mode = -1;
			status.flags |= NEED_UPDATE;
			return 1;
		} else if (k->sym == SDLK_BACKSPACE) {
			if (k->state) return 1;
			slash_search_mode--;
			status.flags |= NEED_UPDATE;
			reposition_at_slash_search();
			return 1;
		} else if (c >= 32) {
			if (k->state) return 1;
			if (slash_search_mode < PATH_MAX) {
				slash_search_str[ slash_search_mode ] = c;
				slash_search_mode++;
				reposition_at_slash_search();
				status.flags |= NEED_UPDATE;
			}
			return 1;
		}
	}
	switch (k->sym) {
	case SDLK_UP:
		new_file--;
		break;
	case SDLK_DOWN:
		new_file++;
		break;
	case SDLK_PAGEUP:
		new_file -= 35;
		break;
	case SDLK_PAGEDOWN:
		new_file += 35;
		break;
	case SDLK_HOME:
		new_file = 0;
		break;
	case SDLK_END:
		new_file = flist.num_files - 1;
		break;
	case SDLK_RETURN:
		if (!k->state) return 0;
		handle_enter_key();
		return 1;
	case SDLK_ESCAPE:
		if (k->state && NO_MODIFIER(k->mod))
			set_page(PAGE_SAMPLE_LIST);
		return 1;
	case SDLK_SLASH:
		if (k->orig_sym == SDLK_SLASH) {
			if (status.flags & CLASSIC_MODE) return 0;
			if (!k->state) return 0;
			slash_search_mode = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		}
	default:
		if (!k->mouse) return 0;
	}
	
	if (k->mouse == MOUSE_CLICK) {
		if (!k->state) return 0;
	} else if (k->mouse == MOUSE_DBLCLICK) {
		handle_enter_key();
		return 1;
	} else {
		if (k->state) return 0;
	}
	new_file = CLAMP(new_file, 0, flist.num_files - 1);
	if (new_file < 0) new_file = 0;
	if (new_file != current_file) {
		fake_slot = -1;
		slash_search_mode = -1;
		current_file = new_file;
		file_list_reposition();
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

static void load_sample_handle_key(struct key_event * k)
{
	int n, v;

	if (!k->state && k->sym == SDLK_ESCAPE && NO_MODIFIER(k->mod)) {
		set_page(PAGE_SAMPLE_LIST);
		return;
	}
	if (!NO_MODIFIER(k->mod)) return;

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

	if (fake_slot < 0 && current_file >= 0 && current_file < flist.num_files) {
		dmoz_file_t *file = flist.files[current_file];
		if (file) {
			fake_slot = song_preload_sample(file);
		}
	}

	if (fake_slot > -1) {
		if ((status.flags & CLASSIC_MODE) || !song_is_multichannel_mode()) {
			if (!k->state && !k->is_repeat) {
				need_trigger = n;
				status.flags |= NEED_UPDATE;
			}
		} else {
			if (need_keyoff > -1) {
				song_keyup(fake_slot, -1, need_keyoff, -1, 0);
				need_keyoff = -1;
			}
			if (k->state) {
				song_keyup(fake_slot, -1, n, -1, 0);
				status.last_keysym = 0;
			} else if (!k->is_repeat) {
				song_keydown(fake_slot, -1, n, v, -1, 0);
			}
			need_trigger = -1;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------- */

void load_sample_load_page(struct page *page)
{
	clear_directory();

	page->title = "Load Sample";
	page->draw_const = load_sample_draw_const;
	page->set_page = load_sample_set_page;
	page->handle_key = load_sample_handle_key;
	page->total_widgets = 1;
	page->widgets = widgets_loadsample;
	page->help_index = HELP_GLOBAL;

	create_other(widgets_loadsample + 0, 0, file_list_handle_key, file_list_draw);
	widgets_loadsample[0].accept_text = 1;
}

void library_sample_load_page(struct page *page)
{
	clear_directory();

	page->title = "Sample Library (Ctrl-F3)";
	page->draw_const = load_sample_draw_const;
	page->set_page = library_sample_set_page;
	page->handle_key = load_sample_handle_key;
	page->total_widgets = 1;
	page->widgets = widgets_loadsample;
	page->help_index = HELP_GLOBAL;

	create_other(widgets_loadsample + 0, 0, file_list_handle_key, file_list_draw);
	widgets_loadsample[0].accept_text = 1;
}
