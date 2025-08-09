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

#ifndef SCHISM_TEST_TEMPFILE_H_
#define SCHISM_TEST_TEMPFILE_H_

/* must end with the literal string "XXXXXX" (mkfstemp) */
#define TEST_TEMP_FILE_NAME_TEMPLATE "test_tmp_XXXXXX"
#define TEST_TEMP_FILE_NAME_LENGTH (sizeof(TEST_TEMP_FILE_NAME_TEMPLATE))

int test_temp_file(char temp_file[TEST_TEMP_FILE_NAME_LENGTH], const char *template, int template_length);
void test_temp_files_cleanup(void);

#endif /* SCHISM_TEST_TEMPFILE_H_ */
