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
#include "sdlmain.h"

#include <windows.h>

/* eek... */
void win32_get_modkey(int *mk)
{
	BYTE ks[256];
	if (GetKeyboardState(ks) == 0) return;

	(*mk) = ((*mk) & ~(KMOD_NUM|KMOD_CAPS))
		| ((ks[VK_NUMLOCK]&1) ? KMOD_NUM : 0)
		| ((ks[VK_CAPITAL]&1) ? KMOD_CAPS : 0);
}

/* more windows key stuff... */
unsigned key_repeat_rate(void)
{
	DWORD spd;
	if (!SystemParametersInfo(SPI_GETKEYBOARDSPEED, 0, &spd, 0)) return 0;
	if (!spd) return 1;
	return spd;
}
unsigned key_repeat_delay(void)
{
	int delay;

	if (!SystemParametersInfo(SPI_GETKEYBOARDDELAY, 0, &delay, 0)) return 0;
	switch (delay) {
	case 0: return 250;
	case 1: return 500;
	case 2: return 750;
	};
	return 1000;
}
