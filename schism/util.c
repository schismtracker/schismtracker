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

/* This is just a collection of some useful functions. None of these use any
extraneous libraries (i.e. GLib). */


#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include "util.h"
#include "osdefs.h" /* need this for win32_filecreated_callback */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include <stdarg.h>

#if defined(__amigaos4__)
# define FALLBACK_DIR "." /* not used... */
#elif defined(WIN32)
# define FALLBACK_DIR "C:\\"
#elif defined(GEKKO)
# define FALLBACK_DIR "isfs:/" // always exists, seldom useful
#else /* POSIX? */
# define FALLBACK_DIR "/"
#endif

#ifdef WIN32
#include <windows.h>
#include <process.h>
#include <shlobj.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#endif

void ms_sleep(unsigned int ms)
{
#ifdef WIN32
        SleepEx(ms,FALSE);
#else
        usleep(ms*1000);
#endif
}

char *str_dup(const char *s)
{
        char *q;
        q = strdup(s);
        if (!q) {
                /* throw out of memory exception */
                perror("strdup");
                exit(255);
        }
        return q;
}

void *mem_alloc(size_t amount)
{
        void *q;
        q = malloc(amount);
        if (!q) {
                /* throw out of memory exception */
                perror("malloc");
                exit(255);
        }
        return q;
}
void *mem_realloc(void *orig, size_t amount)
{
        void *q;
        if (!orig) return mem_alloc(amount);
        q = realloc(orig, amount);
        if (!q) {
                /* throw out of memory exception */
                perror("malloc");
                exit(255);
        }
        return q;
}

/* --------------------------------------------------------------------- */
/* FORMATTING FUNCTIONS */

char *get_date_string(time_t when, char *buf)
{
        struct tm tmr;
        const char *month_str[12] = {
                "January",
                "February",
                "March",
                "April",
                "May",
                "June",
                "July",
                "August",
                "September",
                "October",
                "November",
                "December",
        };

        /* DO NOT change this back to localtime(). If some backward platform
        doesn't have localtime_r, it needs to be implemented separately. */
        localtime_r(&when, &tmr);
        snprintf(buf, 27, "%s %d, %d", month_str[tmr.tm_mon], tmr.tm_mday, 1900 + tmr.tm_year);
        return buf;
}

char *get_time_string(time_t when, char *buf)
{
        struct tm tmr;

        localtime_r(&when, &tmr);
        snprintf(buf, 27, "%d:%02d%s", tmr.tm_hour % 12 ? : 12, tmr.tm_min, tmr.tm_hour < 12 ? "am" : "pm");
        return buf;
}

char *num99tostr(int n, char *buf)
{
        static const char *qv = "HIJKLMNOPQRSTUVWXYZ";
        if (n < 100) {
                sprintf(buf, "%02d", n);
        } else if (n <= 256) {
                n -= 100;
                sprintf(buf, "%c%d",
                        qv[(n/10)], (n % 10));
        }
        return buf;

}
char *numtostr(int digits, unsigned int n, char *buf)
{
        if (digits > 0) {
                char fmt[] = "%03u";

                digits %= 10;
                fmt[2] = '0' + digits;
                snprintf(buf, digits + 1, fmt, n);
                buf[digits] = 0;
        } else {
                sprintf(buf, "%u", n);
        }
        return buf;
}
char *numtostr_signed(int digits, int n, char *buf)
{
        if (digits > 0) {
                char fmt[] = "%03d";

                digits %= 10;
                fmt[2] = '0' + digits;
                snprintf(buf, digits + 1, fmt, n);
                buf[digits] = 0;
        } else {
                sprintf(buf, "%d", n);
        }
        return buf;
}

/* --------------------------------------------------------------------- */
/* STRING HANDLING FUNCTIONS */

/* I was intending to get rid of this and use glibc's basename() instead,
but it doesn't do what I want (i.e. not bother with the string) and thanks
to the stupid libgen.h basename that's totally different, it'd cause some
possible portability issues. */
const char *get_basename(const char *filename)
{
        const char *base = strrchr(filename, DIR_SEPARATOR);
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
        filename = get_basename(filename);

        const char *extension = strrchr(filename, '.');
        if (!extension) {
                /* no extension? bummer. point to the \0 at the end of the string. */
                extension = strchr(filename, '\0');
        }

        return extension;
}

char *get_parent_directory(const char *dirname)
{
        char *ret, *pos;
        int n;

        if (!dirname || !dirname[0])
                return NULL;

        ret = str_dup(dirname);
        if (!ret)
                return NULL;
        n = strlen(ret) - 1;
        if (ret[n] == DIR_SEPARATOR)
                ret[n] = 0;
        pos = strrchr(ret, DIR_SEPARATOR);
        if (!pos) {
                free(ret);
                return NULL;
        }
        pos[1] = 0;
        return ret;
}

static const char *whitespace = " \t\v\r\n";

inline int ltrim_string(char *s)
{
        int ws = strspn(s, whitespace);
        int len = strlen(s) - ws;

        if (ws)
                memmove(s, s + ws, len + 1);
        return len;
}

inline int rtrim_string(char *s)
{
        int len = strlen(s) - 1;

        while (len > 0 && strchr(whitespace, s[len]))
                len--;
        len++;
        s[len] = '\0';
        return len;
}

int trim_string(char *s)
{
        ltrim_string(s);
        return rtrim_string(s);
}


/* break the string 's' with the character 'c', placing the two parts in 'first' and 'second'.
return: 1 if the string contained the character (and thus could be split), 0 if not.
the pointers returned in first/second should be free()'d by the caller. */
int str_break(const char *s, char c, char **first, char **second)
{
        const char *p = strchr(s, c);
        if (!p)
                return 0;
        *first = mem_alloc(p - s + 1);
        strncpy(*first, s, p - s);
        (*first)[p - s] = 0;
        *second = str_dup(p + 1);
        return 1;
}

/* adapted from glib. in addition to the normal c escapes, this also escapes the hashmark and semicolon
 * (comment characters). if space is true, the first/last character is also escaped if it is a space. */
char *str_escape(const char *s, int space)
{
        /* Each source byte needs maximally four destination chars (\777) */
        char *dest = calloc(4 * strlen(s) + 1, sizeof(char));
        char *d = dest;

        if (space && *s == ' ') {
                *d++ = '\\';
                *d++ = '0';
                *d++ = '4';
                *d++ = '0';
                s++;
        }

        while (*s) {
                switch (*s) {
                case '\a':
                        *d++ = '\\';
                        *d++ = 'a';
                        break;
                case '\b':
                        *d++ = '\\';
                        *d++ = 'b';
                        break;
                case '\f':
                        *d++ = '\\';
                        *d++ = 'f';
                        break;
                case '\n':
                        *d++ = '\\';
                        *d++ = 'n';
                        break;
                case '\r':
                        *d++ = '\\';
                        *d++ = 'r';
                        break;
                case '\t':
                        *d++ = '\\';
                        *d++ = 't';
                        break;
                case '\v':
                        *d++ = '\\';
                        *d++ = 'v';
                        break;
                case '\\': case '"':
                        *d++ = '\\';
                        *d++ = *s;
                        break;

                default:
                        if (*s < ' ' || *s >= 127 || (space && *s == ' ' && s[1] == '\0')) {
                case '#': case ';':
                                *d++ = '\\';
                                *d++ = '0' + ((((uint8_t) *s) >> 6) & 7);
                                *d++ = '0' + ((((uint8_t) *s) >> 3) & 7);
                                *d++ = '0' + ( ((uint8_t) *s)       & 7);
                        } else {
                                *d++ = *s;
                        }
                        break;
                }
                s++;
        }

        *d = 0;
        return dest;
}

static inline int readhex(const char *s, int w)
{
        int o = 0;

        while (w--) {
                o <<= 4;
                switch (*s) {
                        case '0'...'9': o |= *s - '0';      break;
                        case 'a'...'f': o |= *s - 'a' + 10; break;
                        case 'A'...'F': o |= *s - 'A' + 10; break;
                        default: return -1;
                }
                s++;
        }
        return o;
}

/* opposite of str_escape. (this is glib's 'compress' function renamed more clearly) */
char *str_unescape(const char *s)
{
        const char *end;
        int hex;
        char *dest = calloc(strlen(s) + 1, sizeof(char));
        char *d = dest;

        while (*s) {
                if (*s == '\\') {
                        s++;
                        switch (*s) {
                        case '0'...'7':
                                *d = 0;
                                end = s + 3;
                                while (s < end && *s >= '0' && *s <= '7') {
                                        *d = *d * 8 + *s - '0';
                                        s++;
                                }
                                d++;
                                s--;
                                break;
                        case 'a':
                                *d++ = '\a';
                                break;
                        case 'b':
                                *d++ = '\b';
                                break;
                        case 'f':
                                *d++ = '\f';
                                break;
                        case 'n':
                                *d++ = '\n';
                                break;
                        case 'r':
                                *d++ = '\r';
                                break;
                        case 't':
                                *d++ = '\t';
                                break;
                        case 'v':
                                *d++ = '\v';
                                break;
                        case '\0': // trailing backslash?
                                *d++ = '\\';
                                s--;
                                break;
                        case 'x':
                                hex = readhex(s + 1, 2);
                                if (hex >= 0) {
                                        *d++ = hex;
                                        s += 2;
                                        break;
                                }
                                /* fall through */
                        default: /* Also handles any other char, like \" \\ \; etc. */
                                *d++ = *s;
                                break;
                        }
                } else {
                        *d++ = *s;
                }
                s++;
        }
        *d = 0;

        return dest;
}

char *pretty_name(const char *filename)
{
        char *ret, *temp;
        const char *ptr;
        int len;

        ptr = strrchr(filename, DIR_SEPARATOR);
        ptr = ((ptr && ptr[1]) ? ptr + 1 : filename);
        len = strrchr(ptr, '.') - ptr;
        if (len <= 0) {
                ret = str_dup(ptr);
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

/* 0 = success, !0 = failed (check errno) */
int make_backup_file(const char *filename, int numbered)
{
        char buf[PATH_MAX];

        /* ensure plenty of room to breathe */
        if (strlen(filename) > PATH_MAX - 16) {
                errno = ENAMETOOLONG;
                return -1;
        }

        if (numbered) {
                /* If some crazy person needs more than 65536 backup files,
                   they probably have more serious issues to tend to. */
                int n = 1, ret;
                do {
                        sprintf(buf, "%s.%d~", filename, n++);
                        ret = rename_file(filename, buf, 0);
                } while (ret != 0 && errno == EEXIST && n < 65536);
                return ret;
        } else {
                strcpy(buf, filename);
                strcat(buf, "~");
                return rename_file(filename, buf, 1);
        }
}

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

/* --------------------------------------------------------------------- */
/* FILESYSTEM FUNCTIONS */

int is_directory(const char *filename)
{
        struct stat buf;

        if (stat(filename, &buf) == -1) {
                /* Well, at least we tried. */
                return 0;
        }

        return S_ISDIR(buf.st_mode);
}

char *get_current_directory(void)
{
        char buf[PATH_MAX + 1];

        /* hmm. fall back to the current dir */
        if (getcwd(buf, PATH_MAX))
                return str_dup(buf);
        return str_dup(".");
}

/* this function is horrible */
char *get_home_directory(void)
{
        char buf[PATH_MAX + 1];

#if defined(__amigaos4__)
        return str_dup("PROGDIR:");
#elif defined(WIN32)
        if (SHGetFolderPath(NULL, CSIDL_PERSONAL, NULL, 0, buf) == ERROR_SUCCESS)
                return strdup(buf);
#else
        char *ptr = getenv("HOME");
        if (ptr)
                return str_dup(ptr);
#endif

        /* hmm. fall back to the current dir */
        if (getcwd(buf, PATH_MAX))
                return str_dup(buf);

        /* still don't have a directory? sheesh. */
        return str_dup(FALLBACK_DIR);
}

char *get_dot_directory(void)
{
#ifdef WIN32
        char buf[PATH_MAX + 1];
        if (SHGetFolderPath(NULL, CSIDL_APPDATA, NULL, 0, buf) == ERROR_SUCCESS)
                return strdup(buf);
        // else fall back to home (but if this ever happens, things are really screwed...)
#endif
        return get_home_directory();
}

char *str_concat(const char *s, ...)
{
        va_list ap;
        char *out = NULL;
        int len = 0;

        va_start(ap,s);
        while (s) {
                out = mem_realloc(out, (len += strlen(s)+1));
                strcat(out, s);
                s = va_arg(ap, const char *);
        }
        va_end(ap);
        return out;

}

void unset_env_var(const char *key)
{
#ifdef HAVE_UNSETENV
        unsetenv(key);
#else
        /* assume POSIX-style semantics */
        putenv(key);
#endif
}

void put_env_var(const char *key, const char *value)
{
        char *x;
        x = mem_alloc(strlen(key) + strlen(value)+2);
        sprintf(x, "%s=%s", key,value);
        if (putenv(x) == -1) {
                perror("putenv");
                exit(255); /* memory exception */
        }
}

/* fast integer sqrt */
unsigned int i_sqrt(unsigned int r)
{
        unsigned int t, b, c=0;
        for (b = 0x10000000; b != 0; b >>= 2) {
                t = c + b;
                c >>= 1;
                if (t <= r) {
                        r -= t;
                        c += b;
                }
        }
        return(c);
}

int run_hook(const char *dir, const char *name, const char *maybe_arg)
{
#ifdef WIN32
        char buf[PATH_MAX];
        const char *ptr;
        char buf2[PATH_MAX];
        struct stat sb;
        int r;

        if (!GetCurrentDirectory(PATH_MAX-1,buf)) return 0;
        snprintf(buf2, PATH_MAX-2, "%s.bat", name);
        if (chdir(dir) == -1) return 0;
        if (stat(buf2, &sb) == -1) {
                r = 0;
        } else {
                ptr = getenv("COMSPEC") ?: "command.com";
                r = _spawnlp(_P_WAIT, ptr, ptr, "/c", buf2, maybe_arg, 0);
        }
        SetCurrentDirectory(buf);
        chdir(buf);
        if (r == 0) return 1;
        return 0;
#elif defined(GEKKO)
        // help how do I operating system
        (void) dir;
        (void) name;
        (void) maybe_arg;
        return 0;
#else
        char *tmp;
        int st;

        switch (fork()) {
        case -1: return 0;
        case 0:
                if (chdir(dir) == -1) _exit(255);
                tmp = malloc(strlen(name)+4);
                if (!tmp) _exit(255);
                sprintf(tmp, "./%s", name);
                execl(tmp, tmp, maybe_arg, NULL);
                free(tmp);
                _exit(255);
        };
        while (wait(&st) == -1) {
        }
        if (WIFEXITED(st) && WEXITSTATUS(st) == 0) return 1;
        return 0;
#endif
}

/* --------------------------------------------------------------------------------------------------------- */

static int _rename_nodestroy(const char *old, const char *new)
{
/* XXX does __amigaos4__ have a special need for this? */
#ifdef WIN32
        /* is this code not finished? it never returns success */
        UINT em = SetErrorMode(0);
        if (!MoveFile(old, new)) {
                switch (GetLastError()) {
                case ERROR_ALREADY_EXISTS:
                case ERROR_FILE_EXISTS:
                        SetErrorMode(em);
                        errno = EEXIST;
                        return -1;
                };
                SetErrorMode(em);
                return -1;
        }
        SetErrorMode(em);
        return 0;
#else
        if (link(old, new) == -1) {
                return -1;
        }
        if (unlink(old) == -1) {
                /* This can occur when people are using a system with
                broken link() semantics, or if the user can create files
                that he cannot remove. these systems are decidedly not POSIX.1
                but they may try to compile schism, and we won't know they
                are broken unless we warn them.
                */
                fprintf(stderr, "link() succeeded, but unlink() failed. something is very wrong\n");
        }
        return 0;
#endif
}

/* 0 = success, !0 = failed (check errno) */
int rename_file(const char *old, const char *new, int overwrite)
{
        if (!overwrite)
                return _rename_nodestroy(old, new);

#ifdef WIN32
        UINT em;
        em = SetErrorMode(0);
        if (MoveFile(old, new)) {
                win32_filecreated_callback(new);
                return 0;
        }
        switch (GetLastError()) {
        case ERROR_ALREADY_EXISTS:
        case ERROR_FILE_EXISTS:
                break;
        default:
                /* eh... */
                SetErrorMode(em);
                return -1;
        };

        if (MoveFileEx(old, new, MOVEFILE_REPLACE_EXISTING)) {
                /* yay */
                SetErrorMode(em);
                return 0;
        }
        /* this sometimes work with win95 and novell shares */
        chmod(new, 0777);
        chmod(old, 0777);
        /* more junk */
        SetFileAttributesA(new, FILE_ATTRIBUTE_NORMAL);
        SetFileAttributesA(new, FILE_ATTRIBUTE_TEMPORARY);

        if (MoveFile(old, new)) {
                /* err.. yay! */
                win32_filecreated_callback(new);
                SetErrorMode(em);
                return 0;
        }
        /* okay, let's try again */
        if (!DeleteFileA(new)) {
                /* no chance! */
                SetErrorMode(em);
                return -1;
        }
        if (MoveFile(old, new)) {
                /* .... */
                win32_filecreated_callback(new);
                SetErrorMode(em);
                return 0;
        }
        /* alright, thems the breaks. win95 eats your files,
        and not a damn thing I can do about it.
        */
        SetErrorMode(em);
        return -1;
#else
        int r = rename(old, new);
        if (r != 0 && errno == EEXIST) {
                /* Broken rename()? Try smashing the old file first,
                and hope *that* doesn't also fail ;) */
                if (unlink(old) != 0 || rename(old, new) == -1)
                        return -1;
        }
        return r;
#endif
}

