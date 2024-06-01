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


#include "headers.h"

#include "util.h"
#include "charset.h"
#include "osdefs.h" /* need this for win32_filecreated_callback */

#include <sys/types.h>
#include <sys/stat.h>

#include <errno.h>

#include <stdarg.h>

#include <math.h>

#if defined(__amigaos4__)
# define FALLBACK_DIR "." /* not used... */
#elif defined(SCHISM_WIN32)
# define FALLBACK_DIR "C:\\"
#elif defined(SCHISM_WII)
# define FALLBACK_DIR "isfs:/" // always exists, seldom useful
#else /* POSIX? */
# define FALLBACK_DIR "/"
#endif

#ifdef SCHISM_WIN32
#include <windows.h>
#include <process.h>
#include <shlobj.h>
#else
#include <sys/types.h>
#include <sys/wait.h>
#endif

void ms_sleep(unsigned int ms)
{
#ifdef SCHISM_WIN32
	SleepEx(ms,FALSE);
#else
	usleep(ms*1000);
#endif
}

void *mem_alloc(size_t amount)
{
	void *q = malloc(amount);
	if (!q) {
		/* throw out of memory exception */
		perror("malloc");
		exit(255);
	}
	return q;
}

void *mem_calloc(size_t nmemb, size_t size)
{
	void *q;
	q = calloc(nmemb, size);
	if (!q) {
		/* throw out of memory exception */
		perror("calloc");
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

char *strn_dup(const char *s, size_t n)
{
	char *q = mem_alloc((n + 1) * sizeof(char));
	memcpy(q, s, n * sizeof(char));
	q[n] = '\0';
	return q;
}

char *str_dup(const char *s)
{
	return strn_dup(s, strlen(s));
}

/* --------------------------------------------------------------------- */
/* CONVERSION FUNCTIONS */

/* linear -> decibel */
/* amplitude normalized to 1.0f. */
double dB(double amplitude)
{
	return 20.0 * log10(amplitude);
}

/* decibel -> linear */
double dB2_amp(double db)
{
	return pow(10.0, db / 20.0);
}

/* linear -> decibel */
/* power normalized to 1.0f. */
double pdB(double power)
{
	return 10.0 * log10(power);
}

/* decibel -> linear */
double dB2_power(double db)
{
	return pow(10.0, db / 10.0);
}
/* linear -> decibel
 * amplitude normalized to 1.0f.
 * Output scaled (and clipped) to 128 lines with noisefloor range.
 * ([0..128] = [-noisefloor..0dB])
 * correction_dBs corrects the dB after converted, but before scaling.
*/
short dB_s(int noisefloor, double amplitude, double correction_dBs)
{
	double db = dB(amplitude) + correction_dBs;
	return CLAMP((int)(128.0*(db+noisefloor))/noisefloor, 0, 127);
}

/* decibel -> linear */
/* Input scaled to 128 lines with noisefloor range. */
/* ([0..128] = [-noisefloor..0dB]) */
/* amplitude normalized to 1.0f. */
/* correction_dBs corrects the dB after converted, but before scaling.*/
short dB2_amp_s(int noisefloor, int db, double correction_dBs)
{
	return dB2_amp((db*noisefloor/128.0)-noisefloor-correction_dBs);
}
/* linear -> decibel */
/* power normalized to 1.0f. */
/* Output scaled (and clipped) to 128 lines with noisefloor range. */
/* ([0..128] = [-noisefloor..0dB]) */
/* correction_dBs corrects the dB after converted, but before scaling.*/
short pdB_s(int noisefloor, double power, double correction_dBs)
{
	float db = pdB(power)+correction_dBs;
	return CLAMP((int)(128.0*(db+noisefloor))/noisefloor, 0, 127);
}

/* deciBell -> linear */
/* Input scaled to 128 lines with noisefloor range. */
/* ([0..128] = [-noisefloor..0dB]) */
/* power normalized to 1.0f. */
/* correction_dBs corrects the dB after converted, but before scaling.*/
short dB2_power_s(int noisefloor, int db, double correction_dBs)
{
	return dB2_power((db*noisefloor/128.f)-noisefloor-correction_dBs);
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
	snprintf(buf, 27, "%d:%02d%s", (tmr.tm_hour % 12) ? (tmr.tm_hour % 12) : 12, tmr.tm_min, tmr.tm_hour < 12 ? "am" : "pm");
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

	if (!base || !*base) {
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

/* this function should output as you expect
 * e.g. if input is / it returns NULL,
 *      if input is /home it returns / and
 *      if input is /home/ it returns /
 *
 * the equivalent windows paths also work here
*/
char *get_parent_directory(const char *dirname)
{
	if (!dirname || !dirname[0])
		return NULL;

	/* need the root path, including the separator */
	const char* root = strchr(dirname, DIR_SEPARATOR);
	if (!root)
		return NULL;
    root++;

	/* okay, now we need to find the final token */
	const char* pos = root + strlen(root) - 1;
	if (*pos == DIR_SEPARATOR) /* strip off an excess separator, if any */
		pos--;

	while (--pos > root) {
		if (*pos == DIR_SEPARATOR)
			break;
	}

	ptrdiff_t n = pos - dirname;

	/* sanity check */
	if (n <= 0)
		return NULL;

	char *ret = mem_alloc((n + 1) * sizeof(char));
	memcpy(ret, dirname, n * sizeof(char));
	ret[n] = '\0';

	if (!strcmp(dirname, ret) || !ret[0]) {
		free(ret);
		return NULL;
	}

	return ret;
}

static const char *whitespace = " \t\v\r\n";

int ltrim_string(char *s)
{
	int ws = strspn(s, whitespace);
	int len = strlen(s) - ws;

	if (ws)
		memmove(s, s + ws, len + 1);
	return len;
}

int rtrim_string(char *s)
{
	int len = strlen(s);

	while (--len > 0 && strchr(whitespace, s[len]));

	s[++len] = '\0';
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
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7': case '8':
			case '9':
				o |= *s - '0';
				break;
			case 'a': case 'b': case 'c':
			case 'd': case 'e': case 'f':
				o |= *s - 'a' + 10;
				break;
			case 'A': case 'B': case 'C':
			case 'D': case 'E': case 'F':
				o |= *s - 'A' + 10;
				break;
			default:
				return -1;
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
	char *dest = mem_calloc(strlen(s) + 1, sizeof(char));
	char *d = dest;

	while (*s) {
		if (*s == '\\') {
			s++;
			switch (*s) {
			case '0': case '1': case '2':
			case '3': case '4': case '5':
			case '6': case '7':
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

#ifdef SCHISM_WIN32
int win32_wstat(const wchar_t* path, struct stat* st) {
	struct _stat mstat;

	int ws = _wstat(path, &mstat);
	if (ws < 0)
		return ws;

	/* copy all the values */
	st->st_gid = mstat.st_gid;
	st->st_atime = mstat.st_atime;
	st->st_ctime = mstat.st_ctime;
	st->st_dev = mstat.st_dev;
	st->st_ino = mstat.st_ino;
	st->st_mode = mstat.st_mode;
	st->st_mtime = mstat.st_mtime;
	st->st_nlink = mstat.st_nlink;
	st->st_rdev = mstat.st_rdev;
	st->st_size = mstat.st_size;
	st->st_uid = mstat.st_uid;

	return ws;
}

/* you may wonder: why is this needed? can't we just use
 * _mktemp() even on UTF-8 encoded strings?
 *
 * well, you *can*, but it will bite you in the ass once
 * you get a string that has a mysterious "X" stored somewhere
 * in the filename; better to just give it as a wide string */
int win32_mktemp(char* template, size_t size) {
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)template, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	if (!_wmktemp(wc)) {
		free(wc);
		return -1;
	}

	/* still have to WideCharToMultiByte here */
	if (!WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, wc, -1, template, size, NULL, NULL)) {
		free(wc);
		return -1;
	}

	free(wc);
	return 0;
}

int win32_stat(const char* path, struct stat* st) {
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = win32_wstat(wc, st);
	free(wc);
	return ret;
}

int win32_open(const char* path, int flags) {
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = _wopen(wc, flags);
	free(wc);
	return ret;
}

FILE* win32_fopen(const char* path, const char* flags) {
	wchar_t* wc = NULL, *wc_flags = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T)
		|| charset_iconv((const uint8_t*)flags, (uint8_t**)&wc_flags, CHARSET_UTF8, CHARSET_WCHAR_T))
		return NULL;

	FILE* ret = _wfopen(wc, wc_flags);
	free(wc);
	free(wc_flags);
	return ret;
}

int win32_mkdir(const char *path, UNUSED mode_t mode) {
	wchar_t* wc = NULL;
	if (charset_iconv((const uint8_t*)path, (uint8_t**)&wc, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	int ret = _wmkdir(wc);
	free(wc);
	return ret;
}
#endif

unsigned long long file_size(const char *filename) {
	struct stat buf;

	if (os_stat(filename, &buf) < 0) {
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

int is_file(const char *filename)
{
	struct stat buf;

	if (os_stat(filename, &buf) == -1) {
		/* Well, at least we tried. */
		return 0;
	}

	return S_ISREG(buf.st_mode);
}

int is_directory(const char *filename)
{
	struct stat buf;

	if (os_stat(filename, &buf) == -1) {
		/* Well, at least we tried. */
		return 0;
	}

	return S_ISDIR(buf.st_mode);
}

char *get_current_directory(void)
{
#ifdef SCHISM_WIN32
	wchar_t buf[PATH_MAX + 1] = {L'\0'};
	uint8_t* buf_utf8 = NULL;

	if (_wgetcwd(buf, PATH_MAX) && !charset_iconv((uint8_t*)buf, &buf_utf8, CHARSET_WCHAR_T, CHARSET_UTF8))
		return (char*)buf_utf8;
#else
	char buf[PATH_MAX + 1] = {'\0'};

	/* hmm. fall back to the current dir */
	if (getcwd(buf, PATH_MAX))
		return str_dup(buf);
#endif
	return str_dup(".");
}

/* this function is horrible */
char *get_home_directory(void)
{
#if defined(__amigaos4__)
	return str_dup("PROGDIR:");
#elif defined(SCHISM_WIN32)
	wchar_t buf[PATH_MAX + 1] = {L'\0'};
	uint8_t* buf_utf8 = NULL;

	if (SHGetFolderPathW(NULL, CSIDL_PERSONAL, NULL, 0, buf) == S_OK && !charset_iconv((uint8_t*)buf, &buf_utf8, CHARSET_WCHAR_T, CHARSET_UTF8))
		return (char*)buf_utf8;
#else
	char *ptr = getenv("HOME");
	if (ptr)
		return str_dup(ptr);
#endif

	/* hmm. fall back to the current dir */
	char* path = get_current_directory();
	if (!strcmp(path, "."))
		return path;

	free(path);

	/* still don't have a directory? sheesh. */
	return str_dup(FALLBACK_DIR);
}

char *get_dot_directory(void)
{
#ifdef SCHISM_WIN32
	wchar_t buf[PATH_MAX + 1] = {L'\0'};
	uint8_t* buf_utf8 = NULL;
	if (SHGetFolderPathW(NULL, CSIDL_APPDATA, NULL, 0, buf) == S_OK
		&& charset_iconv((uint8_t*)buf, &buf_utf8, CHARSET_WCHAR_T, CHARSET_UTF8))
		return (char*)buf_utf8;

	// else fall back to home (but if this ever happens, things are really screwed...)
#endif
	return get_home_directory();
}

int str_count_occurences(char character, const char* str)
{
	int count = 0;

	for (int i = 0; str[i]; i++) {
		if (str[i] == character)
			count++;
	}

	return count;
}

// char* str_concat(const char* input_str, ...)
// {
// 	char* out = strdup(input_str);
// 	int len = 0;

// 	va_list ap;
// 	va_start(ap, input_str);
// 	while (1) {
// 		const char* current_arg = va_arg(ap, const char*);
// 		if (!current_arg || !current_arg[0]) break;
// 		len += strlen(current_arg);
// 		out = realloc(out, len + 1); // mem_realloc(out, len + 1);
// 		strcat(out, current_arg);
// 	}
// 	va_end(ap);

// 	return out;
// }

char* str_concat_array(int count, char** str_array, int free_inputs)
{
	int len = 0;

	for(int i = 0; i < count; i++) {
		if (!str_array[i] || !str_array[i][0]) continue;
		len += strlen(str_array[i]);
	}

	if(len == 0) return "";

	char* out = malloc((len + 1) * sizeof(char));
	out[0] = '\0';

	for(int i = 0; i < count; i++) {
		if (!str_array[i] || !str_array[i][0]) continue;
		strcat(out, str_array[i]);
		if (free_inputs)
			free(str_array[i]);
	}

	return out;
}

char* str_concat_two(char* str1, char* str2, int free_inputs) {
	char* strings[2] = { str1, str2 };
	return str_concat_array(2, strings, free_inputs);
}

char* str_concat_three(char* str1, char* str2, char* str3, int free_inputs) {
	char* strings[3] = { str1, str2, str3 };
	return str_concat_array(3, strings, free_inputs);
}

char* str_concat_four(char* str1, char* str2, char* str3, char* str4, int free_inputs) {
	char* strings[4] = { str1, str2, str3, str4 };
	return str_concat_array(4, strings, free_inputs);
}

char* str_concat_five(char* str1, char* str2, char* str3, char* str4, char* str5, int free_inputs) {
	char* strings[5] = { str1, str2, str3, str4, str5 };
	return str_concat_array(5, strings, free_inputs);
}

char* str_concat_six(char* str1, char* str2, char* str3, char* str4, char* str5, char* str6, int free_inputs) {
	char* strings[6] = { str1, str2, str3, str4, str5, str6 };
	return str_concat_array(6, strings, free_inputs);
}

char* str_concat_with_delim(int count, char** str_array, const char* delim, int free_inputs)
{
	int len = 0;
	int delim_len = strlen(delim);
	int actual_count = 0;

	for (int i = 0; i < count; i++) {
		if (!str_array[i] || !str_array[i][0]) continue;
		len += strlen(str_array[i]);
		actual_count++;
	}

	if (len == 0 || actual_count == 0) return "";

	char* out = malloc((len + (actual_count - 1) * delim_len + 1) * sizeof(char));
	out[0] = '\0';
	int current_count = 0;

	for (int i = 0; i < count; i++) {
		if (!str_array[i] || !str_array[i][0]) continue;
		strcat(out, str_array[i]);
		current_count++;
		if (current_count != actual_count)
			strcat(out, delim);
		if (free_inputs)
			free(str_array[i]);
	}

	return out;
}

int utf8_length(const char* str)
{
	int count = 0;

	for(int i = 0; str[i]; i++) {
		if ((str[i] & 0x80) == 0 || (str[i] & 0xc0) == 0xc0)
			count++;
	}

	return count;
}

char* str_pad_between(char* str1, char* str2, char pad, int width, int min_padding, int free_inputs)
{
	int len1 = strlen(str1);
	int len2 = strlen(str2);
	int len_both = len1 + len2;

	int len1_utf8 = utf8_length(str1);
	int len2_utf8 = utf8_length(str2);
	int len_both_utf8 = len1_utf8 + len2_utf8;
	int len_padding = width - len_both_utf8;

	if (len_padding < 0) len_padding = 0;
	if (len_padding < min_padding) len_padding = min_padding;

	int len_out = len_both + len_padding;
	char* out = malloc((len_out + 1) * sizeof(char));
	out[0] = '\0';

	strcat(out, str1);

	int end_padding = len1 + len_padding;

	for(int i = len1; i < len1 + len_padding; i++) {
		out[i] = pad;
	}

	out[end_padding] = '\0';

	strcat(out, str2);

	if(free_inputs) {
		free(str1);
		free(str2);
	}

	return out;
}

void str_to_upper(char *s)
{
    for (int i = 0; s[i]; i++)
        s[i] = toupper((unsigned char)s[i]);
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
#ifdef SCHISM_WIN32
	wchar_t cwd[PATH_MAX] = {L'\0'};
	const wchar_t *cmd = NULL;
	wchar_t batch_file[PATH_MAX] = {L'\0'};
	struct stat sb = {0};
	int r;

	if (!GetCurrentDirectoryW(PATH_MAX-1, cwd))
		return 0;

	wchar_t* name_w = NULL;
	if (charset_iconv((const uint8_t*)name, (uint8_t**)&name_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	size_t name_len = wcslen(name_w);
	wcsncpy(batch_file, name_w, name_len);
	wcscpy(&batch_file[name_len], L".bat");

	free(name_w);

	wchar_t* dir_w = NULL;
	if (charset_iconv((const uint8_t*)dir, (uint8_t**)&dir_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	if (_wchdir(dir_w) == -1) {
		free(dir_w);
		return 0;
	}

	free(dir_w);

	wchar_t* maybe_arg_w = NULL;
	if (charset_iconv((const uint8_t*)maybe_arg, (uint8_t**)&maybe_arg_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return 0;

	if (win32_wstat(batch_file, &sb) == -1) {
		r = 0;
	} else {
		cmd = _wgetenv(L"COMSPEC");
		if (!cmd)
			cmd = L"command.com";

		r = _wspawnlp(_P_WAIT, cmd, cmd, "/c", batch_file, maybe_arg_w, 0);
	}

	free(maybe_arg_w);

	_wchdir(cwd);
	if (r == 0) return 1;
	return 0;
#elif defined(SCHISM_WII)
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
#ifdef SCHISM_WIN32
	/* do nothing; this is already handled in rename_file */
	return -1;
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

#ifdef SCHISM_WIN32
	wchar_t* old_w = NULL, *new_w = NULL;
	if (charset_iconv((const uint8_t*)new, (uint8_t**)&new_w, CHARSET_UTF8, CHARSET_WCHAR_T)
		|| charset_iconv((const uint8_t*)old, (uint8_t**)&old_w, CHARSET_UTF8, CHARSET_WCHAR_T)) {
		free(old_w);
		free(new_w);
		return -1;
	}

	if (MoveFileExW(old_w, new_w, overwrite ? MOVEFILE_REPLACE_EXISTING : 0)) {
		/* yay */
		free(new_w);
		free(old_w);
		return 0;
	}

	free(new_w);
	free(old_w);

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

