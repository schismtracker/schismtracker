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
// this driver is especially terrible there. If it's wanted I can write
// a directsound backend but tbh I don't care for the few systems that
// actually need it. (namely, probably win2000 and maybe winme.)
//  - paper

#include "headers.h"
#include "charset.h"
#include "threads.h"
#include "mem.h"
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
	schism_thread_t *thread;
	int cancelled;

	// The callback and the protecting mutex
	void (*callback)(uint8_t *stream, int len);
	schism_mutex_t *mutex;

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

/* ------------------------------------------------------------------------ */

// devices name cache; refreshed after every call to win32_audio_device_count
static char **devices = NULL;
static size_t devices_size = 0;

// FIXME: Apparently some devices provide a name guid, but nothing seems to
// actually be put into MediaCategories, which essentially makes this useless.
static int _win32_audio_lookup_device_name(const GUID *nameguid, char **result)
{
#define GUIDF "%08" PRIx32 "-%04" PRIx16 "-%04" PRIx16 "-%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8 "%02" PRIx8
#define GUIDX(x) (x).Data1, (x).Data2, (x).Data3, (x).Data4[0], (x).Data4[1], (x).Data4[2], (x).Data4[3], (x).Data4[4], (x).Data4[5], (x).Data4[6], (x).Data4[7]
	// Set this to NULL before doing anything
	*result = NULL;

	// A list of key formats to try (only one for now, i guess)
	static const struct {
		LPCWSTR key, value;
	} key_fmts[] = {
		{L"SYSTEM\\CurrentControlSet\\Control\\MediaCategories\\{" GUIDF "}", L"Name"},
	};

	for (int i = 0; i < ARRAY_SIZE(key_fmts); i++) {
		WCHAR *strw = NULL;
		DWORD len = 0;

		static const GUID nullguid = {0};
		if (!memcmp(nameguid, &nullguid, sizeof(nullguid)))
			return 0;

		{
			HKEY hkey;
			WCHAR keystr[256];
			_snwprintf(keystr, ARRAY_SIZE(keystr), key_fmts[i].key, GUIDX(*nameguid));
			if (RegOpenKeyExW(HKEY_LOCAL_MACHINE, keystr, 0, KEY_QUERY_VALUE, &hkey) != ERROR_SUCCESS)
				continue;

			DWORD len;
			if (RegQueryValueExW(hkey, key_fmts[i].value, NULL, NULL, NULL, &len) != ERROR_SUCCESS) {
				RegCloseKey(hkey);
				continue;
			}

			WCHAR *strw = mem_alloc(len + sizeof(WCHAR));

			if (RegQueryValueExW(hkey, key_fmts[i].value, NULL, NULL, (LPBYTE)strw, &len) != ERROR_SUCCESS) {
				RegCloseKey(hkey);
				free(strw);
				continue;
			}

			RegCloseKey(hkey);
		}

		// force NUL terminate
		strw[len >> 1] = L'\0';

		if (charset_iconv(strw, result, CHARSET_WCHAR_T, CHARSET_UTF8, len + sizeof(WCHAR))) {
			free(strw);
			continue;
		}

		free(strw);
		return 1;
	}

	return 0;
#undef GUIDF
#undef GUIDX
}

static int win32_audio_device_count(void)
{
	const UINT devs = waveOutGetNumDevs();

	if (devices) {
		for (size_t i = 0; i < devices_size; i++)
			free(devices[i]);
		free(devices);
	}

	devices = mem_alloc(sizeof(*devices) * devs);
	devices_size = 0;

	UINT i;
	for (i = 0; i < devs; i++) {
		union {
			WAVEOUTCAPSA a;
			WAVEOUTCAPSW w;
			struct waveoutcaps2w w2;
		} caps = {0};
		int win9x = (GetVersion() & UINT32_C(0x80000000));
		if (win9x) {
			if (waveOutGetDevCapsA(i, &caps.a, sizeof(caps.a)) != MMSYSERR_NOERROR)
				continue;

			if (charset_iconv(caps.a.szPname, &devices[devices_size], CHARSET_ANSI, CHARSET_UTF8, sizeof(caps.a.szPname)))
				continue;
		} else {
			// Try WAVEOUTCAPS2 before WAVEOUTCAPS
			if (waveOutGetDevCapsW(i, (LPWAVEOUTCAPSW)&caps.w2, sizeof(caps.w2)) == MMSYSERR_NOERROR) {
				// Try receiving based on the name GUID. Otherwise, fall back to the short name.
				if (!_win32_audio_lookup_device_name(&caps.w2.NameGuid, &devices[devices_size])
					&& charset_iconv(caps.w2.szPname, &devices[devices_size], CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(caps.w2.szPname)))
					continue;
			} else if (waveOutGetDevCapsW(i, &caps.w, sizeof(caps.w)) == MMSYSERR_NOERROR) {
				if (charset_iconv(caps.w.szPname, &devices[devices_size], CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(caps.w.szPname)))
					continue;
			} else {
				continue;
			}
		}

		if (charset_iconv(win9x ? ((void *)caps.a.szPname) : ((void *)caps.w.szPname), &devices[devices_size], win9x ? CHARSET_ANSI : CHARSET_WCHAR_T, CHARSET_UTF8, win9x ? (sizeof(caps.a.szPname)) : (sizeof(caps.w.szPname))))
			continue;

		devices_size++;
	}

	return devs;
}

static const char *win32_audio_device_name(int i)
{
	// If this ever happens it is a catastrophic bug and we
	// should crash before anything bad happens.
	if (i >= devices_size || i < 0)
		return NULL;

	return devices[i];
}

/* ---------------------------------------------------------- */

static int win32_audio_init_driver(const char *driver)
{
	// Get the devices
	(void)win32_audio_device_count();
	return 0;
}

static void win32_audio_quit_driver(void)
{
	// Free the devices
	if (devices) {
		for (size_t i = 0; i < devices_size; i++)
			free(devices[i]);
		free(devices);

		devices = NULL;
	}
}

/* -------------------------------------------------------- */

static int win32_audio_thread(void *data)
{
	schism_audio_device_t *dev = data;

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

static void CALLBACK win32_audio_callback(HWAVEOUT hwo, UINT uMsg, DWORD_PTR dwInstance, DWORD dwParam1, DWORD dwParam2)
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
static schism_audio_device_t *win32_audio_open_device(const char *name, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	// Default to some device that can handle our output
	UINT device_id = WAVE_MAPPER;
	
	// Lookup the device ID if one is requested
	if (name) {
		device_id = 0;

		int fnd = 0;
		for (; device_id < devices_size; device_id++) {
			if (!strcmp(devices[device_id], name)) {
				fnd = 1;
				break;
			}
		}
		
		// Fail; the audio code will try again with a
		// default device
		if (!fnd)
			return NULL;
	}

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
	// TODO does this actually work
	case 32: format.wBitsPerSample = 32; break;
	}

	format.nBlockAlign = format.nChannels * (format.wBitsPerSample / 8);
	format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

	// ok, now we can allocate the device
	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));

	dev->callback = desired->callback;

	MMRESULT err = waveOutOpen(&dev->hwaveout, device_id, &format, (UINT_PTR)win32_audio_callback, (UINT_PTR)dev, CALLBACK_FUNCTION | WAVE_ALLOWSYNC);
	if (err != MMSYSERR_NOERROR) {
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
	DWORD buflen = desired->samples * desired->channels * (desired->bits / 8);
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
