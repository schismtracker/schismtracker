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

#include <SDL.h>

struct schism_audio_device {
	void (*callback)(uint8_t *stream, int len);
};

/* ---------------------------------------------------------- */
/* drivers */

static const char *drivers[] = {
	/* SDL 1.2 doesn't provide an API for this, lol */
	"pulse",
	"alsa",
	"artsc",
	"arts",
	"esd",
	"dsp",
	"dma",
	"sndio",
	"qnxnto",
	"audio", // SUN audio
	"dmedia", // ???
	"paud",
	"baudio",
	"sndmgr",
	"mint",

	"coreaudio",

	/* win32 */
#ifdef SCHISM_WIN32
	"dsound",
	"waveout",
#endif

	/* these two absolutely have to be last */
	"disk",
	"dummy",
};

int sdl12_audio_driver_count()
{
	return ARRAY_SIZE(drivers);
}

const char *sdl12_audio_driver_name(int i)
{
	if (i < 0 || i >= ARRAY_SIZE(drivers))
		return 0;

	return drivers[i];
}

/* --------------------------------------------------------------- */

/* SDL 1.2 doesn't have a concept of audio devices */
int sdl12_audio_device_count(void)
{
	return 0;
}

const char *sdl12_audio_device_name(int i)
{
	return NULL;
}

/* ---------------------------------------------------------- */

int sdl12_audio_init(const char *name)
{
	const char *orig_drv = getenv("SDL_AUDIODRIVER");

	if (name)
		setenv("SDL_AUDIODRIVER", name, 1);

	int ret = SDL_InitSubSystem(SDL_INIT_AUDIO);

	/* clean up our dirty work, or empty the var */
	setenv("SDL_AUDIODRIVER", orig_drv ? orig_drv : "", 1);

	/* forward any error, if any */
	return ret;
}

void sdl12_audio_quit(void)
{
	SDL_QuitSubSystem(SDL_INIT_AUDIO);
}

/* -------------------------------------------------------- */

// This is here to prevent having to put SDLCALL into
// the original audio callback
static void SDLCALL sdl12_dummy_callback(void *userdata, uint8_t *stream, int len)
{
	// call our own callback
	schism_audio_device_t *dev = userdata;

	dev->callback(stream, len);
}

schism_audio_device_t *sdl12_audio_open_device(const char *name, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));
	dev->callback = desired->callback;
	//dev->userdata = desired->userdata;

	SDL_AudioSpec sdl_desired = {
		.freq = desired->freq,
		.format = (desired->bits == 8) ? (AUDIO_U8) : (AUDIO_S16),
		.channels = desired->channels,
		.samples = desired->samples,
		.callback = sdl12_dummy_callback,
		.userdata = dev,
	};
	SDL_AudioSpec sdl_obtained;

	if (SDL_OpenAudio(&sdl_desired, &sdl_obtained)) {
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

void sdl12_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	SDL_CloseAudio();
	free(dev);
}

/* lock/unlock/pause */

void sdl12_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	SDL_LockAudio();
}

void sdl12_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	SDL_UnlockAudio();
}

void sdl12_audio_pause_device(schism_audio_device_t *dev, int paused)
{
	if (!dev)
		return;

	SDL_PauseAudio(paused);
}
