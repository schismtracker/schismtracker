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
 
// Win32 directsound backend
// TODO: We ought to be able to detect changes in the default device and
// reopen it if necessary.
// This should be viable via GetDeviceID() in dsound.dll which lets us
// check the GUID of the current default device.

#include "headers.h"
#include "charset.h"
#include "threads.h"
#include "mem.h"
#include "osdefs.h"
#include "loadso.h"
#include "threads.h"
#include "video.h" // video_get_wm_data
#include "backend/audio.h"

// request compatibility with DirectX 5
#define DIRECTSOUND_VERSION 0x0500

#include <windows.h>
#include <dsound.h>

// ripped from SDL
#define NUM_CHUNKS 8

static HRESULT (WINAPI *DSOUND_DirectSoundCreate)(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter) = NULL;

// https://source.winehq.org/WineAPI/dsound.html
#ifdef SCHISM_WIN32_COMPILE_ANSI
static HRESULT (WINAPI *DSOUND_DirectSoundEnumerateA)(LPDSENUMCALLBACKA lpDSEnumCallback, LPVOID lpContext) = NULL;
#endif
static HRESULT (WINAPI *DSOUND_DirectSoundEnumerateW)(LPDSENUMCALLBACKW lpDSEnumCallback, LPVOID lpContext) = NULL;

struct schism_audio_device {
	// The thread where we callback
	mt_thread_t *thread;
	int cancelled;

	// Kid named callback:
	void (*callback)(uint8_t *stream, int len);

	mt_mutex_t *mutex;

	// pass to memset() to make silence.
	// used when paused and when initializing the device buffer
	uint8_t silence;

	LPDIRECTSOUND dsound;
	LPDIRECTSOUNDBUFFER lpbuffer;

	// ...
	int next_chunk;
	int paused;

	// wah, size of one audio buffer
	size_t size;
};

/* ---------------------------------------------------------- */
/* drivers */

// lol
static const char *drivers[] = {
	"dsound",
};

static int dsound_audio_driver_count()
{
	return ARRAY_SIZE(drivers);
}

static const char *dsound_audio_driver_name(int i)
{
	if (i >= ARRAY_SIZE(drivers) || i < 0)
		return NULL;

	return drivers[i];
}

/* ------------------------------------------------------------------------ */

// devices name cache; refreshed after every call to dsound_audio_device_count
static struct {
	GUID guid;
	char *name;
} *devices = NULL;
static size_t devices_size = 0;
static size_t devices_alloc = 0;

static void _dsound_free_devices(void)
{
	if (devices) {
		for (size_t i = 0; i < devices_size; i++)
			free(devices[i].name);
		free(devices);

		devices = NULL;
		devices_size = 0;
		devices_alloc = 0;
	}
}

// This function takes ownership of `name` and is responsible for either freeing it
// or adding it to a list which will eventually be freed.
// Note: lpguid AND name must be valid pointers. No null pointers.
static inline void _dsound_device_append(LPGUID lpguid, char *name)
{
	// Filter out waveout emulated devices. If we don't do this, it causes a bit of
	// CPU overhead and is utterly pointless when we can just use waveout directly
	// anyway.
	{
		LPDIRECTSOUND dsound;
		if (DSOUND_DirectSoundCreate(lpguid, &dsound, NULL) != DS_OK) {
			free(name);
			return;
		}

		DSCAPS caps = {.dwSize = sizeof(DSCAPS)};
		if (IDirectSound_GetCaps(dsound, &caps) != DS_OK
			|| (caps.dwFlags & DSCAPS_EMULDRIVER)) {
			free(name);
			IDirectSound_Release(dsound);
			return;
		}

		IDirectSound_Release(dsound);
	}

	if (devices_size >= devices_alloc) {
		devices_alloc = ((!devices_alloc) ? 1 : (devices_alloc * 2));

		devices = mem_realloc(devices, devices_alloc * sizeof(*devices));
	}

	// put the bread in the basket
	memcpy(&devices[devices_size].guid, lpguid, sizeof(GUID));
	devices[devices_size].name = name;

	devices_size++;
}

// We need two different callbacks for ANSI and UNICODE variants
#define DSOUND_ENUMERATE_CALLBACK_VARIANT(type, charset, suffix) \
	static BOOL CALLBACK _dsound_enumerate_callback_##suffix(LPGUID lpGuid, type lpcstrDescription, SCHISM_UNUSED type lpcstrModule, SCHISM_UNUSED LPVOID lpContext) \
	{ \
		if (lpGuid != NULL) { \
			char *name = NULL; \
	\
			if (win32_audio_lookup_device_name(lpGuid, &name) || !charset_iconv(lpcstrDescription, &name, charset, CHARSET_UTF8, SIZE_MAX)) \
				_dsound_device_append(lpGuid, name); \
	\
			/* device list takes ownership of `name` */ \
		} \
	\
		return TRUE; \
	}

#ifdef SCHISM_WIN32_COMPILE_ANSI
DSOUND_ENUMERATE_CALLBACK_VARIANT(LPCSTR, CHARSET_ANSI, a)
#endif
DSOUND_ENUMERATE_CALLBACK_VARIANT(LPCWSTR, CHARSET_WCHAR_T, w)

#undef DSOUND_ENUMERATE_CALLBACK_VARIANT

static uint32_t dsound_audio_device_count(void)
{
	_dsound_free_devices();

	// Prefer Unicode
	if (DSOUND_DirectSoundEnumerateW) {
		if (DSOUND_DirectSoundEnumerateW(_dsound_enumerate_callback_w, NULL) == DS_OK)
			return devices_size;

		// Free any devices that might have been added
		_dsound_free_devices();
	}

#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (DSOUND_DirectSoundEnumerateA) {
		if (DSOUND_DirectSoundEnumerateA(_dsound_enumerate_callback_a, NULL) == DS_OK)
			return devices_size;

		_dsound_free_devices();
	}
#endif

	return 0;
}

static const char *dsound_audio_device_name(uint32_t i)
{
	// If this ever happens it is a catastrophic bug and we
	// should crash before anything bad happens.
	if (i >= devices_size)
		return NULL;

	return devices[i].name;
}

/* ---------------------------------------------------------- */

static int dsound_audio_init_driver(const char *driver)
{
	for (int i = 0; i < ARRAY_SIZE(drivers); i++) {
		if (!strcmp(drivers[i], driver)) {
			// Get the devices
			(void)dsound_audio_device_count();
			return 0;
		}
	}

	return -1;
}

static void dsound_audio_quit_driver(void)
{
	_dsound_free_devices();
}

/* -------------------------------------------------------- */

static int _dsound_audio_thread(void *data)
{
	schism_audio_device_t *dev = data;

	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	DWORD cursor = 0;
	HRESULT res = DS_OK;

	res = IDirectSoundBuffer_GetCurrentPosition(dev->lpbuffer, NULL, &cursor);
	if (res == DSERR_BUFFERLOST) {
		IDirectSoundBuffer_Restore(dev->lpbuffer);
		res = IDirectSoundBuffer_GetCurrentPosition(dev->lpbuffer, NULL, &cursor);
	}

	if (res != DS_OK)
		return 0; // I don't know why this would ever happen

	// agh !!!
	cursor /= dev->size;

	while (!dev->cancelled) {
		void *buf;
		DWORD buflen;

		DWORD last_chunk = cursor;
		cursor = (cursor + 1) % NUM_CHUNKS;

		res = IDirectSoundBuffer_Lock(dev->lpbuffer, cursor * dev->size, dev->size, &buf, &buflen, NULL, NULL, 0);
		if (res == DSERR_BUFFERLOST) {
			IDirectSoundBuffer_Release(dev->lpbuffer);
			res = IDirectSoundBuffer_Lock(dev->lpbuffer, cursor * dev->size, dev->size, &buf, &buflen, NULL, NULL, 0);
		}
		if (res != DS_OK) {
			timer_msleep(5);
			continue;
		}

		if (dev->paused) {
			memset(buf, dev->silence, buflen);
		} else {
			mt_mutex_lock(dev->mutex);
			dev->callback(buf, buflen);
			mt_mutex_unlock(dev->mutex);
		}

		IDirectSoundBuffer_Unlock(dev->lpbuffer, buf, buflen, NULL, 0);

		do {
			timer_msleep(1);

			DWORD status;
			IDirectSoundBuffer_GetStatus(dev->lpbuffer, &status);
			if (status & DSBSTATUS_BUFFERLOST) {
				IDirectSoundBuffer_Restore(dev->lpbuffer);
				IDirectSoundBuffer_GetStatus(dev->lpbuffer, &status);
				if (status & DSBSTATUS_BUFFERLOST)
					break;
			}

			if (!(status & DSBSTATUS_PLAYING)) {
				if (IDirectSoundBuffer_Play(dev->lpbuffer, 0, 0, DSBPLAY_LOOPING) != DS_OK)
					// This should never happen
					break;
			}

			res = IDirectSoundBuffer_GetCurrentPosition(dev->lpbuffer, NULL, &cursor);
			if (res == DSERR_BUFFERLOST) {
				IDirectSoundBuffer_Restore(dev->lpbuffer);
				res = IDirectSoundBuffer_GetCurrentPosition(dev->lpbuffer, NULL, &cursor);
			}
			if (res != DS_OK)
				continue; // what?

			cursor /= dev->size;
		} while (cursor == last_chunk && !dev->cancelled);
	}

	return 0;
}

// nonzero on success
static schism_audio_device_t *dsound_audio_open_device(uint32_t id, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	// If no device is specified pass NULL to DirectSoundCreate
	LPGUID guid = (id != AUDIO_BACKEND_DEFAULT) ? &devices[id].guid : NULL;

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

	// ok, now allocate
	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));

	dev->callback = desired->callback;

	dev->mutex = mt_mutex_create();
	if (!dev->mutex) {
		free(dev);
		return NULL;
	}

	if (DSOUND_DirectSoundCreate(guid, &dev->dsound, NULL) != DS_OK) {
		mt_mutex_delete(dev->mutex);
		free(dev);
		return NULL;
	}

	// Set the cooperative level
	{
		DWORD dwlevel;
		HWND hwnd;

		video_wm_data_t wm_data;
		if (video_get_wm_data(&wm_data)) {
			hwnd = wm_data.data.windows.hwnd;
			dwlevel = DSSCL_PRIORITY;
		} else {
			hwnd = GetDesktopWindow();
			dwlevel = DSSCL_NORMAL;
		}

		if (IDirectSound_SetCooperativeLevel(dev->dsound, hwnd, dwlevel) != DS_OK) {
			IDirectSound_Release(dev->dsound);
			mt_mutex_delete(dev->mutex);
			free(dev);
			return NULL;
		}
	}

	DSBUFFERDESC dsformat;

	for (;;) {
		// Recalculate wave format
		format.nBlockAlign = format.nChannels * (format.wBitsPerSample / 8);
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

		dev->size = desired->samples * format.nChannels * (format.wBitsPerSample / 8);

		DWORD bufsize = NUM_CHUNKS * dev->size;
		if ((bufsize < DSBSIZE_MIN) || (bufsize > DSBSIZE_MAX))
			goto DS_badformat; // UGH!

		dsformat = (DSBUFFERDESC){
			.dwSize = sizeof(DSBUFFERDESC),
			.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_STATIC | DSBCAPS_GETCURRENTPOSITION2,
			.lpwfxFormat = &format,
			.dwBufferBytes = NUM_CHUNKS * dev->size,
		};

		HRESULT err = IDirectSound_CreateSoundBuffer(dev->dsound, &dsformat, &dev->lpbuffer, NULL);
		if (err == DS_OK) {
			break;
		} else if (err == DSERR_BADFORMAT || err == DSERR_INVALIDPARAM /* Win2K */) {
DS_badformat:
			if (format.wBitsPerSample == 32) {
				// Retry again, with 16-bit audio. 32-bit audio doesn't seem
				// to work on Win2k at all...
				format.wBitsPerSample = 16;
				continue;
			}
		}

		// NOTE: Many VM audio drivers (namely virtual pc and vmware) are broken
		// under Win2k and return DSERR_CONTROLUNAVAIL. This doesn't seem to be the
		// full story however, since SDL seems to create the buffer just fine.
		// I'm just not going to worry about it for now...

		// Punt if nothing worked
		mt_mutex_delete(dev->mutex);
		IDirectSound_Release(dev->dsound);
		free(dev);
		return NULL;
	}

	if (IDirectSoundBuffer_SetFormat(dev->lpbuffer, &format) != DS_OK) {
#if 0 // SDL doesn't error here, and it seems to cause issues on my mac mini
		mt_mutex_delete(dev->mutex);
		IDirectSoundBuffer_Release(dev->lpbuffer);
		IDirectSound_Release(dev->dsound);
		free(dev);
		return NULL;
#endif
	}

	dev->silence = (format.wBitsPerSample == 8) ? 0x80 : 0;

	{
		// Silence the initial buffer (ripped from SDL)
		// FIXME why are we retrieving ptr2 and bytes2?
		LPVOID ptr1, ptr2;
		DWORD bytes1, bytes2;

		if (IDirectSoundBuffer_Lock(dev->lpbuffer, 0, dsformat.dwBufferBytes, &ptr1, &bytes1, &ptr2, &bytes2, DSBLOCK_ENTIREBUFFER) == DS_OK) {
			memset(ptr1, dev->silence, bytes1);
			IDirectSoundBuffer_Unlock(dev->lpbuffer, ptr1, bytes1, ptr2, bytes2);
		}
	}

	// ok, now start the full thread
	dev->thread = mt_thread_create(_dsound_audio_thread, "DirectSound audio thread", dev);
	if (!dev->thread) {
		mt_mutex_delete(dev->mutex);
		IDirectSoundBuffer_Release(dev->lpbuffer);
		IDirectSound_Release(dev->dsound);
		free(dev);
		return NULL;
	}

	obtained->freq = format.nSamplesPerSec;
	obtained->channels = format.nChannels;
	obtained->bits = format.wBitsPerSample;
	obtained->samples = desired->samples;

	return dev;
}

static void dsound_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	dev->cancelled = 1;
	mt_thread_wait(dev->thread, NULL);

	if (dev->lpbuffer) {
		IDirectSoundBuffer_Stop(dev->lpbuffer);
		IDirectSoundBuffer_Release(dev->lpbuffer);
	}
	if (dev->dsound) {
		IDirectSound_Release(dev->dsound);
	}
	mt_mutex_delete(dev->mutex);
	free(dev);
}

static void dsound_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
}

static void dsound_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_unlock(dev->mutex);
}

static void dsound_audio_pause_device(schism_audio_device_t *dev, int paused)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
	dev->paused = paused;
	mt_mutex_unlock(dev->mutex);
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

static void *lib_dsound = NULL;

static int dsound_audio_init(void)
{
	// XXX:
	// SDL also checks for at least Windows 2000 here, citing
	// that the audio subsystem on NT 4 is somewhat high latency
	// while using DirectSound. I don't know whether this is
	// entirely true...
	lib_dsound = loadso_object_load("DSOUND.DLL");
	if (!lib_dsound)
		return 0;

	DSOUND_DirectSoundCreate = loadso_function_load(lib_dsound, "DirectSoundCreate");
#ifdef SCHISM_WIN32_COMPILE_ANSI
	DSOUND_DirectSoundEnumerateA = loadso_function_load(lib_dsound, "DirectSoundEnumerateA");
#endif
	DSOUND_DirectSoundEnumerateW = loadso_function_load(lib_dsound, "DirectSoundEnumerateW");

	// DirectSoundCaptureCreate was added in DirectX 5
	if (!DSOUND_DirectSoundCreate) {
		loadso_object_unload(lib_dsound);
		return 0;
	}

	return 1;
}

static void dsound_audio_quit(void)
{
	DSOUND_DirectSoundCreate = NULL;
#ifdef SCHISM_WIN32_COMPILE_ANSI
	DSOUND_DirectSoundEnumerateA = NULL;
#endif
	DSOUND_DirectSoundEnumerateW = NULL;

	if (lib_dsound) {
		loadso_object_unload(lib_dsound);
		lib_dsound = NULL;
	}
}

//////////////////////////////////////////////////////////////////////////////

const schism_audio_backend_t schism_audio_backend_dsound = {
	.init = dsound_audio_init,
	.quit = dsound_audio_quit,

	.driver_count = dsound_audio_driver_count,
	.driver_name = dsound_audio_driver_name,

	.device_count = dsound_audio_device_count,
	.device_name = dsound_audio_device_name,

	.init_driver = dsound_audio_init_driver,
	.quit_driver = dsound_audio_quit_driver,

	.open_device = dsound_audio_open_device,
	.close_device = dsound_audio_close_device,
	.lock_device = dsound_audio_lock_device,
	.unlock_device = dsound_audio_unlock_device,
	.pause_device = dsound_audio_pause_device,
};
