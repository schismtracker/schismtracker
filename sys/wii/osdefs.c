/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2011 Storlek
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

const char *osname = "wii";

void wii_sysinit(int *pargc, char ***pargv)
{
        DIR_ITER *dir;
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
                free(ptr);
                dir = diropen("sd:/");
                if (dir) {
                        // Ok at least the sd card works, there's some other dysfunction
                        dirclose(dir);
                        ptr = str_dup("sd:/");
                } else {
                        // Safe (but useless) default
                        ptr = str_dup("isfs:/");
                }
                chdir(ptr); // Hope that worked, otherwise we're hosed
        }
        put_env_var("HOME", ptr);
        free(ptr);
}

void wii_sysexit(void)
{
        ISFS_Deinitialize();
}

void wii_sdlinit(void)
{
        int n, total;

        if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) != 0) {
                log_appendf(4, "joystick init failed: %s", SDL_GetError());
                return;
        }

        total = SDL_NumJoysticks();
        for (n = 0; n < total; n++) {
                SDL_Joystick *js = SDL_JoystickOpen(n);
                if (js == NULL) {
                        log_appendf(4, "[%d] open fail", n);
                        continue;
                }
        }
}


static int lasthatsym = 0;

static SDLKey hat_to_keysym(int value)
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

// Huge event-rewriting hack to get at least a sort of useful interface with no keyboard.
// It's obviously impossible to provide any sort of editing functions in this manner,
// but it at least allows simple song playback.
int wii_sdlevent(SDL_Event *event)
{
        SDL_Event newev = {};
        SDLKey sym;

        switch (event->type) {
        case SDL_KEYDOWN:
        case SDL_KEYUP:
                { // argh
                        struct key_event k = {
                                .mod = event->key.keysym.mod,
                                .sym = event->key.keysym.sym,
                        };
                        event->key.keysym.unicode = kbd_get_alnum(&k);
                }
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
                newev.key.which = event->jhat.which;
                newev.key.keysym.sym = sym;
                newev.key.type = newev.type; // is this a no-op?
                *event = newev;
                return 1;

        case SDL_JOYBUTTONDOWN:
        case SDL_JOYBUTTONUP:
                switch (event->jbutton.button) {
                case 0: // A
                case 1: // B
                default:
                        return 0;
                case 2: // 1
                        if (song_get_mode() == MODE_STOPPED) {
                                // nothing playing? go to load screen
                                sym = SDLK_F9;
                        } else {
                                sym = SDLK_F8;
                        }
                        break;
                case 3: // 2
                        if (status.current_page == PAGE_LOAD_MODULE) {
                                // if the cursor is on a song, load then play; otherwise handle as enter
                                // (hmm. ctrl-enter?)
                                sym = SDLK_RETURN;
                        } else {
                                // F5 key
                                sym = SDLK_F5;
                        }
                        break;
                case 4: // -
                        // dialog escape, or jump back a pattern
                        if (status.dialog_type) {
                                sym = SDLK_ESCAPE;
                                break;
                        } else if (event->type == SDL_JOYBUTTONDOWN && song_get_mode() == MODE_PLAYING) {
                                song_set_current_order(song_get_current_order() - 1);
                        }
                        return 0;
                case 5: // +
                        // dialog enter, or jump forward a pattern
                        if (status.dialog_type) {
                                sym = SDLK_RETURN;
                                break;
                        } else if (event->type == SDL_JOYBUTTONDOWN && song_get_mode() == MODE_PLAYING) {
                                song_set_current_order(song_get_current_order() + 1);
                        }
                        return 0;
                case 6: // Home
                        event->type = SDL_QUIT;
                        return 1;
                }
                newev.key.which = event->jbutton.which;
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
        }
        return 1;
}

