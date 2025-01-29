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

// Win32 Waveout audio backend - written because SDL 1.2 kind of sucks, and
// this driver is especially terrible there.
//  - paper

#include "headers.h"
#include "charset.h"
#include "threads.h"
#include "mem.h"
#include "osdefs.h"
#include "backend/audio.h"

#include <windows.h>

// Define this ourselves; old toolchains don't have it
struct waveoutcaps2w {
	WORD wMid;
	WORD wPid;
	MMVERSION vDriverVersion;
	WCHAR szPname[MAXPNAMELEN];
	DWORD dwFormats;
	WORD wChannels;
	WORD wReserved1;
	DWORD dwSupport;
	GUID ManufacturerGuid;
	GUID ProductGuid;
	GUID NameGuid;
};

// This is needed because waveout is weird, and the WAVEHDR buffers need
// some time to "cool down", so we cycle between buffers.
#define NUM_BUFFERS 2
SCHISM_STATIC_ASSERT(NUM_BUFFERS >= 2, "NUM_BUFFERS must be at least 2");

struct schism_audio_device {
	// The thread where we callback
	mt_thread_t *thread;
	int cancelled;

	// The callback and the protecting mutex
	void (*callback)(uint8_t *stream, int len);
	mt_mutex_t *mutex;

	// This is for synchronizing the audio thread with
	// the actual audio device
	HANDLE sem;

	HWAVEOUT hwaveout;

	// The allocated raw mixing buffer
	uint8_t *buffer;

	WAVEHDR wavehdr[NUM_BUFFERS];
	int next_buffer;
};

/* ---------------------------------------------------------- */
/* drivers */

// lol
static const char *drivers[] = {
	"waveout",
};

static int waveout_audio_driver_count()
{
	return ARRAY_SIZE(drivers);
}

static const char *waveout_audio_driver_name(int i)
{
	if (i >= ARRAY_SIZE(drivers) || i < 0)
		return NULL;

	return drivers[i];
}

/* ------------------------------------------------------------------------ */

// devices name cache; refreshed after every call to waveout_audio_device_count
static struct {
	uint32_t id;
	char *name;
} *devices = NULL;
static size_t devices_size = 0;

static uint32_t waveout_audio_device_count(void)
{
	const UINT devs = waveOutGetNumDevs();

	if (devices) {
		for (size_t i = 0; i < devices_size; i++)
			free(devices[i].name);
		free(devices);
	}

	devices = mem_alloc(sizeof(*devices) * devs);
	devices_size = 0;

	UINT i;
	for (i = 0; i < devs; i++) {
		union {
#ifdef SCHISM_WIN32_COMPILE_ANSI
			WAVEOUTCAPSA a;
#endif
			WAVEOUTCAPSW w;
			struct waveoutcaps2w w2;
		} caps = {0};

#ifdef SCHISM_WIN32_COMPILE_ANSI
		int win9x = (GetVersion() & UINT32_C(0x80000000));
		if (win9x) {
			if (waveOutGetDevCapsA(i, &caps.a, sizeof(caps.a)) != MMSYSERR_NOERROR)
				continue;

			if (charset_iconv(caps.a.szPname, &devices[devices_size], CHARSET_ANSI, CHARSET_UTF8, sizeof(caps.a.szPname)))
				continue;
		} else
#endif
		{
			// Try WAVEOUTCAPS2 before WAVEOUTCAPS
			if (waveOutGetDevCapsW(i, (LPWAVEOUTCAPSW)&caps.w2, sizeof(caps.w2)) == MMSYSERR_NOERROR) {
				// Try receiving based on the name GUID. Otherwise, fall back to the short name.
				if (!win32_audio_lookup_device_name(&caps.w2.NameGuid, &devices[devices_size].name)
					&& charset_iconv(caps.w2.szPname, &devices[devices_size].name, CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(caps.w2.szPname)))
					continue;
			} else if (waveOutGetDevCapsW(i, &caps.w, sizeof(caps.w)) == MMSYSERR_NOERROR) {
				if (charset_iconv(caps.w.szPname, &devices[devices_size].name, CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(caps.w.szPname)))
					continue;
			} else {
				continue;
			}
		}

		devices[devices_size].id = i;

		devices_size++;
	}

	return devs;
}

static const char *waveout_audio_device_name(uint32_t i)
{
	// If this ever happens it is a catastrophic bug and we
	// should crash before anything bad happens.
	if (i >= devices_size)
		return NULL;

	return devices[i].name;
}

/* ---------------------------------------------------------- */

static int waveout_audio_init_driver(const char *driver)
{
	int fnd = 0;
	for (int i = 0; i < ARRAY_SIZE(drivers); i++) {
		if (!strcmp(drivers[i], driver)) {
			fnd = 1;
			break;
		}
	}
	if (!fnd)
		return -1;

	// Get the devices
	(void)waveout_audio_device_count();
	return 0;
}

static void waveout_audio_quit_driver(void)
{
	// Free the devices
	if (devices) {
		for (size_t i = 0; i < devices_size; i++)
			free(devices[i].name);
		free(devices);

		devices = NULL;
	}
}

/* -------------------------------------------------------- */

static int waveout_audio_thread(void *data)
{
	schism_audio_device_t *dev = data;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	while (!dev->cancelled) {
		// FIXME add a timeout here
		mt_mutex_lock(dev->mutex);

		dev->callback(dev->wavehdr[dev->next_buffer].lpData, dev->wavehdr[dev->next_buffer].dwBufferLength);

		mt_mutex_unlock(dev->mutex);

		waveOutWrite(dev->hwaveout, &dev->wavehdr[dev->next_buffer], sizeof(dev->wavehdr[dev->next_buffer]));
		dev->next_buffer = (dev->next_buffer + 1) % NUM_BUFFERS;

		// Wait until either the semaphore is polled or we've been asked to exit
		// This prevents a hang on device close, since after waveOutClose is called
		// the semaphore is never released, which will cause us to wait forever.
		while (!dev->cancelled && (WaitForSingleObject(dev->sem, 10) == WAIT_TIMEOUT));
	}

	return 0;
}

static void CALLBACK waveout_audio_callback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	schism_audio_device_t *dev = (schism_audio_device_t *)dwInstance;

	// don't care about other messages
	switch (uMsg) {
	case WOM_DONE:
	case WOM_CLOSE:
		ReleaseSemaphore(dev->sem, 1, NULL);
	default: return;
	}
}

// nonzero on success
static schism_audio_device_t *waveout_audio_open_device(uint32_t id, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	// Default to some device that can handle our output
	UINT device_id = (id == AUDIO_BACKEND_DEFAULT || id < devices_size) ? (WAVE_MAPPER) : devices[id].id;

	// Fill in the format structure
	WAVEFORMATEX format = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = desired->channels,
		.nSamplesPerSec = desired->freq,
	};

	// filter invalid bps values (should never happen, but eh...)
	switch (desired->bits) {
	case 8: format.wBitsPerSample = 8; break;
	default:
	case 16: format.wBitsPerSample = 16; break;
	case 32: format.wBitsPerSample = 32; break;
	}

	// ok, now we can allocate the device
	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));

	dev->callback = desired->callback;

	for (;;) {
		// Recalculate format
		format.nBlockAlign = format.nChannels * (format.wBitsPerSample / 8);
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

		MMRESULT err = waveOutOpen(&dev->hwaveout, device_id, &format, (UINT_PTR)waveout_audio_callback, (UINT_PTR)dev, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
		if (err == MMSYSERR_NOERROR) {
			// We're done here
			break;
		} else if (err == WAVERR_BADFORMAT) {
			// Retry with 16-bit. 32-bit samples
			// don't work everywhere (notably windows xp
			// is broken and so is everything before it)
			if (format.wBitsPerSample == 32) {
				format.wBitsPerSample = 16;
				continue;
			}
		}

		// Punt if we failed and we can't do anything about it
		free(dev);
		return NULL;
	}

	dev->mutex = mt_mutex_create();
	if (!dev->mutex) {
		waveOutClose(dev->hwaveout);
		free(dev);
		return NULL;
	}

	dev->sem = CreateSemaphoreA(NULL, 1, NUM_BUFFERS, "WinMM audio sync semaphore");
	if (!dev->sem) {
		waveOutClose(dev->hwaveout);
		mt_mutex_delete(dev->mutex);
		free(dev);
		return NULL;
	}

	// allocate the buffer
	DWORD buflen = desired->samples * format.nChannels * (format.wBitsPerSample / 8);
	dev->buffer = mem_alloc(buflen * NUM_BUFFERS);

	// fill in the wavehdrs
	for (int i = 0; i < NUM_BUFFERS; i++) {
		dev->wavehdr[i].lpData = dev->buffer + (buflen * i);
		dev->wavehdr[i].dwBufferLength = buflen;
		dev->wavehdr[i].dwFlags = WHDR_DONE;

		if (waveOutPrepareHeader(dev->hwaveout, &dev->wavehdr[i], sizeof(dev->wavehdr[i])) != MMSYSERR_NOERROR) {
			waveOutClose(dev->hwaveout);
			mt_mutex_delete(dev->mutex);
			free(dev->buffer);
			free(dev);
			return NULL;
		}
	}

	// ok, now start the full thread
	dev->thread = mt_thread_create(waveout_audio_thread, "WinMM audio thread", dev);
	if (!dev->thread) {
		waveOutClose(dev->hwaveout);
		CloseHandle(dev->sem);
		mt_mutex_delete(dev->mutex);
		free(dev);
		return NULL;
	}

	obtained->freq = format.nSamplesPerSec;
	obtained->channels = format.nChannels;
	obtained->bits = format.wBitsPerSample;
	obtained->samples = desired->samples;

	return dev;
}

static void waveout_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	// kill the thread before doing anything else
	dev->cancelled = 1;
	mt_thread_wait(dev->thread, NULL);

	// close the device
	waveOutClose(dev->hwaveout);

	for (int i = 0; i < NUM_BUFFERS; i++) {
		// sleep until the device is done with our buffer
		while (!(dev->wavehdr[i].dwFlags & WHDR_DONE))
			timer_delay(10);

		waveOutUnprepareHeader(dev->hwaveout, &dev->wavehdr[i], sizeof(dev->wavehdr[i]));
	}

	free(dev->buffer);

	CloseHandle(dev->sem);
	mt_mutex_delete(dev->mutex);
	free(dev);
}

static void waveout_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
}

static void waveout_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_unlock(dev->mutex);
}

static void waveout_audio_pause_device(schism_audio_device_t *dev, int paused)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
	if (paused) {
		waveOutPause(dev->hwaveout);
	} else {
		waveOutRestart(dev->hwaveout);
	}
	mt_mutex_unlock(dev->mutex);
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static int waveout_audio_init(void)
{
	return 1;
}

static void waveout_audio_quit(void)
{
	// dont do anything
}

//////////////////////////////////////////////////////////////////////////////

const schism_audio_backend_t schism_audio_backend_waveout = {
	.init = waveout_audio_init,
	.quit = waveout_audio_quit,

	.driver_count = waveout_audio_driver_count,
	.driver_name = waveout_audio_driver_name,

	.device_count = waveout_audio_device_count,
	.device_name = waveout_audio_device_name,

	.init_driver = waveout_audio_init_driver,
	.quit_driver = waveout_audio_quit_driver,

	.open_device = waveout_audio_open_device,
	.close_device = waveout_audio_close_device,
	.lock_device = waveout_audio_lock_device,
	.unlock_device = waveout_audio_unlock_device,
	.pause_device = waveout_audio_pause_device,
};
