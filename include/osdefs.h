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

/* OS-dependent code implementations are defined here; the files for each target OS exist in sys/blah/osdefs.c,
and possibly other files as well. Only one osdefs.c should be in use at a time. */

#ifndef SCHISM_OSDEFS_H_
#define SCHISM_OSDEFS_H_

#include "headers.h"
#include "event.h"

/*
os_sysinit: any platform-dependent setup that needs to occur directly upon startup.
This code is processed right as soon as main() starts.

os_sdlinit: any platform-dependent setup that needs to occur after SDL is up and running.
Currently only used on the Wii in order to get the Wiimote working.

os_sdlevent: preprocessing for SDL events.
This is used to hack in system-dependent input methods (e.g. F16 and other scancodes on OS X; Wiimote buttons;
etc.) If defined, this function will be called after capturing an SDL event.
A return value of 0 indicates that the event should NOT be processed by the main event handler.
*/
#if defined(SCHISM_WII)
# define os_sysinit wii_sysinit
# define os_sdlinit wii_sdlinit
# define os_sysexit wii_sysexit
# define os_sdlevent wii_sdlevent
#elif defined(SCHISM_WIN32)
# define os_sdlevent win32_sdlevent
# define os_sdlinit win32_sdlinit
# define os_sysinit win32_sysinit
# define os_sysexit win32_sysexit
# define os_get_modkey win32_get_modkey
#elif defined(SCHISM_MACOSX)
# define os_sdlevent macosx_sdlevent
# define os_sysexit macosx_sysexit
# define os_sysinit macosx_sysinit
# define os_get_modkey macosx_get_modkey
#endif

#ifndef os_sdlevent
# define os_sdlevent(ev) 1
#endif
#ifndef os_sdlinit
# define os_sdlinit()
#endif
#ifndef os_sysinit
# define os_sysinit(pargc,argv)
#endif
#ifndef os_sysexit
# define os_sysexit()
#endif
#ifndef os_get_modkey
#define os_get_modkey(m)
#endif

// Implementations for the above, and more.

int macosx_ibook_fnswitch(int setting);

void wii_sysinit(int *pargc, char ***pargv); // set up filesystem
void wii_sysexit(void); // close filesystem
void wii_sdlinit(void); // set up wiimote
int wii_sdlevent(SDL_Event *event); // add unicode values; wiimote hack to allow simple playback

int win32_sdlevent(SDL_Event* event);
void win32_sysinit(int *pargc, char ***pargv);
void win32_sysexit(void);
void win32_sdlinit(void);
void win32_get_modkey(int *m);
void win32_filecreated_callback(const char *filename);
void win32_toggle_menu(SDL_Window* window);

int macosx_sdlevent(SDL_Event* event);
void macosx_sysexit(void);
void macosx_sysinit(int *pargc, char ***pargv); /* set up ibook helper */
void macosx_get_modkey(int *m);

#endif /* SCHISM_OSDEFS_H_ */
