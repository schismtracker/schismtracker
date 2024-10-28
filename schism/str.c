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

#include "mem.h"
#include "str.h"
#include "util.h"
#include "charset.h"

#include <stddef.h>

/* --------------------------------------------------------------------- */
/* FORMATTING FUNCTIONS */

char *str_date_from_tm(struct tm *tm, char buf[27])
{
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

	snprintf(buf, 27, "%s %d, %d", month_str[tm->tm_mon], tm->tm_mday, 1900 + tm->tm_year);

	return buf;
}

char *str_time_from_tm(struct tm *tm, char buf[27])
{
	snprintf(buf, 27, "%d:%02d%s", (tm->tm_hour % 12) ? (tm->tm_hour % 12) : 12, tm->tm_min, tm->tm_hour < 12 ? "am" : "pm");

	return buf;
}

char *str_from_date(time_t when, char buf[27])
{
	struct tm tmr;

	/* DO NOT change this back to localtime(). If some backward platform
	 * doesn't have localtime_r, it needs to be implemented separately. */
	localtime_r(&when, &tmr);

	return str_date_from_tm(&tmr, buf);
}

char *str_from_time(time_t when, char buf[27])
{
	struct tm tmr;

	localtime_r(&when, &tmr);

	return str_time_from_tm(&tmr, buf);
}

char *str_from_num99(int n, char *buf)
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
char *str_from_num(int digits, unsigned int n, char *buf)
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
char *str_from_num_signed(int digits, int n, char *buf)
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

static const char *whitespace = " \t\v\r\n";

int str_ltrim(char *s)
{
	int ws = strspn(s, whitespace);
	int len = strlen(s) - ws;

	if (ws)
		memmove(s, s + ws, len + 1);
	return len;
}

int str_rtrim(char *s)
{
	int len = strlen(s);

	while (--len > 0 && strchr(whitespace, s[len]));

	s[++len] = '\0';
	return len;
}

int str_trim(char *s)
{
	str_ltrim(s);
	return str_rtrim(s);
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

char *str_pretty_name(const char *filename)
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

	str_trim(ret);
	return ret;
}

/* blecch */
int str_get_num_lines(const char *text)
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

size_t str_count_occurrences(char character, const char *str)
{
	size_t count = 0;

	for (; *str; count += (*str == character), str++);

	return count;
}

char *str_concat(size_t count, const char **str_array)
{
	size_t str_array_lens[count];
	size_t len;
	size_t i, c;

	for (len = 0, i = 0; i < count; i++)
		len += (str_array_lens[i] = (str_array[i] && *str_array[i]) ? strlen(str_array[i]) : 0);

	if (!len)
		return str_dup("");

	char *out = malloc(len + 1);

	for (i = 0, c = 0; i < count && c < len; i++) {
		if (!str_array[i] || !*str_array[i])
			continue;

		memcpy(out + c, str_array[i], str_array_lens[i]);
		c += str_array_lens[i];
	}

	out[len] = '\0';

	return out;
}

char *str_concat_free(size_t count, char **str_array)
{
	char* out = str_concat(count, (const char **)str_array);

	for (size_t i = 0; i < count; i++)
		free(str_array[i]);

	return out;
}

char *str_implode(size_t count, const char *delim, const char **str_array)
{
	const char *str_array_with_delims[count * 2];
	memset(str_array_with_delims, 0, count * 2);

	size_t i, c;

	for (i = 0, c = 0; i < count; i++) {
		if (!str_array[i] || !str_array[i][0])
			continue;

		str_array_with_delims[c++] = str_array[i];
		str_array_with_delims[c++] = delim;
	}

	/* welp */
	if (!c)
		return str_dup("");

	/* decrement c by one so we don't get an extra delim */
	return str_concat(--c, str_array_with_delims);
}

char *str_implode_free(size_t count, const char *delim, char **str_array)
{
	char *out = str_implode(count, delim, (const char **)str_array);

	for (size_t i = 0; i < count; i++)
		free(str_array[i]);

	return out;
}

char *str_pad_between(const char* str1, const char* str2, unsigned char pad, int width, int min_padding)
{
	size_t len1 = strlen(str1), len2 = strlen(str2);

	/* ptrdiff_t is close enough to a signed size_t */
	ptrdiff_t len_padding = (ptrdiff_t)width - (charset_strlen(str1, CHARSET_UTF8) + charset_strlen(str2, CHARSET_UTF8));
	len_padding = MAX(len_padding, min_padding);

	char *out = malloc(len1 + len_padding + len2 + 1);

	memcpy(out, str1, len1);
	memset(out + len1, pad, len_padding);
	memcpy(out + len1 + len_padding, str2, len2);

	out[len1 + len_padding + len2] = '\0';

	return out;
}
