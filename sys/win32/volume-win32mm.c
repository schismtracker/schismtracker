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

#include "util.h"
#include "osdefs.h"

#ifndef WIN32
# error Why do you want to build this if you do not intend to use it?
#endif

#include <windows.h>
#include <mmsystem.h>

/*  Note: [Gargaj]

      WinMM DOES support max volumes up to 65535, but the scroller is
      so goddamn slow and it only supports 3 digits anyway, that
      it doesn't make any sense to keep the precision.
*/

int win32mm_volume_get_max(void)
{
        return 0xFF;
}

static HWAVEOUT open_mixer(void)
{
        HWAVEOUT hwo=NULL;
        WAVEFORMATEX pwfx;
#if 0
        pwfx.wFormatTag = WAVE_FORMAT_UNKNOWN;
        pwfx.nChannels = 0;
        pwfx.nSamplesPerSec = 0;
        pwfx.wBitsPerSample = 0;
        pwfx.nBlockAlign = 0;
        pwfx.nAvgBytesPerSec = 0;
        pwfx.cbSize = 0;
#else
        pwfx.wFormatTag = WAVE_FORMAT_PCM;
        pwfx.nChannels = 1;
        pwfx.nSamplesPerSec = 44100;
        pwfx.wBitsPerSample = 8;
        pwfx.nBlockAlign = 4;
        pwfx.nAvgBytesPerSec = 44100*1*1;
        pwfx.cbSize = 0;
#endif
        if (waveOutOpen(&hwo, WAVE_MAPPER, &pwfx, 0, 0, CALLBACK_NULL)!=MMSYSERR_NOERROR)
                return NULL;
        return hwo;
}

void win32mm_volume_read(int *left, int *right)
{
        DWORD vol;
        HWAVEOUT hwo=open_mixer();

        *left = *right = 0;
        if (!hwo) return;

        waveOutGetVolume(hwo,&vol);

        *left = (vol & 0xFFFF) >> 8;
        *right = (vol >> 16) >> 8;

        waveOutClose(hwo);
}

void win32mm_volume_write(int left, int right)
{
        DWORD vol = ((left & 0xFF)<<8) | ((right & 0xFF)<<(16+8));
        HWAVEOUT hwo = open_mixer();
        if (!hwo) return;

        waveOutSetVolume(hwo,vol);

        waveOutClose(hwo);
}
