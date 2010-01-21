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

/* Here be demons. */

#include "headers.h"
#include "util.h"
#include "osdefs.h"

#if defined(USE_DLTRICK_ALSA)
# include <dlfcn.h>
void *_dltrick_handle = NULL;
static void *_alsaless_sdl_hack = NULL;
#elif defined(USE_ALSA)
# include <alsa/pcm.h>
#else
# error You are in a maze of twisty little passages, all alike.
#endif

/* --------------------------------------------------------------------- */

void alsa_init(const char **p_driver)
{
        const char *driver = *p_driver;

#if defined(USE_DLTRICK_ALSA)
        /* okay, this is how this works:
         * to operate the alsa mixer and alsa midi, we need functions in
         * libasound.so.2 -- if we can do that, *AND* libSDL has the
         * ALSA_bootstrap routine- then SDL was built with alsa-support-
         * which means schism can probably use ALSA - so we set that as the
         * default here.
         */
        _dltrick_handle = dlopen("libasound.so.2", RTLD_NOW);
        if (!_dltrick_handle)
                _dltrick_handle = dlopen("libasound.so", RTLD_NOW);
        if (!getenv("SDL_AUDIODRIVER")) {
                _alsaless_sdl_hack = dlopen("libSDL-1.2.so.0", RTLD_NOW);
                if (!_alsaless_sdl_hack)
                        _alsaless_sdl_hack = RTLD_DEFAULT;

                if (_dltrick_handle && _alsaless_sdl_hack
                && (dlsym(_alsaless_sdl_hack, "ALSA_bootstrap")
                || dlsym(_alsaless_sdl_hack, "snd_pcm_open"))) {
                        static int (*alsa_snd_pcm_open)(void **pcm,
                                        const char *name,
                                        int stream,
                                        int mode);
                        static int (*alsa_snd_pcm_close)(void *pcm);
                        static void *ick;
                        static int r;

                        alsa_snd_pcm_open = dlsym(_dltrick_handle, "snd_pcm_open");
                        alsa_snd_pcm_close = dlsym(_dltrick_handle, "snd_pcm_close");

                        if (alsa_snd_pcm_open && alsa_snd_pcm_close) {
                                if (!driver) {
                                        driver = "alsa";
                                } else if (strcmp(driver, "default") == 0) {
                                        driver = "sdlauto";
                                } else if (!getenv("AUDIODEV")) {
                                        r = alsa_snd_pcm_open(&ick,
                                                driver, 0, 1);
                                        if (r >= 0) {
                                                put_env_var("AUDIODEV", driver);
                                                driver = "alsa";
                                                alsa_snd_pcm_close(ick);
                                        }
                                }
                        }
                }
        }
#else
        if (driver) {
                static snd_pcm_t *h;
                if (snd_pcm_open(&h, driver, SND_PCM_STREAM_PLAYBACK,
                                        SND_PCM_NONBLOCK) >= 0) {
                        put_env_var("AUDIODEV", driver);
                        driver = "alsa";
                        snd_pcm_close(h);
                }
        }
#endif

        *p_driver = driver;
}

