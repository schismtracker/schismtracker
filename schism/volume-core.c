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
#include "headers.h"

#include "util.h"
#include "sdlmain.h"
#include "osdefs.h"

static int (*__volume_get_max)(void) = NULL;
static void (*__volume_read)(int *left, int *right) = NULL;
static void (*__volume_write)(int left, int right) = NULL;


void volume_setup(void)
{
        char *drv, drv_buf[256];

        drv = SDL_AudioDriverName(drv_buf,sizeof(drv_buf));

#ifdef USE_ALSA
        if ((!drv && !__volume_get_max)
            || (drv && (!strcmp(drv, "alsa")))) {
                __volume_get_max = alsa_volume_get_max;
                __volume_read = alsa_volume_read;
                __volume_write = alsa_volume_write;
        }
#endif
#ifdef USE_OSS
        if ((!drv && !__volume_get_max)
            || (drv && (!strcmp(drv, "oss") || !strcmp(drv, "dsp")))) {
                __volume_get_max = oss_volume_get_max;
                __volume_read = oss_volume_read;
                __volume_write = oss_volume_write;
        }
#endif
#ifdef MACOSX
        if ((!drv && !__volume_get_max)
            || (drv && (!strcmp(drv, "coreaudio") || !strcmp(drv, "macosx")))) {
                __volume_get_max = macosx_volume_get_max;
                __volume_read = macosx_volume_read;
                __volume_write = macosx_volume_write;
        }
#endif
#ifdef WIN32
        if ((!drv && !__volume_get_max)
            || (drv && (!strcmp(drv, "waveout") || !strcmp(drv, "dsound")))) {
                __volume_get_max = win32mm_volume_get_max;
                __volume_read = win32mm_volume_read;
                __volume_write = win32mm_volume_write;
        }
#endif
}


int volume_get_max(void)
{
        if (__volume_get_max) return __volume_get_max();
        return 1; /* Can't return 0, that breaks things. */
}
void volume_read(int *left, int *right)
{
        if (__volume_read) __volume_read(left,right);
        else { *left=0; *right=0; }
}
void volume_write(int left, int right)
{
        if (__volume_write) __volume_write(left,right);
}
