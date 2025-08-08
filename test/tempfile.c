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

#include "test-tempfile.h"

int test_temp_file(char **temp_file, const char *template, int template_length)
{
	*temp_file = strdup("test_tmp_XXXXXX");

	int fd = mkstemp(*temp_file);

	if (fd < 0)
	{
		free(*temp_file);
		*temp_file = NULL;
		return 0;
	}

	int remaining = 0;

	if (template != NULL) {
		if (template_length < 0)
			template_length = strlen(template);

		const char *buf = template;
		remaining = template_length;

		while (1) {
			int num_written = write(fd, buf, remaining);

			if (num_written <= 0)
				break;

			remaining -= num_written;
		}
	}

	close(fd);

	if (remaining == 0) {
		FILE *log = fopen("test_tmp.lst", "ab");

		if (log) {
			fputs(*temp_file, log);
			fclose(log);
		}

		return 1;
	} else {
		unlink(*temp_file);
		free(*temp_file);
		return 0;
	}
}

void test_temp_files_cleanup(void)
{
	FILE *log = fopen("test_tmp.lst", "rb");

	if (log) {
		char temp_file[50];

		while (1) {
			if (fgets(temp_file, 50, log) == NULL)
				break;

			// Silly fgets, includes the line terminator
			temp_file[strcspn(temp_file, "\r\n")] = '\0';

			unlink(temp_file);
		}

		unlink("test_tmp.lst");
	}
}
