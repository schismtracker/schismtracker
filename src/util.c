#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include <sys/stat.h>
#include <errno.h>

#include "util.h"

/* This is just a collection of some useful functions. None of these use
 * any extraneous libraries (i.e. GLib).
 * I use this file for several programs, so please don't break it. ;) */

/* --------------------------------------------------------------------- */
/* FORMATTING FUNCTIONS
 * FIXME: format_time/format_date use static buffers == unthreadful */

/* TODO: custom time format, get rid of the static return buffer */
const char *format_time(int seconds)
{
        static char buf[16];

        snprintf(buf, 16, "%.2d'%.02d\"", seconds / 60, seconds % 60);
        return buf;
}

/* ls-style time/date formatting (including either the time or year) */
const char *format_date(time_t t)
{
        static char ret[64];
        int format_id = 0;
        time_t now = time(NULL);
        const char *formats[4] = {
                "%b %d %H:%M",  /* 0: normal */
                "%b %d %Y",     /* 1: use_year */
                "%m-%d %H:%M",  /* 2: non_posix_locale */
                "%Y-%m-%d",     /* 3: use_year, non_posix_locale */
        };

        if (t > now || now - t > 15768000)
                format_id += 1;
#if 0
        /* TODO: check for non posix locale... how does ls do this? */
        if (in_a_non_posix_locale)
                format_id += 2;
#endif

        strftime(ret, sizeof(ret), formats[format_id], localtime(&t));
        return ret;
}

char *format_size(size_t size, bool power_of_two, const char *base_unit)
{
        /* this should be long double, but it makes glibc barf */
        double dsize = size;
        int n = -1, divide_by = (power_of_two ? 1024 : 1000);
        const char *units = "KMGTPEZY";
        char *buf;

        while (dsize > (divide_by * 1.1) && n < (signed) sizeof(units)) {
                dsize /= divide_by;
                n++;
        }

        if (n == -1)
                n = asprintf(&buf, "%.0f %s", dsize, base_unit);
        else
                n = asprintf(&buf, "%.02f %c%s%s", dsize, units[n],
                             power_of_two ? "i" : "", base_unit);

        return (n < 0) ? NULL : buf;
}

/* --------------------------------------------------------------------- */
/* STRING HANDLING FUNCTIONS
 * 
 * (yes, string. some of these work with file paths but just as strings.
 * you could give clean_path "////zugzwang/monkeybutt/.././/./pr0n/" and
 * it would happily return "/zugzwang/pr0n".)
 * ... well, not now, because I'm just cheating and using realpath, but
 * that's the general idea. */

/* FIXME: ugh. replace calls to this with glibc's basename()... */
const char *get_basename(const char *filename)
{
        const char *base = strrchr(filename, '/');
        if (base) {
                /* skip the slash */
                base++;
        }
        if (!(base && *base)) {
                /* well, there isn't one, so just return the filename */
                base = filename;
        }

        return base;
}

const char *get_extension(const char *filename)
{
        const char *extension = strrchr(filename, '.');
        if (extension) {
                /* skip the dot */
                extension++;
        } else {
                /* no extension? bummer. point to the \0
                 * at the end of the string. */
                extension = strrchr(filename, '\0');
        }

        return extension;
}

char *clean_path(const char *path)
{
        char buf[PATH_MAX];

        /* FIXME: don't use realpath! */
        if (realpath(path, buf) == NULL) {
                return NULL;
        }
        return strdup(buf);
}

char *get_parent_directory(const char *dirname)
{
        char *ret, *pos;

        if (!dirname)
                return NULL;

        ret = clean_path(dirname);
        if (!ret)
                return NULL;

        pos = strrchr(ret, '/');
        if (!pos) {
                free(ret);
                return NULL;
        }
        pos[1] = 0;
        return ret;
}

static const char *whitespace = " \t\v\r\n";
void trim_string(char *s)
{
        int i = strspn(s, whitespace);

        if (i)
                memmove(s, &(s[i]), strlen(s) - i + 1);
        for (i = strlen(s); i > 0 && strchr(whitespace, s[i]); i--);
        s[1 + i] = 0;
}

/* blecch */
int get_num_lines(const char *text)
{
        const char *ptr = text;
        int n = 0;

        if (!text)
                return 0;
        for (;;) {
                ptr = strpbrk(ptr, "\015\012");
                if (!ptr)
                        return n;
                if (ptr[0] == 13 && ptr[1] == 10)
                        ptr += 2;
                else
                        ptr++;
                n++;
        }
}

/* --------------------------------------------------------------------- */
/* FILE INFO FUNCTIONS */

long file_size(const char *filename)
{
        struct stat buf;

        if (stat(filename, &buf) < 0) {
                return EOF;
        }
        if (S_ISDIR(buf.st_mode)) {
                errno = EISDIR;
                return EOF;
        }
        return buf.st_size;
}

long file_size_fd(int fd)
{
        struct stat buf;

        if (fstat(fd, &buf) == -1) {
                return EOF;
        }
        if (S_ISDIR(buf.st_mode)) {
                errno = EISDIR;
                return EOF;
        }
        return buf.st_size;
}

char *pretty_name(const char *filename)
{
        char *ret, *temp;
        const char *ptr;
        int len;

        ptr = strrchr(filename, '/');
        ptr = ((ptr && ptr[1]) ? ptr + 1 : filename);
        len = strrchr(ptr, '.') - ptr;
        if (len <= 0) {
                ret = strdup(ptr);
        } else {
                ret = calloc(len + 1, sizeof(char));
                strncpy(ret, ptr, len);
                ret[len] = 0;
        }

        /* change underscores to spaces (of course, this could be adapted
         * to use strpbrk and strip any number of characters) */
        while ((temp = strchr(ret, '_')) != NULL)
                *temp = ' ';

        /* TODO | the first letter, and any letter following a space,
         * TODO | should be capitalized; multiple spaces should be cut
         * TODO | down to one */

        trim_string(ret);
        return ret;
}

/* --------------------------------------------------------------------- */
/* FILESYSTEM FUNCTIONS */

bool is_directory(const char *filename)
{
        struct stat buf;

        if (stat(filename, &buf) == -1) {
                /* Well, at least we tried. */
                return false;
        }

        return S_ISDIR(buf.st_mode);
}

/* Borrowed from XMMS, and then hacked a lot to get rid of the dependency
 * on GLib and to make the code a bit cleaner.
 *
 * FIXME | (eventually) This doesn't handle errors like it should.
 * FIXME | i.e. malloc needs a check, error from opendir should be somehow
 * FIXME | relayed to the caller, etc.) -- but this doesn't really bother
 * FIXME | me *too* much so I'm leaving it alone :) */
bool has_subdirectories(const char *dirname)
{
        struct dirent *ent;
        char npath[PATH_MAX];
        DIR *dir = opendir(dirname);

        if (!dir)
                return false;

        while ((ent = readdir(dir)) != NULL) {
                if (ent->d_name[0] == '.')
                        continue;
                /* FIXME: check return of snprintf */
                snprintf(npath, PATH_MAX, "%s/%s", dirname, ent->d_name);
                if (is_directory(npath)) {
                        closedir(dir);
                        return true;
                }
        }

        closedir(dir);
        return false;
}
