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

#ifdef MACOSX

#include <CoreServices/CoreServices.h>
#include <CoreAudio/AudioHardware.h>

int macosx_volume_get_max(void);
int macosx_volume_get_max(void)
{
	return 65535;
}

void macosx_volume_read(int *left, int *right);
void macosx_volume_read(int *left, int *right)
{
	UInt32 size;
	AudioDeviceID od;
	OSStatus e;
	UInt32 ch[2];
	Float32 fl[2];
	int i;

	if (left) *left = 0;
	if (right) *right = 0;

	size=sizeof(od);
	e = AudioHardwareGetProperty(kAudioHardwarePropertyDefaultOutputDevice,
			&size, &od);
	if (e != 0) return;

	size=sizeof(ch);
	e = AudioDeviceGetProperty(od,
		0, /* QA1016 says "0" is master channel */
		false,
		kAudioDevicePropertyPreferredChannelsForStereo,
		&size,
		&ch);
	if (e != 0) return;

	for (i = 0; i < 2; i++) {
		size = sizeof(Float32);
		e = AudioDeviceGetProperty(od, /* device */
			ch[i], /* preferred stereo channel */
			false, /* output device */
			kAudioDevicePropertyVolumeScalar,
			&size,
			&fl[i]);
		if (e != 0) return;
	}
	if (left) *left = fl[0] * 65536.0f;
	if (right) *right = fl[1] * 65536.0f;
}

void macosx_volume_write(int left, int right);
void macosx_volume_write(int left, int right)
{
	/* XXX the code that used to be here changed the master volume of the
	   OS mixer, which is stupid; better to do nothing at all like some of
	   the other broken code (pulseaudio?) */
	return;
}

#endif
