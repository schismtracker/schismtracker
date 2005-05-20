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

#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"
#include "dmoz.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <SDL.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

/* --------------------------------------------------------------------------------------------------------- */
/* the locals */

static struct widget widgets_loadsample[1];

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
static int current_file = 0;
static time_t directory_mtime;
static dmoz_filelist_t flist;


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
	top_file = current_file = 0;
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
}

/* return: 1 = success, 0 = failure
TODO: provide some sort of feedback if something went wrong. */
static int change_dir(const char *dir)
{
	char *ptr = dmoz_path_normal(dir);

	if (!ptr)
		return 0;

	/* FIXME: need to make sure it exists, and that it's a directory */
	strncpy(cfg_dir_samples, ptr, PATH_MAX);
	cfg_dir_samples[PATH_MAX] = 0;
	free(ptr);
	
	read_directory();
	top_file = current_file = 0;
	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */

static void file_list_reposition(void)
{
	if (current_file < top_file)
		top_file = current_file;
	else if (current_file > top_file + 34)
		top_file = current_file - 34;
}

/* --------------------------------------------------------------------------------------------------------- */

static void load_sample_draw_const(void)
{
	draw_box(5, 12, 50, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(6, 13, 49, 47, 0);
	
	draw_box(63, 12, 77, 23, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(64, 13, 76, 22, 0);
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

	draw_box(51, 24, 77, 29, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(52, 25, 76, 28, 0);

	draw_box(51, 30, 77, 42, BOX_THIN | BOX_INNER | BOX_INSET);

	/* these abbreviations are sucky and lame. any suggestions? */
	draw_text("Def. Vol.", 53, 33, 0, 2);
	draw_text("Glb. Vol.", 53, 34, 0, 2);
	draw_text("Vib.Speed", 53, 37, 0, 2);
	draw_text("Vib.Depth", 53, 38, 0, 2);
	draw_text("Vib. Rate", 53, 39, 0, 2);

	draw_box(52, 43, 77, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(53, 44, 76, 47, 0);
}

/* --------------------------------------------------------------------------------------------------------- */

static void load_sample_set_page(void)
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

	*selected_widget = 0;
}

/* --------------------------------------------------------------------------------------------------------- */

static void file_list_draw(void)
{
	int n, pos, fg, bg;
	char buf[8];
	dmoz_file_t *file;
	
	/* there's no need to have if (files) { ... } like in the load-module page,
	   because there will always be at least "/" in the list */
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
		draw_text(numtostr(3, n, buf), 2, pos, 0, 2);
		draw_text_len(file->title, 25, 6, pos, fg, bg);
		draw_char(168, 31, pos, 2, bg);
		draw_text_len(file->base, 18, 32, pos, fg, bg);
	}
	
	/* draw the info for the current file (or directory...) */
	
	while (pos < 48)
		draw_char(168, 31, pos++, 2, 0);
}

/* on the file list, that is */
static void handle_enter_key(void)
{
	dmoz_file_t *file = flist.files[current_file];
	int cur = sample_get_current();
	
	if (file->type & TYPE_BROWSABLE_MASK) {
		change_dir(file->path);
		status.flags |= NEED_UPDATE;
	} else if (file->sample) {
		/* it's already been loaded, so copy it */
		song_sample *smp = song_get_sample(cur, NULL);
		song_copy_sample(cur, file->sample, file->title);
		strncpy(smp->filename, file->base, 12);
		smp->filename[12] = 0;
		clear_cached_waveform(cur);
		set_page(PAGE_SAMPLE_LIST);
	} else if (file->type & TYPE_SAMPLE_MASK) {
		/* load the sample */
		song_load_sample(cur, file->path);
		clear_cached_waveform(cur);
		set_page(PAGE_SAMPLE_LIST);
	}
}

static int file_list_handle_key(SDL_keysym * k)
{
	int new_file = current_file;
	
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
	case SDLK_KP_ENTER:
		handle_enter_key();
		return 1;
	default:
		return 0;
	}
	
	new_file = CLAMP(new_file, 0, flist.num_files - 1);
	if (new_file != current_file) {
		current_file = new_file;
		file_list_reposition();
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

static void load_sample_handle_key(SDL_keysym * k)
{
	if (k->sym == SDLK_ESCAPE && NO_MODIFIER(k->mod))
		set_page(PAGE_SAMPLE_LIST);
}

/* --------------------------------------------------------------------------------------------------------- */

void load_sample_load_page(struct page *page)
{
	page->title = "Load Sample";
	page->draw_const = load_sample_draw_const;
	page->set_page = load_sample_set_page;
	page->handle_key = load_sample_handle_key;
	page->total_widgets = 1;
	page->widgets = widgets_loadsample;
	page->help_index = HELP_GLOBAL;

	create_other(widgets_loadsample + 0, 0, file_list_handle_key, file_list_draw);
}
