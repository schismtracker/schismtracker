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

/* Predominantly this file is keyboard crap, but we also get the network configured here */

#include "headers.h"
#include "sdlmain.h"
#include "it.h"
#include "osdefs.h"

#include <windows.h>
#include <ws2tcpip.h>

/* eek... */
void win32_get_modkey(int *mk)
{
	BYTE ks[256];
	if (GetKeyboardState(ks) == 0) return;

	if (ks[VK_CAPITAL] & 128) {
		status.flags |= CAPS_PRESSED;
	} else {
		status.flags &= ~CAPS_PRESSED;
	}

	(*mk) = ((*mk) & ~(KMOD_NUM|KMOD_CAPS))
		| ((ks[VK_NUMLOCK]&1) ? KMOD_NUM : 0)
		| ((ks[VK_CAPITAL]&1) ? KMOD_CAPS : 0);
}

static HKL default_keymap;
static HKL us_keymap;

static void win32_setup_keymap(void)
{
	default_keymap = GetKeyboardLayout(0);
	us_keymap = LoadKeyboardLayout("00000409", KLF_ACTIVATE|KLF_REPLACELANG|KLF_NOTELLSHELL);
	ActivateKeyboardLayout(default_keymap,0);
}


void win32_sysinit(UNUSED int *pargc, UNUSED char ***pargv)
{
	static WSADATA ignored = {};

	win32_setup_keymap();

	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		status.flags |= NO_NETWORK;
	}
}

