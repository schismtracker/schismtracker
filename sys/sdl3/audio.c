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
#include "str.h"
#include "backend/audio.h"
#include "mt.h"

#include "init.h"

struct schism_audio_device {
	// In SDL3, everything is just a stream.
	SDL_AudioStream *stream;

	// We have to do this ourselves now
	mt_mutex_t *mutex;

	void (*callback)(uint8_t *stream, int len);
};

static bool (SDLCALL *sdl3_InitSubSystem)(SDL_InitFlags flags) = NULL;
static void (SDLCALL *sdl3_QuitSubSystem)(SDL_InitFlags flags) = NULL;

static int (SDLCALL *sdl3_GetNumAudioDrivers)(void) = NULL;
static const char *(SDLCALL *sdl3_GetAudioDriver)(int i) = NULL;

static bool (SDLCALL *sdl3_GetAudioDeviceFormat)(SDL_AudioDeviceID devid, SDL_AudioSpec *spec, int *sample_frames) = NULL;
static const char * (SDLCALL *sdl3_GetAudioDeviceName)(SDL_AudioDeviceID devid) = NULL;
static SDL_AudioDeviceID * (SDLCALL *sdl3_GetAudioPlaybackDevices)(int *count) = NULL;

static SDL_AudioStream *(SDLCALL *sdl3_OpenAudioDeviceStream)(SDL_AudioDeviceID devid, const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata) = NULL;
static void (SDLCALL *sdl3_DestroyAudioStream)(SDL_AudioStream *stream) = NULL;
static bool (SDLCALL *sdl3_PauseAudioDevice)(SDL_AudioDeviceID dev) = NULL;
static bool (SDLCALL *sdl3_ResumeAudioDevice)(SDL_AudioDeviceID dev) = NULL;
static SDL_AudioDeviceID (SDLCALL *sdl3_GetAudioStreamDevice)(SDL_AudioStream *stream) = NULL;
static bool (SDLCALL *sdl3_PutAudioStreamData)(SDL_AudioStream *stream, const void *buf, int len) = NULL;

// used to request a specific sample frame size
static bool (SDLCALL *sdl3_SetHint)(const char *name, const char *value);
static bool (SDLCALL *sdl3_ResetHint)(const char *name);

static void (SDLCALL *sdl3_free)(void *ptr) = NULL;

/* SDL_AudioInit and SDL_AudioQuit were completely removed
 * in SDL3, which means we have to do this always regardless. */
static int schism_init_audio_impl(const char *name)
{
	const char *orig_drv = getenv("SDL_AUDIO_DRIVER");

	if (name)
		setenv("SDL_AUDIO_DRIVER", name, 1);

	int ret = sdl3_InitSubSystem(SDL_INIT_AUDIO);

	/* clean up our dirty work, or empty the var */
	if (name) {
		if (orig_drv) {
			setenv("SDL_AUDIO_DRIVER", orig_drv, 1);
		} else {
			unsetenv("SDL_AUDIO_DRIVER");
		}
	}

	/* forward any error, if any */
	return ret;
}

static void schism_quit_audio_impl(void)
{
	sdl3_QuitSubSystem(SDL_INIT_AUDIO);
}

/* ---------------------------------------------------------- */
/* drivers */

static int sdl3_audio_driver_count(void)
{
	return sdl3_GetNumAudioDrivers();
}

static const char *sdl3_audio_driver_name(int i)
{
	return sdl3_GetAudioDriver(i);
}

/* --------------------------------------------------------------- */

static SDL_AudioDeviceID *devices = NULL;
static int device_count = 0;

static inline SCHISM_ALWAYS_INLINE void _free_devices(void)
{
	if (devices) {
		sdl3_free(devices);
		devices = NULL;
	}
}

static uint32_t sdl3_audio_device_count(void)
{
	_free_devices();

	devices = sdl3_GetAudioPlaybackDevices(&device_count);

	return device_count;
}

static const char *sdl3_audio_device_name(uint32_t i)
{
	if (i >= INT_MAX) return NULL;
	if ((int)i >= device_count) return NULL;

	return sdl3_GetAudioDeviceName(devices[i]);
}

/* ---------------------------------------------------------- */

static int sdl3_audio_init_driver(const char *driver)
{
	if (!schism_init_audio_impl(driver))
		return -1;

	// force poll for audio devices
	sdl3_audio_device_count();

	return 0;
}

static void sdl3_audio_quit_driver(void)
{
	schism_quit_audio_impl();

	devices = NULL;
}

/* -------------------------------------------------------- */

static void SDLCALL sdl3_audio_callback(void *userdata, SDL_AudioStream *stream, int additional_amount, SCHISM_UNUSED int total_amount)
{
	schism_audio_device_t *dev = (schism_audio_device_t *)userdata;

	if (additional_amount > 0) {
		SCHISM_VLA_ALLOC(uint8_t, data, additional_amount);

		mt_mutex_lock(dev->mutex);
		dev->callback(data, additional_amount);
		mt_mutex_unlock(dev->mutex);

		sdl3_PutAudioStreamData(stream, data, additional_amount);

		SCHISM_VLA_FREE(data);
	}
}

static void sdl3_audio_close_device(schism_audio_device_t *dev);

static schism_audio_device_t *sdl3_audio_open_device(uint32_t id, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));
	dev->callback = desired->callback;

	uint32_t format;

	switch (desired->bits) {
	case 8: format = SDL_AUDIO_U8; break;
	default:
	case 16: format = SDL_AUDIO_S16; break;
	case 32: format = SDL_AUDIO_S32; break;
	}

	const SDL_AudioSpec sdl_desired = {
		.freq = desired->freq,
		.format = format,
		.channels = desired->channels,
	};

	dev->mutex = mt_mutex_create();
	if (!dev->mutex)
		goto fail;

	{
		// As it turns out, SDL is still just a shell script in disguise, and requires you to
		// pass everything as strings in order to change behavior. As for why they don't just
		// include this in the spec structure anymore is beyond me.
		char buf[64];
		str_from_num(0, desired->samples, buf);
		sdl3_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, buf);
	}

	dev->stream = sdl3_OpenAudioDeviceStream((id == AUDIO_BACKEND_DEFAULT || id >= (uint32_t)device_count) ? SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK : devices[id], &sdl_desired, sdl3_audio_callback, dev);

	// reset this before checking if opening succeeded
	sdl3_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);

	if (!dev->stream)
		goto fail;

	// PAUSE!
	sdl3_PauseAudioDevice(sdl3_GetAudioStreamDevice(dev->stream));

	// lolwut
	memcpy(obtained, desired, sizeof(schism_audio_spec_t));

	// Retrieve the actual buffer size SDL is using (i.e., don't lie to the user)
	int samples;
	{
		SDL_AudioSpec xyzzy;
		sdl3_GetAudioDeviceFormat(id, &xyzzy, &samples);
	}
	obtained->samples = samples;

	return dev;

fail:
	if (dev)
		sdl3_audio_close_device(dev);

	return NULL;
}

static void sdl3_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	if (dev->stream)
		sdl3_DestroyAudioStream(dev->stream);

	if (dev->mutex)
		mt_mutex_delete(dev->mutex);

	free(dev);
}

static void sdl3_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
}

static void sdl3_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_unlock(dev->mutex);
}

static void sdl3_audio_pause_device(schism_audio_device_t *dev, int pause)
{
	if (!dev)
		return;

	(pause ? sdl3_PauseAudioDevice : sdl3_ResumeAudioDevice)(sdl3_GetAudioStreamDevice(dev->stream));
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int sdl3_audio_load_syms(void)
{
	SCHISM_SDL3_SYM(InitSubSystem);
	SCHISM_SDL3_SYM(QuitSubSystem);

	SCHISM_SDL3_SYM(GetNumAudioDrivers);
	SCHISM_SDL3_SYM(GetAudioDriver);

	SCHISM_SDL3_SYM(GetAudioPlaybackDevices);
	SCHISM_SDL3_SYM(GetAudioDeviceName);

	SCHISM_SDL3_SYM(OpenAudioDeviceStream);
	SCHISM_SDL3_SYM(DestroyAudioStream);
	SCHISM_SDL3_SYM(PauseAudioDevice);
	SCHISM_SDL3_SYM(ResumeAudioDevice);
	SCHISM_SDL3_SYM(GetAudioStreamDevice);
	SCHISM_SDL3_SYM(GetAudioDeviceFormat);
	SCHISM_SDL3_SYM(PutAudioStreamData);

	SCHISM_SDL3_SYM(SetHint);
	SCHISM_SDL3_SYM(ResetHint);

	SCHISM_SDL3_SYM(free);

	return 0;
}

static int sdl3_audio_init(void)
{
	if (!sdl3_init())
		return 0;

	if (sdl3_audio_load_syms())
		return 0;

	return 1;
}

static void sdl3_audio_quit(void)
{
	// the subsystem quitting is handled by the quit driver function
	sdl3_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_audio_backend_t schism_audio_backend_sdl3 = {
	.init = sdl3_audio_init,
	.quit = sdl3_audio_quit,

	.driver_count = sdl3_audio_driver_count,
	.driver_name = sdl3_audio_driver_name,

	.device_count = sdl3_audio_device_count,
	.device_name = sdl3_audio_device_name,

	.init_driver = sdl3_audio_init_driver,
	.quit_driver = sdl3_audio_quit_driver,

	.open_device = sdl3_audio_open_device,
	.close_device = sdl3_audio_close_device,
	.lock_device = sdl3_audio_lock_device,
	.unlock_device = sdl3_audio_unlock_device,
	.pause_device = sdl3_audio_pause_device,
};
