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

/* This page is modeled on the save-module page (page_loadmodule.c): a file
   list on the left, a directory list on the upper right, and filename/directory
   text entries along the bottom. The save page's column of output-format
   buttons (at x=70) is replaced here by two "tab" buttons that switch the main
   list box between the directory browser ("Dirs") and the current playlist
   queue ("List"). The queue itself and auto-advance live in playlist.c. */

#include "headers.h"

#include "it.h"
#include "config.h"
#include "charset.h"
#include "song.h"
#include "page.h"
#include "dmoz.h"
#include "log.h"
#include "widget.h"
#include "dialog.h"
#include "vgamem.h"
#include "osdefs.h"
#include "mem.h"
#include "str.h"
#include "playlist.h"

/* --------------------------------------------------------------------- */

enum { VIEW_DIRS = 0, VIEW_LIST = 1 };

/* 0 = file list, 1 = dir list, 2 = filename entry, 3 = dirname entry,
   4 = "Dirs" tab, 5 = "List" tab */
static struct widget widgets_playlist[6];
static const int tab_group[] = { 4, 5, -1 };
static int current_view = VIEW_DIRS;

static int top_file = 0, top_dir = 0;
static time_t directory_mtime;
static dmoz_filelist_t flist;
static dmoz_dirlist_t dlist;
#define current_file flist.selected
#define current_dir dlist.selected

/* queue cursor / scroll */
static int top_q = 0, cur_q = 0;

/* stored as CP437 */
static char filename_entry[SCHISM_PATH_MAX + 1] = {0};
static char dirname_entry[SCHISM_PATH_MAX + 1] = {0};

/* --------------------------------------------------------------------- */
/* file type colors (from page_loadmodule.c) */

static inline int get_type_color(int type)
{
	switch (type) {
		case TYPE_MODULE_MOD:   return 2;
		case TYPE_MODULE_S3M:   return 5;
		case TYPE_MODULE_XM:    return 6;
		case TYPE_MODULE_IT:    return 3;
		case TYPE_SAMPLE_COMPR: return 4;
		default: return 7;
	}
}

static void clear_directory(void)
{
	dmoz_free(&flist, &dlist);
}

/* keep everything dmoz can read metadata for; unloadable picks are simply
   skipped (and logged) by the engine at play time */
static int pl_grep(dmoz_file_t *f)
{
	return dmoz_fill_ext_data(f);
}

/* --------------------------------------------------------------------- */

static void file_list_reposition(void)
{
	if (current_file >= flist.num_files)
		current_file = flist.num_files - 1;
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
		current_dir = dlist.num_dirs - 1;
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

	directory_mtime = (os_stat(cfg_dir_modules, &st) < 0) ? 0 : st.st_mtime;

	if (dmoz_read(cfg_dir_modules, &flist, &dlist, NULL) < 0)
		log_perror(cfg_dir_modules);
	dmoz_filter_filelist(&flist, pl_grep, &current_file, file_list_reposition);
	dmoz_cache_lookup(cfg_dir_modules, &flist, &dlist);
	file_list_reposition();
	dir_list_reposition();
}

/* --------------------------------------------------------------------- */
/* incremental search (from page_loadmodule.c) */

static uint32_t search_text[SCHISM_NAME_MAX + 1] = {0};
static int search_first_char = 0;
static int search_text_length = 0;

static void search_redraw(void)
{
	draw_fill_chars(51, 37, 76, 37, DEFAULT_FG, 0);
	draw_text_charset_len(search_text + search_first_char, CHARSET_UCS4, 25, 51, 37, 5, 0);

	if (ACTIVE_PAGE.selected_widget == 0 || ACTIVE_PAGE.selected_widget == 1)
		draw_char(0, 51 + search_text_length - search_first_char, 37, 0, 6);
}

static void search_update(void)
{
	int n;

	search_first_char = (search_text_length > 25) ? (search_text_length - 25) : 0;

	if (*selected_widget == 0) {
		for (n = 0; n < flist.num_files; n++) {
			if (charset_strncasecmp(flist.files[n]->base, CHARSET_CHAR,
					search_text, CHARSET_UCS4, search_text_length) == 0) {
				current_file = n;
				file_list_reposition();
				break;
			}
		}
	} else {
		for (n = 0; n < dlist.num_dirs; n++) {
			if (charset_strncasecmp(dlist.dirs[n]->base, CHARSET_CHAR,
					search_text, CHARSET_UCS4, search_text_length) == 0) {
				current_dir = n;
				dir_list_reposition();
				break;
			}
		}
	}

	status.flags |= NEED_UPDATE;
}

static int search_text_add_char(uint32_t c)
{
	if (c < 32)
		return 0;
	if (search_text_length + 1 >= (int)ARRAY_SIZE(search_text))
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
	search_first_char = (search_text_length > 25) ? (search_text_length - 25) : 0;
	status.flags |= NEED_UPDATE;
}

static void search_text_clear(void)
{
	search_text[0] = search_text_length = search_first_char = 0;
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static int change_dir(const char *dir)
{
	char *ptr = dmoz_path_normal(dir);

	if (!ptr)
		return 0;

	dmoz_cache_update(cfg_dir_modules, &flist, &dlist);

	strncpy(cfg_dir_modules, ptr, ARRAY_SIZE(cfg_dir_modules) - 1);
	cfg_dir_modules[ARRAY_SIZE(cfg_dir_modules) - 1] = 0;

	void *out = charset_iconv_easy(ptr, CHARSET_CHAR, CHARSET_CP437);
	if (out) {
		strncpy(dirname_entry, out, ARRAY_SIZE(dirname_entry) - 1);
		free(out);
	}

	free(ptr);

	search_text_clear();
	read_directory();

	return 1;
}

/* enqueue a module path */
static void enqueue_file(const char *path)
{
	playlist_add(path);
	status_text_flash("Added %s", dmoz_path_get_basename(path));
}

/* --------------------------------------------------------------------- */
/* drawing (geometry copied from page_loadmodule.c's save layout) */

static void playlist_draw_const(void)
{
	draw_text("Filename", 4, 46, 0, 2);
	draw_text("Directory", 3, 47, 0, 2);
	draw_char(0, 51, 37, 0, 6);
	draw_box(2, 12, 49, 44, BOX_THICK | BOX_INNER | BOX_INSET);  /* file/queue list */
	draw_box(50, 36, 77, 38, BOX_THICK | BOX_INNER | BOX_INSET); /* search */
	draw_box(50, 39, 77, 44, BOX_THICK | BOX_INNER | BOX_INSET); /* file info */
	draw_box(12, 45, 77, 48, BOX_THICK | BOX_INNER | BOX_INSET); /* filename + dir input */
	draw_fill_chars(13, 46, 76, 47, DEFAULT_FG, 0);

	/* dir list (narrowed to leave room for the tab buttons at x=70) */
	draw_box(50, 12, 68, 35, BOX_THICK | BOX_INNER | BOX_INSET);
	draw_fill_chars(51, 37, 67, 37, DEFAULT_FG, 0);
}

/* --------------------------------------------------------------------- */
/* the playlist (queue) view -- shown in the side column in place of the dir
   list. geometry matches the dir list box: cols 51..67, rows 13..34 (22 rows) */

static void queue_reposition(void)
{
	int count = playlist_count();

	if (cur_q >= count) cur_q = count - 1;
	if (cur_q < 0) cur_q = 0;
	if (cur_q < top_q)
		top_q = cur_q;
	else if (cur_q > top_q + 21)
		top_q = cur_q - 21;
	if (top_q < 0) top_q = 0;
	status.flags |= NEED_UPDATE;
}

static void queue_draw(void)
{
	int n, pos, count = playlist_count();
	int nowplaying = playlist_current();
	int focused = (ACTIVE_PAGE.selected_widget == 1);

	draw_fill_chars(51, 13, 67, 34, DEFAULT_FG, 0);

	if (count == 0) {
		draw_text("(empty)", 51, 13, 7, 0);
		search_redraw();
		return;
	}

	for (n = top_q, pos = 13; n < count && pos < 35; n++, pos++) {
		int fg, bg;

		if (n == cur_q && focused) {
			fg = 0;
			bg = 3;
		} else if (n == nowplaying) {
			fg = 3;
			bg = 0;
		} else {
			fg = 6;
			bg = 0;
		}

		draw_char((n == nowplaying) ? 16 : ' ', 51, pos, fg, bg); /* 16 = play triangle */
		draw_text_utf8_len(playlist_basename(n), 16, 52, pos, fg, bg);
	}

	search_redraw();
}

static int queue_handle_key(struct key_event *k)
{
	int count = playlist_count();
	int newpos = cur_q;

	switch (k->sym) {
	case SCHISM_KEYSYM_UP:
		if (k->mod & SCHISM_KEYMOD_SHIFT) {
			if (k->state == KEY_RELEASE) return 1;
			if (cur_q > 0) { playlist_move(cur_q, cur_q - 1); cur_q--; queue_reposition(); }
			return 1;
		}
		newpos--;
		break;
	case SCHISM_KEYSYM_DOWN:
		if (k->mod & SCHISM_KEYMOD_SHIFT) {
			if (k->state == KEY_RELEASE) return 1;
			if (cur_q < count - 1) { playlist_move(cur_q, cur_q + 1); cur_q++; queue_reposition(); }
			return 1;
		}
		newpos++;
		break;
	case SCHISM_KEYSYM_PAGEUP:   newpos -= 22; break;
	case SCHISM_KEYSYM_PAGEDOWN: newpos += 22; break;
	case SCHISM_KEYSYM_HOME:     newpos = 0; break;
	case SCHISM_KEYSYM_END:      newpos = count - 1; break;
	case SCHISM_KEYSYM_RETURN:
		if (k->state == KEY_PRESS) return 1;
		if (count > 0) playlist_start(cur_q);
		return 1;
	case SCHISM_KEYSYM_DELETE:
		if (k->state == KEY_RELEASE) return 1;
		if (count > 0) { playlist_remove(cur_q); queue_reposition(); }
		return 1;
	default:
		if (k->mouse == MOUSE_NONE)
			return 0;
	}

	/* mouse on the side column */
	if (k->mouse != MOUSE_NONE) {
		if (!(k->x >= 51 && k->x <= 67 && k->y >= 13 && k->y <= 34))
			return 0;
		switch (k->mouse) {
		case MOUSE_CLICK:
			if (k->state != KEY_PRESS) return 0;
			newpos = (k->y - 13) + top_q;
			break;
		case MOUSE_DBLCLICK:
			if (count > 0) playlist_start(cur_q);
			return 1;
		case MOUSE_SCROLL_UP:
		case MOUSE_SCROLL_DOWN:
			if (k->state == KEY_PRESS) return 0;
			top_q += (k->mouse == MOUSE_SCROLL_UP) ? -MOUSE_SCROLL_LINES : MOUSE_SCROLL_LINES;
			if (top_q > count - 22) top_q = count - 22;
			if (top_q < 0) top_q = 0;
			status.flags |= NEED_UPDATE;
			return 1;
		default:
			if (k->state == KEY_RELEASE) return 1;
		}
	} else if (k->state == KEY_RELEASE) {
		return 1;
	}

	if (count > 0) {
		newpos = CLAMP(newpos, 0, count - 1);
		if (newpos != cur_q) { cur_q = newpos; queue_reposition(); }
	}
	return 1;
}

/* --------------------------------------------------------------------- */
/* file list (browser) -- the main box switches to the queue in VIEW_LIST */

static void file_list_draw(void)
{
	int n, pos;
	int fg1, fg2, bg;
	char buf[32];
	dmoz_file_t *file;

	draw_fill_chars(3, 13, 48, 43, DEFAULT_FG, 0);

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

			draw_text_utf8_len(file->base ? file->base : "", 20, 3, pos, fg1, bg);
			draw_char(168, 23, pos, 2, bg);
			draw_text_len(file->title ? file->title : "", 25, 24, pos, fg2, bg);
		}

		if (current_file >= 0 && current_file < flist.num_files) {
			file = flist.files[current_file];
			draw_text_len(file->description ? file->description : "", 26, 51, 40, 5, 0);
			snprintf(buf, sizeof(buf), "%09lu", (unsigned long)file->filesize);
			draw_text_len(buf, 26, 51, 41, 5, 0);
			draw_text_len(str_from_date(file->timestamp, buf, cfg_str_date_format), 26, 51, 42, 5, 0);
			draw_text_len(str_from_time(file->timestamp, buf, cfg_str_time_format), 26, 51, 43, 5, 0);
		}
	} else {
		if (ACTIVE_PAGE.selected_widget == 0) {
			draw_text("No files.", 3, 13, 0, 3);
			draw_fill_chars(12, 13, 48, 13, DEFAULT_FG, 3);
			draw_char(168, 23, 13, 2, 3);
			pos = 14;
		} else {
			draw_text("No files.", 3, 13, 7, 0);
			pos = 13;
		}
		draw_fill_chars(51, 40, 76, 43, DEFAULT_FG, 0);
	}

	while (pos < 44)
		draw_char(168, 23, pos++, 2, 0);

	search_redraw();
}

static int file_list_handle_text_input(const char *text)
{
	int success = 0;
	uint32_t *ucs4;
	size_t i;

	ucs4 = charset_iconv_easy(text, CHARSET_UTF8, CHARSET_UCS4);
	if (!ucs4)
		return 0;

	for (i = 0; ucs4[i]; i++)
		if (search_text_add_char(ucs4[i]))
			success = 1;

	free(ucs4);
	return success;
}

static int file_list_handle_key(struct key_event *k)
{
	int new_file = current_file;

	switch (k->sym) {
	case SCHISM_KEYSYM_UP:        new_file--; break;
	case SCHISM_KEYSYM_DOWN:      new_file++; break;
	case SCHISM_KEYSYM_PAGEUP:    new_file -= 31; break;
	case SCHISM_KEYSYM_PAGEDOWN:  new_file += 31; break;
	case SCHISM_KEYSYM_HOME:      new_file = 0; break;
	case SCHISM_KEYSYM_END:       new_file = flist.num_files - 1; break;
	case SCHISM_KEYSYM_RETURN:
		if (k->state == KEY_PRESS)
			return 1;
		if (current_file >= 0 && current_file < flist.num_files) {
			dmoz_cache_update(cfg_dir_modules, &flist, &dlist);
			enqueue_file(flist.files[current_file]->path);
		}
		search_text_clear();
		return 1;
	case SCHISM_KEYSYM_BACKSPACE:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & SCHISM_KEYMOD_CTRL)
			search_text_clear();
		else
			search_text_delete_char();
		return 1;
	default:
		if (k->mouse == MOUSE_NONE) {
			if (k->text)
				return file_list_handle_text_input(k->text);
			return 0;
		}
	}

	struct widget *w = &widgets_playlist[0];

	if (k->mouse != MOUSE_NONE && !(k->x >= w->x && k->x <= w->x + w->width && k->y >= w->y && k->y <= w->y + w->height))
		return 0;
	switch (k->mouse) {
	case MOUSE_CLICK:
		if (k->state != KEY_PRESS)
			return 0;
		new_file = (k->y - 13) + top_file;
		break;
	case MOUSE_DBLCLICK:
		if (current_file >= 0 && current_file < flist.num_files) {
			dmoz_cache_update(cfg_dir_modules, &flist, &dlist);
			enqueue_file(flist.files[current_file]->path);
		}
		search_text_clear();
		return 1;
	case MOUSE_SCROLL_UP:
	case MOUSE_SCROLL_DOWN:
		if (k->state == KEY_PRESS)
			return 0;
		top_file += (k->mouse == MOUSE_SCROLL_UP) ? -MOUSE_SCROLL_LINES : MOUSE_SCROLL_LINES;
		if (top_file > flist.num_files - 31)
			top_file = flist.num_files - 31;
		if (top_file < 0)
			top_file = 0;
		status.flags |= NEED_UPDATE;
		return 1;
	default:
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
/* directory list */

static void dir_list_draw(void)
{
	int n, pos, fg, bg;
	int width = 68 - 51;

	if (current_view == VIEW_LIST) {
		queue_draw();
		return;
	}

	draw_fill_chars(51, 13, 51 + width - 1, 34, DEFAULT_FG, 0);

	for (n = top_dir, pos = 13; pos < 35; n++, pos++) {
		if (n < 0) continue;
		if (n >= dlist.num_dirs)
			break;

		if (n == current_dir && ACTIVE_PAGE.selected_widget == 1) {
			fg = 0;
			bg = 3;
		} else {
			fg = 5;
			bg = 0;
		}

		draw_text_utf8_len(dlist.dirs[n]->base, width, 51, pos, fg, bg);
	}

	search_redraw();
}

static int dir_list_handle_text_input(const char *text)
{
	if (current_view == VIEW_LIST)
		return 0; /* no incremental search over the queue */
	return file_list_handle_text_input(text);
}

static int dir_list_handle_key(struct key_event *k)
{
	int new_dir = current_dir;
	int width = 68 - 51;

	if (current_view == VIEW_LIST)
		return queue_handle_key(k);

	if (k->mouse != MOUSE_NONE) {
		if (k->x >= 51 && k->x <= (51 + width - 1) && k->y >= 13 && k->y <= 34) {
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
			case MOUSE_SCROLL_UP:
			case MOUSE_SCROLL_DOWN:
				top_dir += (k->mouse == MOUSE_SCROLL_UP) ? -MOUSE_SCROLL_LINES : MOUSE_SCROLL_LINES;
				if (top_dir > dlist.num_dirs - 21)
					top_dir = dlist.num_dirs - 21;
				if (top_dir < 0)
					top_dir = 0;
				status.flags |= NEED_UPDATE;
				break;
			default:
				break;
			}
		} else {
			return 0;
		}
	}

	switch (k->sym) {
	case SCHISM_KEYSYM_UP:        new_dir--; break;
	case SCHISM_KEYSYM_DOWN:      new_dir++; break;
	case SCHISM_KEYSYM_PAGEUP:    new_dir -= 21; break;
	case SCHISM_KEYSYM_PAGEDOWN:  new_dir += 21; break;
	case SCHISM_KEYSYM_HOME:      new_dir = 0; break;
	case SCHISM_KEYSYM_END:       new_dir = dlist.num_dirs - 1; break;
	case SCHISM_KEYSYM_RETURN:
		if (k->state == KEY_PRESS)
			return 0;
		top_file = current_file = 0;
		if (current_dir >= 0 && current_dir < dlist.num_dirs)
			change_dir(dlist.dirs[current_dir]->path);
		if (flist.num_files > 0)
			*selected_widget = 0;
		status.flags |= NEED_UPDATE;
		return 1;
	case SCHISM_KEYSYM_BACKSPACE:
		if (k->state == KEY_RELEASE)
			return 0;
		if (k->mod & SCHISM_KEYMOD_CTRL)
			search_text_clear();
		else
			search_text_delete_char();
		return 1;
	case SCHISM_KEYSYM_SLASH:
#ifdef SCHISM_WIN32
	case SCHISM_KEYSYM_BACKSLASH:
#endif
		if (k->state == KEY_RELEASE)
			return 0;
		if (search_text_length == 0 && current_dir != 0) {
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

	if (k->state != KEY_PRESS)
		return 0;
	new_dir = CLAMP(new_dir, 0, dlist.num_dirs - 1);
	if (new_dir != current_dir) {
		current_dir = new_dir;
		dir_list_reposition();
		status.flags |= NEED_UPDATE;
	}
	return 1;
}

/* --------------------------------------------------------------------- */
/* the bottom text entries */

static void filename_entered(void)
{
	void *fn = charset_iconv_easy(filename_entry, CHARSET_CP437, CHARSET_CHAR);
	char *ptr = dmoz_path_concat(cfg_dir_modules, fn);
	struct stat sb;

	free(fn);
	if (!ptr)
		return;

	if (os_stat(ptr, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		change_dir(ptr);
		*selected_widget = (flist.num_files > 0) ? 0 : 1;
	} else {
		enqueue_file(ptr);
	}
	free(ptr);
	filename_entry[0] = 0;
	status.flags |= NEED_UPDATE;
}

static void dirname_entered(void)
{
	void *out = charset_iconv_easy(dirname_entry, CHARSET_CP437, CHARSET_CHAR);
	if (!out)
		return;

	if (!change_dir(out)) {
		free(out);
		return;
	}
	free(out);

	*selected_widget = (flist.num_files > 0) ? 0 : 1;
	status.flags |= NEED_UPDATE;
	top_file = current_file = 0;
}

/* --------------------------------------------------------------------- */
/* tabs */

static void tab_changed(void)
{
	current_view = widgets_playlist[5].d.togglebutton.state ? VIEW_LIST : VIEW_DIRS;

	if (current_view == VIEW_LIST)
		queue_reposition();

	/* focus the side column so the toggled view is immediately navigable */
	pages[PAGE_PLAYLIST].selected_widget = 1;
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static int update_directory(void)
{
	struct stat st;

	if ((status.flags & DIR_MODULES_CHANGED) == 0
		&& os_stat(cfg_dir_modules, &st) == 0
		&& st.st_mtime == directory_mtime) {
		return 0;
	}

	change_dir(cfg_dir_modules);
	status.flags &= ~DIR_MODULES_CHANGED;
	return 1;
}

static void playlist_set_page(void)
{
	if (update_directory())
		pages[PAGE_PLAYLIST].selected_widget = (flist.num_files > 0) ? 0 : 1;
	if (current_view == VIEW_LIST)
		queue_reposition();
}

static void playlist_song_mode_changed_cb(void)
{
	if (current_view == VIEW_LIST)
		status.flags |= NEED_UPDATE;
}

void playlist_load_page(struct page *page)
{
	clear_directory();
	top_file = top_dir = 0;
	current_file = current_dir = 0;
	dir_list_reposition();
	file_list_reposition();

	page->title = "Playlist (Shift-F11)";
	page->draw_const = playlist_draw_const;
	page->set_page = playlist_set_page;
	page->song_mode_changed_cb = playlist_song_mode_changed_cb;
	page->total_widgets = 6;
	page->widgets = widgets_playlist;
	page->help_index = HELP_GLOBAL;

	/* file/queue list */
	widget_create_other(widgets_playlist + 0, 1, file_list_handle_key,
		file_list_handle_text_input, file_list_draw);
	widgets_playlist[0].accept_text = 1;
	widgets_playlist[0].x = 3;
	widgets_playlist[0].y = 13;
	widgets_playlist[0].width = 45;
	widgets_playlist[0].height = 30;
	widgets_playlist[0].next.left = 4;
	widgets_playlist[0].next.right = widgets_playlist[0].next.tab = 1;

	/* dir list */
	widget_create_other(widgets_playlist + 1, 2, dir_list_handle_key,
		dir_list_handle_text_input, dir_list_draw);
	widgets_playlist[1].accept_text = 1;
	widgets_playlist[1].x = 50;
	widgets_playlist[1].y = 13;
	widgets_playlist[1].width = 18;
	widgets_playlist[1].height = 22;
	widgets_playlist[1].next.left = 0;
	widgets_playlist[1].next.right = widgets_playlist[1].next.tab = 4;

	/* filename / directory text entries */
	widget_create_textentry(widgets_playlist + 2, 13, 46, 64, 0, 3, 3, NULL, filename_entry, ARRAY_SIZE(filename_entry) - 1);
	widgets_playlist[2].activate = filename_entered;
	widget_create_textentry(widgets_playlist + 3, 13, 47, 64, 2, 0, 0, NULL, dirname_entry, ARRAY_SIZE(dirname_entry) - 1);
	widgets_playlist[3].activate = dirname_entered;

	/* tab buttons (in place of the save page's output-format buttons) */
	widget_create_togglebutton(widgets_playlist + 4, 70, 13, 5,
		4, 5, 0, 4, 5, tab_changed, "Dirs", 1, tab_group);
	widget_create_togglebutton(widgets_playlist + 5, 70, 16, 5,
		4, 5, 0, 5, 2, tab_changed, "List", 1, tab_group);
	widgets_playlist[4].d.togglebutton.state = 1; /* Dirs active by default */
	widgets_playlist[5].next.backtab = 0;
}
