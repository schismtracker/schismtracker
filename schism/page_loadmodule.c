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
#include "charset.h"
#include "song.h"
#include "page.h"
#include "dmoz.h"
#include "log.h"
#include "fmt.h" /* only needed for SAVE_SUCCESS ... */

#include <sys/types.h>
#include <sys/stat.h>

#include "sdlmain.h"

#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "sndfile.h"

#include "disko.h"

/* --------------------------------------------------------------------- */
/* this was adapted from a really slick two-line fnmatch()
at http://compressionratings.com/d_archiver_template.html
and fuglified to add FNM_CASEFOLD|FNM_PERIOD behavior */

#if HAVE_FNMATCH
# include <fnmatch.h>
/* GNU extension, ignore */
# ifndef FNM_CASEFOLD
#  define FNM_CASEFOLD 0
# endif
#else
# define FNM_CASEFOLD 0
# define FNM_PERIOD 0
# define fnmatch xfnmatch
inline static int _fnmatch(const char *m, const char *s);
inline static int _fnmatch(const char *m, const char *s)
{
	if (*m == '*') for (++m; *s; ++s) if (!_fnmatch(m, s)) return 0;
	return (!*s || !(*m == '?' || tolower(*s) == tolower(*m)))
		? tolower(*m) | tolower(*s) : _fnmatch(++m, ++s);
}
inline static int xfnmatch(const char *m, const char *s, UNUSED int f)
{
	return (*s == '.' && *m != '.') ? 0 : _fnmatch(m, s);
}
#endif /* !HAVE_FNMATCH */

/* --------------------------------------------------------------------- */
/* the locals */

static int modgrep(dmoz_file_t *f);

static struct widget widgets_loadmodule[5];
static struct widget widgets_exportmodule[16];
static struct widget widgets_savemodule[16];
static struct widget *widgets_exportsave;

/* XXX this needs to be kept in sync with diskwriters
   (FIXME: it shouldn't have to! build it when the savemodule page is built or something, idk -storlek) */
static const int filetype_saves[] = { 4, 5, 6, 7, 8, 9, -1 };

static int top_file = 0, top_dir = 0;
static time_t directory_mtime;
static dmoz_filelist_t flist;
static dmoz_dirlist_t dlist;
#define current_file flist.selected
#define current_dir dlist.selected


/*
filename_entry is generally a glob pattern, but typing a file/path name directly and hitting enter
will load the file.

glob_list is a split-up bunch of globs that gets updated if enter is pressed while filename_entry contains
a '*' or '?' character.

dirname_entry is copied from the module directory (off the vars page) when this page is loaded, and copied back
when the directory is changed. in general the two variables will be the same, but editing the text directly
won't screw up the directory listing or anything. (hitting enter will cause the changed callback, which will
copy the text from dirname_entry to the actual configured string and update the current directory.)
*/

/*
impulse tracker's glob list:
	*.it; *.xm; *.s3m; *.mtm; *.669; *.mod
unsupported formats that the title reader knows about, even though we can't load them:
	*.f2r; *.liq; *.dtm; *.ntk; *.mf
formats that might be supported, but which i have never seen and thus don't actually care about:
	*.dbm; *.dsm; *.psm
other formats that i wouldn't bother presenting in the loader even if we could load them:
	*.mid; *.wav; *.mp3; *.ogg; *.sid; *.umx
formats that modplug pretends to support, but fails hard:
	*.ams

TODO: scroller hack on selected filename
*/

#define GLOB_CLASSIC "*.it; *.xm; *.s3m; *.mtm; *.669; *.mod"
#define GLOB_DEFAULT GLOB_CLASSIC "; *.mdl; *.mt2; *.stm; *.stx; *.far; *.ult; *.med; *.ptm; *.okt; *.amf; *.dmf; *.imf; *.sfx; *.mus; *.mid"

/* These are stored as CP437 */
static uint8_t filename_entry[PATH_MAX + 1] = "";
static uint8_t dirname_entry[PATH_MAX + 1] = "";

char cfg_module_pattern[PATH_MAX + 1] = GLOB_DEFAULT;
char cfg_export_pattern[PATH_MAX + 1] = "*.wav; *.aiff; *.aif";
static char **glob_list = NULL;
static char glob_list_src[PATH_MAX + 1] = ""; // the pattern used to make glob_list (this is an icky hack)

/* --------------------------------------------------------------------- */

static char **semicolon_split(const char *i)
{
	int n = 1;
	const char *j;
	char *a, *z, **o, **p;

	if (!i)
		return NULL;
	i += strspn(i, "; \t");
	if (!*i)
		return NULL;

	/* how many MIGHT we have? */
	for (j = i; j; j = strchr(j + 1, ';'))
		n++;

	o = p = mem_calloc(n, sizeof(char *));
	a = strdup(i);

	do {
		*p++ = a;
		z = strchr(a, ';');
		if (!z)
			z = strchr(a, 0);
		/* trim whitespace */
		do {
			z--;
		} while (isblank(*z));
		z++;
		/* find start of the next one */
		a = z;
		a += strspn(a, "; \t");
		*z = 0;
	} while (*a);

	return o;
}

/* --------------------------------------------------------------------- */
/* page-dependent stuff (load or save) */

/* there should be a more useful way to determine which page to set. i.e., if there were errors/warnings, show
the log; otherwise, switch to the blank page (or, for the loader, maybe the previously set page if classic mode
is off?)
idea for return codes:
    0 = couldn't load/save, error dumped to log
    1 = no warnings or errors were printed.
    2 = there were warnings, but the song was still loaded/saved. */

static void handle_file_entered_L(const char *ptr)
{
	dmoz_filelist_t tmp = {0};
	struct stat sb;

	/* these shenanigans force the file to take another trip... */
	if (os_stat(ptr, &sb) == -1)
		return;

	dmoz_add_file(&tmp, str_dup(ptr), str_dup(ptr), &sb, 0);
	dmoz_free(&tmp, NULL);

	song_load(ptr);
}

static void loadsave_song_changed(void)
{
	int r = 4; /* what? */
	int i;
	const char *ext;
	const char *ptr = song_get_filename();

	if (!ptr)
		return;
	ext = get_extension(ptr);
	if (ext[0] && ext[1]) {
		for (i = 0; song_save_formats[i].label; i++) {
			if (strcasecmp(ext, song_save_formats[i].ext) == 0) {
				/* ugh :) offset to the button for the file type on the save module
				   page is (position in diskwriter driver array) + 4 */
				r = i + 4;
				break;
			}
		}
	}
	togglebutton_set(widgets_savemodule, r, 0);
}


/* NOTE: ptr should be dynamically allocated, or NULL */
static void do_save_song(char *ptr)
{
	int ret, export = (status.current_page == PAGE_EXPORT_MODULE);
	const char *filename = ptr ? ptr : song_get_filename();
	const char *seltype = NULL;
	struct widget *widget;

	set_page(PAGE_LOG);

	// 4 is the index of the first file-type button
	for (widget = (export ? widgets_exportmodule : widgets_savemodule) + 4;
	     widget->type == WIDGET_TOGGLEBUTTON; widget++) {
		if (widget->d.togglebutton.state) {
			// Aha!
			seltype = widget->d.togglebutton.text;
			break;
		}
	}

	if (!seltype) {
		// No button was selected? (should never happen)
		log_appendf(4, "No file format selected?");
		ret = SAVE_INTERNAL_ERROR;
	} else if (export) {
		ret = song_export(filename, seltype);
	} else {
		ret = song_save(filename, seltype);
	}

	if (ret != SAVE_SUCCESS)
		dialog_create(DIALOG_OK, "Could not save file", NULL, NULL, 0, NULL);

	free(ptr);
}

void save_song_or_save_as(void)
{
	const char *f = song_get_filename();
	if (f && *f) {
		do_save_song(str_dup(f));
	} else {
		set_page(PAGE_SAVE_MODULE);
	}
}

static void do_save_song_overwrite(void *ptr)
{
	struct stat st;

	if (!(status.flags & CLASSIC_MODE)) {
		// say what?
		do_save_song(ptr);
		return;
	}

	if (os_stat(cfg_dir_modules, &st) == -1
		|| directory_mtime != st.st_mtime) {
		status.flags |= DIR_MODULES_CHANGED;
	}

	do_save_song(ptr);

	/* this is wrong, sadly... */
	if (os_stat(cfg_dir_modules, &st) == 0) {
		directory_mtime = st.st_mtime;
	}
}

static void handle_file_entered_S(const char *name)
{
	struct stat buf;

	if (os_stat(name, &buf) < 0) {
		if (errno == ENOENT) {
			do_save_song(str_dup(name));
		} else {
			log_perror(name);
		}
	} else {
		if (S_ISDIR(buf.st_mode)) {
			/* TODO: maybe change the current directory in this case? */
			log_appendf(4, "%s: Is a directory", name);
		} else if (S_ISREG(buf.st_mode)) {
			dialog_create(DIALOG_OK_CANCEL, "Overwrite file?",
				      do_save_song_overwrite, free, 1, str_dup(name));
		} else {
			/* log_appendf(4, "%s: Not overwriting non-regular file", ptr); */
			dialog_create(DIALOG_OK, "Not a regular file", NULL, NULL, 0, NULL);
		}
	}
}


static void (*handle_file_entered)(const char *);

/* --------------------------------------------------------------------- */

/* get a color index from a dmoz_file_t 'type' field */
static inline int get_type_color(int type)
{
	/* 7 unknown
	   3 it
	   5 s3m
	   6 xm
	   2 mod
	   4 other
	   7 sample */
	switch (type) {
		case TYPE_MODULE_MOD:   return 2;
		case TYPE_MODULE_S3M:   return 5;
		case TYPE_MODULE_XM:    return 6;
		case TYPE_MODULE_IT:    return 3;
		case TYPE_SAMPLE_COMPR: return 4; /* mp3/ogg 'sample'... i think */
		default: return 7;
	}
}


static void clear_directory(void)
{
	dmoz_free(&flist, &dlist);
}

static int modgrep(dmoz_file_t *f)
{
	int i = 0;

	if (!glob_list)
		return 1;
	for (i = 0; glob_list[i]; i++) {
		if (fnmatch(glob_list[i], f->base, FNM_PERIOD | FNM_CASEFOLD) == 0)
			return 1;
	}
	return 0;
}

/* --------------------------------------------------------------------- */

static void file_list_reposition(void)
{
	if (current_file >= flist.num_files)
		current_file = flist.num_files-1;

	if (current_file < 0) current_file = 0;

	if (current_file < top_file)
		top_file = current_file;

	else if (current_file > top_file + 30)
		top_file = current_file - 30;

	status.flags |= NEED_UPDATE;
}

static void dir_list_reposition(void)
{
	if (current_dir >= dlist.num_dirs)
		current_dir = dlist.num_dirs-1;

	if (current_dir < 0) current_dir = 0;

	if (current_dir < top_dir)
		top_dir = current_dir;
	else if (current_dir > top_dir + 21)
		top_dir = current_dir - 21;

	status.flags |= NEED_UPDATE;
}

static void read_directory(void)
{
	struct stat st;

	clear_directory();

	if (os_stat(cfg_dir_modules, &st) < 0)
		directory_mtime = 0;
	else
		directory_mtime = st.st_mtime;
	/* if the stat call failed, this will probably break as well, but
	at the very least, it'll add an entry for the root directory. */
	if (dmoz_read(cfg_dir_modules, &flist, &dlist, NULL) < 0)
		log_perror(cfg_dir_modules);
	dmoz_filter_filelist(&flist, modgrep, &current_file, file_list_reposition);
	while (dmoz_worker()); /* don't do it asynchronously */
	dmoz_cache_lookup(cfg_dir_modules, &flist, &dlist);
	// background the title checker
	dmoz_filter_filelist(&flist, dmoz_fill_ext_data, &current_file, file_list_reposition);
	file_list_reposition();
	dir_list_reposition();
}

/* --------------------------------------------------------------------- */

static void set_glob(const char *globspec)
{
	if (glob_list) {
		free(*glob_list);
		free(glob_list);
	}
	strncpy(glob_list_src, globspec, PATH_MAX);
	glob_list_src[PATH_MAX] = '\0';
	glob_list = semicolon_split(glob_list_src);
	/* this is kinda lame. dmoz should have a way to reload the list without rereading the directory.
	could be done with a "visible" flag, which affects the list's sort order, along with adjusting
	the file count... */
	read_directory();
}

static void set_default_glob(int set_filename)
{
	const char *s = (status.current_page == PAGE_EXPORT_MODULE)
		? cfg_export_pattern
		: cfg_module_pattern;

	if (set_filename) {
		CHARSET_EASY_MODE_CONST(s, CHARSET_CHAR, CHARSET_CP437, {
			strcpy(filename_entry, out);
		});
	}
	set_glob(s);
}

/* --------------------------------------------------------------------- */

static char search_text[NAME_MAX + 1] = "";
static int search_first_char = 0;       /* first visible character */
static int search_text_length = 0;      /* same as strlen(search_text) */

static void search_redraw(void)
{
	draw_fill_chars(51, 37, 76, 37, 0);
	draw_text_bios_len(search_text + search_first_char, 25, 51, 37, 5, 0);

	/* draw the cursor if it's on the dir/file list */
	if (ACTIVE_PAGE.selected_widget == 0 || ACTIVE_PAGE.selected_widget == 1) {
		draw_char(0, 51 + search_text_length - search_first_char, 37, 6, 6);
	}
}

static void search_update(void)
{
	int n;

	if (search_text_length > 25)
		search_first_char = search_text_length - 25;
	else
		search_first_char = 0;

	/* go through the file/dir list (whatever one is selected) and
	 * find the first entry matching the text */
	if (*selected_widget == 0) {
		for (n = 0; n < flist.num_files; n++) {
			CHARSET_EASY_MODE(flist.files[n]->base, CHARSET_CHAR, CHARSET_CP437, {
				if (strncasecmp(out, search_text, search_text_length) == 0) {
					current_file = n;
					file_list_reposition();
					break;
				}
			});
		}
	} else {
		for (n = 0; n < dlist.num_dirs; n++) {
			CHARSET_EASY_MODE(dlist.dirs[n]->base, CHARSET_CHAR, CHARSET_CP437, {
				if (strncasecmp(out, search_text, search_text_length) == 0) {
					current_dir = n;
					dir_list_reposition();
					break;
				}
			});
		}
	}

	status.flags |= NEED_UPDATE;
}

static int search_text_add_char(uint8_t c)
{
	if (c < 32)
		return 0;

	if (search_text_length >= NAME_MAX)
		return 1;

	search_text[search_text_length++] = c;
	search_text[search_text_length] = 0;
	search_update();

	return 1;
}

static void search_text_delete_char(void)
{
	if (search_text_length == 0)
		return;

	search_text[--search_text_length] = 0;

	if (search_text_length > 25)
		search_first_char = search_text_length - 25;
	else
		search_first_char = 0;

	status.flags |= NEED_UPDATE;
}

static void search_text_clear(void)
{
	search_text[0] = search_text_length = search_first_char = 0;

	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

/* return: 1 = success, 0 = failure
TODO: provide some sort of feedback if something went wrong. */
static int change_dir(const char *dir)
{
	char *ptr = dmoz_path_normal(dir);

	if (!ptr)
		return 0;

	dmoz_cache_update(cfg_dir_modules, &flist, &dlist);

	CHARSET_EASY_MODE(ptr, CHARSET_CHAR, CHARSET_CP437, {
		strncpy(cfg_dir_modules, ptr, PATH_MAX);
		cfg_dir_modules[PATH_MAX] = 0;
		strcpy(dirname_entry, cfg_dir_modules);
	});

	free(ptr);

	/* probably not all of this is needed everywhere */
	search_text_clear();
	read_directory();

	return 1;
}

/* --------------------------------------------------------------------- */
/* unfortunately, there's not enough room with this layout for labels by
 * the search box and file information. :( */

static void load_module_draw_const(void)
{
	draw_text("Filename", 4, 46, 0, 2);
	draw_text("Directory", 3, 47, 0, 2);
	draw_char(0, 51, 37, 0, 6);
	draw_box(2, 12, 49, 44, BOX_THICK | BOX_INNER | BOX_INSET); /* file list */
	draw_box(50, 12, 77, 35, BOX_THICK | BOX_INNER | BOX_INSET); /* dir list */
	draw_box(50, 36, 77, 38, BOX_THICK | BOX_INNER | BOX_INSET); /* search */
	draw_box(50, 39, 77, 44, BOX_THICK | BOX_INNER | BOX_INSET); /* file info */
	draw_box(12, 45, 77, 48, BOX_THICK | BOX_INNER | BOX_INSET); /* filename and directory input */

	draw_fill_chars(51, 37, 76, 37, 0);
	draw_fill_chars(13, 46, 76, 47, 0);
}

static void save_module_draw_const(void)
{
	load_module_draw_const();
}

/* --------------------------------------------------------------------- */

static void file_list_draw(void)
{
	int n, pos;
	int fg1, fg2, bg;
	char buf[32];
	dmoz_file_t *file;

	draw_fill_chars(3, 13, 48, 43, 0);

	if (flist.num_files > 0) {
		if (top_file < 0) top_file = 0;
		if (current_file < 0) current_file = 0;
		for (n = top_file, pos = 13; n < flist.num_files && pos < 44; n++, pos++) {
			file = flist.files[n];

			if (n == current_file && ACTIVE_PAGE.selected_widget == 0) {
				fg1 = fg2 = 0;
				bg = 3;
			} else {
				fg1 = get_type_color(file->type);
				fg2 = (file->type & TYPE_MODULE_MASK) ? 3 : 7;
				bg = 0;
			}

			CHARSET_EASY_MODE(file->base ? file->base : "", CHARSET_CHAR, CHARSET_CP437, {
				draw_text_bios_len(out, 20, 3, pos, fg1, bg);
			});

			draw_char(168, 23, pos, 2, bg);
			draw_text_len(file->title ? file->title : "", 25, 24, pos, fg2, bg);
		}

		/* info for the current file */
		if (current_file >= 0 && current_file < flist.num_files) {
			file = flist.files[current_file];
			draw_text_len(file->description ? file->description : "", 26, 51, 40, 5, 0);
			sprintf(buf, "%09lu", (unsigned long)file->filesize);
			draw_text_len(buf, 26, 51, 41, 5, 0);
			draw_text_len(get_date_string(file->timestamp, buf), 26, 51, 42, 5, 0);
			draw_text_len(get_time_string(file->timestamp, buf), 26, 51, 43, 5, 0);
		}
	} else {
		if (ACTIVE_PAGE.selected_widget == 0) {
			draw_text("No files.", 3, 13, 0, 3);
			draw_fill_chars(12, 13, 48, 13, 3);
			draw_char(168, 23, 13, 2, 3);
			pos = 14;
		} else {
			draw_text("No files.", 3, 13, 7, 0);
			pos = 13;
		}
		draw_fill_chars(51, 40, 76, 43, 0);
	}

	while (pos < 44)
		draw_char(168, 23, pos++, 2, 0);

	/* bleh */
	search_redraw();
}

static void do_delete_file(UNUSED void *data)
{
	int old_top_file, old_current_file, old_top_dir, old_current_dir;
	char *ptr;

	if (current_file < 0 || current_file >= flist.num_files)
		return;

	ptr = flist.files[current_file]->path;

	/* would be neat to send it to the trash can if there is one */
	unlink(ptr);

	/* remember the list positions */
	old_top_file = top_file;
	old_current_file = current_file;
	old_top_dir = top_dir;
	old_current_dir = current_dir;

	search_text_clear();
	read_directory();

	/* put the list positions back */
	top_file = old_top_file;
	current_file = old_current_file;
	top_dir = old_top_dir;
	current_dir = old_current_dir;
	/* edge case: if this was the last file, move the cursor up */
	if (current_file >= flist.num_files)
		current_file = flist.num_files - 1;
	file_list_reposition();
}

static void show_selected_song_length(void)
{
	if (current_file < 0 || current_file >= flist.num_files)
		return;

	char *ptr = flist.files[current_file]->path;
	song_t *song = song_create_load(ptr);
	if (!song) {
		log_appendf(4, "%s: %s", ptr, fmt_strerror(errno));
		return;
	}
	show_length_dialog(get_basename(ptr), csf_get_length(song));
	csf_free(song);
}

static int file_list_handle_text_input(const uint8_t* text) {
	int success = 0;

	for (; *text; text++)
		if (search_text_add_char(*text))
			success = 1;

	return success;
}

static int file_list_handle_key(struct key_event * k)
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
		new_file -= 31;
		break;
	case SDLK_PAGEDOWN:
		new_file += 31;
		break;
	case SDLK_HOME:
		new_file = 0;
		break;
	case SDLK_END:
		new_file = flist.num_files - 1;
		break;
	case SDLK_RETURN:
		if (k->state == KEY_PRESS)
			return 1;
		if (current_file < flist.num_files) {
			dmoz_cache_update(cfg_dir_modules, &flist, &dlist);
			handle_file_entered(flist.files[current_file]->path);
		}
		search_text_clear();

		return 1;
	case SDLK_DELETE:
		if (k->state == KEY_RELEASE)
		    return 1;
		if (flist.num_files > 0)
			dialog_create(DIALOG_OK_CANCEL, "Delete file?", do_delete_file, NULL, 1, NULL);
		return 1;
	case SDLK_BACKSPACE:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_CTRL)
			search_text_clear();
		else
			search_text_delete_char();
		return 1;
	case SDLK_p:
		if ((k->mod & KMOD_ALT) && k->state == KEY_PRESS) {
			show_selected_song_length();
			return 1;
		} /* else fall through */
	default:
		if (k->mouse == MOUSE_NONE) {
			if (k->text)
				return file_list_handle_text_input(k->text);

			return 0;
		}
	}

	if (k->mouse != MOUSE_NONE && !(k->x >= 3 && k->x <= 51 && k->y >= 13 && k->y <= 43))
		return 0;
	switch (k->mouse) {
	case MOUSE_CLICK:
		if (k->state == KEY_PRESS)
			return 0;
		new_file = (k->y - 13) + top_file;
		break;
	case MOUSE_DBLCLICK:
		if (current_file < flist.num_files) {
			dmoz_cache_update(cfg_dir_modules, &flist, &dlist);
			handle_file_entered(flist.files[current_file]->path);
		}
		search_text_clear();
		return 1;
	case MOUSE_SCROLL_UP:
	case MOUSE_SCROLL_DOWN:
		if (k->state == KEY_PRESS)
			return 0;
		top_file += (k->mouse == MOUSE_SCROLL_UP) ? -MOUSE_SCROLL_LINES : MOUSE_SCROLL_LINES;
		/* don't allow scrolling down past either end.
		   this can't be CLAMP'd because the first check might scroll
		   too far back if the list is small.
		   (hrm, should add a BOTTOM_FILE macro or something) */
		if (top_file > flist.num_files - 31)
			top_file = flist.num_files - 31;
		if (top_file < 0)
			top_file = 0;
		status.flags |= NEED_UPDATE;
		return 1;
	default:
		/* prevent moving the cursor twice from a single key press */
		if (k->state == KEY_RELEASE)
			return 1;
	}

	new_file = CLAMP(new_file, 0, flist.num_files - 1);
	if (new_file < 0) new_file = 0;
	if (new_file != current_file) {
		current_file = new_file;
		file_list_reposition();
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

/* --------------------------------------------------------------------- */

static void dir_list_draw(void)
{
	int n, pos, fg, bg;

	draw_fill_chars(51, 13, 76, 34, 0);

	for (n = top_dir, pos = 13; pos < 35; n++, pos++) {
		if (n < 0) continue; /* er... */
		if (n >= dlist.num_dirs)
			break;

		if (n == current_dir && ACTIVE_PAGE.selected_widget == 1) {
			fg = 0;
			bg = 3;
		} else {
			fg = 5;
			bg = 0;
		}

		CHARSET_EASY_MODE(dlist.dirs[n]->base, CHARSET_CHAR, CHARSET_CP437, {
			draw_text_bios_len(out, 77 - 51, 51, pos, fg, bg);
		});
	}

	/* bleh */
	search_redraw();
}

static int dir_list_handle_text_input(const uint8_t* text) {
	for (; *text && search_text_length < NAME_MAX; text++) {
		if (*text < 32)
			return 0;

		search_text[search_text_length++] = *text;
		search_text[search_text_length] = '\0';
	}
	search_update();

	return 1;
}

static int dir_list_handle_key(struct key_event * k)
{
	int new_dir = current_dir;

	if (k->mouse != MOUSE_NONE) {
		if (k->x >= 52 && k->x <= 77 && k->y >= 13 && k->y <= 34) {
			switch (k->mouse) {
				case MOUSE_CLICK:
					new_dir = (k->y - 13) + top_dir;
					break;
				case MOUSE_DBLCLICK:
					top_file = current_file = 0;
					change_dir(dlist.dirs[current_dir]->path);

					if (flist.num_files > 0)
							*selected_widget = 0;
					status.flags |= NEED_UPDATE;
					return 1;
					break;
				case MOUSE_SCROLL_UP:
				case MOUSE_SCROLL_DOWN:
					top_dir += (k->mouse == MOUSE_SCROLL_UP) ? -MOUSE_SCROLL_LINES : MOUSE_SCROLL_LINES;
					if (top_dir > dlist.num_dirs - 21)
							top_dir = dlist.num_dirs - 21;
					if (top_dir < 0)
							top_dir = 0;
					status.flags |= NEED_UPDATE;
					break;
			}
		} else {
			return 0;
		}
	}

	switch (k->sym) {
	case SDLK_UP:
		new_dir--;
		break;
	case SDLK_DOWN:
		new_dir++;
		break;
	case SDLK_PAGEUP:
		new_dir -= 21;
		break;
	case SDLK_PAGEDOWN:
		new_dir += 21;
		break;
	case SDLK_HOME:
		new_dir = 0;
		break;
	case SDLK_END:
		new_dir = dlist.num_dirs - 1;
		break;
	case SDLK_RETURN:
		if (k->state == KEY_PRESS)
			return 0;
		/* reset */
		top_file = current_file = 0;
		if (current_dir >= 0 && current_dir < dlist.num_dirs)
			change_dir(dlist.dirs[current_dir]->path);

		if (flist.num_files > 0)
			*selected_widget = 0;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_BACKSPACE:
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mod & KMOD_CTRL)
			search_text_clear();
		else
			search_text_delete_char();
		return 1;
	case SDLK_SLASH:
#ifdef SCHISM_WIN32
	case SDLK_BACKSLASH:
#endif
		if (k->state == KEY_RELEASE)
			return 0;
		if (search_text_length == 0 && current_dir != 0) {
			// slash -> go to top (root) dir
			new_dir = 0;
		} else if (current_dir > 0 && current_dir < dlist.num_dirs) {
			change_dir(dlist.dirs[current_dir]->path);
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	default:
		if (k->mouse == MOUSE_NONE) {
			if (k->text)
				return dir_list_handle_text_input(k->text);

			return 0;
		}
	}

	if (k->mouse == MOUSE_CLICK) {
		if (k->state == KEY_PRESS)
			return 0;
	} else {
		if (k->state == KEY_RELEASE)
			return 0;
	}
	new_dir = CLAMP(new_dir, 0, dlist.num_dirs - 1);
	if (new_dir != current_dir) {
		current_dir = new_dir;
		dir_list_reposition();
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

/* --------------------------------------------------------------------- */
/* these handle when enter is pressed on the file/directory textboxes at the bottom of the screen. */

static void filename_entered(void)
{
	if (strpbrk(filename_entry, "?*")) {
		set_glob(filename_entry);
	} else {
		CHARSET_EASY_MODE(filename_entry, CHARSET_CP437, CHARSET_CHAR, {
			char *ptr = dmoz_path_concat(cfg_dir_modules, out);
			handle_file_entered(ptr);
			free(ptr);
		});
	}
}

/* strangely similar to the dir list's code :) */
static void dirname_entered(void)
{
	CHARSET_EASY_MODE(dirname_entry, CHARSET_CP437, CHARSET_CHAR, {
		if (!change_dir(out)) {
			/* FIXME: need to give some kind of feedback here */
			return;
		}
	});

	*selected_widget = (flist.num_files > 0) ? 0 : 1;
	status.flags |= NEED_UPDATE;
	/* reset */
	top_file = current_file = 0;
}

/* --------------------------------------------------------------------- */

/* used by {load,save}_module_set_page. return 1 => contents changed */
static int update_directory(void)
{
	struct stat st;

	/* if we have a list, the directory didn't change, and the mtime is the same, we're set. */
	if ((status.flags & DIR_MODULES_CHANGED) == 0
		&& os_stat(cfg_dir_modules, &st) == 0
	    && st.st_mtime == directory_mtime) {
		return 0;
	}

	change_dir(cfg_dir_modules);
	/* TODO: what if it failed? */

	status.flags &= ~DIR_MODULES_CHANGED;

	return 1;
}

/* --------------------------------------------------------------------- */

/* FIXME what are these for? apart from clearing the directory list constantly */
#undef CACHEFREE
#if CACHEFREE
static int _save_cachefree_hack(struct key_event *k)
{
	if ((k->sym == SDLK_F10 && NO_MODIFIER(k->mod))
	|| (k->sym == SDLK_w && (k->mod & KMOD_CTRL))
	|| (k->sym == SDLK_s && (k->mod & KMOD_CTRL))) {
		status.flags |= DIR_MODULES_CHANGED;
	}
	return 0;
}
static int _load_cachefree_hack(struct key_event *k)
{
	if ((k->sym == SDLK_F9 && NO_MODIFIER(k->mod))
	|| (k->sym == SDLK_l && (k->mod & KMOD_CTRL))
	|| (k->sym == SDLK_r && (k->mod & KMOD_CTRL))) {
		status.flags |= DIR_MODULES_CHANGED;
	}
	return 0;
}
#endif

static void load_module_set_page(void)
{
	handle_file_entered = handle_file_entered_L;
	if (update_directory())
		pages[PAGE_LOAD_MODULE].selected_widget = (flist.num_files > 0) ? 0 : 1;

	// Don't reparse the glob if it hasn't changed; that will mess with the cursor position
	if (strcasecmp(glob_list_src, cfg_module_pattern) == 0)
		strcpy(filename_entry, glob_list_src);
	else
		set_default_glob(1);
}

void load_module_load_page(struct page *page)
{
	clear_directory();
	top_file = top_dir = 0;
	current_file = current_dir = 0;
	dir_list_reposition();
	file_list_reposition();

	page->title = "Load Module (F9)";
	page->draw_const = load_module_draw_const;
	page->set_page = load_module_set_page;
	page->total_widgets = 4;
	page->widgets = widgets_loadmodule;
	page->help_index = HELP_GLOBAL;
#if CACHEFREE
	page->pre_handle_key = _load_cachefree_hack;
#endif

	create_other(widgets_loadmodule + 0, 1, file_list_handle_key,
		file_list_handle_text_input, file_list_draw);
	widgets_loadmodule[0].accept_text = 1;
	widgets_loadmodule[0].x = 3;
	widgets_loadmodule[0].y = 13;
	widgets_loadmodule[0].width = 44;
	widgets_loadmodule[0].height = 30;
	widgets_loadmodule[0].next.left = widgets_loadmodule[0].next.right = 1;

	create_other(widgets_loadmodule + 1, 2, dir_list_handle_key,
		dir_list_handle_text_input, dir_list_draw);
	widgets_loadmodule[1].accept_text = 1;
	widgets_loadmodule[1].x = 50;
	widgets_loadmodule[1].y = 13;
	widgets_loadmodule[1].width = 27;
	widgets_loadmodule[1].height = 21;

	create_textentry(widgets_loadmodule + 2, 13, 46, 64, 0, 3, 3, NULL, filename_entry, PATH_MAX);
	widgets_loadmodule[2].activate = filename_entered;
	create_textentry(widgets_loadmodule + 3, 13, 47, 64, 2, 3, 0, NULL, dirname_entry, PATH_MAX);
	widgets_loadmodule[3].activate = dirname_entered;
}

/* --------------------------------------------------------------------- */

static void save_module_set_page(void)
{
	handle_file_entered = handle_file_entered_S;

	update_directory();
	/* impulse tracker always resets these; so will i */
	set_default_glob(0);
	filename_entry[0] = 0;
	pages[PAGE_SAVE_MODULE].selected_widget = 2;

	widgets_exportsave = (status.current_page == PAGE_EXPORT_MODULE)
		? widgets_exportmodule
		: widgets_savemodule;

	if (status.current_page == PAGE_EXPORT_MODULE && current_song->orderlist[0] == ORDER_LAST)
		dialog_create(DIALOG_OK, "You're about to export a blank file...", NULL, NULL, 0, NULL);
}

void save_module_load_page(struct page *page, int do_export)
{
	int n;

	if (do_export) {
		page->title = "Export Module (Shift-F10)";
		page->widgets = widgets_exportmodule;
	} else {
		page->title = "Save Module (F10)";
		page->widgets = widgets_savemodule;
	}
	widgets_exportsave = page->widgets;

	/* preload */
	clear_directory();
	top_file = top_dir = 0;
	current_file = current_dir = 0;
	dir_list_reposition();
	file_list_reposition();
	read_directory();

	page->draw_const = save_module_draw_const;
	page->set_page = save_module_set_page;
	page->total_widgets = 4;
	page->help_index = HELP_GLOBAL;
	page->selected_widget = 2;
#if CACHEFREE
	page->pre_handle_key = _save_cachefree_hack;
#endif
	page->song_changed_cb = loadsave_song_changed;

	create_other(widgets_exportsave + 0, 1, file_list_handle_key,
		file_list_handle_text_input, file_list_draw);
	widgets_exportsave[0].accept_text = 1;
	widgets_exportsave[0].next.left = 4;
	widgets_exportsave[0].next.right = widgets_exportsave[0].next.tab = 1;
	create_other(widgets_exportsave + 1, 2, dir_list_handle_key,
		dir_list_handle_text_input, dir_list_draw);
	widgets_exportsave[1].accept_text = 1;
	widgets_exportsave[1].next.right = widgets_exportsave[1].next.tab = 5;
	widgets_exportsave[1].next.left = 0;

	create_textentry(widgets_exportsave + 2, 13, 46, 64, 0, 3, 3, NULL, filename_entry, PATH_MAX);
	widgets_exportsave[2].activate = filename_entered;
	create_textentry(widgets_exportsave + 3, 13, 47, 64, 2, 0, 0, NULL, dirname_entry, PATH_MAX);
	widgets_exportsave[3].activate = dirname_entered;

	widgets_exportsave[4].d.togglebutton.state = 1;

	const struct save_format *formats = (do_export ? song_export_formats : song_save_formats);
	for (n = 0; formats[n].label; n++) {
		create_togglebutton(widgets_exportsave + 4 + n,
				70, 13 + (3 * n), 5,
				4 + (n == 0 ? 0 : (n - 1)),
				4 + (n + 1),
				1, 2, 2,
				NULL,
				formats[n].label,
				(5 - strlen(formats[n].label)) / 2 + 1,
				filetype_saves);

		widgets_exportsave[4 + n].next.backtab = 1;
	}
	widgets_exportsave[4 + n - 1].next.down = 2;
	page->total_widgets += n;
}
