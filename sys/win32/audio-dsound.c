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

// TODO: We should also detect whether a device opened is just emulating
// waveout, and use the APIs directly if so. It looks like this can be
// accomplished via DSPROPERTY_DIRECTSOUNDDEVICE_WAVEDEVICEMAPPING_DATA
// and IKsPropertySet, which involves dynamic loading (we are already
// doing that anyway)
// Additionally, we should also use that API to retrieve full device names
// when using waveout, since the method we are currently using doesn't
// seem to work all too well.

#define COBJMACROS

#include "headers.h"
#include "charset.h"
#include "mt.h"
#include "mem.h"
#include "osdefs.h"
#include "loadso.h"
#include "video.h" // video_get_wm_data
#include "backend/audio.h"

// request compatibility with DirectX 5
#define DIRECTSOUND_VERSION 0x0500

#include <windows.h>

// define GUIDs locally:
#include <initguid.h>

#include <dsound.h>

// ripped from SDL
#define NUM_CHUNKS 8

// Define this to use DirectX 6 position notify events when available.
// This is turned off here because they cause weird dislocation in the
// audio buffer when moving/resizing/changing the window at all.
//#define COMPILE_POSITION_NOTIFY

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

	// A semaphore we use for notifications under DX6 and higher
	HANDLE event;

	// A pointer to the function we use to wait for audio to finish playing
	void (*audio_wait)(schism_audio_device_t *dev);

	// pass to memset() to make silence.
	// used when paused and when initializing the device buffer
	uint8_t silence;

	LPDIRECTSOUND dsound;
	LPDIRECTSOUNDBUFFER lpbuffer;

	// ...
	DWORD last_chunk;
	int paused;

	// audio buffer info
	uint32_t bps; // bits per sample
	uint32_t channels; // channels
	uint32_t samples; // samples per chunk
	uint32_t size; // size in bytes of one chunk (bps * channels * samples)
	uint32_t rate; // sample rate
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

	for (size_t i = 0; i < devices_size; i++)
		if (!memcmp(&devices[i].guid, lpguid, sizeof(GUID)))
			return;

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
			if (win32_audio_lookup_device_name(lpGuid, NULL, &name) || !charset_iconv(lpcstrDescription, &name, charset, CHARSET_UTF8, SIZE_MAX)) \
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
	// Prefer Unicode
	if (DSOUND_DirectSoundEnumerateW && DSOUND_DirectSoundEnumerateW(_dsound_enumerate_callback_w, NULL) == DS_OK)
		return devices_size;

#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (DSOUND_DirectSoundEnumerateA && DSOUND_DirectSoundEnumerateA(_dsound_enumerate_callback_a, NULL) == DS_OK)
		return devices_size;
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

static void _dsound_audio_wait_dx5(schism_audio_device_t *dev)
{
	DWORD cursor, xyzzy;
	HRESULT res;

	while (!dev->cancelled) {
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
		} else {
			res = IDirectSoundBuffer_GetCurrentPosition(dev->lpbuffer, &xyzzy, &cursor);
			if (res == DSERR_BUFFERLOST) {
				IDirectSoundBuffer_Restore(dev->lpbuffer);
				res = IDirectSoundBuffer_GetCurrentPosition(dev->lpbuffer, &xyzzy, &cursor);
			}
			if (res != DS_OK)
				continue; // what?

			if ((cursor / dev->size) != dev->last_chunk)
				break;

			cursor -= (dev->last_chunk * dev->size);

			uint32_t ms = (cursor / dev->bps / dev->channels) * 1000UL / dev->rate;
			ms = MAX(1, ms);

			timer_msleep(ms);
		}
	}
}

#ifdef COMPILE_POSITION_NOTIFY
static void _dsound_audio_wait_dx6(schism_audio_device_t *dev)
{
	DWORD status;
	IDirectSoundBuffer_GetStatus(dev->lpbuffer, &status);
	if (status & DSBSTATUS_BUFFERLOST) {
		IDirectSoundBuffer_Restore(dev->lpbuffer);
		IDirectSoundBuffer_GetStatus(dev->lpbuffer, &status);
		if (status & DSBSTATUS_BUFFERLOST)
			return;
	}

	if (!(status & DSBSTATUS_PLAYING)) {
		if (IDirectSoundBuffer_Play(dev->lpbuffer, 0, 0, DSBPLAY_LOOPING) != DS_OK)
			// This should never happen
			return;
	}

	while (!dev->cancelled && (WaitForSingleObject(dev->event, 10) == WAIT_TIMEOUT));
}
#endif

static int _dsound_audio_thread(void *data)
{
	schism_audio_device_t *dev = data;

	mt_thread_set_priority(MT_THREAD_PRIORITY_TIME_CRITICAL);

	DWORD cursor = 0;
	HRESULT res = DS_OK;

	while (!dev->cancelled) {
		void *buf;
		DWORD buflen, xyzzy;

		dev->last_chunk = cursor;
		cursor = (cursor + 1) % NUM_CHUNKS;

		res = IDirectSoundBuffer_Lock(dev->lpbuffer, cursor * dev->size, dev->size, &buf, &buflen, NULL, &xyzzy, 0);
		if (res == DSERR_BUFFERLOST) {
			IDirectSoundBuffer_Restore(dev->lpbuffer);
			res = IDirectSoundBuffer_Lock(dev->lpbuffer, cursor * dev->size, dev->size, &buf, &buflen, NULL, &xyzzy, 0);
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

		dev->audio_wait(dev);
	}

	return 0;
}

static void dsound_audio_close_device(schism_audio_device_t *dev);

#ifdef COMPILE_POSITION_NOTIFY
static int _dsound_dx6_init_notify_position(schism_audio_device_t *dev)
{
	LPDIRECTSOUNDNOTIFY notify = NULL;
	int res = -1; // default to failing
	size_t i;

	DSBPOSITIONNOTIFY notify_positions[NUM_CHUNKS];
	if (IDirectSoundBuffer_QueryInterface(dev->lpbuffer, &IID_IDirectSoundNotify, (void *)&notify) != DS_OK)
		goto done;

	dev->event = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (!dev->event)
		goto done;

	for (i = 0; i < ARRAY_SIZE(notify_positions); i++) {
		notify_positions[i].dwOffset = i * dev->size;
		notify_positions[i].hEventNotify = dev->event;
	}

	if (IDirectSoundNotify_SetNotificationPositions(notify, ARRAY_SIZE(notify_positions), notify_positions) != DS_OK)
		goto done;

	res = 0;

done:
	if (notify)
		IDirectSoundNotify_Release(notify);

	return res;
}
#endif

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
	dev->paused = 1; // always start paused

	dev->mutex = mt_mutex_create();
	if (!dev->mutex)
		goto fail;

	if (DSOUND_DirectSoundCreate(guid, &dev->dsound, NULL) != DS_OK) {
		goto fail;
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

		if (IDirectSound_SetCooperativeLevel(dev->dsound, hwnd, dwlevel) != DS_OK)
			goto fail;
	}

	DSBUFFERDESC dsformat = {
		.dwSize = sizeof(DSBUFFERDESC),
		.dwFlags = DSBCAPS_GLOBALFOCUS | DSBCAPS_STATIC | DSBCAPS_GETCURRENTPOSITION2
#ifdef COMPILE_POSITION_NOTIFY
			| DSBCAPS_CTRLPOSITIONNOTIFY
#endif
			,
		.lpwfxFormat = &format,
	};

	for (;;) {
		// Recalculate wave format
		format.nBlockAlign = format.nChannels * (format.wBitsPerSample / 8);
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

		dev->bps = format.wBitsPerSample;
		dev->channels = format.nChannels;
		dev->samples = desired->samples;
		dev->size = dev->samples * dev->channels * (dev->bps / 8);
		dev->rate = format.nSamplesPerSec;

		dsformat.dwBufferBytes = NUM_CHUNKS * dev->size;
		if ((dsformat.dwBufferBytes < DSBSIZE_MIN) || (dsformat.dwBufferBytes > DSBSIZE_MAX))
			goto DS_badformat; // UGH!

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

#ifdef COMPILE_POSITION_NOTIFY
			// Maybe we're on DX5 or lower?
			if (dsformat.dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY) {
				dsformat.dwFlags &= ~(DSBCAPS_CTRLPOSITIONNOTIFY);
				continue;
			}
#endif
		}

		// NOTE: Many VM audio drivers (namely virtual pc and vmware) are broken
		// under Win2k and return DSERR_CONTROLUNAVAIL. This doesn't seem to be the
		// full story however, since SDL seems to create the buffer just fine.
		// I'm just not going to worry about it for now...

		// Punt if nothing worked
		goto fail;
	}

	if (IDirectSoundBuffer_SetFormat(dev->lpbuffer, &format) != DS_OK) {
#if 0 // SDL doesn't error here, and it seems to cause issues on my mac mini
		goto fail;
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

#ifdef COMPILE_POSITION_NOTIFY
	// Use position notify events to wait for audio to finish under DX6
	dev->audio_wait = ((dsformat.dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY) && !_dsound_dx6_init_notify_position(dev))
		? _dsound_audio_wait_dx6
		: _dsound_audio_wait_dx5;
#else
	dev->audio_wait = _dsound_audio_wait_dx5;
#endif

	// ok, now start the full thread
	dev->thread = mt_thread_create(_dsound_audio_thread, "DirectSound audio thread", dev);
	if (!dev->thread)
		goto fail;

	obtained->freq = format.nSamplesPerSec;
	obtained->channels = format.nChannels;
	obtained->bits = format.wBitsPerSample;
	obtained->samples = desired->samples;

	return dev;

fail:
	dsound_audio_close_device(dev);

	return NULL;
}

static void dsound_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	if (dev->thread) {
		dev->cancelled = 1;
		mt_thread_wait(dev->thread, NULL);
	}
	if (dev->lpbuffer) {
		IDirectSoundBuffer_Stop(dev->lpbuffer);
		IDirectSoundBuffer_Release(dev->lpbuffer);
	}
	if (dev->dsound) {
		IDirectSound_Release(dev->dsound);
	}
	if (dev->mutex) {
		mt_mutex_delete(dev->mutex);
	}
	if (dev->event) {
		CloseHandle(dev->event);
	}
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
	dev->paused = !!paused;
	mt_mutex_unlock(dev->mutex);
}

//////////////////////////////////////////////////////////////////////////////
// dynamic loading

#include <dsconf.h>
#include <unknwn.h>

static void *lib_ole32 = NULL;
static void *lib_dsound = NULL;

static HRESULT (WINAPI *OLE32_CoInitializeEx)(LPVOID, DWORD) = NULL;
static void (WINAPI *OLE32_CoUninitialize)(void) = NULL;

static IKsPropertySet *dsound_propset = NULL;

static int dsound_audio_init(void)
{
	lib_dsound = loadso_object_load("DSOUND.DLL");
	if (!lib_dsound)
		return 0;

	lib_ole32 = loadso_object_load("OLE32.DLL");
	if (!lib_ole32) {
		loadso_object_unload(lib_dsound);
		return 0;
	}

	DSOUND_DirectSoundCreate = loadso_function_load(lib_dsound, "DirectSoundCreate");
#ifdef SCHISM_WIN32_COMPILE_ANSI
	DSOUND_DirectSoundEnumerateA = loadso_function_load(lib_dsound, "DirectSoundEnumerateA");
#endif
	DSOUND_DirectSoundEnumerateW = loadso_function_load(lib_dsound, "DirectSoundEnumerateW");

	OLE32_CoInitializeEx = loadso_function_load(lib_ole32, "CoInitializeEx");
	OLE32_CoUninitialize = loadso_function_load(lib_ole32, "CoUninitialize");

	if (!OLE32_CoInitializeEx || !OLE32_CoUninitialize) {
		loadso_object_unload(lib_dsound);
		loadso_object_unload(lib_ole32);
		return 0;
	}

	switch (OLE32_CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)) {
	case S_OK:
	case S_FALSE:
	case RPC_E_CHANGED_MODE:
		break;
	default:
		loadso_object_unload(lib_dsound);
		loadso_object_unload(lib_ole32);
		return 0;
	}

	dsound_propset = NULL;

	// wuh?
	HRESULT (WINAPI *DSOUND_DllGetClassObject)(REFCLSID, REFIID, LPVOID *) = loadso_function_load(lib_dsound, "DllGetClassObject");
	if (DSOUND_DllGetClassObject) {
		IClassFactory *factory;
		if (SUCCEEDED(DSOUND_DllGetClassObject(&CLSID_DirectSoundPrivate, &IID_IClassFactory, (LPVOID *)&factory)))
			IClassFactory_CreateInstance(factory, NULL, &IID_IKsPropertySet, (LPVOID *)&dsound_propset);
	}

	if (!DSOUND_DirectSoundCreate || !loadso_function_load(lib_dsound, "DirectSoundCaptureCreate")) {
		loadso_object_unload(lib_dsound);
		loadso_object_unload(lib_ole32);
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

	OLE32_CoUninitialize();

	if (lib_ole32) {
		loadso_object_unload(lib_ole32);
		lib_ole32 = NULL;
	}

	if (dsound_propset) {
		IKsPropertySet_Release(dsound_propset);
		dsound_propset = NULL;
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

//////////////////////////////////////////////////////////////////////////////

// no charset-specific stuff here, that cruft is handled in the callbacks
struct dsound_audio_lookup_callback_data {
	UINT id;      // input
	GUID guid;    // output for description callback, input for device callback
	char *result; // output
};

#define WIN32_DSOUND_AUDIO_LOOKUP_WAVEOUT_NAME_IMPL(AorW, TYPE, CHARSET) \
	static BOOL CALLBACK dsound_enumerate_lookup_device_callback_##AorW##_(LPGUID lpGuid, const TYPE *lpcstrDescription, SCHISM_UNUSED const TYPE *lpcstrModule, LPVOID lpContext) \
	{ \
		struct dsound_audio_lookup_callback_data *data = lpContext; \
	\
		if (lpGuid && !memcmp(lpGuid, &data->guid, sizeof(GUID))) { \
			data->result = charset_iconv_easy(lpcstrDescription, CHARSET, CHARSET_UTF8); \
			return FALSE; \
		} \
	\
		return TRUE; \
	} \
	\
	static BOOL CALLBACK dsound_enumerate_lookup_description_callback_##AorW##_(PDSPROPERTY_DIRECTSOUNDDEVICE_DESCRIPTION_##AorW##_DATA pdata, LPVOID puserdata) \
	{ \
		struct dsound_audio_lookup_callback_data *pcbdata = puserdata; \
	\
		if (pdata && (pdata->WaveDeviceId == pcbdata->id)) { \
			memcpy(&pcbdata->guid, &pdata->DeviceId, sizeof(GUID)); \
			if (pdata->Description) pcbdata->result = charset_iconv_easy(pdata->Description, CHARSET, CHARSET_ANSI); \
			return FALSE; \
		} \
	\
		return TRUE; \
	} \
	\
	static inline int SCHISM_ALWAYS_INLINE win32_dsound_audio_lookup_waveout_name_##AorW(uint32_t waveoutdevid, char **result) \
	{ \
		ULONG ulxyzzy; \
	\
		if (!DSOUND_DirectSoundEnumerate##AorW) \
			return 0; \
	\
		struct dsound_audio_lookup_callback_data cbdata = { .id = waveoutdevid }; \
	\
		DSPROPERTY_DIRECTSOUNDDEVICE_ENUMERATE_##AorW##_DATA data = { \
			.Callback = dsound_enumerate_lookup_description_callback_##AorW##_, \
			.Context = &cbdata, \
		}; \
	\
		if (SUCCEEDED(IKsPropertySet_Get(dsound_propset, &DSPROPSETID_DirectSoundDevice, DSPROPERTY_DIRECTSOUNDDEVICE_ENUMERATE_##AorW, &data, sizeof(data), &data, sizeof(data), &ulxyzzy))) { \
			/* we don't need to enumerate twice if we already received the result */ \
			if (cbdata.result) { *result = cbdata.result; return 1; } \
	\
			DSOUND_DirectSoundEnumerate##AorW(dsound_enumerate_lookup_device_callback_##AorW##_, &cbdata); \
			if (cbdata.result) { *result = cbdata.result; return 1; } \
		} \
	\
		return 0; \
	}

#ifdef SCHISM_WIN32_COMPILE_ANSI
WIN32_DSOUND_AUDIO_LOOKUP_WAVEOUT_NAME_IMPL(A, CHAR, CHARSET_ANSI)
#endif
WIN32_DSOUND_AUDIO_LOOKUP_WAVEOUT_NAME_IMPL(W, WCHAR, CHARSET_WCHAR_T)

#undef WIN32_DSOUND_AUDIO_LOOKUP_WAVEOUT_NAME_IMPL

int win32_dsound_audio_lookup_waveout_name(const uint32_t *waveoutdevid, char **result)
{
	if (!waveoutdevid || !dsound_propset)
		return 0;

#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & 0x80000000U) { // Win9x
		return win32_dsound_audio_lookup_waveout_name_A(*waveoutdevid, result);
	} else
#endif
	{ // WinNT
		return win32_dsound_audio_lookup_waveout_name_W(*waveoutdevid, result);
	}
}
