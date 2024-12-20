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
#include "events.h"

/* need stat; TODO autoconf test */
#include <sys/stat.h> /* roundabout way to get time_t */

/*
os_sysinit: any platform-dependent setup that needs to occur directly upon startup.
This code is processed right as soon as main() starts.

os_event: preprocessing for events.
This is used to hack in system-dependent input methods (e.g. F16 and other scancodes on OS X; Wiimote buttons;
etc.) If defined, this function will be called after capturing an SDL event.
A return value of 0 indicates that the event should NOT be processed by the main event handler.
*/
#if defined(SCHISM_WII)
# define os_sysinit wii_sysinit
# define os_sysexit wii_sysexit
#elif defined(SCHISM_WIIU)
# define os_sysinit wiiu_sysinit
# define os_sysexit wiiu_sysexit
#elif defined(SCHISM_WIN32)
# define os_sysinit win32_sysinit
# define os_sysexit win32_sysexit
# define os_get_modkey win32_get_modkey
# define os_fopen win32_fopen
# define os_stat win32_stat
# define os_mkdir win32_mkdir
# define os_get_key_repeat win32_get_key_repeat
# define os_show_message_box win32_show_message_box
#elif defined(SCHISM_MACOSX)
# define os_sysexit macosx_sysexit
# define os_sysinit macosx_sysinit
# define os_get_modkey macosx_get_modkey
# define os_get_key_repeat macosx_get_key_repeat
#elif defined(SCHISM_MACOS)
# define os_mkdir macos_mkdir
# define os_stat macos_stat
# define os_show_message_box macos_show_message_box
# define os_sysinit macos_sysinit
# define os_get_modkey macos_get_modkey
#endif

#if defined(SCHISM_WIN32)
# define os_run_hook win32_run_hook
#elif defined(HAVE_EXECL) && defined(HAVE_FORK)
# define os_run_hook posix_run_hook
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
#ifndef os_fopen
# define os_fopen fopen
#endif
#ifndef os_stat
# define os_stat stat
#endif
#ifndef os_mkdir
# define os_mkdir mkdir
#endif
#ifndef os_run_hook
# define os_run_hook(a,b,c) 0
#endif
#ifndef os_get_key_repeat
# define os_get_key_repeat(pdelay, prate) (0)
#endif
#ifndef os_show_message_box
# define os_show_message_box(title, text) ((void)printf("%s: %s\n", title, text))
#endif

// Implementations for the above, and more.

int macosx_ibook_fnswitch(int setting);

void wiiu_sysinit(int *pargc, char ***pargv); // fixup HOME envvar
void wiiu_sysexit(void);

void wii_sysinit(int *pargc, char ***pargv); // set up filesystem
void wii_sysexit(void); // close filesystem

int win32_event(schism_event_t *event);
void win32_sysinit(int *pargc, char ***pargv);
void win32_sysexit(void);
void win32_sdlinit(void);
void win32_get_modkey(schism_keymod_t *m);
void win32_filecreated_callback(const char *filename);
void win32_toggle_menu(void* window, int on); // window should be a pointer to the window HWND
int win32_stat(const char* path, struct stat* st);
int win32_mkdir(const char* path, mode_t mode);
FILE* win32_fopen(const char* path, const char* flags);
#define win32_wmkdir(path, mode) _wmkdir(path)
int win32_run_hook(const char *dir, const char *name, const char *maybe_arg);
int win32_get_key_repeat(int *pdelay, int *prate);
void win32_show_message_box(const char *title, const char *text);

int posix_run_hook(const char *dir, const char *name, const char *maybe_arg);

int macosx_event(schism_event_t *event);
void macosx_sysexit(void);
void macosx_sysinit(int *pargc, char ***pargv); /* set up ibook helper */
void macosx_get_modkey(schism_keymod_t *m);
int macosx_get_key_repeat(int *pdelay, int *prate);

int macos_mkdir(const char *path, mode_t mode);
int macos_stat(const char *file, struct stat *st);
void macos_show_message_box(const char *title, const char *text);
void macos_sysinit(int *pargc, char ***pargv);
void macos_get_modkey(schism_keymod_t *mk);

int x11_event(schism_event_t *event);

#endif /* SCHISM_OSDEFS_H_ */
