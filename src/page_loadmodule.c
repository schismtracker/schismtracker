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

/* --------------------------------------------------------------------- */
/* the locals */

static const int type_colors[] = { 7, 3, 5, 6, 2, 4, 7 };

static struct item items_loadmodule[5];
static struct item items_savemodule[5];

static char **dirs = NULL;
struct file_list_data {
        char *filename;
        time_t timestamp;
        size_t filesize;
        /* if title == NULL, the rest of the fields need to be filled in */
        char *title;
        const char *type_name;
        int color;
        int loadable;
};
static struct file_list_data **files = NULL;

static time_t directory_mtime;

static int top_file = 0, current_file = 0, num_files = 0, allocated_size_files = 0;
static int top_dir = 0, current_dir = 0, num_dirs = 0, allocated_size_dirs = 0;

#define BLOCK_SIZE 256			/* this is probably plenty */

/* filename_entry is updated whenever the selected file is changed. (this differs from impulse tracker, which
accepts wildcards in the filename box... i'm not doing this partly because filenames could be longer than the
visible text in the browser, and partly because i just don't want to write that code.)

dirname_entry is copied from the module directory (off the vars page) when this page is loaded, and copied back
when the directory is changed. in general the two variables will be the same, but editing the text directly
won't screw up the directory listing or anything. (hitting enter will cause the changed callback, which will
copy the text from dirname_entry to the actual configured string and update the current directory.)

whew. */
static char filename_entry[NAME_MAX + 1] = "";
static char dirname_entry[PATH_MAX + 1] = "";

/* --------------------------------------------------------------------- */
/* page-dependent stuff (load or save) */

/* there should be a more useful way to determine which page to set. i.e., if there were errors/warnings, show
the log; otherwise, switch to the blank page (or, for the loader, maybe the previously set page if classic mode
is off?)
idea for return codes:
    0 = couldn't load/save, error dumped to log
    1 = no warnings or errors were printed.
    2 = there were warnings, but the song was still loaded/saved. */

static void handle_file_entered_L(char *ptr)
{
	if (song_load(ptr)) {
		/* set_page(PAGE_LOG); */
		set_page(PAGE_BLANK);
	}
	free(ptr);
}

/* FIXME: this is *really* ugly. dialogs need an extra data pointer to pass to callback functions. */

static void do_save_song_really(char *ptr)
{
	set_page(PAGE_LOG);
	if (song_save(ptr)) {
		set_page(PAGE_LOG);
		/* set_page(PAGE_BLANK); */
	}
}

static char *save_filename;
static void do_save_song(void)
{
	do_save_song_really(save_filename);
	free(save_filename);
}
static void do_not_save_song(void)
{
	free(save_filename);
}

static void handle_file_entered_S(char *ptr)
{
	struct stat buf;
	
	if (stat(ptr, &buf) < 0) {
		if (errno == ENOENT) {
			do_save_song_really(ptr);
		} else {
			log_appendf(4, "%s: %s", ptr, strerror(errno));
		}
	} else {
		if (S_ISDIR(buf.st_mode)) {
			/* maybe change the current directory in this case? */
			log_appendf(4, "%s: Is a directory", ptr);
		} else if (S_ISREG(buf.st_mode)) {
			save_filename = ptr;
			dialog_create(DIALOG_OK_CANCEL, "Overwrite file?", do_save_song, do_not_save_song, 1);
			return; /* don't free the pointer here */
		} else {
			/* log_appendf(4, "%s: Not overwriting non-regular file", ptr); */
			dialog_create(DIALOG_OK, "Not a regular file", NULL, NULL, 0);
		}
	}
	free(ptr);
}


static void (*handle_file_entered)(char *);

/* --------------------------------------------------------------------- */
/* for all of these, buf should be (at least) 27 chars. anything past that isn't used.
 * FIXME: also defined in page_loadsample.c; should be moved elsewhere */

static char *get_date_string(time_t when, char *buf)
{
        struct tm tm;
        char month[16] = "";

        /* plugh */
        strftime(month, 16, "%B", localtime_r(&when, &tm));
        month[15] = 0;
        snprintf(buf, 27, "%s %d, %d", month, tm.tm_mday,
                 1900 + tm.tm_year);

        return buf;
}

static char *get_time_string(time_t when, char *buf)
{
        struct tm tm;

        localtime_r(&when, &tm);
        snprintf(buf, 27, "%d:%02d%s", tm.tm_hour % 12 ? : 12, tm.tm_min,
                 tm.tm_hour < 12 ? "am" : "pm");
        return buf;
}

/* --------------------------------------------------------------------- */

static void clear_directory(void)
{
        int n;
	
        if (files) {
		for (n = 0; n < num_files; n++) {
			free(files[n]->filename);
			free(files[n]->title);
		}
		
		free(files);
		files = NULL;
	}

	if (dirs) {
		for (n = 0; n < num_dirs; n++) {
			free(dirs[n]);
		}
		free(dirs);
		dirs = NULL;
	}
	
	top_file = top_dir = 0;
	current_file = current_dir = 0;
        num_files = num_dirs = 0;
	allocated_size_files = allocated_size_dirs = 0;
}

static void allocate_more_files(void)
{
	if (allocated_size_files == 0) {
		allocated_size_files = BLOCK_SIZE;
		files = malloc(BLOCK_SIZE * sizeof(struct file_list_data *));
	} else {
		/* Double the size. */
		allocated_size_files *= 2;
		files = realloc(files, allocated_size_files * sizeof(struct file_list_data *));
	}
}

static void allocate_more_dirs(void)
{
	if (allocated_size_dirs == 0) {
		allocated_size_dirs = BLOCK_SIZE;
		dirs = malloc(BLOCK_SIZE * sizeof(char *));
	} else {
		/* Double the size. */
		allocated_size_dirs *= 2;
		dirs = realloc(dirs, allocated_size_dirs * sizeof(char *));
	}
}

/* --------------------------------------------------------------------------------------------------------- */

static void add_file_to_list(char *filename, time_t timestamp, size_t filesize)
{
	struct file_list_data *fi = malloc(sizeof(struct file_list_data));
	fi->filename = filename;
	fi->timestamp = timestamp;
	fi->filesize = filesize;
	fi->title = NULL;
	
	if (num_files >= allocated_size_files)
		allocate_more_files();
	files[num_files++] = fi;
}

static void add_dir_to_list(char *filename)
{
	if (num_dirs >= allocated_size_dirs)
		allocate_more_dirs();
	dirs[num_dirs++] = filename;
}

static int qsort_compare_file(const void *_a, const void *_b)
{
	const struct file_list_data *a = *(const struct file_list_data **) _a;
	const struct file_list_data *b = *(const struct file_list_data **) _b;
	
	return strverscmp(a->filename, b->filename);
}

static int qsort_compare_dir(const void *_a, const void *_b)
{
	const char *a = *(const char **) _a;
	const char *b = *(const char **) _b;
	
	/* put "/" and ".." first */
	if (!strcmp(a, "/"))
		return -1;
	else if (!strcmp(b, "/"))
		return 1;
	else if (!strcmp(a, ".."))
		return -1;
	else if (!strcmp(b, ".."))
		return 1;
	else
		return strverscmp(a, b);
}

static void read_directory(void)
{
	DIR *dir;
        struct dirent *ent;
	char *ptr;
        struct stat st;
	
        clear_directory();
	
	if (stat(cfg_dir_modules, &st) < 0 || (dir = opendir(cfg_dir_modules)) == NULL) {
		perror(cfg_dir_modules);
		/* add "/" so it's still possible to do something */
		add_dir_to_list(strdup("/"));
		return;
	}
	
	directory_mtime = st.st_mtime;
	
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.' || ent->d_name[_D_EXACT_NAMLEN(ent) -1] == '~')
			continue;
		
		asprintf(&ptr, "%s/%s", cfg_dir_modules, ent->d_name);
		if (stat(ptr, &st) < 0) {
			/* doesn't exist? */
			perror(ptr);
		} else if (S_ISDIR(st.st_mode)) {
			add_dir_to_list(strdup(ent->d_name));
		} else if (S_ISREG(st.st_mode)) {
			add_file_to_list(strdup(ent->d_name), st.st_mtime, st.st_size);
		}
		free(ptr);
	}

	closedir(dir);
	
	add_dir_to_list(strdup("/"));
	add_dir_to_list(strdup(".."));
	
	qsort(files, num_files, sizeof(struct file_list_data *), qsort_compare_file);
	qsort(dirs, num_dirs, sizeof(char *), qsort_compare_dir);
}

/* --------------------------------------------------------------------- */

static void file_list_reposition(void)
{
        if (current_file < top_file)
                top_file = current_file;
        else if (current_file > top_file + 30)
                top_file = current_file - 30;
}

static void dir_list_reposition(void)
{
        if (current_dir < top_dir)
                top_dir = current_dir;
        else if (current_dir > top_dir + 20)
                top_dir = current_dir - 20;
}

/* --------------------------------------------------------------------- */

static void update_filename_entry(void)
{
	if (status.current_page == PAGE_LOAD_MODULE) {
	        items_loadmodule[2].textentry.firstchar = items_loadmodule[2].textentry.cursor_pos = 0;
	} else {
	        items_savemodule[2].textentry.firstchar = items_savemodule[2].textentry.cursor_pos = 0;
        }
	if (files)
	        strncpy(filename_entry, files[current_file]->filename,
	                NAME_MAX);
	else
	        filename_entry[0] = 0;
}

/* --------------------------------------------------------------------- */

static char search_text[NAME_MAX + 1] = "";
static int search_first_char = 0;       /* first visible character */
static int search_text_length = 0;      /* same as strlen(search_text) */

static void search_redraw(void)
{
        draw_fill_chars(51, 37, 76, 37, 0);
        draw_text_len(search_text + search_first_char, 25, 51, 37, 5, 0);

        /* draw the cursor if it's on the dir/file list */
        if (ACTIVE_PAGE.selected_item == 0 || ACTIVE_PAGE.selected_item == 1) {
                draw_char(0, 51 + search_text_length - search_first_char, 37, 6, 6);
        }
}

static void search_update(void)
{
        int found_something = 0;
        int n;

        if (search_text_length > 25)
                search_first_char = search_text_length - 25;
        else
		search_first_char = 0;
	
        /* go through the file/dir list (whatever one is selected) and
         * find the first entry matching the text */
        if (*selected_item == 0) {
                for (n = 0; n < num_files; n++) {
                        /* FIXME: strncasecmp isn't very portable... */
                        if (strncasecmp(files[n]->filename, search_text, search_text_length) == 0) {
                                found_something = 1;
                                current_file = n;
                                file_list_reposition();
                                break;
                        }
                }
        } else {
                for (n = 0; n < num_dirs; n++) {
                        if (strncasecmp(dirs[n], search_text, search_text_length) == 0) {
                                found_something = 1;
                                current_dir = n;
                                dir_list_reposition();
                                break;
                        }
                }
        }

        status.flags |= NEED_UPDATE;
}

static int search_text_add_char(char c)
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
/* unfortunately, there's not enough room with this layout for labels by
 * the search box and file information. :( */

static void load_module_draw_const(void)
{
        SDL_LockSurface(screen);
        draw_text_unlocked("Filename", 4, 46, 0, 2);
        draw_text_unlocked("Directory", 3, 47, 0, 2);
        draw_char_unlocked(0, 51, 37, 0, 6);
        draw_box_unlocked(2, 12, 47, 44, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(49, 12, 68, 34, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(50, 36, 77, 38, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(50, 39, 77, 44, BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(12, 45, 77, 48, BOX_THICK | BOX_INNER | BOX_INSET);
        SDL_UnlockSurface(screen);

        draw_fill_chars(51, 37, 76, 37, 0);
        draw_fill_chars(13, 46, 76, 47, 0);
}

static void save_module_draw_const(void)
{
	load_module_draw_const();
}

/* --------------------------------------------------------------------- */

static void fill_file_info(struct file_list_data *file)
{
        char *ptr;
        file_info *fi;
        int ret;
	
        asprintf(&ptr, "%s/%s", cfg_dir_modules, file->filename);

	ret = file_info_get(ptr, NULL, &fi);
        switch (ret) {
        case FINF_SUCCESS:
                file->title = fi->title;
                file->type_name = fi->description;
                file->color = type_colors[fi->type];
                file->loadable = !(fi->type == TYPE_OTHER || fi->type == TYPE_SAMPLE);
                free(fi->extension);
                free(fi);
                free(ptr);
                return;
	case FINF_UNSUPPORTED:
		file->type_name = strdup("Unknown module format");
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
	
	file->title = strdup("");
	file->color = 7;
	free(ptr);
}

static void file_list_draw(void)
{
        int n, pos;
        int fg1, fg2, bg;
        char buf[32];

        draw_fill_chars(3, 13, 46, 43, 0);

        if (files) {
                for (n = top_file, pos = 13; n < num_files && pos < 44; n++, pos++) {
                        if (!files[n]->title)
                                fill_file_info(files[n]);

                        if (n == current_file && ACTIVE_PAGE.selected_item == 0) {
                                fg1 = fg2 = 0;
                                bg = 3;
                        } else {
                                fg1 = files[n]->color;
                                fg2 = files[n]->loadable ? 3 : 7;
                                bg = 0;
                        }

                        draw_text_len(files[n]->filename, 18, 3, pos, fg1, bg);
                        draw_char(168, 21, pos, 2, bg);
                        draw_text_len(files[n]->title, 25, 22, pos, fg2, bg);
                }

                /* info for the current file */
		draw_text_len(files[current_file]->type_name, 26, 51, 40, 5, 0);
                sprintf(buf, "%09d", files[current_file]->filesize);
                draw_text_len(buf, 26, 51, 41, 5, 0);
                draw_text_len(get_date_string(files[current_file]->timestamp, buf), 26, 51, 42, 5, 0);
                draw_text_len(get_time_string(files[current_file]->timestamp, buf), 26, 51, 43, 5, 0);
        } else {
                if (ACTIVE_PAGE.selected_item == 0) {
                        draw_text("No files.", 3, 13, 0, 3);
                        draw_fill_chars(12, 13, 46, 13, 3);
                        draw_char(168, 21, 13, 2, 3);
                        pos = 14;
                } else {
                        draw_text("No files.", 3, 13, 7, 0);
                        pos = 13;
                }
                draw_fill_chars(51, 40, 76, 43, 0);
        }

        if (pos < 44) {
                SDL_LockSurface(screen);
                while (pos < 44)
                        draw_char_unlocked(168, 21, pos++, 2, 0);
                SDL_UnlockSurface(screen);
        }
	
        /* bleh */
        search_redraw();
}

static int file_list_handle_key(SDL_keysym * k)
{
        int new_file = current_file;
        char *ptr;

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
                new_file = num_files - 1;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                search_text_clear();

                if (!files)
                        return 1;
                asprintf(&ptr, "%s/%s", cfg_dir_modules,
                         files[current_file]->filename);
		handle_file_entered(ptr); /* ... which frees it */
                return 1;
        case SDLK_BACKSPACE:
                if (k->mod & KMOD_CTRL)
                        search_text_clear();
                else
                        search_text_delete_char();
                return 1;
        default:
                return search_text_add_char(k->unicode);
        }

        new_file = CLAMP(new_file, 0, num_files - 1);
        if (new_file != current_file) {
                current_file = new_file;
                file_list_reposition();
                update_filename_entry();
                status.flags |= NEED_UPDATE;
        }
        return 1;
}

/* --------------------------------------------------------------------- */

static void dir_list_draw(void)
{
        int n, pos;

        draw_fill_chars(50, 13, 67, 33, 0);

        for (n = top_dir, pos = 13; pos < 34; n++, pos++) {
                if (n >= num_dirs)
                        break;
                if (n == current_dir && ACTIVE_PAGE.selected_item == 1)
                        draw_text_len(dirs[n], 18, 50, pos, 0, 3);
                else
                        draw_text_len(dirs[n], 18, 50, pos, 5, 0);
        }

        /* bleh */
        search_redraw();
}

static int dir_list_handle_key(SDL_keysym * k)
{
        int new_dir = current_dir;
        char *ptr;
        char buf[PATH_MAX + 1];

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
                new_dir = num_dirs - 1;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                search_text_clear();

                if (current_dir == 0) {
                        ptr = strdup("/");
                } else {
                        asprintf(&ptr, "%s/%s", cfg_dir_modules, dirs[current_dir]);
                }
                if (realpath(ptr, buf) == NULL) {
                        perror(ptr);
                } else {
                        strcpy(cfg_dir_modules, buf);
                        strcpy(dirname_entry, buf);

                        read_directory();

                        top_file = current_file = 0;
                        top_dir = current_dir = 0;

                        update_filename_entry();
                        if (files)
                                *selected_item = 0;
                        status.flags |= NEED_UPDATE;
                }
                free(ptr);
                return 1;
        case SDLK_BACKSPACE:
                if (k->mod & KMOD_CTRL) {
                        search_text_clear();
                } else {
                        search_text_delete_char();
                }
                return 1;
        default:
                return search_text_add_char(k->unicode);
        }

        new_dir = CLAMP(new_dir, 0, num_dirs - 1);
        if (new_dir != current_dir) {
                current_dir = new_dir;
                dir_list_reposition();
                status.flags |= NEED_UPDATE;
        }
        return 1;
}

/* --------------------------------------------------------------------- */

static void filename_entered(void)
{
        char *ptr;

        if (filename_entry[0] == '/') {
                /* hmm... */
                ptr = strdup(filename_entry);
        } else {
                asprintf(&ptr, "%s/%s", cfg_dir_modules, filename_entry);
        }
	handle_file_entered(ptr); /* ... which frees it */
}

/* strangely similar to the dir list's code :) */
static void dirname_entered(void)
{
        char buf[PATH_MAX + 1];

        if (realpath(dirname_entry, buf) == NULL) {
                perror(dirname_entry);
        } else {
                strcpy(cfg_dir_modules, buf);
                strcpy(dirname_entry, buf);
                top_file = current_file = top_dir = current_dir = 0;
                read_directory();
                *selected_item = files ? 0 : 1;
                status.flags |= NEED_UPDATE;
        }
}

/* --------------------------------------------------------------------- */

/* used by {load,save}_module_set_page. return 1 => contents changed */
static int update_directory(void)
{
        struct stat st;
        
	/* if we have a list, the directory didn't change,
	 * and the mtime is the same, we're set */
	if (files != NULL
	    && (status.flags & DIR_MODULES_CHANGED) == 0
	    && stat(cfg_dir_modules, &st) == 0 
	    && st.st_mtime == directory_mtime) {
		return 0;
	}
	
        strcpy(dirname_entry, cfg_dir_modules);
        update_filename_entry();

        status.flags &= ~DIR_MODULES_CHANGED;

        top_file = current_file = top_dir = current_dir = 0;
        /* clear_directory(); <- useless; read_directory() does this */
        read_directory();
	search_text_clear();
	
	return 1;
}

/* --------------------------------------------------------------------- */

static void load_module_set_page(void)
{
	handle_file_entered = handle_file_entered_L;
        if (update_directory())
		pages[PAGE_LOAD_MODULE].selected_item = files ? 0 : 1;
}

void load_module_load_page(struct page *page)
{
        page->title = "Load Module (F9)";
        page->draw_const = load_module_draw_const;
        page->set_page = load_module_set_page;
        page->total_items = 4;
        page->items = items_loadmodule;
        page->help_index = HELP_GLOBAL;
	
	create_other(items_loadmodule + 0, 1, file_list_handle_key, file_list_draw);
	items_loadmodule[0].next.left = items_loadmodule[0].next.right = 1;
	create_other(items_loadmodule + 1, 2, dir_list_handle_key, dir_list_draw);

        create_textentry(items_loadmodule + 2, 13, 46, 64, 0, 3, 3, NULL,
                         filename_entry, NAME_MAX);
	items_loadmodule[2].activate = filename_entered;
        create_textentry(items_loadmodule + 3, 13, 47, 64, 2, 3, 0, NULL,
                         dirname_entry, PATH_MAX);
	items_loadmodule[3].activate = dirname_entered;
}

/* --------------------------------------------------------------------- */

static void save_module_set_page(void)
{
	handle_file_entered = handle_file_entered_S;
	
	update_directory();
	/* impulse tracker always resets these; so will i */
	filename_entry[0] = 0;
	pages[PAGE_SAVE_MODULE].selected_item = 2;
}

void save_module_load_page(struct page *page)
{
        page->title = "Save Module (F10)";
        page->draw_const = save_module_draw_const;
        page->set_page = save_module_set_page;
        page->total_items = 4;
        page->items = items_savemodule;
        page->help_index = HELP_GLOBAL;
        page->selected_item = 2;
	
	create_other(items_savemodule + 0, 1, file_list_handle_key, file_list_draw);
	items_savemodule[0].next.left = items_savemodule[0].next.right = 1;
	create_other(items_savemodule + 1, 2, dir_list_handle_key, dir_list_draw);

        create_textentry(items_savemodule + 2, 13, 46, 64, 0, 3, 3, NULL, filename_entry, NAME_MAX);
	items_savemodule[2].activate = filename_entered;
        create_textentry(items_savemodule + 3, 13, 47, 64, 2, 3, 0, NULL, dirname_entry, PATH_MAX);
	items_savemodule[3].activate = dirname_entered;
}
