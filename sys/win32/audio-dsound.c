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
 
/* Win32 directsound backend */

#define COBJMACROS

#include "headers.h"
#include "charset.h"
#include "mt.h"
#include "mem.h"
#include "osdefs.h"
#include "loadso.h"
#include "video.h" /* video_get_wm_data */
#include "backend/audio.h"

/* request compatibility with DirectX 5 */
#define DIRECTSOUND_VERSION 0x0500

#include <windows.h>

/* define GUIDs locally: */
#include <initguid.h>

#include <dsound.h>

/* ripped from SDL */
#define NUM_CHUNKS 8

/* Define this to use DirectX 6 position notify events when available.
 * This is turned off here because they cause weird dislocation in the
 * audio buffer when moving/resizing/changing the window at all. */

#define COMPILE_POSITION_NOTIFY

static HRESULT (WINAPI *DSOUND_DirectSoundCreate)(LPGUID lpGuid, LPDIRECTSOUND* ppDS, LPUNKNOWN pUnkOuter) = NULL;

/* https://source.winehq.org/WineAPI/dsound.html */
#ifdef SCHISM_WIN32_COMPILE_ANSI
static HRESULT (WINAPI *DSOUND_DirectSoundEnumerateA)(LPDSENUMCALLBACKA lpDSEnumCallback, LPVOID lpContext) = NULL;
#endif
static HRESULT (WINAPI *DSOUND_DirectSoundEnumerateW)(LPDSENUMCALLBACKW lpDSEnumCallback, LPVOID lpContext) = NULL;

struct schism_audio_device {
	struct schism_audio_device_simple simple;

#ifdef COMPILE_POSITION_NOTIFY
	/* An event where we get notifications under DX6 and higher
	 * Disabled by default; see anti-definition of COMPILE_POSITION_NOTIFY above. */
	HANDLE event;
#endif

	LPDIRECTSOUND dsound;
	LPDIRECTSOUNDBUFFER lpbuffer;

	/* ... */
	DWORD cursor;
	DWORD last_chunk;

	/* audio buffer info */
	uint32_t bps; /* bits per sample */
	uint32_t channels; /* channels */
	uint32_t samples; /* samples per chunk */
	uint32_t size; /* size in bytes of one chunk (bps * channels * samples) */
	uint32_t rate; /* sample rate */

	/* current locked buffer */
	void *buf;
	DWORD buflen;
};

/* ---------------------------------------------------------- */
/* drivers */

/* lol */
static const char *drivers[] = {
	"dsound",
};

static int dsound_audio_driver_count(void)
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

/* devices name cache; refreshed after every call to dsound_audio_device_count */
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

/* This function takes ownership of `name` and is responsible for either freeing it
 * or adding it to a list which will eventually be freed.
 * Note: lpguid AND name must be valid pointers. No null pointers. */
static inline void _dsound_device_append(LPGUID lpguid, char *name)
{
	/* Filter out waveout emulated devices. If we don't do this, it causes a bit of
	 * CPU overhead and is utterly pointless when we can just use waveout directly
	 * anyway. */
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

	/* put the bread in the basket */
	memcpy(&devices[devices_size].guid, lpguid, sizeof(GUID));
	devices[devices_size].name = name;

	devices_size++;
}

/* We need two different callbacks for ANSI and UNICODE variants */
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

static uint32_t dsound_audio_device_count(uint32_t flags)
{
	if (flags & AUDIO_BACKEND_CAPTURE)
		return 0;

	/* Prefer Unicode */
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
	/* If this ever happens it is a catastrophic bug and we
	 * should crash before anything bad happens. */
	if (i >= devices_size)
		return NULL;

	return devices[i].name;
}

/* ---------------------------------------------------------- */

static int dsound_audio_init_driver(const char *driver)
{
	for (int i = 0; i < ARRAY_SIZE(drivers); i++) {
		if (!strcmp(drivers[i], driver)) {
			/* Get the devices */
			(void)dsound_audio_device_count(0);
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

static void *_dsound_audio_get_buffer(schism_audio_device_t *dev,
	size_t *buflen)
{
	HRESULT res;
	DWORD dw, xyzzy;

	dev->last_chunk = dev->cursor;
	dev->cursor = (dev->cursor + 1) % NUM_CHUNKS;

	res = IDirectSoundBuffer_Lock(dev->lpbuffer, dev->cursor * dev->size, dev->size, &dev->buf, &dw, NULL, &xyzzy, 0);
	if (res == DSERR_BUFFERLOST) {
		IDirectSoundBuffer_Restore(dev->lpbuffer);
		res = IDirectSoundBuffer_Lock(dev->lpbuffer, dev->cursor * dev->size, dev->size, &dev->buf, &dw, NULL, &xyzzy, 0);
	}
	if (res != DS_OK)
		return NULL;

	dev->buflen = dw;
	*buflen = dw;
	return dev->buf;
}

static int _dsound_audio_play(schism_audio_device_t *dev)
{
	if (dev->buf) {
		IDirectSoundBuffer_Unlock(dev->lpbuffer, dev->buf, dev->buflen, NULL, 0);
		dev->buf = NULL;
		dev->buflen = 0;
		return 0;
	}

	return -1;
}

static int _dsound_audio_wait_dx5(schism_audio_device_t *dev)
{
	DWORD status;
	DWORD cursor, xyzzy;
	HRESULT res;

	while (!atm_load(dev->simple.cancelled)) { /* :/ */
		IDirectSoundBuffer_GetStatus(dev->lpbuffer, &status);
		if (status & DSBSTATUS_BUFFERLOST) {
			IDirectSoundBuffer_Restore(dev->lpbuffer);
			IDirectSoundBuffer_GetStatus(dev->lpbuffer, &status);
			if (status & DSBSTATUS_BUFFERLOST)
				return -1;
		}

		if (!(status & DSBSTATUS_PLAYING)) {
			if (IDirectSoundBuffer_Play(dev->lpbuffer, 0, 0, DSBPLAY_LOOPING) != DS_OK)
				/* This should never happen */
				return -1;
		} else {
			res = IDirectSoundBuffer_GetCurrentPosition(dev->lpbuffer, &xyzzy, &cursor);
			if (res == DSERR_BUFFERLOST) {
				IDirectSoundBuffer_Restore(dev->lpbuffer);
				res = IDirectSoundBuffer_GetCurrentPosition(dev->lpbuffer, &xyzzy, &cursor);
			}
			if (res != DS_OK)
				continue; /* what? */

			if ((cursor / dev->size) != dev->last_chunk)
				return -1;

			cursor -= (dev->last_chunk * dev->size);

			uint64_t us = (cursor / dev->bps / dev->channels) * 1000000ULL / dev->rate;
			us = MAX(1000, us);

			timer_usleep(us);
		}
	}

	return 0;
}

static const struct schism_audio_device_simple_vtable dsound_dx5_vtbl = {
	_dsound_audio_get_buffer,
	_dsound_audio_play,
	_dsound_audio_wait_dx5,
	NULL
};

#ifdef COMPILE_POSITION_NOTIFY
static int _dsound_audio_wait_dx6(schism_audio_device_t *dev)
{
	DWORD status;
	IDirectSoundBuffer_GetStatus(dev->lpbuffer, &status);
	if (status & DSBSTATUS_BUFFERLOST) {
		IDirectSoundBuffer_Restore(dev->lpbuffer);
		IDirectSoundBuffer_GetStatus(dev->lpbuffer, &status);
		if (status & DSBSTATUS_BUFFERLOST)
			return -1;
	}

	if (!(status & DSBSTATUS_PLAYING)) {
		if (IDirectSoundBuffer_Play(dev->lpbuffer, 0, 0, DSBPLAY_LOOPING) != DS_OK)
			/* This should never happen */
			return -1;
	}

	WaitForSingleObject(dev->event, INFINITE);

	return 0;
}

static void _dsound_audio_aftercancel_dx6(schism_audio_device_t *dev)
{
	SetEvent(dev->event);
}

static const struct schism_audio_device_simple_vtable dsound_dx6_vtbl = {
	_dsound_audio_get_buffer,
	_dsound_audio_play,
	_dsound_audio_wait_dx6,
	_dsound_audio_aftercancel_dx6
};
#endif

static void dsound_audio_close_device(schism_audio_device_t *dev);

#ifdef COMPILE_POSITION_NOTIFY
static int _dsound_dx6_init_notify_position(schism_audio_device_t *dev)
{
	LPDIRECTSOUNDNOTIFY notify = NULL;
	int res = -1; /* default to failing */
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

/* nonzero on success */
static schism_audio_device_t *dsound_audio_open_device(uint32_t id, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	/* If no device is specified pass NULL to DirectSoundCreate */
	LPGUID guid = (id != AUDIO_BACKEND_DEFAULT) ? &devices[id].guid : NULL;

	/* Fill in the format structure */
	WAVEFORMATEX format = {
		.wFormatTag = WAVE_FORMAT_PCM,
		.nChannels = desired->channels,
		.nSamplesPerSec = desired->freq,
	};

	/* filter invalid bps values (should never happen, but eh...) */
	switch (desired->bits) {
	case 8: format.wBitsPerSample = 8; break;
	default:
	case 16: format.wBitsPerSample = 16; break;
	case 32: format.wBitsPerSample = 32; break;
	}

	/* ok, now allocate */
	schism_audio_device_t *dev = mem_calloc(1, sizeof(*dev));

	if (DSOUND_DirectSoundCreate(guid, &dev->dsound, NULL) != DS_OK) {
		goto fail;
	}

	/* Set the cooperative level */
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
		/* Recalculate wave format */
		format.nBlockAlign = format.nChannels * (format.wBitsPerSample / 8);
		format.nAvgBytesPerSec = format.nSamplesPerSec * format.nBlockAlign;

		dev->bps = format.wBitsPerSample;
		dev->channels = format.nChannels;
		dev->samples = desired->samples;
		dev->size = dev->samples * dev->channels * (dev->bps / 8);
		dev->rate = format.nSamplesPerSec;

		dsformat.dwBufferBytes = NUM_CHUNKS * dev->size;
		if ((dsformat.dwBufferBytes < DSBSIZE_MIN) || (dsformat.dwBufferBytes > DSBSIZE_MAX))
			goto DS_badformat; /* UGH! */

		HRESULT err = IDirectSound_CreateSoundBuffer(dev->dsound, &dsformat, &dev->lpbuffer, NULL);
		if (err == DS_OK) {
			break;
		} else if (err == DSERR_BADFORMAT || err == DSERR_INVALIDPARAM /* Win2K */) {
DS_badformat:
			if (format.wBitsPerSample == 32) {
				/* Retry again, with 16-bit audio. 32-bit audio doesn't seem
				 * to work on Win2k at all... */
				format.wBitsPerSample = 16;
				continue;
			}

#ifdef COMPILE_POSITION_NOTIFY
			/* Maybe we're on DX5 or lower? */
			if (dsformat.dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY) {
				dsformat.dwFlags &= ~(DSBCAPS_CTRLPOSITIONNOTIFY);
				continue;
			}
#endif
		}

		/* NOTE: Many VM audio drivers (namely virtual pc and vmware) are broken
		 * under Win2k and return DSERR_CONTROLUNAVAIL. This doesn't seem to be the
		 * full story however, since SDL seems to create the buffer just fine.
		 * I'm just not going to worry about it for now... */

		/* Punt if nothing worked */
		goto fail;
	}

	if (IDirectSoundBuffer_SetFormat(dev->lpbuffer, &format) != DS_OK) {
#if 0 /* SDL doesn't error here, and it seems to cause issues on my mac mini */
		goto fail;
#endif
	}

	obtained->freq = format.nSamplesPerSec;
	obtained->channels = format.nChannels;
	obtained->bits = format.wBitsPerSample;
	obtained->samples = desired->samples;

	{
		/* Silence the initial buffer (ripped from SDL)
		 * FIXME why are we retrieving ptr2 and bytes2? */
		LPVOID ptr1, ptr2;
		DWORD bytes1, bytes2;

		if (IDirectSoundBuffer_Lock(dev->lpbuffer, 0, dsformat.dwBufferBytes, &ptr1, &bytes1, &ptr2, &bytes2, DSBLOCK_ENTIREBUFFER) == DS_OK) {
			memset(ptr1, AUDIO_SPEC_SILENCE(*obtained), bytes1);
			IDirectSoundBuffer_Unlock(dev->lpbuffer, ptr1, bytes1, ptr2, bytes2);
		}
	}

	if (audio_simple_init(dev,
#ifdef COMPILE_POSITION_NOTIFY
			((dsformat.dwFlags & DSBCAPS_CTRLPOSITIONNOTIFY) && !_dsound_dx6_init_notify_position(dev))
				? &dsound_dx6_vtbl :
#endif
				&dsound_dx5_vtbl, desired->callback))
		goto fail;

	return dev;

fail:
	dsound_audio_close_device(dev);

	return NULL;
}

static void dsound_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	audio_simple_close(&dev->simple);

	if (dev->lpbuffer) {
		IDirectSoundBuffer_Stop(dev->lpbuffer);
		IDirectSoundBuffer_Release(dev->lpbuffer);
	}
	if (dev->dsound) {
		IDirectSound_Release(dev->dsound);
	}
#ifdef COMPILE_POSITION_NOTIFY
	if (dev->event)
		CloseHandle(dev->event);
#endif
	free(dev);
}

/* -------------------------------------------------------------------- */
/* dynamic loading */

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

	/* wuh? */
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

/* -------------------------------------------------------------------- */

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
	.lock_device = audio_simple_device_lock,
	.unlock_device = audio_simple_device_unlock,
	.pause_device = audio_simple_device_pause,
};

/* -------------------------------------------------------------------- */

/* no charset-specific stuff here, that cruft is handled in the callbacks */
struct dsound_audio_lookup_callback_data {
	UINT id;      /* input */
	GUID guid;    /* output for description callback, input for device callback */
	char *result; /* output */
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

	SCHISM_ANSI_UNICODE({
		return win32_dsound_audio_lookup_waveout_name_A(*waveoutdevid, result);
	}, {
		return win32_dsound_audio_lookup_waveout_name_W(*waveoutdevid, result);
	})
}
