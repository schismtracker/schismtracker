/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#include "mixer.h"
#include "util.h"
#include "SDL.h"

static int (*__mixer_get_max_volume)(void) = NULL;
static void (*__mixer_read_volume)(int *left, int *right) = NULL;
static void (*__mixer_write_volume)(int left, int right) = NULL;

#ifdef USE_ALSA
extern int alsa_mixer_get_max_volume(void);
extern void alsa_mixer_read_volume(int *, int *);
extern void alsa_mixer_write_volume(int, int);
#endif

#ifdef USE_OSS
extern int oss_mixer_get_max_volume(void);
extern void oss_mixer_read_volume(int *, int *);
extern void oss_mixer_write_volume(int, int);
#endif

#ifdef MACOSX
extern int macosx_mixer_get_max_volume(void);
extern void macosx_mixer_read_volume(int *, int *);
extern void macosx_mixer_write_volume(int, int);
#endif

#ifdef USE_WIN32MM
extern int win32mm_mixer_get_max_volume(void);
extern void win32mm_mixer_read_volume(int *, int *);
extern void win32mm_mixer_write_volume(int, int);
#endif

void mixer_setup(void)
{
	char *drv, drv_buf[256];

	drv = SDL_AudioDriverName(drv_buf,sizeof(drv_buf));

#ifdef USE_ALSA
	if ((!drv && !__mixer_get_max_volume) || (drv && !strcmp(drv, "alsa"))) {
		__mixer_get_max_volume = alsa_mixer_get_max_volume;
		__mixer_read_volume = alsa_mixer_read_volume;
		__mixer_write_volume = alsa_mixer_write_volume;
	}
#endif
#ifdef USE_OSS
	if ((!drv && !__mixer_get_max_volume) || (drv && !strcmp(drv, "oss"))
					|| (drv && !strcmp(drv, "dsp"))) {
		__mixer_get_max_volume = oss_mixer_get_max_volume;
		__mixer_read_volume = oss_mixer_read_volume;
		__mixer_write_volume = oss_mixer_write_volume;
	}
#endif
#ifdef MACOSX
	if ((!drv && !__mixer_get_max_volume) || (drv && (!strcmp(drv, "coreaudio") || !strcmp(drv, "macosx")) )) {
		__mixer_get_max_volume = macosx_mixer_get_max_volume;
		__mixer_read_volume = macosx_mixer_read_volume;
		__mixer_write_volume = macosx_mixer_write_volume;
	}
#endif
#ifdef USE_WIN32MM
	if ((!drv && !__mixer_get_max_volume) || (drv && (!strcmp(drv, "waveout") || !strcmp(drv, "dsound")) )) {
		__mixer_get_max_volume = win32mm_mixer_get_max_volume;
		__mixer_read_volume = win32mm_mixer_read_volume;
		__mixer_write_volume = win32mm_mixer_write_volume;
	}
#endif
}


int mixer_get_max_volume(void)
{
	if (__mixer_get_max_volume) return __mixer_get_max_volume();
	return 1; /* Can't return 0, that breaks things. */
}
void mixer_read_volume(int *left, int *right)
{
	if (__mixer_read_volume) __mixer_read_volume(left,right);
	else { *left=0; *right=0; }
}
void mixer_write_volume(int left, int right)
{
	if (__mixer_write_volume) __mixer_write_volume(left,right);
}
