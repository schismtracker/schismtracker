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

/* OS-dependent code implementations are defined here; the files for each target OS exist in sys/blah/osdefs.c,
and possibly other files as well. Only one osdefs.c should be in use at a time. */

#ifndef OSDEFS_H
#define OSDEFS_H

#include "headers.h"
#include "event.h"

// This is defined in osdefs.c but not used anywhere.
// Currently, its only purpose is to prevent erroneous linking of multiple osdefs.o files in the same build.
extern const char *osname;


/* os_sdlevent: preprocessing for SDL events.
This is used to hack in system-dependent input methods (e.g. F16 and other scancodes on OS X; Wiimote buttons;
etc.) If defined, this function will be called after capturing an SDL event.
A return value of 0 indicates that the event should NOT be processed by the main event handler. */
#if defined(MACOSX)
# define os_sdlevent macosx_sdlevent
#elif defined(GEKKO)
# define os_sdlinit wii_sdlinit
# define os_sdlevent wii_sdlevent
#endif
#ifndef os_sdlevent
# define os_sdlevent(ev) 1
#endif
#ifndef os_sdlinit
# define os_sdlinit()
#endif

/* os_screensaver_deactivate: whatever is needed to keep the screensaver away.
Leave this *undefined* if no implementation exists. */
#if defined(USE_X11)
# define os_screensaver_deactivate x11_screensaver_deactivate
#else
# undef os_screensaver_deactivate
#endif

/* os_yuvlayout: return the best YUV layout. */
#if defined(USE_XV)
#  define os_yuvlayout xv_yuvlayout
#elif defined(USE_X11)
# define os_yuvlayout() VIDEO_YUV_NONE
#else
# define os_yuvlayout() VIDEO_YUV_YUY2
#endif


// Implementations for the above, and more.

int macosx_sdlevent(SDL_Event *event); // patch up osx scancodes for printscreen et al; numlock hack?
int macosx_ibook_fnswitch(int setting);

void wii_sdlinit(void); // set up wiimote
int wii_sdlevent(SDL_Event *event); // add unicode values; wiimote hack to allow simple playback

void x11_screensaver_deactivate(void);
unsigned int xv_yuvlayout(void);

void win32_get_modkey(int *m);
void win32_setup_keymap(void);
void win32_filecreated_callback(const char *filename);

// migrated from xkb.c
#if defined(HAVE_X11_XKBLIB_H)
# define USE_XKB 1
#endif

#if defined(USE_XKB) || defined(WIN32) || defined(MACOSX)
int key_scancode_lookup(int k, int def);
#else
#define key_scancode_lookup(k, def) def
#endif

#if defined(USE_X11) || defined(WIN32) || defined(MACOSX)
unsigned int key_repeat_delay(void);
unsigned int key_repeat_rate(void);
#else
# include "sdlmain.h" // blecch
# define key_repeat_delay() SDL_DEFAULT_REPEAT_DELAY
# define key_repeat_rate() SDL_DEFAULT_REPEAT_INTERVAL
#endif


// Mixer interfaces

int alsa_mixer_get_max_volume(void);
void alsa_mixer_read_volume(int *, int *);
void alsa_mixer_write_volume(int, int);

int oss_mixer_get_max_volume(void);
void oss_mixer_read_volume(int *, int *);
void oss_mixer_write_volume(int, int);

int macosx_mixer_get_max_volume(void);
void macosx_mixer_read_volume(int *, int *);
void macosx_mixer_write_volume(int, int);

int win32mm_mixer_get_max_volume(void);
void win32mm_mixer_read_volume(int *, int *);
void win32mm_mixer_write_volume(int, int);

#endif /* ! OSDEFS_H */

