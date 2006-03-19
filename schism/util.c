/*
 * util.c - Various useful functions
 * copyright (c) 2003-2005 chisel <storlek@chisel.cjb.net>
 * URL: http://rigelseven.com/
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

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include <stdarg.h>

#if defined(__amigaos4__)
# define FALLBACK_DIR "." /* not used... */
#elif defined(WIN32)
# define FALLBACK_DIR "C:\\"
#else /* POSIX? */
# define FALLBACK_DIR "/"
#endif

#ifdef WIN32
#include <windows.h>
#endif

void ms_sleep(unsigned int ms)
{
#ifdef WIN32
	SleepEx(ms,FALSE);
#else
	usleep(ms*1000);
#endif
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

unsigned char *get_date_string(time_t when, unsigned char *buf)
{
        struct tm *tmr;
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

	/* note; these aren't thread safe! */
	tmr = localtime(&when);
        snprintf((char *) buf, 27, "%s %d, %d", month_str[tmr->tm_mon],
				tmr->tm_mday, 1900 + tmr->tm_year);
	
        return buf;
}

unsigned char *get_time_string(time_t when, unsigned char *buf)
{
        struct tm tm, *tmr;

	/* note; these aren't thread safe! */
	tmr = localtime(&when);
        snprintf((char *) buf, 27, "%d:%02d%s", tmr->tm_hour % 12 ? tmr->tm_hour : 12,
		 tmr->tm_min, tmr->tm_hour < 12 ? "am" : "pm");
        return buf;
}

unsigned char *num99tostr(int n, unsigned char *buf)
{
	if (n < 100) {
		sprintf((char*)buf, "%02d", n);
	} else {
		n -= 100;
		sprintf((char*)buf, "%c%d", 
			('A' + (n / 10)), (n % 10));
	}
	return buf;

}
unsigned char *numtostr(int digits, int n, unsigned char *buf)
{
	if (digits > 0) {
		char fmt[] = "%03d";
		
		digits %= 10;
		fmt[2] = '0' + digits;
		snprintf((char *) buf, digits + 1, fmt, n);
		buf[digits] = 0;
	} else {
		sprintf((char *) buf, "%d", n);
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

char *get_parent_directory(const char *dirname)
{
	char *ret, *pos;
	int n;
	
	if (!dirname || !dirname[0])
		return NULL;
	
	ret = strdup(dirname);
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
void trim_string(char *s)
{
        int i = strspn(s, whitespace);

        if (i)
                memmove(s, &(s[i]), strlen(s) - i + 1);
        for (i = strlen(s)-1; i > 0 && strchr(whitespace, s[i]); i--);
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
	*first = mem_alloc(p - s + 1);
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
			case '0': case '1': case '2': case '3':
			case '4': case '5': case '6': case '7':
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

        ptr = strrchr(filename, DIR_SEPARATOR);
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
	int e = 0;
	
	/* kind of a hack for now, but would be easy to do other things like numbered backups (like emacs),
	rename the extension to .bak (or add it if the file doesn't have an extension), etc. */
	b = mem_alloc(strlen(filename) + 2);
	strcpy(b, filename);
	strcat(b, "~");
	if (!rename_file(filename, b))
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

/* this function is horrible */
char *get_home_directory(void)
{
#if defined(__amigaos4__)
	return strdup("PROGDIR:");
#else
	char *ptr, buf[PATH_MAX + 1];
	
	ptr = getenv("HOME");
#ifdef WIN32
	if (!ptr) /* let $HOME override %USERPROFILE% on win32... */
		ptr = getenv("USERPROFILE");
#endif
	if (ptr)
		return strdup(ptr);
	
	/* hmm. fall back to the current dir */
	if (getcwd(buf, PATH_MAX))
		return strdup(buf);
	
	/* still don't have a directory? sheesh. */
	return strdup(FALLBACK_DIR);
#endif
}

char *str_concat(const char *s, ...)
{
	va_list ap;
	char *out = 0;
	int len = 0;
	
	va_start(ap,s);
	while (s) {
		out = (char*)mem_realloc(out, (len += strlen(s)+1));
		strcat(out, s);
		s = va_arg(ap, const char *);
	}
	return out;

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
