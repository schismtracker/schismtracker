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
#include "mem.h"
#include "backend/audio.h"

#include <SDL.h>

struct schism_audio_device {
	SDL_AudioDeviceID id;
	void (*callback)(uint8_t *stream, int len);
};

#ifndef SDL_AUDIO_ALLOW_SAMPLES_CHANGE /* added in SDL 2.0.9 */
#define SDL_AUDIO_ALLOW_SAMPLES_CHANGE 0x00000008
#endif

/* ---------------------------------------------------------- */
/* drivers */

int sdl2_audio_driver_count()
{
	return SDL_GetNumAudioDrivers();
}

const char *sdl2_audio_driver_name(int i)
{
	return SDL_GetAudioDriver(i);
}

/* --------------------------------------------------------------- */

int sdl2_audio_device_count(void)
{
	return SDL_GetNumAudioDevices(0);
}

const char *sdl2_audio_device_name(int i)
{
	return SDL_GetAudioDeviceName(i, 0);
}

/* ---------------------------------------------------------- */

/* explanation for this:
 * in 2.0.18, the logic for SDL's audio initialization functions
 * changed, so that you can use SDL_AudioInit() directly without
 * any repercussions; before that, SDL would do a sanity check
 * calling SDL_WasInit() which surprise surprise doesn't actually
 * get initialized from SDL_AudioInit(). to work around this, we
 * have to use a separate audio driver initialization function
 * under SDL pre-2.0.18. */
static SDLCALL int schism_init_audio_impl(const char *name)
{
	const char *orig_drv = SDL_getenv("SDL_AUDIODRIVER");

	if (name)
		SDL_setenv("SDL_AUDIODRIVER", name, 1);

	int ret = SDL_InitSubSystem(SDL_INIT_AUDIO);

	/* clean up our dirty work, or empty the var */
	SDL_setenv("SDL_AUDIODRIVER", orig_drv ? orig_drv : "", 1);

	/* forward any error, if any */
	return ret;
}

static SDLCALL void schism_quit_audio_impl(void)
{
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

static int (SDLCALL *audio_init_func)(const char *) = schism_init_audio_impl;
static void (SDLCALL *audio_quit_func)(void) = schism_quit_audio_impl;

static void audio_check_init_funcs(void)
{
	static int sdl_version_checked = 0;

	if (sdl_version_checked)
		return;

	// see if we can use the normal audio init and quit functions
	SDL_version ver;
	SDL_GetVersion(&ver);
	if ((ver.major >= 2)
		 && (ver.major > 2 || ver.minor >= 0)
		 && (ver.major > 2 || ver.minor > 0 || ver.patch >= 18)) {
		audio_init_func = SDL_AudioInit;
		audio_quit_func = SDL_AudioQuit;
	}

	sdl_version_checked = 1;
}

int sdl2_audio_init(const char *driver)
{
	audio_check_init_funcs();
	return audio_init_func(driver);
}

void sdl2_audio_quit(void)
{
	audio_check_init_funcs();
	audio_quit_func();
}

/* -------------------------------------------------------- */

// This is here to prevent having to put SDLCALL into
// the original audio callback
static void SDLCALL sdl2_dummy_callback(void *userdata, uint8_t *stream, int len)
{
	// call our own callback
	schism_audio_device_t *dev = userdata;

	dev->callback(stream, len);
}

// nonzero on success
static inline int sdl2_audio_open_device_impl(schism_audio_device_t *dev, const char *name, const SDL_AudioSpec *desired, SDL_AudioSpec *obtained, int change)
{
	// cache the current error
	const char *err = SDL_GetError();

	SDL_ClearError();

	SDL_AudioDeviceID id = SDL_OpenAudioDevice(name, 0, desired, obtained, change);

	const char *new_err = SDL_GetError();

	int failed = (new_err && *new_err);

	// reset the original error
	SDL_SetError("%s", err);

	if (!failed) {
		dev->id = id;
		return 1;
	}

	return 0;
}

schism_audio_device_t *sdl2_audio_open_device(const char *name, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));
	dev->callback = desired->callback;
	//dev->userdata = desired->userdata;

	SDL_AudioSpec sdl_desired = {
		.freq = desired->freq,
		.format = (desired->bits == 8) ? (AUDIO_U8) : (AUDIO_S16),
		.channels = desired->channels,
		.samples = desired->samples,
		.callback = sdl2_dummy_callback,
		.userdata = dev,
	};
	SDL_AudioSpec sdl_obtained;

	int change = SDL_AUDIO_ALLOW_FREQUENCY_CHANGE | SDL_AUDIO_ALLOW_FORMAT_CHANGE | SDL_AUDIO_ALLOW_CHANNELS_CHANGE | SDL_AUDIO_ALLOW_SAMPLES_CHANGE;

	for (;;) {
		if (!sdl2_audio_open_device_impl(dev, name, &sdl_desired, &sdl_obtained, change)) {
			free(dev);
			return NULL;
		}

		int need_reopen = 0;

		// hm :)
		switch (sdl_obtained.format) {
		case AUDIO_U8: case AUDIO_S16: break;
		default: change &= (~SDL_AUDIO_ALLOW_FORMAT_CHANGE); need_reopen = 1; break;
		}

		switch (sdl_obtained.channels) {
		case 1: case 2: break;
		default: change &= (~SDL_AUDIO_ALLOW_CHANNELS_CHANGE); need_reopen = 1; break;
		}

		if (!need_reopen)
			break;

		SDL_CloseAudioDevice(dev->id);
	}

	*obtained = (schism_audio_spec_t){
		.freq = sdl_obtained.freq,
		.bits = SDL_AUDIO_BITSIZE(sdl_obtained.format),
		.channels = sdl_obtained.channels,
		.samples = sdl_obtained.samples,
	};

	return dev;
}

void sdl2_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	SDL_CloseAudioDevice(dev->id);
	free(dev);
}

/* lock/unlock/pause */

void sdl2_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;
	SDL_LockAudioDevice(dev->id);
}

void sdl2_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;
	SDL_UnlockAudioDevice(dev->id);
}

void sdl2_audio_pause_device(schism_audio_device_t *dev, int paused)
{
	if (!dev)
		return;

	SDL_PauseAudioDevice(dev->id, paused);
}
