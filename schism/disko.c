/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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

/*
mrsbrisby:

thanks for obfuscating this code, it was really fun untangling
all of those nondescript function names like "p" and "wl".
especially the ones that weren't actually used anywhere, those
were a lot of fun to figure out.
*/

#define NEED_BYTESWAP
#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"
#include "util.h"
#include "song.h"
#include "mplink.h"
#include "dmoz.h"


#include "cmixer.h"
#include "disko.h"

#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

/* this buffer is used for diskwriting; stdio seems to use a much
 * smaller buffer which is PAINFULLY SLOW on devices with -o sync
 * (XXX should there be a separate buffer per file descriptor?)
 */
static char dwbuf[65536];



/* these are used for pattern->sample linking crap */

int disko_writeout_sample(UNUSED int sampno, UNUSED int patno, UNUSED int dobind)
{
        return DW_ERROR;
}

int disko_writeout(UNUSED const char *file, UNUSED disko_t *f)
{
        return DW_ERROR;
}



disko_t *disko_open(UNUSED const char *filename)
{
        //int r;
        //disko_t *f = calloc(1, sizeof(disko_t));
        printf("bailing\n");
        return NULL;
}

int disko_close(disko_t *ds)
{
        //int err = disko_finish();
        free(ds);
        return DW_ERROR;
}


/* main calls this periodically when the .wav exporter is busy */
int disko_sync(void)
{
        return DW_SYNC_DONE;
}

/* called from main and disko_dialog */
int disko_finish(void)
{
        return DW_NOT_RUNNING; /* no writer running */
}


void disko_write(disko_t *ds, const void *buf, unsigned int len)
{
        return ds->_write(ds, buf, len);
}

void disko_seek(disko_t *ds, off_t pos, int whence)
{
        return ds->_seek(ds, pos, whence);
}

void disko_seterror(disko_t *ds)
{
        return ds->_seterror(ds);
}

/* called from audio_playback.c _schism_midi_out_raw() */
int _disko_writemidi(UNUSED const void *data, UNUSED unsigned int len, UNUSED unsigned int delay)
{
        return DW_ERROR;
}


unsigned int disko_output_rate = 44100;
unsigned int disko_output_bits = 16;
unsigned int disko_output_channels = 2;

