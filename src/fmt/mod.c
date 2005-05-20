/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
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
#include "fmt.h"

/* --------------------------------------------------------------------- */

/* TODO: WOW files */

/* Ugh. */
static const char *valid_tags[][2] = {
	/* M.K. must be the first tag! (to test for WOW files) */
	/* the first 5 descriptions are a bit weird */
	{"M.K.", "Amiga-NewTracker"},
	{"M!K!", "Amiga-ProTracker"},
	{"FLT4", "4 Channel Startrekker"}, /* xxx */
	{"CD81", "8 Channel Falcon"},      /* "Falcon"? */
	{"FLT8", "8 Channel Startrekker"}, /* xxx */

	{"8CHN", "8 Channel MOD"},  /* what is the difference */
	{"OCTA", "8 Channel MOD"},  /* between these two? */
	{"TDZ1", "1 Channel MOD"},
	{"2CHN", "2 Channel MOD"},
	{"TDZ2", "2 Channel MOD"},
	{"TDZ3", "3 Channel MOD"},
	{"5CHN", "5 Channel MOD"},
	{"6CHN", "6 Channel MOD"},
	{"7CHN", "7 Channel MOD"},
	{"9CHN", "9 Channel MOD"},
	{"10CH", "10 Channel MOD"},
	{"11CH", "11 Channel MOD"},
	{"12CH", "12 Channel MOD"},
	{"13CH", "13 Channel MOD"},
	{"14CH", "14 Channel MOD"},
	{"15CH", "15 Channel MOD"},
	{"16CH", "16 Channel MOD"},
	{"18CH", "18 Channel MOD"},
	{"20CH", "20 Channel MOD"},
	{"22CH", "22 Channel MOD"},
	{"24CH", "24 Channel MOD"},
	{"26CH", "26 Channel MOD"},
	{"28CH", "28 Channel MOD"},
	{"30CH", "30 Channel MOD"},
	{"32CH", "32 Channel MOD"},
	{NULL, NULL}
};

bool fmt_mod_read_info(dmoz_file_t *file, const byte *data, size_t length)
{
	char tag[5];
	int i = 0;

	if (length < 1085)
		return false;

	memcpy(tag, data + 1080, 4);
	tag[4] = 0;

	for (i = 0; valid_tags[i][0] != NULL; i++) {
		if (strcmp(tag, valid_tags[i][0]) == 0) {
			/* if (i == 0) {
				Might be a .wow; need to calculate some crap to find out for sure.
				For now, since I have no wow's, I'm not going to care.
			} */

			file->description = valid_tags[i][1];
			/*file->extension = strdup("mod");*/
			file->title = calloc(21, sizeof(char));
			memcpy(file->title, data, 20);
			file->title[20] = 0;
			file->type = TYPE_MODULE_MOD;
			return true;
		}
	}

	return false;
}
