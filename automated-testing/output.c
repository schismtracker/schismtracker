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

#include "automated-testing.h"

#include "headers.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#define BUFFER_SIZE 1024

struct test_output_buffer {
	char data[BUFFER_SIZE];
	int used;
	struct test_output_buffer *next;
} head = {0}, *tail = &head;

void test_output_clear(void)
{
	struct test_output_buffer *trace = head.next;

	while (trace && trace->next)
	{
		struct test_output_buffer *next_trace = trace->next;

		free(trace);

		trace = next_trace;
	}

	head.next = NULL;
	head.used = 0;
}

void test_output(const char *str)
{
	int chars = strlen(str);

	while (chars > 0) {
		int remaining = BUFFER_SIZE - tail->used;

		if (remaining > chars)
			remaining = chars;

		memcpy(tail->data + tail->used, str, remaining);

		tail->used += remaining;
		str += remaining;

		chars -= remaining;

		if ((tail->used == BUFFER_SIZE) && (chars > 0)) {
			tail->next = (struct test_output_buffer *)malloc(sizeof(struct test_output_buffer));
			tail = tail->next;

			memset(tail, 0, sizeof(struct test_output_buffer));
		}
	}
}

void test_printf(const char *fmt, ...)
{
	va_list args;
	char *formatted;
	int n;

	va_start(args, fmt);
	n = vasprintf(&formatted, fmt, args);

	test_output(formatted);

	free(formatted);
}

void test_dump_output(void)
{
	struct test_output_buffer *trace = &head;

	if (trace->used) {
		printf("\nTEST OUTPUT: ");

		while (trace != NULL) {
			fwrite(trace->data, 1, trace->used, stdout);
			trace = trace->next;
		}

		printf("\n\n");
	}
}

