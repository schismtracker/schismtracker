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
#include "osdefs.h"
#include "event.h"
#include "song.h"
#include "it.h" // need for kbd_get_alnum
#include "page.h" // need for struct key_event
#include "log.h"

#include <di/di.h>
#include <fat.h>
#include <ogc/machine/processor.h>
#include <ogc/system.h>
#include <ogc/es.h>
#include <ogc/ios.h>
#include <errno.h>
#include <sys/dir.h>
#include "isfs.h"
#define CACHE_PAGES 8

/* whichever joystick we get is up to libogc to decide!
 *
 * note: do NOT check if joystick_id is zero to see if
 * there are no joysticks attached, check if joystick
 * is NULL.
*/
static SDL_JoystickID joystick_id = 0;
static SDL_Joystick* joystick = NULL;

// cargopasta'd from libogc git __di_check_ahbprot
static u32 _check_ahbprot(void) {
	s32 res;
	u64 title_id;
	u32 tmd_size;
	STACK_ALIGN(u32, tmdbuf, 1024, 32);

	res = ES_GetTitleID(&title_id);
	if (res < 0) {
		log_appendf(4, "ES_GetTitleID() failed: %d", res);
		return res;
	}

	res = ES_GetStoredTMDSize(title_id, &tmd_size);
	if (res < 0) {
		log_appendf(4, "ES_GetStoredTMDSize() failed: %d", res);
		return res;
	}

	if (tmd_size > 4096) {
		log_appendf(4, "TMD too big: %d", tmd_size);
		return -EINVAL;
	}

	res = ES_GetStoredTMD(title_id, tmdbuf, tmd_size);
	if (res < 0) {
		log_appendf(4, "ES_GetStoredTMD() failed: %d", res);
		return -EINVAL;
	}

	if ((tmdbuf[0x76] & 3) == 3) {
		return 1;
	}

	return 0;
}

void wii_sysinit(int *pargc, char ***pargv)
{
	char *ptr = NULL;

	log_appendf(1, "[Wii] This is IOS%d v%X, and AHBPROT is %s",
		IOS_GetVersion(), IOS_GetRevision(), _check_ahbprot() > 0 ? "enabled" : "disabled");
	if (*pargc == 0 && *pargv == NULL) {
		// I don't know if any other loaders provide similarly broken environments
		log_appendf(1, "[Wii] Was I just bannerbombed? Prepare for crash at exit...");
	} else if (memcmp((void *) 0x80001804, "STUBHAXX", 8) == 0) {
		log_appendf(1, "[Wii] Hello, HBC user!");
	} else {
		log_appendf(1, "[Wii] Where am I?!");
	}

	ISFS_SU();
	if (ISFS_Initialize() == IPC_OK)
		ISFS_Mount();
	fatInit(CACHE_PAGES, 0);

	// Attempt to locate a suitable home directory.
	if (!*pargc || !*pargv) {
		// loader didn't bother setting these
		*pargc = 1;
		*pargv = malloc(sizeof(char **));
		*pargv[0] = str_dup("?");
	} else if (strchr(*pargv[0], '/') != NULL) {
		// presumably launched from hbc menu - put stuff in the boot dir
		// (does get_parent_directory do what I want here?)
		ptr = get_parent_directory(*pargv[0]);
	}
	if (!ptr) {
		// Make a guess anyway
		ptr = str_dup("sd:/apps/schismtracker");
	}
	if (chdir(ptr) != 0) {
		DIR* dir = opendir("sd:/");
		free(ptr);
		if (dir) {
			// Ok at least the sd card works, there's some other dysfunction
			closedir(dir);
			ptr = str_dup("sd:/");
		} else {
			// Safe (but useless) default
			ptr = str_dup("isfs:/");
		}
		chdir(ptr); // Hope that worked, otherwise we're hosed
	}
	SDL_setenv("HOME", ptr, 1);
	free(ptr);
}

void wii_sysexit(void)
{
	ISFS_Deinitialize();
}

static void open_joystick(int n) {
	joystick = SDL_JoystickOpen(n);
	if (joystick) {
		joystick_id = SDL_JoystickInstanceID(joystick);
	} else {
		log_appendf(4, "joystick [%d] open fail: %s", n, SDL_GetError());
	}
}

void wii_sdlinit(void)
{
	int n, total;

	if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
		log_appendf(4, "joystick init failed: %s", SDL_GetError());
		return;
	}
}


static int lasthatsym = 0;

static SDL_Keycode hat_to_keysym(int value)
{
	// up/down take precedence over left/right
	switch (value) {
	case SDL_HAT_LEFTUP:
	case SDL_HAT_UP:
	case SDL_HAT_RIGHTUP:
		return SDLK_UP;
	case SDL_HAT_LEFTDOWN:
	case SDL_HAT_DOWN:
	case SDL_HAT_RIGHTDOWN:
		return SDLK_DOWN;
	case SDL_HAT_LEFT:
		return SDLK_LEFT;
	case SDL_HAT_RIGHT:
		return SDLK_RIGHT;
	default: // SDL_HAT_CENTERED
		return 0;
	}
}

/* rewrite events to where you can at least do *something*
 * without a keyboard */
int wii_sdlevent(SDL_Event *event)
{
	SDL_Event newev = {0};
	SDL_Keycode sym;

	switch (event->type) {
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		return 1;

	case SDL_JOYHATMOTION:
		// TODO key repeat for these, somehow
		sym = hat_to_keysym(event->jhat.value);
		if (sym) {
			newev.type = SDL_KEYDOWN;
			newev.key.state = SDL_PRESSED;
			lasthatsym = sym;
		} else {
			newev.type = SDL_KEYUP;
			newev.key.state = SDL_RELEASED;
			sym = lasthatsym;
			lasthatsym = 0;
		}
		newev.key.keysym.sym = sym;
		newev.key.type = newev.type; // is this a no-op?
		*event = newev;
		return 1;

	case SDL_JOYBUTTONDOWN:
	case SDL_JOYBUTTONUP:
		switch (event->jbutton.button) {
		case 2: /* 1 */
			/* "Load Module" if the song is stopped, else stop the song */
			sym = (song_get_mode() == MODE_STOPPED) ? SDLK_F9 : SDLK_F8;
			break;
		case 3: /* 2 */
			if (status.current_page == PAGE_LOAD_MODULE) {
				// if the cursor is on a song, load then play; otherwise handle as enter
				// (hmm. ctrl-enter?)
				sym = SDLK_RETURN;
			} else {
				// F5 key
				sym = SDLK_F5;
			}
			break;
		case 4: /* - */
			// dialog escape, or jump back a pattern
			if (status.dialog_type) {
				sym = SDLK_ESCAPE;
				break;
			} else if (event->type == SDL_JOYBUTTONDOWN && song_get_mode() == MODE_PLAYING) {
				song_set_current_order(song_get_current_order() - 1);
			}
			return 0;
		case 5: /* + */
			// dialog enter, or jump forward a pattern
			if (status.dialog_type) {
				sym = SDLK_RETURN;
				break;
			} else if (event->type == SDL_JOYBUTTONDOWN && song_get_mode() == MODE_PLAYING) {
				song_set_current_order(song_get_current_order() + 1);
			}
			return 0;
		case 6: /* Home */
			event->type = SDL_QUIT;
			return 1;
		case 0: /* A */
		case 1: /* B */
		default:
			return 0;
		}
		newev.key.keysym.sym = sym;
		if (event->type == SDL_JOYBUTTONDOWN) {
			newev.type = SDL_KEYDOWN;
			newev.key.state = SDL_PRESSED;
		} else {
			newev.type = SDL_KEYUP;
			newev.key.state = SDL_RELEASED;
		}
		newev.key.type = newev.type; // no-op?
		*event = newev;
		return 1;
	case SDL_JOYDEVICEADDED:
		if (!joystick)
			open_joystick(event->jdevice.which);
		return 0;
	case SDL_JOYDEVICEREMOVED:
		if (joystick && event->jdevice.which == joystick_id) {
			SDL_JoystickClose(joystick);
			joystick = NULL;
		}
		return 0;
	}
	return 1;
}

