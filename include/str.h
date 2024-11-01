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

#ifndef SCHISM_STR_H_
#define SCHISM_STR_H_

#include "headers.h"

#include <sys/types.h>

/* formatting */
char *str_date_from_tm(struct tm *tm, char buf[27]);
char *str_time_from_tm(struct tm *tm, char buf[27]);
char *str_from_date(time_t when, char buf[27]);
char *str_from_time(time_t when, char buf[27]);
char *str_from_num(int digits, unsigned int n, char *buf); // what size
char *str_from_num_signed(int digits, int n, char *buf);   // buffer do
char *str_from_num99(int n, char *buf);                    // these use?

/* string handling */
int str_ltrim(char *s); // return: length of string after trimming
int str_rtrim(char *s); // ditto
int str_trim(char *s);  // ditto
int str_break(const char *s, char c, char **first, char **second);
char *str_escape(const char *source, int space_hack);
char *str_unescape(const char *source);
char *str_pretty_name(const char *filename);
int str_get_num_lines(const char *text);

/* Count how many times character is in str */
size_t str_count_occurrences(char character, const char* str);

/* Concatenates strings in array. count is size of array. str_array is the array. */
char *str_concat(size_t count, const char** str_array);
/* Concatenates strings in array. count is size of array. str_array is the array. Frees the strings in the array. */
char *str_concat_free(size_t count, char** str_array);

/* variadic macros for the above function, so one can write e.g. STR_CONCAT(3, "123", "123", "123") and receive "123123123" */
#define STR_CONCAT(count, ...) \
	str_concat((count), (const char *[]){ __VA_ARGS__ })

#define STR_CONCAT_FREE(count, ...) \
	str_concat_free((count), (char *[]){ __VA_ARGS__ })

/* The following two functions are named after the php implode() function. */

/* Concatenates strings in array, putting a delimiter between them. count is size of array. str_array is the array. */
char *str_implode(size_t count, const char* delim, const char** str_array);
/* Concatenates strings in array, putting a delimiter between them. count is size of array. str_array is the array. Frees the strings in the array. */
char *str_implode_free(size_t count, const char* delim, char** str_array);

#define STR_IMPLODE(count, delim, ...) \
	str_implode((count), (delim), (const char *[]){ __VA_ARGS__ })

#define STR_IMPLODE_FREE(count, delim, ...) \
	str_implode_free((count), (delim), (char *[]){ __VA_ARGS__ })

/* pad will be placed between str1 and str2 until width is reached.
 * !!! THIS FUNCTION ASSUMES PROPER UTF-8 ENCODED STRINGS !!! */
char *str_pad_between(const char* str1, const char* str2, unsigned char pad, int width, int min_padding);
#define str_pad_end(str, pad, width) str_pad_between(str, "", pad, width, 0)

#endif