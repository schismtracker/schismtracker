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

//#include "title.h"

/*
 1. Read the directory, skip *~ and .*, add directories to dirs[] and
    plain files to files[]. (Ignore devices, sockets, etc.)
 2. Create a new list with size (num_files + num_dirs + 2).
 3. Add '/' (unless current_dir is already '/') and '..' to this list.
 4. Add the directories and files collected earlier.
 5. Attach names to the items in the new list. (the sample-title stuff is
    coming soon from the title library) -- Actually, this should be done
    while the files are added to the list. Directories all get a title of
    "........Directory........" (where the dots are char 154).
 */

/* --------------------------------------------------------------------- */
/* the locals */

static struct item items_loadsample[5];

/* --------------------------------------------------------------------- */

#if 0

static char **dirs = NULL;
static struct file_list_data {
        char *filename;
        time_t timestamp;
        size_t filesize;
        /* if sample == NULL, the rest of fields need to be filled in.
         * note: all strings in the sample structure are malloc'd. */
        song_sample *sample;
        const char *type_name;
} **files = NULL;

int top_file = 0;
int current_file = 0;
int num_files = 0;

int top_dir = 0;
int current_dir = 0;
int num_dirs = 0;

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
}

static void read_directory(void)
{
        struct dirent **ents;
        int total, i;
        struct stat st;
        char *ptr;

        clear_directory();

        /* FIXME: some systems don't have scandir */
        total = scandir(cfg_dir_samples, &ents, dirent_select,
                        versionsort);
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
                        asprintf(&ptr, "%s/%s", cfg_dir_samples,
                                 ents[i]->d_name);
                        if (stat(ptr, &st) < 0) {
                                /* doesn't exist? */
                                perror(ptr);
                        } else if (S_ISREG(st.st_mode)) {
                                ents[i]->d_type = DT_REG;
                                num_files++;
                        } else if (S_ISDIR(st.st_mode)) {
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
                        asprintf(&ptr, "%s/%s", cfg_dir_samples,
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

/* --------------------------------------------------------------------- */

static void update_filename_entry(void)
{
        items_loadsample[2].textentry.firstchar =
                items_loadsample[2].textentry.cursor_pos = 0;
        if (files)
                strncpy(filename_entry, files[current_file]->filename,
                        NAME_MAX);
        else
                filename_entry[0] = 0;
}

/* --------------------------------------------------------------------- */
/* unfortunately, there's not enough room with this layout for labels by
 * the search box and file information. :( */

static void load_sample_draw_const(void)
{
        SDL_LockSurface(screen);
        draw_text_unlocked("Filename", 4, 46, 0, 2);
        draw_char_unlocked(0, 51, 37, 0, 6);
        draw_box_unlocked(49, 12, 68, 34,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        SDL_UnlockSurface(screen);

        draw_fill_chars(13, 46, 76, 47, 0);
}

/* --------------------------------------------------------------------- */

static void fill_file_info(struct file_list_data *file)
{
        char fname[PATH_MAX + 1];
        file_info *fi;

        snprintf(fname, PATH_MAX, "%s/%s", cfg_dir_samples,
                 file->filename);

        fi = file_info_get(fname, NULL);
        if (fi == NULL) {
                file->type_name =
                        strdup((errno ==
                                EPROTO) ? "Unknown sample format" :
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

/* --------------------------------------------------------------------- */

static void load_sample_set_page(void)
{
        /* if we have a list, and the directory's the same, we're set */
        if (files && (status.flags & DIR_SAMPLES_CHANGED) == 0)
                return;

        strcpy(dirname_entry, cfg_dir_samples);
        update_filename_entry();

        status.flags &= ~DIR_SAMPLES_CHANGED;

        top_file = current_file = top_dir = current_dir = 0;
        clear_directory();
        read_directory();
        *selected_item = files ? 0 : 1;

        search_text_clear();
}

#endif

/* --------------------------------------------------------------------- */

static void file_list_draw(void)
{
        int n;
        char buf[8];

        SDL_LockSurface(screen);

        /* constant */
        draw_box_unlocked(5, 12, 50, 48,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_fill_chars(6, 13, 49, 47, 0);

        draw_box_unlocked(63, 12, 77, 23,
                          BOX_THICK | BOX_INNER | BOX_INSET);
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

        draw_box_unlocked(51, 24, 77, 29,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_fill_chars(52, 25, 76, 28, 0);

        draw_box_unlocked(51, 30, 77, 42,
                          BOX_THIN | BOX_INNER | BOX_INSET);

        /* these abbreviations are sucky and lame. any suggestions? */
        draw_text_unlocked("Def. Vol.", 53, 33, 0, 2);
        draw_text_unlocked("Glb. Vol.", 53, 34, 0, 2);
        draw_text_unlocked("Vib.Speed", 53, 37, 0, 2);
        draw_text_unlocked("Vib.Depth", 53, 38, 0, 2);
        draw_text_unlocked("Vib. Rate", 53, 39, 0, 2);

        draw_box_unlocked(52, 43, 77, 48,
                          BOX_THICK | BOX_INNER | BOX_INSET);
        draw_fill_chars(53, 44, 76, 47, 0);

        /* variable */
        for (n = 13; n < 48; n++) {
                draw_char_unlocked(168, 31, n, 2, 0);
        }
        for (n = 0; n < 5; n++) {
                draw_text_unlocked(numtostr_3(n, buf), 2, 13 + n, 0, 2);
        }

        SDL_UnlockSurface(screen);
}

static int file_list_handle_key(UNUSED SDL_keysym * k)
{
        /* no keys do anything useful yet */
        return 0;
}

static void load_sample_draw_const(void)
{
}

static void load_sample_set_page(void)
{
}

static void load_sample_handle_key(UNUSED SDL_keysym * k)
{
        /* somehow get around the menu being triggered by the escape
         * key... probably this will require yet another hack in the
         * key handler in page.c :( */
}

/* --------------------------------------------------------------------- */

void load_sample_load_page(struct page *page)
{
        page->title = "Load Sample";
        page->draw_const = load_sample_draw_const;
        page->set_page = load_sample_set_page;
        page->handle_key = load_sample_handle_key;
        page->total_items = 1;
        page->items = items_loadsample;
        page->help_index = HELP_GLOBAL;

        items_loadsample[0].type = ITEM_OTHER;
        items_loadsample[0].next.left = items_loadsample[0].next.right =
                items_loadsample[0].next.tab = 1;
        items_loadsample[0].other.handle_key = file_list_handle_key;
        items_loadsample[0].other.redraw = file_list_draw;

        /*
           create_textentry(items_loadsample + 2, 13, 46, 64, 0, 3, 3, NULL,
           filename_entered, filename_entry, NAME_MAX);
           create_textentry(items_loadsample + 3, 13, 47, 64, 2, 3, 0, NULL,
           dirname_entered, dirname_entry, PATH_MAX);
         */
}
