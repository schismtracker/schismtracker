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

#include "it.h"

#if HAVE_LIBBZ2
# include <bzlib.h>
#else
# error "TODO: gzip/null help text compression"
#endif

/* --------------------------------------------------------------------- */

/* compressed_help_text and UNCOMPRESSED_HELP_TEXT_SIZE are defined here */
#include "helptext.h"

static char uncompressed_help_text[UNCOMPRESSED_HELP_TEXT_SIZE];

char *help_text_pointers[HELP_NUM_ITEMS] = { NULL };

/* --------------------------------------------------------------------- */

static void decompress_help_text(void)
{
        int insize = sizeof(compressed_help_text);
        int outsize = sizeof(uncompressed_help_text);
        int ret = BZ2_bzBuffToBuffDecompress
                (uncompressed_help_text, &outsize,
                 compressed_help_text, insize, 0, 0);

        if (ret != BZ_OK) {
                fprintf(stderr, "error decompressing help text:"
                        " libbz2 returned %d\n", ret);
                exit(1);
        }
}

/* --------------------------------------------------------------------- */

void setup_help_text_pointers(void)
{
        int n;
        char *ptr = uncompressed_help_text;

        decompress_help_text();
        for (n = 0; n < HELP_NUM_ITEMS; n++) {
                help_text_pointers[n] = ptr;
                ptr = strchr(ptr, 0) + 1;
        }
}
