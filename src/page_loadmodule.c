#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include <SDL.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#include "it.h"
#include "song.h"
#include "page.h"

#include "title.h"

static const int type_colors[] = { 7, 3, 5, 6, 2, 4 };

/* --------------------------------------------------------------------- */
/* the locals */

static struct item items_loadmodule[5];

static char **dirs = NULL;
static struct file_list_data {
        char *filename;
        time_t timestamp;
        size_t filesize;
        /* if title == NULL, the rest of fields need to be filled in */
        char *title;
        const char *type_name;
        int color;
} **files = NULL;

int top_file = 0;
int current_file = 0;
int num_files = 0;

int top_dir = 0;
int current_dir = 0;
int num_dirs = 0;

/* filename_entry is updated whenever the selected file is changed.
 * (this differs from impulse tracker, which accepts wildcards in the
 * filename box... i'm not doing this partly because filenames could be
 * longer than the visible text in the browser, and partly because i
 * just don't want to write that code.)
 * 
 * dirname_entry is copied from the module directory (off the vars page)
 * when this page is loaded, and copied back when the directory is
 * changed. in general the two variables will be the same, but editing
 * the text directly won't screw up the directory listing or anything.
 * (hitting enter will cause the changed callback, which will copy the
 * text from dirname_entry to the actual configured string and update
 * the current directory.)
 * 
 * whew. */
static char filename_entry[NAME_MAX + 1] = "";
static char dirname_entry[PATH_MAX + 1] = "";

/* --------------------------------------------------------------------- */
/* for all of these, buf should be (at least) 27 chars. anything past
 * that isn't used. */

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

static int dirent_select(const struct dirent *ent)
{
        return (ent->d_name[0] != '.'
                && ent->d_name[_D_EXACT_NAMLEN(ent) - 1] != '~');
}

static void clear_directory(void)
{
        int n;

        if (!files)
                return;

        for (n = 0; n < num_files; n++) {
                free(files[n]->filename);
                free(files[n]->title);
        }
        free(files);
        files = NULL;

        for (n = 0; n < num_dirs; n++) {
                free(dirs[n]);
        }
        free(dirs);
        dirs = NULL;

        num_files = num_dirs = 0;
}

static void read_directory(void)
{
        struct dirent **ents;
        int total, i;
        struct stat st;
        char *ptr;

        clear_directory();

        /* FIXME: some systems don't have scandir */
        total = scandir(dir_modules, &ents, dirent_select, versionsort);
        if (total < 0) {
                perror("scandir");
                ents = NULL;
        }

        for (i = 0; i < total; i++) {
                switch (ents[i]->d_type) {
                case DT_REG:
                        num_files++;
                        break;
                case DT_DIR:
                        num_dirs++;
                        break;
                case DT_LNK:
                case DT_UNKNOWN:
                        /* symlinks to files get stat()'ed twice
                         * (which isn't too much of a problem, as the
                         * inode is cached for the second stat...) */
                        asprintf(&ptr, "%s/%s", dir_modules,
                                 ents[i]->d_name);
                        if (stat(ptr, &st) < 0) {
                                /* doesn't exist? */
                                perror(ptr);
                        } else if (st.st_mode & S_IFREG) {
                                ents[i]->d_type = DT_REG;
                                num_files++;
                        } else if (st.st_mode & S_IFDIR) {
                                ents[i]->d_type = DT_DIR;
                                num_dirs++;
                        }
                        free(ptr);
                        break;
                }
        }

        if (num_files)
                files = calloc(num_files, sizeof(struct file_list_data *));

        num_files = 0;
        if (ents) {
                dirs = calloc(num_dirs + 2, sizeof(char *));
                num_dirs = 2;
                dirs[0] = strdup("/");
                dirs[1] = strdup("..");
        } else {
                dirs = malloc(sizeof(char *));
                num_dirs = 1;
                dirs[0] = strdup("/");
        }

        for (i = 0; i < total; i++) {
                switch (ents[i]->d_type) {
                case DT_REG:
                        asprintf(&ptr, "%s/%s", dir_modules,
                                 ents[i]->d_name);
                        if (stat(ptr, &st) == 0) {
                                free(ptr);
                        } else {
                                perror(ptr);
                                free(ptr);
                                break;
                        }

                        files[num_files] =
                                malloc(sizeof(struct file_list_data));
                        files[num_files]->filename =
                                strdup(ents[i]->d_name);
                        files[num_files]->title = NULL;
                        files[num_files]->timestamp = st.st_mtime;
                        files[num_files]->filesize = st.st_size;
                        num_files++;
                        break;
                case DT_DIR:
                        dirs[num_dirs] = strdup(ents[i]->d_name);
                        num_dirs++;
                        break;
                }
                free(ents[i]);
        }

        if (ents)
                free(ents);
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
        items_loadmodule[2].textentry.firstchar =
                items_loadmodule[2].textentry.cursor_pos = 0;
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
        if (ACTIVE_PAGE.selected_item == 0
            || ACTIVE_PAGE.selected_item == 1) {
                draw_char(0, 51 + search_text_length - search_first_char,
                          37, 6, 6);
        }
}

static void search_update(void)
{
        int found_something = 0;
        int n;

        search_first_char = 0;
        if (search_text_length > search_first_char + 25) {
                search_first_char = search_text_length - 25;
        }

        /* go through the file/dir list (whatever one is selected) and
         * find the first entry matching the text */
        if (*selected_item == 0) {
                for (n = 0; n < num_files; n++) {
                        /* FIXME: strncasecmp isn't very portable... */
                        if (strncasecmp
                            (files[n]->filename, search_text,
                             search_text_length) == 0) {
                                found_something = 1;
                                current_file = n;
                                file_list_reposition();
                                break;
                        }
                }
        } else {
                for (n = 0; n < num_dirs; n++) {
                        if (strncasecmp
                            (dirs[n], search_text,
                             search_text_length) == 0) {
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

        search_first_char = 0;
        if (search_text_length > search_first_char + 25) {
                search_first_char = search_text_length - 25;
        }

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
        draw_box_unlocked(2, 12, 47, 44,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(49, 12, 68, 34,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(50, 36, 77, 38,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(50, 39, 77, 44,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_box_unlocked(12, 45, 77, 48,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        SDL_UnlockSurface(screen);

        draw_fill_chars(51, 37, 76, 37, 0);
        draw_fill_chars(13, 46, 76, 47, 0);
}

/* --------------------------------------------------------------------- */

static void fill_file_info(struct file_list_data *file)
{
        char fname[PATH_MAX + 1];
        file_info *fi;

        snprintf(fname, PATH_MAX, "%s/%s", dir_modules, file->filename);

        fi = file_info_get(fname, NULL);
        if (fi == NULL) {
                file->type_name =
                        strdup((errno ==
                                EPROTO) ? "Unknown module format" :
                               strerror(errno));
                file->title = strdup("");
                file->color = 7;
        } else {
                file->title = fi->title;
                file->type_name = fi->description;
                file->color = type_colors[fi->type];
                free(fi->extension);
                free(fi);
        }
}

static void file_list_draw(void)
{
        int n, pos;
        int fg1, fg2, bg;
        char buf[32];

        draw_fill_chars(3, 13, 46, 43, 0);

        if (files) {
                for (n = top_file, pos = 13; n < num_files && pos < 44;
                     n++, pos++) {
                        if (!files[n]->title)
                                fill_file_info(files[n]);

                        if (n == current_file
                            && ACTIVE_PAGE.selected_item == 0) {
                                fg1 = fg2 = 0;
                                bg = 3;
                        } else {
                                fg1 = files[n]->color;
                                fg2 = 3;
                                bg = 0;
                        }

                        draw_text_len(files[n]->filename, 18, 3, pos, fg1,
                                      bg);
                        draw_char(168, 21, pos, 2, bg);
                        draw_text_len(files[n]->title, 25, 22, pos, fg2,
                                      bg);
                }

                /* info for the current file */
                draw_text_len(files[current_file]->type_name, 26, 51, 40,
                              5, 0);
                sprintf(buf, "%09d", files[current_file]->filesize);
                draw_text_len(buf, 26, 51, 41, 5, 0);
                draw_text_len(get_date_string
                              (files[current_file]->timestamp, buf), 26,
                              51, 42, 5, 0);
                draw_text_len(get_time_string
                              (files[current_file]->timestamp, buf), 26,
                              51, 43, 5, 0);
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
                asprintf(&ptr, "%s/%s", dir_modules,
                         files[current_file]->filename);
                set_page(PAGE_LOG);
                song_load(ptr);
                free(ptr);
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
                        asprintf(&ptr, "%s/%s", dir_modules,
                                 dirs[current_dir]);
                }
                if (realpath(ptr, buf) == NULL) {
                        perror(ptr);
                } else {
                        strcpy(dir_modules, buf);
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
                asprintf(&ptr, "%s/%s", dir_modules, filename_entry);
        }
        set_page(PAGE_LOG);
        song_load(ptr);
        free(ptr);
}

/* strangely similar to the dir list's code :) */
static void dirname_entered(void)
{
        char buf[PATH_MAX + 1];

        if (realpath(dirname_entry, buf) == NULL) {
                perror(dirname_entry);
        } else {
                strcpy(dir_modules, buf);
                strcpy(dirname_entry, buf);
                top_file = current_file = top_dir = current_dir = 0;
                read_directory();
                *selected_item = files ? 0 : 1;
                status.flags |= NEED_UPDATE;
        }
}

/* --------------------------------------------------------------------- */

static void load_module_set_page(void)
{
        /* if we have a list, and the directory's the same, we're set */
        if (files && (status.flags & DIR_MODULES_CHANGED) == 0)
                return;

        strcpy(dirname_entry, dir_modules);
        update_filename_entry();

        status.flags &= ~DIR_MODULES_CHANGED;

        top_file = current_file = top_dir = current_dir = 0;
        clear_directory();
        read_directory();
        *selected_item = files ? 0 : 1;

        search_text_clear();
}

/* --------------------------------------------------------------------- */

void load_module_load_page(struct page *page)
{
        page->title = "Load Module (F9)";
        page->draw_const = load_module_draw_const;
        page->set_page = load_module_set_page;
        page->total_items = 4;
        page->items = items_loadmodule;
        page->help_index = HELP_GLOBAL;

        items_loadmodule[0].type = ITEM_OTHER;
        items_loadmodule[0].next.left = items_loadmodule[0].next.right =
                items_loadmodule[0].next.tab = 1;
        items_loadmodule[0].other.handle_key = file_list_handle_key;
        items_loadmodule[0].other.redraw = file_list_draw;

        items_loadmodule[1].type = ITEM_OTHER;
        items_loadmodule[1].next.left = items_loadmodule[1].next.right = 0;
        items_loadmodule[1].next.tab = 2;
        items_loadmodule[1].other.handle_key = dir_list_handle_key;
        items_loadmodule[1].other.redraw = dir_list_draw;

        create_textentry(items_loadmodule + 2, 13, 46, 64, 0, 3, 3, NULL,
                         filename_entered, filename_entry, NAME_MAX);
        create_textentry(items_loadmodule + 3, 13, 47, 64, 2, 3, 0, NULL,
                         dirname_entered, dirname_entry, PATH_MAX);
}
