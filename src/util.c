/*
 * util.c - Various useful functions
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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
extraneous libraries (i.e. GLib).
I use this file for several programs, so please don't break it. ;) */


#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

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

char *numtostr(int digits, int n, char *buf)
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

/* break the string 's' with the character 'c', placing the two parts in 'first' and 'second'.
return: 1 if the string contained the character (and thus could be split), 0 if not.
the pointers returned in first/second should be free()'d by the caller. */
int str_break(const char *s, char c, char **first, char **second)
{
	const char *p = strchr(s, c);
	if (!p)
		return 0;
	*first = malloc(p - s + 1);
	strncpy(*first, s, p - s);
	(*first)[p - s] = 0;
	*second = strdup(p + 1);
	return 1;
}

/* adapted from glib. in addition to the normal c escapes, this also escapes the comment character (#)
 * as \043. if space_hack is true, the first/last character is also escaped if it is a space. */
char *str_escape(const char *source, bool space_hack)
{
	const char *p = source;
	/* Each source byte needs maximally four destination chars (\777) */
	char *dest = calloc(4 * strlen(source) + 1, sizeof(char));
	char *q = dest;
	
	if (space_hack) {
		if (*p == ' ') {
			*q++ = '\\';
			*q++ = '0';
			*q++ = '4';
			*q++ = '0';
			*p++;
		}
	}
	
	while (*p) {
		switch (*p) {
		case '\a':
			*q++ = '\\';
			*q++ = 'a';
		case '\b':
			*q++ = '\\';
			*q++ = 'b';
			break;
		case '\f':
			*q++ = '\\';
			*q++ = 'f';
			break;
		case '\n':
			*q++ = '\\';
			*q++ = 'n';
			break;
		case '\r':
			*q++ = '\\';
			*q++ = 'r';
			break;
		case '\t':
			*q++ = '\\';
			*q++ = 't';
			break;
		case '\v':
			*q++ = '\\';
			*q++ = 'v';
			break;
		case '\\': case '"':
			*q++ = '\\';
			*q++ = *p;
			break;
		default:
			if ((*p < ' ') || (*p >= 0177) || (*p == '#')
			    || (space_hack && p[1] == '\0' && *p == ' ')) {
				*q++ = '\\';
				*q++ = '0' + (((*p) >> 6) & 07);
				*q++ = '0' + (((*p) >> 3) & 07);
				*q++ = '0' + ((*p) & 07);
			} else {
				*q++ = *p;
			}
			break;
		}
		p++;
	}
	
	*q = 0;
	return dest;
}

/* opposite of str_escape. (this is glib's 'compress' function renamed more clearly)
TODO: it'd be nice to handle \xNN as well... */
char *str_unescape(const char *source)
{
	const char *p = source;
	const char *octal;
	char *dest = calloc(strlen(source) + 1, sizeof(char));
	char *q = dest;
	
	while (*p) {
		if (*p == '\\') {
			p++;
			switch (*p) {
			case '0'...'7':
				*q = 0;
				octal = p;
				while ((p < octal + 3) && (*p >= '0') && (*p <= '7')) {
					*q = (*q * 8) + (*p - '0');
					p++;
				}
				q++;
				p--;
				break;
			case 'a':
				*q++ = '\a';
				break;
			case 'b':
				*q++ = '\b';
				break;
			case 'f':
				*q++ = '\f';
				break;
			case 'n':
				*q++ = '\n';
				break;
			case 'r':
				*q++ = '\r';
				break;
			case 't':
				*q++ = '\t';
				break;
			case 'v':
				*q++ = '\v';
				break;
			default:		/* Also handles \" and \\ */
				*q++ = *p;
				break;
			}
		} else {
			*q++ = *p;
		}
		p++;
	}
	*q = 0;
	
	return dest;
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

bool make_backup_file(const char *filename)
{
	char *b;
	int e;
	
	/* kind of a hack for now, but would be easy to do other things like
	 * numbered backups (like emacs), rename the extension to .bak (or
	 * add it if the file doesn't have an extension), etc. */
	b = malloc(strlen(filename) + 2);
	if (!b)
		return false;
	strcpy(b, filename);
	strcat(b, "~");
	if (rename(filename, b) < 0)
		e = errno;
	free(b);
	
	if (e) {
		errno = e;
		return false;
	} else {
		return true;
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
                snprintf(npath, PATH_MAX, "%s/%s", dirname, ent->d_name);
		npath[PATH_MAX - 1] = 0;
                if (is_directory(npath)) {
                        closedir(dir);
                        return true;
                }
        }

        closedir(dir);
        return false;
}
