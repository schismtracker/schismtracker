/* Copyright (C) 1991, 1992, 1996, 1998 Free Software Foundation, Inc.
   This file is part of the GNU C Library.

   The GNU C Library is free software; you can redistribute it and/or
   modify it under the terms of the GNU Library General Public License as
   published by the Free Software Foundation; either version 2 of the
   License, or (at your option) any later version.

   The GNU C Library is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Library General Public License for more details.

   You should have received a copy of the GNU Library General Public
   License along with the GNU C Library; see the file COPYING.LIB.  If not,
   write to the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
   Boston, MA 02111-1307, USA.  */

#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/time.h>

#include <stdint.h>
typedef uint64_t big_type;

/* this ifdef crap lifted from slurp
This is a hack!! We need binary mode files, but the 'b' flag to fdopen is
evidently useless, so I'm doing it at this level instead. Dumb. */
#ifndef O_BINARY
# ifdef O_RAW
#  define O_BINARY O_RAW
# else
#  define O_BINARY 0
# endif
#endif

int mkstemp(char *template);

/* Generate a unique temporary file name from TEMPLATE.
   The last six characters of TEMPLATE must be "XXXXXX";
   they are replaced with a string that makes the filename unique.
   Returns a file descriptor open on the file for reading and writing.  */
int mkstemp(char *template)
{
	static const char letters[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
	static big_type value;
	struct timeval tv;
	char *XXXXXX;
	size_t len;
	int count;

	len = strlen(template);
	if (len < 6 || strcmp(&template[len - 6], "XXXXXX")) {
		errno = EINVAL;
		return -1;
	}

	/* This is where the Xs start.  */
	XXXXXX = &template[len - 6];

	/* Get some more or less random data.  */
	gettimeofday(&tv, NULL);
	value += ((big_type)tv.tv_usec << 16) ^ tv.tv_sec ^ getpid();

	for (count = 0; count < TMP_MAX; ++count) {
		big_type v = value;
		int fd;

		/* Fill in the random bits.  */
		XXXXXX[0] = letters[v % 62];
		v /= 62;
		XXXXXX[1] = letters[v % 62];
		v /= 62;
		XXXXXX[2] = letters[v % 62];
		v /= 62;
		XXXXXX[3] = letters[v % 62];
		v /= 62;
		XXXXXX[4] = letters[v % 62];
		v /= 62;
		XXXXXX[5] = letters[v % 62];

		fd = open(template, O_RDWR | O_CREAT | O_EXCL | O_BINARY, 0600);
		if (fd >= 0) /* The file does not exist.  */
			return fd;

		/* This is a random value.  It is only necessary that the next
	 TMP_MAX values generated by adding 7777 to VALUE are different
	 with (module 2^32).  */
		value += 7777;
	}

	/* We return the null string if we can't find a unique file name.  */
	template[0] = '\0';
	return -1;
}
