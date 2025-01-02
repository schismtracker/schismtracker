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
#include "charset.h"
#include "threads.h"
#include "mem.h"
#include "backend/audio.h"

#include <windows.h>

struct schism_audio_device {
	schism_thread_t *thread;
	schism_mutex_t *mutex;

	// This is for synchronizing the audio thread with
	// the actual audio device
	HANDLE sem;

	HWAVEOUT hwaveout;

	unsigned char *buffer;
	uint32_t buffer_size; // in BYTES

	int cancelled;

	void (*callback)(uint8_t *stream, int len);
};

/* ---------------------------------------------------------- */
/* drivers */

static const char *drivers[] = {
	"waveout",
	"winmm",
};

static int win32_audio_driver_count()
{
	return ARRAY_SIZE(drivers);
}

static const char *win32_audio_driver_name(int i)
{
	if (i >= ARRAY_SIZE(drivers) || i < 0)
		return NULL;

	return drivers[i];
}

/* --------------------------------------------------------------- */

static char **devices = NULL;
static size_t devices_size = 0;

static int win32_audio_device_count(void)
{
	const UINT devs = waveOutGetNumDevs();

	for (size_t i = 0; i < devices_size; i++)
		free(devices[i]);
	free(devices);

	devices = mem_alloc(sizeof(*devices) * (devs + 1));

	UINT i;
	for (i = 0; i < devs; i++) {
		union {
			WAVEOUTCAPSA a;
			WAVEOUTCAPSW w;
		} caps;
		int win9x = GetVersion() & UINT32_C(0x80000000);
		if (win9x) {
			if (waveOutGetDevCapsA(i, &caps.a, sizeof(caps.a)) != MMSYSERR_NOERROR)
				continue;
		} else {
			if (waveOutGetDevCapsW(i, &caps.w, sizeof(caps.w)) != MMSYSERR_NOERROR)
				continue;
		}

		if (charset_iconv(win9x ? (void *)caps.a.szPname : (void *)caps.w.szPname, devices + i, win9x ? CHARSET_ANSI : CHARSET_WCHAR_T, CHARSET_UTF8, SIZE_MAX))
			continue;
	}
	devices[i] = NULL;

	return devs;
}

static const char *win32_audio_device_name(int i)
{
	if (i >= devices_size || i < 0 || !devices[i])
		return "";

	return devices[i];
}

/* ---------------------------------------------------------- */

static int win32_audio_init_driver(const char *driver)
{
	return 0;
}

static void win32_audio_quit_driver(void)
{
}

/* -------------------------------------------------------- */

static int win32_audio_thread(void *data)
{
	schism_audio_device_t *dev = data;

	while (!dev->cancelled) {
		mt_mutex_lock(dev->mutex);

		dev->callback(dev->buffer, dev->buffer_size);

		mt_mutex_unlock(dev->mutex);

		// Now actually send the data to the wave device
		WAVEHDR hdr = {
			.lpData = dev->buffer,
			.dwBufferLength = dev->buffer_size,
		};

		if (waveOutPrepareHeader(dev->hwaveout, &hdr, sizeof(hdr)) != MMSYSERR_NOERROR)
			continue;

		if (waveOutWrite(dev->hwaveout, &hdr, sizeof(hdr)) != MMSYSERR_NOERROR)
			continue;

		WaitForSingleObject(dev->sem, INFINITE);
	}

	return 0;
}

static void CALLBACK win32_audio_callback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
{
	schism_audio_device_t *dev = (schism_audio_device_t *)dwInstance;

	if (uMsg != WOM_DONE)
		return;

	ReleaseSemaphore(dev->sem, 1, NULL);
}

// nonzero on success
static schism_audio_device_t *win32_audio_open_device(const char *name, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	UINT device_id = 0;
	if (name) {
		int fnd = 0;
		for (; device_id < devices_size; device_id++)
			if (!strcmp(devices[device_id], name))
				fnd = 1;
		if (!fnd)
			return NULL;
	}
	// found the device ID...

	WAVEFORMATEX format = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = desired->channels,
		.nSamplesPerSec = desired->freq,
	};

	switch (desired->bits) {
	case 8: format.wBitsPerSample = 8; break;
	default:
	case 16: format.wBitsPerSample = 16; break;
	}

	format.nBlockAlign = format.nChannels * format.wBitsPerSample / 8;
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));

	dev->callback = desired->callback;

	MMRESULT err = waveOutOpen(&dev->hwaveout, device_id, &format, (UINT_PTR)win32_audio_callback, (UINT_PTR)dev, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
	if (err != MMSYSERR_NOERROR) {
		printf("waveOutOpen: %u\n", err);
		free(dev);
		return NULL;
	}

	dev->mutex = mt_mutex_create();
	if (!dev->mutex) {
		waveOutClose(dev->hwaveout);
		free(dev);
		return NULL;
	}

	dev->sem = CreateSemaphoreA(NULL, 1, 2, "WinMM audio sync semaphore");
	if (!dev->sem) {
		waveOutClose(dev->hwaveout);
		mt_mutex_delete(dev->mutex);
		free(dev);
	}

	// agh !
	dev->buffer_size = desired->samples * desired->channels * (desired->bits / 8);
	dev->buffer = mem_alloc(dev->buffer_size);

	// start NOW
	dev->thread = mt_thread_create(win32_audio_thread, "WinMM audio thread", dev);
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

static void win32_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	// kill the thread before doing anything else
	dev->cancelled = 1;
	mt_thread_wait(dev->thread, NULL);

	// close the device
	waveOutClose(dev->hwaveout);

	CloseHandle(dev->sem);
	mt_mutex_delete(dev->mutex);
	free(dev->buffer);
	free(dev);
}

static void win32_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
}

static void win32_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_unlock(dev->mutex);
}

static void win32_audio_pause_device(schism_audio_device_t *dev, int paused)
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

static int win32_audio_init(void)
{
	return 1;
}

static void win32_audio_quit(void)
{
	// dont do anything
}

//////////////////////////////////////////////////////////////////////////////

const schism_audio_backend_t schism_audio_backend_win32 = {
	.init = win32_audio_init,
	.quit = win32_audio_quit,

	.driver_count = win32_audio_driver_count,
	.driver_name = win32_audio_driver_name,

	.device_count = win32_audio_device_count,
	.device_name = win32_audio_device_name,

	.init_driver = win32_audio_init_driver,
	.quit_driver = win32_audio_quit_driver,

	.open_device = win32_audio_open_device,
	.close_device = win32_audio_close_device,
	.lock_device = win32_audio_lock_device,
	.unlock_device = win32_audio_unlock_device,
	.pause_device = win32_audio_pause_device,
};
