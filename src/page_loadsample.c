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

#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"

#include "title.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <SDL.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

/* FIXME: do this somewhere else */
#ifndef HAVE_STRVERSCMP
# define strverscmp strcasecmp
#endif

/* --------------------------------------------------------------------------------------------------------- */
/* the locals */

static struct item items_loadsample[1];

/* --------------------------------------------------------------------------------------------------------- */
/* for all of these, buf should be (at least) 27 chars. anything past
 * that isn't used.
 * FIXME: move elsewhere; these are duplicated in page_loadmodule.c */

static char *get_date_string(time_t when, char *buf)
{
	struct tm tm;
	char month[16] = "";

	/* plugh */
	strftime(month, 16, "%B", localtime_r(&when, &tm));
	month[15] = 0;
	snprintf(buf, 27, "%s %d, %d", month, tm.tm_mday, 1900 + tm.tm_year);

	return buf;
}

static char *get_time_string(time_t when, char *buf)
{
	struct tm tm;

	localtime_r(&when, &tm);
	snprintf(buf, 27, "%d:%02d%s", tm.tm_hour % 12 ? : 12, tm.tm_min, tm.tm_hour < 12 ? "am" : "pm");
	return buf;
}

/* --------------------------------------------------------------------------------------------------------- */

/*
LTYP_UNCHECKED = color 6, name = the filename
  - any file that hasn't been identified as a module, sample, or unknown type
    (all non-directories are initially LTYP_UNCHECKED)
    The color for this type doesn't matter: with the way I'm implementing this, all files will be checked
    before they get drawn. This is also the case for the module browser; in fact, I used the unchecked
    color for XM files. I'll use that color for libraries here, since in IT they're the same color as
    regular samples. (Come to think of it, if I use a different color to indicate libraries, maybe I
    could replace the "....Library...." text with the module's name.)
LTYP_DIRECTORY = color 5, name = "........Directory........"
  - directories, duh ;)            [note: the dots are really char 154]
LTYP_SAMPLE = color 3, name = whatever the sample's name is
  - anything loadable as a structured sample, i.e. *.its files
    also, anything within a sample library is LTYP_SAMPLE.
LTYP_LIBRARY = color 3, name ".........Library........."
  - any modplug-readable format
LTYP_UNKNOWN = color 2, name = the filename
  - any file that's not a recognised module or sample type

"ltyp" used to be "type", but the title library is using TYPE_* to identify the "family" of a particular file.

Note: the ordering here roughly corresponds with the cache.its format values.
TODO: check the actual format values cache.its uses, and actually copy them here.
*/
enum { LTYP_UNCHECKED, LTYP_DIRECTORY, LTYP_SAMPLE, LTYP_LIBRARY, LTYP_UNKNOWN };
static const int ltyp_colors[] = { 4, 5, 3, 6, 2 };

/* note: this has do be split up like this; otherwise it gets read as '\x9ad'
 * which is the Wrong Thing. */
static const byte *directory_text = "\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a"
	"Directory\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a";

#if 0
static const byte *library_text =
	"\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9aLibrary\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a";
#endif

static const byte *directory_type_name = "Directory";
static const byte *unknown_type_name = "Unknown sample format";

struct file_list_data {
	int type;			/* LTYP_* above */
	char *filename;			/* the filename (this needs free'd) */
	time_t timestamp;		/* from stat */
	size_t filesize;		/* from stat */
	/* if type == LTYP_UNCHECKED, nothing below this point will have been filled in */
	const char *type_name;		/* i.e. "Impulse Tracker sample" */
	/* The title points to the same place as the filename for LTYP_UNKNOWN,
	 * directory_text or library_text for LTYP_DIRECTORY and LTYP_LIBRARY
	 * respectively, to the title field in the sample structure for LTYP_SAMPLE,
	 * and is undefined for LTYP_UNCHECKED. */
	const char *title;
	/* This field is only defined if type == LTYP_SAMPLE.
	 * 
	 * Actually, this shouldn't be necessary until the sample needs to be loaded -- when does IT load
	 * samples? i.e. should moving the cursor over the sample read the data from disk, update the
	 * waveform, change the loop info, etc., or does that happen once it's actually played (or even
	 * loaded into the song)?
	 * 
	 * Additionally, I need to figure out what to do with libraries: currently I only have support for
	 * loading one song at a time (which is sensible, as there aren't any multi-file editing capabilities
	 * in IT anyway) -- the trouble is, loading a sample from another song pretty much requires loading
	 * that song. So, I probably need to add a new function to load a second song for the sample browser.
	 * 
	 * One more thing: when loading a really big file for the sample browser, should the file remain
	 * loaded once the sample load screen is closed, or should it be re-loaded every time the screen is
	 * opened? Perhaps the best way to handle this would be with some sort of release-timer, but it would
	 * very likely be hard to implement cleanly. */
	/* song_sample *sample; */
};
static struct file_list_data **files = NULL;

static time_t directory_mtime;

static int top_file = 0;
static int current_file = 0;
static int num_files = 0;
static int allocated_size = 0;

#define BLOCK_SIZE 256			/* this is probably plenty */

/* - The file list is malloc'd, and realloc'd whenever num_files reaches allocated_size.
 *   Initially, BLOCK_SIZE items are allocated, and this number is doubled when a realloc
 *   is needed.
 * - The individual items in the list, and the filename and sample fields are malloc'd;
 *   the sample field should only be free'd if type == LTYP_SAMPLE. */

static void clear_directory(void)
{
	int n;
	
	if (!files)
		return;
	
	for (n = 0; n < num_files; n++) {
		if (files[n]->type == LTYP_SAMPLE) {
			/* free(files[n]->sample);
			 * (and free the stuff in the sample structure too!) */
		}
		free(files[n]->filename);
		free(files[n]);
	}
	
	free(files);
	
	top_file = 0;
	current_file = 0;
	num_files = 0;
	allocated_size = 0;
}

static void allocate_more(void)
{
	if (allocated_size == 0) {
		allocated_size = BLOCK_SIZE;
		files = malloc(BLOCK_SIZE * sizeof(struct file_list_data *));
	} else {
		/* Double the size. */
		allocated_size *= 2;
		files = realloc(files, allocated_size * sizeof(struct file_list_data *));
	}
}

static void add_file_to_list(int type, char *filename, time_t timestamp, size_t filesize)
{
	struct file_list_data *fi = malloc(sizeof(struct file_list_data));
	fi->type = type;
	fi->filename = filename;
	fi->timestamp = timestamp;
	fi->filesize = filesize;
	if (type == LTYP_DIRECTORY) {
		fi->type_name = directory_type_name;
		fi->title = directory_text;
	}

	if (num_files >= allocated_size)
		allocate_more();
	files[num_files++] = fi;
}

static int qsort_compare(const void *_a, const void *_b)
{
	const struct file_list_data *a = *(const struct file_list_data **) _a;
	const struct file_list_data *b = *(const struct file_list_data **) _b;
	
	/* Slightly convoluted, but hey. */
	if (a->type == LTYP_DIRECTORY) {
		if (b->type == LTYP_DIRECTORY) {
			/* 'a' and 'b' are both directories. put "/" and ".." first,
			 * and leave the rest to the strcmp below */
			if (!strcmp(a->filename, "/"))
				return -1;
			else if (!strcmp(b->filename, "/"))
				return 1;
			else if (!strcmp(a->filename, ".."))
				return -1;
			else if (!strcmp(b->filename, ".."))
				return 1;
		} else {
			/* 'a' is a directory, but 'b' is not */
			return -1;	/* a goes first */
		}
	} else if (b->type == LTYP_DIRECTORY) {
		/* 'a' is not a directory, but 'b' is */
		return 1;		/* b goes first */
	}
	return strverscmp(a->filename, b->filename);
}

static void read_directory(void)
{
	DIR *dir;
	struct dirent *ent;
	char *ptr;
	struct stat st;
	
	clear_directory();
	
	if (stat(cfg_dir_samples, &st) < 0 || (dir = opendir(cfg_dir_samples)) == NULL) {
		perror(cfg_dir_samples);
		/* add "/" so it's still possible to do something */
		add_file_to_list(LTYP_DIRECTORY, strdup("/"), 0, 0);
		return;
	}
	
	directory_mtime = st.st_mtime;
	
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.' || ent->d_name[_D_EXACT_NAMLEN(ent) - 1] == '~')
			continue;

		asprintf(&ptr, "%s/%s", cfg_dir_samples, ent->d_name);
		if (stat(ptr, &st) < 0) {
			/* doesn't exist? */
			perror(ptr);
		} else if (S_ISDIR(st.st_mode)) {
			add_file_to_list(LTYP_DIRECTORY, strdup(ent->d_name), st.st_mtime, 0);
		} else if (S_ISREG(st.st_mode)) {
			add_file_to_list(LTYP_UNCHECKED, strdup(ent->d_name), st.st_mtime, st.st_size);
		}
		free(ptr);
	}
	
	closedir(dir);
	
	/* TODO: stat "/" and ".." to get the mtimes. certainly not critical, but it would be nice. */
	add_file_to_list(LTYP_DIRECTORY, strdup("/"), 0, 0);
	add_file_to_list(LTYP_DIRECTORY, strdup(".."), 0, 0);

	qsort(files, num_files, sizeof(struct file_list_data *), qsort_compare);
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
	SDL_LockSurface(screen);
	
	draw_box_unlocked(5, 12, 50, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(6, 13, 49, 47, 0);

	draw_box_unlocked(63, 12, 77, 23, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(64, 13, 76, 22, 0);
	/* these are exactly the same as in page_samples.c, apart from
	 * 'quality' and 'length' being one line higher */
	draw_text_unlocked("Filename", 55, 13, 0, 2);
	draw_text_unlocked("Speed", 58, 14, 0, 2);
	draw_text_unlocked("Loop", 59, 15, 0, 2);
	draw_text_unlocked("LoopBeg", 56, 16, 0, 2);
	draw_text_unlocked("LoopEnd", 56, 17, 0, 2);
	draw_text_unlocked("SusLoop", 56, 18, 0, 2);
	draw_text_unlocked("SusLBeg", 56, 19, 0, 2);
	draw_text_unlocked("SusLEnd", 56, 20, 0, 2);
	draw_text_unlocked("Quality", 56, 21, 0, 2);
	draw_text_unlocked("Length", 57, 22, 0, 2);

	draw_box_unlocked(51, 24, 77, 29, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(52, 25, 76, 28, 0);

	draw_box_unlocked(51, 30, 77, 42, BOX_THIN | BOX_INNER | BOX_INSET);

	/* these abbreviations are sucky and lame. any suggestions? */
	draw_text_unlocked("Def. Vol.", 53, 33, 0, 2);
	draw_text_unlocked("Glb. Vol.", 53, 34, 0, 2);
	draw_text_unlocked("Vib.Speed", 53, 37, 0, 2);
	draw_text_unlocked("Vib.Depth", 53, 38, 0, 2);
	draw_text_unlocked("Vib. Rate", 53, 39, 0, 2);

	draw_box_unlocked(52, 43, 77, 48, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(53, 44, 76, 47, 0);

	SDL_UnlockSurface(screen);
}

/* --------------------------------------------------------------------------------------------------------- */

static void load_sample_set_page(void)
{
	struct stat st;
	
	/* if we have a list, the directory didn't change,
	 * and the mtime is the same, we're set */
	if (files != NULL
	    && (status.flags & DIR_SAMPLES_CHANGED) == 0
	    && stat(cfg_dir_samples, &st) == 0
	    && st.st_mtime == directory_mtime) {
		return;
	}

	status.flags &= ~DIR_SAMPLES_CHANGED;

	top_file = current_file = 0;
	read_directory();
	*selected_item = 0;
}

/* --------------------------------------------------------------------------------------------------------- */

static void fill_file_info(struct file_list_data *file)
{
	char *ptr;
	file_info *fi;
	int ret;
	
	asprintf(&ptr, "%s/%s", cfg_dir_samples, file->filename);
	
	ret = file_info_get(ptr, NULL, &fi);
        switch (ret) {
        case FINF_SUCCESS:
                file->title = fi->title;
                file->type_name = fi->description;
                switch (fi->type) {
                case TYPE_SAMPLE:
                case TYPE_OTHER:
                	file->type = LTYP_SAMPLE;
                	break;
                default: /* module (library) */
                	//file->type = LTYP_LIBRARY;
                	file->type = LTYP_UNKNOWN;
                	break;
                }
                free(fi->extension);
                free(fi);
                free(ptr);
                return;
	case FINF_UNSUPPORTED:
		file->type_name = unknown_type_name;
		break;
	case FINF_EMPTY:
		file->type_name = strdup("Empty file");
		break;
	case FINF_ERRNO:
                file->type_name = strdup(strerror(errno));
                break;
        default:
        	log_appendf(4, "file_info_get returned unhandled status (%d)", ret);
        	file->type_name = strdup("Unknown file error");
        	break;
        }
	
	//file->title = file->filename;
	file->title = strdup("");
	file->type = LTYP_UNKNOWN;
	free(ptr);
}

/* --------------------------------------------------------------------------------------------------------- */

static void file_list_draw(void)
{
	int n, pos, fg, bg;
	char buf[8];
	
	/* draw_fill_chars(...); */
	
	/* SDL_LockSurface(screen); */
	
	/* there's no need to have if (files) { ... } like in the load-module page,
	 * because there will always be at least "/" in the list */
	for (n = top_file, pos = 13; n < num_files && pos < 48; n++, pos++) {
		if (files[n]->type == LTYP_UNCHECKED)
			fill_file_info(files[n]);
		
		if (n == current_file && ACTIVE_PAGE.selected_item == 0) {
			fg = 0;
			bg = 3;
		} else {
			fg = ltyp_colors[files[n]->type];
			bg = 0;
		}

		draw_text(numtostr(3, n, buf), 2, pos, 0, 2);
		
		draw_text_len(files[n]->title, 25, 6, pos, fg, bg);
		draw_char(168, 31, pos, 2, bg);
		draw_text_len(files[n]->filename, 18, 32, pos, fg, bg);
	}
	
	/* draw the info for the current file (or directory...) */
	
	if (pos < 48) {
		SDL_LockSurface(screen);
		while (pos < 48)
			draw_char_unlocked(168, 31, pos++, 2, 0);
		SDL_UnlockSurface(screen);
	}
}

/* on the file list, that is */
static void handle_enter_key(void)
{
	char *ptr;
	char buf[PATH_MAX + 1];
	int cur;
	
	switch (files[current_file]->type) {
	default:
		/* TODO */
		break;
		
		/* Since (in theory) the format has already been determined, there should be some way to
		 * identify a sample to the loader as being a particular type. (The unknown type would, of
		 * course, use the 'raw' loader.)
		 * 
		 * This means making another field in the file list structure telling what the sample's type
		 * is. It'd be pretty slick to make it a pointer to the actual loader function, and then
		 * group LTYP_SAMPLE and LTYP_UNKNOWN together here. The problem with doing that is the
		 * special case of a sample within a library -- thus, the LTYP_SAMPLE case would go first
		 * with an if statement, and then fall through for the unknown type to load any random file
		 * on the disk. */
	case LTYP_SAMPLE:
	case LTYP_UNKNOWN:
		cur = sample_get_current();
		asprintf(&ptr, "%s/%s", cfg_dir_samples, files[current_file]->filename);
		song_load_sample(cur, ptr);
		clear_cached_waveform(cur);
		free(ptr);
		set_page(PAGE_SAMPLE_LIST);
		break;
	case LTYP_DIRECTORY:
		/* change to the directory */
		if (current_file == 0) {
			ptr = strdup("/");
		} else {
			asprintf(&ptr, "%s/%s", cfg_dir_samples, files[current_file]->filename);
		}
		if (realpath(ptr, buf) == NULL) {
			perror(ptr);
		} else {
			strcpy(cfg_dir_samples, buf);
			read_directory();
			top_file = current_file = 0;
			status.flags |= NEED_UPDATE;
		}
		free(ptr);
		break;
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
		new_file = num_files - 1;
		break;
	case SDLK_RETURN:
	case SDLK_KP_ENTER:
		handle_enter_key();
		return 1;
	default:
		return 0;
	}
	
	new_file = CLAMP(new_file, 0, num_files - 1);
	if (new_file != current_file) {
		current_file = new_file;
		file_list_reposition();
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

static void load_sample_handle_key(UNUSED SDL_keysym * k)
{
	/* somehow get around the menu being triggered by the escape
	 * key... probably this will require yet another hack in the
	 * key handler in page.c :( */
}

/* --------------------------------------------------------------------------------------------------------- */

void load_sample_load_page(struct page *page)
{
	page->title = "Load Sample";
	page->draw_const = load_sample_draw_const;
	page->set_page = load_sample_set_page;
	page->handle_key = load_sample_handle_key;
	page->total_items = 1;
	page->items = items_loadsample;
	page->help_index = HELP_GLOBAL;

	create_other(items_loadsample + 0, 0, file_list_handle_key, file_list_draw);
}
