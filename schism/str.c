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

/* --------------------------------------------------------------------- */
/* Functions for working with Pascal strings. */

void str_to_pascal(const char *cstr, unsigned char pstr[256], int *truncated)
{
	const size_t len = strlen(cstr);

	if (truncated)
		*truncated = (len > 255);

	pstr[0] = MIN(len, 255);
	memcpy(&pstr[1], cstr, pstr[0]);
}

void str_from_pascal(const unsigned char pstr[256], char cstr[256])
{
	memcpy(cstr, pstr + 1, pstr[0]);
	cstr[pstr[0]] = 0;
}

/* --------------------------------------------------------------------- */

/* if len is zero, this function calls strlen to get the input's
 * length.
 *
 * The input will be free'd if the input isn't a null pointer,
 * so make sure you initialize your strings properly ;)
 *
 * returns 0 on fail or 1 on success */
int str_realloc(char **output, const char *input, size_t len)
{
	if (*output)
		free(*output);
	*output = (len) ? strn_dup(input, len) : str_dup(input);
	if (!*output)
		return 0;
	return 1;
}
