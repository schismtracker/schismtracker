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
#include "util.h"
#include "backend/audio.h"
#include "loadso.h"

#include "init.h"

struct schism_audio_device {
	void (*callback)(uint8_t *stream, int len);
};

static int (SDLCALL *sdl12_InitSubSystem)(uint32_t flags);
static void (SDLCALL *sdl12_QuitSubSystem)(uint32_t flags);

static int (SDLCALL *sdl12_OpenAudio)(SDL_AudioSpec *desired, SDL_AudioSpec *obtained);
static void (SDLCALL *sdl12_CloseAudio)(void);
static void (SDLCALL *sdl12_LockAudio)(void);
static void (SDLCALL *sdl12_UnlockAudio)(void);
static void (SDLCALL *sdl12_PauseAudio)(int);

static void (SDLCALL *sdl12_SetError)(const char *fmt, ...);
static char * (SDLCALL *sdl12_GetError)(void);
static void (SDLCALL *sdl12_ClearError)(void);

static int (SDLCALL *sdl12_AudioInit)(const char *driver);
static void (SDLCALL *sdl12_AudioQuit)(void);

/* ---------------------------------------------------------- */
/* drivers */

static int sdl12_audio_init_driver(const char *name)
{
	char *orig_drv;
	{
		// Calling getenv() with a subsequent setenv() causes
		// the original pointer to get lost, so we have to
		// duplicate it.
		const char *orig_drv_unsafe = getenv("SDL_AUDIODRIVER");
		orig_drv = (orig_drv_unsafe) ? str_dup(orig_drv_unsafe) : NULL;
	}

	if (name)
		setenv("SDL_AUDIODRIVER", name, 1);

	int ret = sdl12_InitSubSystem(SDL_INIT_AUDIO);

	/* clean up our dirty work, or unset the var */
	if (name) {
		if (orig_drv) {
			setenv("SDL_AUDIODRIVER", orig_drv, 1);
		} else {
			unsetenv("SDL_AUDIODRIVER");
		}
	}

	if (orig_drv)
		free(orig_drv);

	/* forward any error, if any */
	return ret;
}

static void sdl12_audio_quit_driver(void)
{
	sdl12_QuitSubSystem(SDL_INIT_AUDIO);
}

/* ---------------------------------------------------------- */

/* This method will silently fail for drivers that don't work on startup,
 * but this can be remedied by simply restarting Schism. */

static struct sdl12_audio_driver_info {
	const char *driver;
	int exists;
} drivers[] = {
	/* SDL 1.2 doesn't provide an API for this, so we have to
	 * build this when initializing the audio subsystem. */
	{"openbsd", 0},
	{"pulse", 0}, // prefer pulseaudio to alsa, then oss
	{"alsa", 0},
	{"dsp", 0},
	{"audio", 0},
	{"AL", 0},
	{"artsc", 0},
	{"esd", 0},
	{"nas", 0},
	{"dma", 0},
	{"coreaudio", 0},
	{"dsound", 0},
	{"waveout", 0},
	{"baudio", 0},
	{"sndmgr", 0},
	{"paud", 0},
	{"AHI", 0},
	{"nds", 0},
	{"dart", 0},
	{"dcaudio", 0},

	// These two are pretty much guaranteed to exist
	{"disk", 1},
	{"dummy", 1},
};

static int sdl12_audio_driver_info_init()
{
	int atleast_one_loaded = 0;

	/* save the last error before we screw with our crap */
	const char *cached_err = sdl12_GetError();

	for (int i = 0; i < ARRAY_SIZE(drivers); i++) {
		// Clear any error before starting so we
		// can check for one later
		sdl12_ClearError();

		if (sdl12_AudioInit(drivers[i].driver))
			continue;

		// Make sure there was no error initializing the
		// driver. (SDL sets the error here to "No available
		// audio device", but we'll use a more broad test
		// instead)
		const char *audio_init_err = sdl12_GetError();
		if (audio_init_err && *audio_init_err)
			continue;

		// Ok, the driver was bootstrapped successfully, now
		// punt and save that info.
		sdl12_AudioQuit();
		atleast_one_loaded = drivers[i].exists = 1;
	}

	/* restore the last error */
	sdl12_SetError(cached_err);

	return atleast_one_loaded;
}

static void sdl12_audio_driver_info_quit()
{
	// reset
	for (int i = 0; i < ARRAY_SIZE(drivers); i++)
		drivers[i].exists = 0;
}

static int sdl12_audio_driver_count()
{
	int c = 0;
	for (int i = 0; i < ARRAY_SIZE(drivers); i++)
		if (drivers[i].exists)
			c++;
	return c;
}

static const char *sdl12_audio_driver_name(int x)
{
	int i = 0;
	for (int c = 0; i < ARRAY_SIZE(drivers); i++) {
		if (!drivers[i].exists)
			continue;

		if (c == x)
			break;

		c++;
	}
	return (i < ARRAY_SIZE(drivers)) ? drivers[i].driver : NULL;
}

/* --------------------------------------------------------------- */

/* SDL 1.2 doesn't have a concept of audio devices */
static int sdl12_audio_device_count(void)
{
	return 0;
}

static const char *sdl12_audio_device_name(int i)
{
	return NULL;
}

/* -------------------------------------------------------- */

static void sdl12_dummy_callback(void *userdata, uint8_t *stream, int len)
{
	// call our own callback
	schism_audio_device_t *dev = userdata;

	dev->callback(stream, len);
}

static schism_audio_device_t *sdl12_audio_open_device(const char *name, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));
	dev->callback = desired->callback;
	//dev->userdata = desired->userdata;

	SDL_AudioSpec sdl_desired = {
		.freq = desired->freq,
		// SDL 1.2 has no support for 32-bit audio at all
		.format = (desired->bits == 8) ? (AUDIO_U8) : (AUDIO_S16SYS),
		.channels = desired->channels,
		.samples = desired->samples,
		.callback = sdl12_dummy_callback,
		.userdata = dev,
	};
	SDL_AudioSpec sdl_obtained;

	if (sdl12_OpenAudio(&sdl_desired, &sdl_obtained)) {
		free(dev);
		return NULL;
	}

	*obtained = (schism_audio_spec_t){
		.freq = sdl_obtained.freq,
		.bits = sdl_obtained.format & 0xFF,
		.channels = sdl_obtained.channels,
		.samples = sdl_obtained.samples,
	};

	return dev;
}

static void sdl12_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	sdl12_CloseAudio();
	free(dev);
}

/* lock/unlock/pause */

static void sdl12_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	sdl12_LockAudio();
}

static void sdl12_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	sdl12_UnlockAudio();
}

static void sdl12_audio_pause_device(schism_audio_device_t *dev, int paused)
{
	if (!dev)
		return;

	sdl12_PauseAudio(paused);
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int sdl12_audio_load_syms(void)
{
	SCHISM_SDL12_SYM(InitSubSystem);
	SCHISM_SDL12_SYM(QuitSubSystem);

	SCHISM_SDL12_SYM(OpenAudio);
	SCHISM_SDL12_SYM(CloseAudio);
	SCHISM_SDL12_SYM(LockAudio);
	SCHISM_SDL12_SYM(UnlockAudio);
	SCHISM_SDL12_SYM(PauseAudio);

	SCHISM_SDL12_SYM(SetError);
	SCHISM_SDL12_SYM(GetError);
	SCHISM_SDL12_SYM(ClearError);

	SCHISM_SDL12_SYM(AudioInit);
	SCHISM_SDL12_SYM(AudioQuit);

	return 0;
}

static int sdl12_audio_init(void)
{
	if (!sdl12_init())
		return 0;

	if (sdl12_audio_load_syms())
		return 0;

	if (!sdl12_audio_driver_info_init())
		return 0;

	return 1;
}

static void sdl12_audio_quit(void)
{
	// the subsystem quitting is handled by the quit driver function
	sdl12_audio_driver_info_quit();
	sdl12_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_audio_backend_t schism_audio_backend_sdl12 = {
	.init = sdl12_audio_init,
	.quit = sdl12_audio_quit,

	.driver_count = sdl12_audio_driver_count,
	.driver_name = sdl12_audio_driver_name,

	.device_count = sdl12_audio_device_count,
	.device_name = sdl12_audio_device_name,

	.init_driver = sdl12_audio_init_driver,
	.quit_driver = sdl12_audio_quit_driver,

	.open_device = sdl12_audio_open_device,
	.close_device = sdl12_audio_close_device,
	.lock_device = sdl12_audio_lock_device,
	.unlock_device = sdl12_audio_unlock_device,
	.pause_device = sdl12_audio_pause_device,
};
