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

	/* heap-allocated buffer, grows if SDL requests more
	 * space than we have space allocated */
	uint8_t *buf;
	uint32_t buflen;
};

static bool (SDLCALL *sdl3_InitSubSystem)(SDL_InitFlags flags) = NULL;
static void (SDLCALL *sdl3_QuitSubSystem)(SDL_InitFlags flags) = NULL;

static int (SDLCALL *sdl3_GetNumAudioDrivers)(void) = NULL;
static const char *(SDLCALL *sdl3_GetAudioDriver)(int i) = NULL;

static bool (SDLCALL *sdl3_GetAudioDeviceFormat)(SDL_AudioDeviceID devid, SDL_AudioSpec *spec, int *sample_frames) = NULL;
static const char * (SDLCALL *sdl3_GetAudioDeviceName)(SDL_AudioDeviceID devid) = NULL;
static SDL_AudioDeviceID * (SDLCALL *sdl3_GetAudioPlaybackDevices)(int *count) = NULL;
static SDL_AudioDeviceID * (SDLCALL *sdl3_GetAudioRecordingDevices)(int *count) = NULL;

static SDL_AudioStream *(SDLCALL *sdl3_OpenAudioDeviceStream)(SDL_AudioDeviceID devid, const SDL_AudioSpec *spec, SDL_AudioStreamCallback callback, void *userdata) = NULL;
static void (SDLCALL *sdl3_DestroyAudioStream)(SDL_AudioStream *stream) = NULL;
static bool (SDLCALL *sdl3_PauseAudioDevice)(SDL_AudioDeviceID dev) = NULL;
static bool (SDLCALL *sdl3_ResumeAudioDevice)(SDL_AudioDeviceID dev) = NULL;
static SDL_AudioDeviceID (SDLCALL *sdl3_GetAudioStreamDevice)(SDL_AudioStream *stream) = NULL;
static bool (SDLCALL *sdl3_PutAudioStreamData)(SDL_AudioStream *stream, const void *buf, int len) = NULL;
static int (SDLCALL *sdl3_GetAudioStreamData)(SDL_AudioStream *stream, void *buf, int len) = NULL;

// used to request a specific sample frame size
static bool (SDLCALL *sdl3_SetHint)(const char *name, const char *value);
static bool (SDLCALL *sdl3_ResetHint)(const char *name);

static void (SDLCALL *sdl3_free)(void *ptr) = NULL;

static bool (SDLCALL *sdl3_SetHintWithPriority)(const char *name, const char *value, SDL_HintPriority priority);
static bool (SDLCALL *sdl3_ResetHint)(const char *name);

/* ------------------------------------------------------------------------ */
/* init/deinit audio subsystem */

/* SDL_AudioInit and SDL_AudioQuit were completely removed
 * in SDL3, which means we have to do this always regardless. */
static int schism_init_audio_impl(const char *name)
{
	int x;
	sdl3_SetHintWithPriority(SDL_HINT_AUDIO_DRIVER, name, SDL_HINT_OVERRIDE);
	x = sdl3_InitSubSystem(SDL_INIT_AUDIO);
	sdl3_ResetHint(SDL_HINT_AUDIO_DRIVER);
	return x;
}

static void schism_quit_audio_impl(void)
{
	sdl3_QuitSubSystem(SDL_INIT_AUDIO);
}

/* ------------------------------------------------------------------------ */
/* drivers */

static int sdl3_audio_driver_count(void)
{
	return sdl3_GetNumAudioDrivers();
}

static const char *sdl3_audio_driver_name(int i)
{
	return sdl3_GetAudioDriver(i);
}

/* ------------------------------------------------------------------------ */

enum {
	DEVICES_OUT,
	DEVICES_IN,

	DEVICES_MAX_
};

static struct {
	SDL_AudioDeviceID *arr;
	int size;
} devices[DEVICES_MAX_] = {0};

static inline SCHISM_ALWAYS_INLINE void _free_devices(void)
{
	size_t i;

	for (i = 0; i < DEVICES_MAX_; i++) {
		if (devices[i].arr)
			free(devices[i].arr);

		/* zero it out */
		memset(devices + i, 0, sizeof(*devices));
	}
}

static uint32_t sdl3_audio_device_count(uint32_t flags)
{
	int off;

	_free_devices();

	/* get array offset */
	off = (flags & AUDIO_BACKEND_CAPTURE) ? DEVICES_IN : DEVICES_OUT;

	/* select function to call based on capture bit */
	devices[off].arr = ((flags & AUDIO_BACKEND_CAPTURE)
		? sdl3_GetAudioRecordingDevices
		: sdl3_GetAudioPlaybackDevices)(&devices[off].size);

	return devices[off].size;
}

static const char *sdl3_audio_device_name(uint32_t i)
{
	int off;

	if ((i & AUDIO_BACKEND_DEVICE_MASK) >= (uint32_t)INT_MAX)
		return NULL;

	/* get array offset */
	off = (i & AUDIO_BACKEND_CAPTURE) ? DEVICES_IN : DEVICES_OUT;

	/* shave off capture bit; we don't need it anymore */
	i &= AUDIO_BACKEND_DEVICE_MASK;

	if ((int)i >= devices[off].size)
		return NULL;

	return sdl3_GetAudioDeviceName(devices[off].arr[i]);
}

/* ---------------------------------------------------------- */

static int sdl3_audio_init_driver(const char *driver)
{
	if (!schism_init_audio_impl(driver))
		return -1;

	// force poll for audio devices
	sdl3_audio_device_count(0);

	return 0;
}

static void sdl3_audio_quit_driver(void)
{
	schism_quit_audio_impl();

	_free_devices();
}

/* -------------------------------------------------------- */

/* XXX need to adapt this callback for input devices */

static void SDLCALL sdl3_audio_output_callback(void *userdata,
	SDL_AudioStream *stream, int additional_amount,
	SCHISM_UNUSED int total_amount)
{
	schism_audio_device_t *dev = (schism_audio_device_t *)userdata;

	SCHISM_RUNTIME_ASSERT(dev->stream == stream,
		"streams should never differ");

	if (additional_amount <= 0)
		return;

	if (additional_amount > dev->buflen) {
		dev->buf = mem_realloc(dev->buf, additional_amount);
		dev->buflen = additional_amount;
	}

	mt_mutex_lock(dev->mutex);
	dev->callback(dev->buf, additional_amount);
	mt_mutex_unlock(dev->mutex);

	sdl3_PutAudioStreamData(stream, dev->buf, additional_amount);
}

static void SDLCALL sdl3_audio_input_callback(void *userdata,
	SDL_AudioStream *stream, int additional_amount,
	SCHISM_UNUSED int total_amount)
{
	schism_audio_device_t *dev = (schism_audio_device_t *)userdata;
	int len;

	SCHISM_RUNTIME_ASSERT(dev->stream == stream,
		"streams should never differ");

	if (additional_amount <= 0)
		return;

	/* reallocate internal buffer if necessary */
	if (additional_amount > dev->buflen) {
		dev->buf = mem_realloc(dev->buf, additional_amount);
		dev->buflen = additional_amount;
	}

	len = sdl3_GetAudioStreamData(stream, dev->buf, additional_amount);

	mt_mutex_lock(dev->mutex);
	dev->callback(dev->buf, len);
	mt_mutex_unlock(dev->mutex);
}

static const SDL_AudioStreamCallback callbacks[DEVICES_MAX_] = {
	[DEVICES_OUT] = sdl3_audio_output_callback,
	[DEVICES_IN]  = sdl3_audio_input_callback,
};

static void sdl3_audio_close_device(schism_audio_device_t *dev);

static schism_audio_device_t *sdl3_audio_open_device(uint32_t id,
	const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	schism_audio_device_t *dev;
	SDL_AudioDeviceID sdl_dev_id;
	uint32_t format;
	uint32_t mid;
	int off;

	dev = mem_calloc(1, sizeof(*dev));
	dev->callback = desired->callback;

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
		/* As it turns out, SDL is still just a shell script in disguise, and requires you to
		 * pass everything as strings in order to change behavior. As for why they don't just
		 * include this in the spec structure anymore is beyond me. */
		char buf[64];
		str_from_num(0, desired->samples, buf);
		sdl3_SetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES, buf);
	}

	off = (id & AUDIO_BACKEND_CAPTURE) ? DEVICES_IN : DEVICES_OUT;

	mid = (id & AUDIO_BACKEND_DEVICE_MASK);

	sdl_dev_id = (mid >= (uint32_t)devices[off].size || mid == AUDIO_BACKEND_DEFAULT)
		? ((id & AUDIO_BACKEND_CAPTURE)
			? SDL_AUDIO_DEVICE_DEFAULT_RECORDING
			: SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK)
		: devices[off].arr[mid];

	dev->stream = sdl3_OpenAudioDeviceStream(sdl_dev_id, &sdl_desired, callbacks[off], dev);

	/* reset this before checking if opening succeeded */
	sdl3_ResetHint(SDL_HINT_AUDIO_DEVICE_SAMPLE_FRAMES);

	if (!dev->stream)
		goto fail;

	/* For the most part we can just copy everything */
	memcpy(obtained, desired, sizeof(schism_audio_spec_t));

	/* Retrieve the actual buffer size SDL is using (i.e., don't lie to the user)
	 * This can also improve speeds since we won't have to deal with different
	 * buffer sizes clashing ;) */
	{
		int samples;
		SDL_AudioSpec xyzzy;
		if (sdl3_GetAudioDeviceFormat(sdl_dev_id, &xyzzy, &samples))
			obtained->samples = samples;
	}

	/* before we hop into the callback, allocate an estimate of what SDL
	 * is going to want */
	dev->buflen = obtained->samples * obtained->channels * (obtained->bits / 8);
	dev->buf = mem_alloc(dev->buflen);

	return dev;

fail:
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

	free(dev->buf);

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
	SCHISM_SDL3_SYM(GetAudioRecordingDevices);
	SCHISM_SDL3_SYM(GetAudioDeviceName);

	SCHISM_SDL3_SYM(OpenAudioDeviceStream);
	SCHISM_SDL3_SYM(DestroyAudioStream);
	SCHISM_SDL3_SYM(PauseAudioDevice);
	SCHISM_SDL3_SYM(ResumeAudioDevice);
	SCHISM_SDL3_SYM(GetAudioStreamDevice);
	SCHISM_SDL3_SYM(GetAudioDeviceFormat);
	SCHISM_SDL3_SYM(PutAudioStreamData);
	SCHISM_SDL3_SYM(GetAudioStreamData);

	SCHISM_SDL3_SYM(SetHint);
	SCHISM_SDL3_SYM(ResetHint);

	SCHISM_SDL3_SYM(free);

	SCHISM_SDL3_SYM(SetHintWithPriority);
	SCHISM_SDL3_SYM(ResetHint);

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
