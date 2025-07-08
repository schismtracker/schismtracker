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

#include "backend/audio.h"
#include "mem.h"
#include "osdefs.h"
#include "loadso.h"
#include "log.h"
#include "mt.h"
#include "video.h"

#include <windows.h>

#define ASIO_C_WRAPPERS

#include "audio-asio.h"

/* ------------------------------------------------------------------------ */

/* format for scanning GUIDs with (s)scanf */
#define GUIDF "%08" SCNx32 "-%04" SCNx16 "-%04" SCNx16 "-%02" SCNx8 "%02" \
	SCNx8 "-%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" SCNx8 "%02" \
	SCNx8
#define GUIDX(x) &((x).Data1), &((x).Data2), &((x).Data3), &((x).Data4[0]), \
	&((x).Data4[1]), &((x).Data4[2]), &((x).Data4[3]), &((x).Data4[4]), \
	&((x).Data4[5]), &((x).Data4[6]), &((x).Data4[7])

/* this is actually drivers, but OH WELL */
static struct {
	CLSID clsid;
	char *description; /* UTF-8 */
} *devices = NULL;
static uint32_t devices_size = 0;

static void asio_free_devices(void)
{
	uint32_t i;
	for (i = 0; i < devices_size; i++)
		free(devices[i].description);

	devices_size = 0;
	free(devices);
	devices = NULL;
}

/* "drivers" are refreshed every time this is called */
static uint32_t asio_device_count(void)
{
	/* this function is one of the unholiest of spaghettis ive ever written */
	LSTATUS lstatus;
	HKEY hkey;
	DWORD i;
	DWORD max_subkey_len, max_subkey;
	union {
#ifdef SCHISM_WIN32_COMPILE_ANSI
		CHAR *a;
#endif
		WCHAR *w;
		void *v; /* for allocation */
	} subkey, dev_desc;

	/* free any existing devices */
	asio_free_devices();

	SCHISM_ANSI_UNICODE({
		lstatus = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASIO", 0,
			KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE, &hkey);
	}, {
		lstatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0,
			KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE, &hkey);
	})

	/* WHAT'S IN THE BOX? */
	if (lstatus != ERROR_SUCCESS)
		return 0;

	lstatus = RegQueryInfoKeyA(hkey, NULL, NULL, NULL, &max_subkey,
		&max_subkey_len, NULL, NULL, NULL, NULL, NULL, NULL);
	if (lstatus != ERROR_SUCCESS) {
		RegCloseKey(hkey);
		return 0;
	}

	SCHISM_ANSI_UNICODE({
		subkey.a = mem_alloc(max_subkey_len + 1);
	}, {
		subkey.w = mem_alloc((max_subkey_len + 1) * sizeof(WCHAR));
	})

	devices = mem_alloc(sizeof(*devices) * max_subkey);

	for (i = 0; i < max_subkey; i++) {
		DWORD subkey_len;
		DWORD desc_sz;
		union {
#ifdef SCHISM_WIN32_COMPILE_ANSI
			CHAR a[39];
#endif
			WCHAR w[39];
		} clsid_s;
		CLSID clsid;

		/* MSDN: "This size should include the terminating [NUL] character." */
		subkey_len = max_subkey_len + 1;

		SCHISM_ANSI_UNICODE({
			lstatus = RegEnumKeyExA(hkey, i, subkey.a, &subkey_len, NULL,
				NULL, NULL, NULL);
		}, {
			lstatus = RegEnumKeyExW(hkey, i, subkey.w, &subkey_len, NULL,
				NULL, NULL, NULL);
		})

		if (lstatus != ERROR_SUCCESS)
			continue; /* ??? */

		SCHISM_ANSI_UNICODE({
			DWORD x = sizeof(clsid_s.a);
			lstatus = RegGetValueA(hkey, subkey.a, "CLSID", RRF_RT_REG_SZ,
				NULL, clsid_s.a, &x);
		}, {
			DWORD x = sizeof(clsid_s.w);
			lstatus = RegGetValueW(hkey, subkey.w, L"CLSID", RRF_RT_REG_SZ,
				NULL, clsid_s.w, &x);
		})

		if (lstatus != ERROR_SUCCESS)
			continue;

		/* before we grab the description, lets parse the CLSID */
		{
			char *s;
			int r;

			SCHISM_ANSI_UNICODE({
				s = clsid_s.a;
			}, {
				s = charset_iconv_easy(clsid_s.w, CHARSET_WCHAR_T, CHARSET_UTF8);
			})

			r = sscanf(s, "{" GUIDF "}", GUIDX(clsid));

			SCHISM_ANSI_UNICODE({
				/* nothing */
			}, {
				free(s);
			})

			if (r != 11)
				continue;
		}

		SCHISM_ANSI_UNICODE({
			lstatus = RegGetValueA(hkey, subkey.a, "Description",
				RRF_RT_REG_SZ, NULL, NULL, &desc_sz);
		}, {
			lstatus = RegGetValueW(hkey, subkey.w, L"Description",
				RRF_RT_REG_SZ, NULL, NULL, &desc_sz);
		})

		if (lstatus != ERROR_SUCCESS)
			continue;

		dev_desc.v = mem_alloc(desc_sz);

		SCHISM_ANSI_UNICODE({
			lstatus = RegGetValueA(hkey, subkey.a, "Description",
				RRF_RT_REG_SZ, NULL, dev_desc.a, &desc_sz);
		}, {
			lstatus = RegGetValueW(hkey, subkey.w, L"Description",
				RRF_RT_REG_SZ, NULL, dev_desc.w, &desc_sz);
		})

		if (lstatus != ERROR_SUCCESS) {
			free(dev_desc.v);
			continue;
		}

		memcpy(&devices[devices_size].clsid, &clsid, sizeof(CLSID));
		SCHISM_ANSI_UNICODE({
			devices[devices_size].description = charset_iconv_easy(dev_desc.a,
				CHARSET_ANSI, CHARSET_UTF8);
		}, {
			devices[devices_size].description = charset_iconv_easy(dev_desc.w,
				CHARSET_WCHAR_T, CHARSET_UTF8);
		})

		free(dev_desc.v);

		if (!devices[devices_size].description) {
			free(devices[devices_size].description);
			continue;
		}

		devices_size++;
	}

	return devices_size;
}

static const char *asio_device_name(uint32_t i)
{
	SCHISM_RUNTIME_ASSERT(i < devices_size, "overflow");

	return devices[i].description;
}

/* ---------------------------------------------------------------------------- */

static const char *drivers[] = {
	"asio",
};

static int asio_driver_count(void)
{
	return ARRAY_SIZE(drivers);
}

static const char *asio_driver_name(int i)
{
	if (i >= ARRAY_SIZE(drivers) || i < 0)
		return NULL;

	return drivers[i];
}

static int asio_init_driver(const char *driver)
{
	int fnd, i;

	fnd = 0;

	for (i = 0; i < ARRAY_SIZE(drivers); i++) {
		if (!strcmp(drivers[i], driver)) {
			fnd = 1;
			break;
		}
	}
	if (!fnd)
		return -1;

	(void)asio_device_count();
	return 0;
}

static void asio_quit_driver(void)
{
	asio_free_devices();
}

/* ---------------------------------------------------------------------------- */

static void *lib_ole32;

typedef HRESULT (WINAPI *OLE32_CoCreateInstanceSpec)(REFCLSID rclsid,
	LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv);
typedef HRESULT (WINAPI *OLE32_CoInitializeExSpec)(LPVOID, DWORD);
typedef void (WINAPI *OLE32_CoUninitializeSpec)(void);

static OLE32_CoCreateInstanceSpec OLE32_CoCreateInstance;
static OLE32_CoInitializeExSpec OLE32_CoInitializeEx;
static OLE32_CoUninitializeSpec OLE32_CoUninitialize;

struct schism_audio_device {
	IAsio *asio;

	/* callback; fills the audio buffer */
	void (*callback)(uint8_t *stream, int len);
	mt_mutex_t *mutex;

	/* the ASIO buffers
	 * these are each responsible for one channel. */
	struct AsioBuffers *buffers;
	uint32_t numbufs;

	void *membuf;

	uint32_t bufsmps; /* buffer length in samples */
	uint32_t bps;     /* bytes per sample */
	uint32_t buflen;  /* bufsmps * bytes per sample * numbufs (only used for mono) */

	/* Some ASIO drivers are BUGGY AS FUCK; they copy the callbacks POINTER,
	 * and not the actual data contained within it.
	 *
	 * So, since there is nothing preventing an ASIO driver from overwriting
	 * our precious callbacks, allocate them with the rest of the device,
	 * just to be safe. */
	struct AsioCreateBufferCallbacks callbacks;
};

/* butt-ugly global because ASIO has an API from the stone age */
static schism_audio_device_t *current_device = NULL;

static uint32_t __cdecl asio_msg(uint32_t class, uint32_t msg, void *unk3, void *unk4)
{
	schism_audio_device_t *dev = current_device;

	switch (class) {
	case ASIO_CLASS_SUPPORTS_CLASS:
		switch (msg) {
		case ASIO_CLASS_ASIO_VERSION:
		case ASIO_CLASS_SUPPORTS_BUFFER_FLIP_EX:
			return 1;
		default:
			break;
		}
		return 0;
	case ASIO_CLASS_ASIO_VERSION:
		return 2;
	case ASIO_CLASS_SUPPORTS_BUFFER_FLIP_EX:
		return 0;
	default:
		break;
	}

	return 0;
}

static void __cdecl asio_dummy2(void)
{
	/* dunno what this is */
}

static void __cdecl asio_buffer_flip(uint32_t buf, uint32_t unk1)
{
	schism_audio_device_t *dev = current_device;
	uint32_t i;

	if (dev->numbufs == 1) {
		/* we can fill the buffer directly */
		mt_mutex_lock(dev->mutex);
		dev->callback(dev->buffers[0].ptrs[buf], dev->buflen);
		mt_mutex_unlock(dev->mutex);
	} else {
		/* we need a temporary buffer to deinterleave stereo */
		uint32_t i;

		mt_mutex_lock(dev->mutex);
		dev->callback(dev->membuf, dev->buflen);
		mt_mutex_unlock(dev->mutex);

		switch (dev->bps) {
		/* I have a love-hate relationship with the preprocessor */

#define DEINTERLEAVE_INT(BITS) \
	do { \
		for (i = 0; i < dev->bufsmps; i++) { \
			((uint##BITS##_t *)(dev->buffers[0].ptrs[buf]))[i] \
				= ((uint##BITS##_t *)dev->membuf)[i*2+0]; \
			((uint##BITS##_t *)(dev->buffers[1].ptrs[buf]))[i] \
				= ((uint##BITS##_t *)dev->membuf)[i*2+1]; \
		} \
	} while (0)

		case 1: DEINTERLEAVE_INT(8);  break;
		case 2: DEINTERLEAVE_INT(16); break;
		case 4: DEINTERLEAVE_INT(32); break;
		case 8: DEINTERLEAVE_INT(64); break;

#undef DEINTERLEAVE_INT

#define DEINTERLEAVE_MEMCPY(BPS) \
	do { \
		for (i = 0; i < dev->bufsmps; i++) { \
			memcpy((char *)(dev->buffers[0].ptrs[buf]) + (i * BPS), \
				(char *)dev->membuf + (BPS * (i * 2 + 0)), BPS); \
			memcpy((char *)(dev->buffers[1].ptrs[buf]) + (BPS * i), \
				(char *)dev->membuf + (BPS * (i * 2 + 1)), BPS); \
		} \
	} while (0)

		/* gcc is usually smart enough to inline calls to memcpy with
		 * an integer literal */
		case 3:  DEINTERLEAVE_MEMCPY(3); break;
		default: DEINTERLEAVE_MEMCPY(dev->bps); break;

#undef DEINTERLEAVE_MEMCPY
		}
	}

	IAsio_OutputReady(dev->asio);
}

static void *__cdecl asio_buffer_flip_ex(void *unk1, uint32_t buf,
	uint32_t unk2)
{
	/* BUG: Steinberg's "built-in" ASIO driver completely ignores the
	 * return value for ASIO_CLASS_SUPPORTS_BUFFER_FLIP_EX, instead
	 * opting to use it even when we say we don't want it. Just forward
	 * the values, I guess... */

	asio_buffer_flip(buf, unk2);

	/* what is this SUPPOSED to return? */
	return NULL;
}

/* ----------------------------------------------------------------------- */
/* stupid ASIO-specific crap */

static void asio_control_panel(schism_audio_device_t *dev)
{
	if (!dev->asio)
		return;

	IAsio_ControlPanel(dev->asio);
}

/* ----------------------------------------------------------------------- */

static void asio_close_device(schism_audio_device_t *dev);

static schism_audio_device_t *asio_open_device(uint32_t id,
	const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	schism_audio_device_t *dev;
	HRESULT hres;
	AsioError err;
	uint32_t i;

	if (current_device || !devices_size)
		return NULL; /* ASIO only supports one device at a time */

	if (id == AUDIO_BACKEND_DEFAULT)
		id = 0;

	dev = mem_calloc(1, sizeof(struct schism_audio_device));

	/* "ASIO under Windows is accessed as a COM in-process server object"
	 *  - ASIO4ALL Private API specification.
	 *
	 * https://asio4all.org/about/developer-resources/private-api-v2-0/
	 *
	 * Yet despite this fact, it is using the same value for CLSID as IID.
	 * If it were truly COM, the IAsio interface would have its own IID.
	 * But I digress. */
	hres = OLE32_CoCreateInstance(&devices[id].clsid, NULL,
		CLSCTX_INPROC_SERVER, &devices[id].clsid, (LPVOID *)&dev->asio);
	if (FAILED(hres))
		goto ASIO_fail;

	err = IAsio_Init(dev->asio, NULL);
	if (err < 0) {
		log_appendf(4, "[ASIO] IAsio_Init error %d", err);
		goto ASIO_fail;
	}

	{
		uint32_t bufmin, bufmax, bufpref, bufunk;

		err = IAsio_GetBufferSize(dev->asio, &bufmin, &bufmax, &bufpref,
			&bufunk);
		if (err < 0) {
			log_appendf(4, "[ASIO] IAsio_GetBufferSize error %d", err);
			goto ASIO_fail;
		}

		dev->bufsmps = CLAMP(desired->samples, bufmin, bufmax);
		obtained->samples = dev->bufsmps;
	}

	{
		double rate = desired->freq;

		err = IAsio_CheckSampleRate(dev->asio, rate);
		if (err >= 0)
			IAsio_SetSampleRate(dev->asio, rate);

		/* grab the actual rate and put it in the audio spec */
		err = IAsio_GetSampleRate(dev->asio, &rate);
		if (err < 0) {
			log_appendf(4, "[ASIO] IAsio_GetSampleRate error %d", err);
			goto ASIO_fail;
		}

		obtained->freq = rate;
	}


	{
		/* don't care about input channels, throw out the value */
		uint32_t xyzzy, maxchn;

		err = IAsio_GetChannels(dev->asio, &xyzzy, &maxchn);
		if (err < 0) {
			log_appendf(4, "[ASIO] IAsio_GetChannels error %d", err);
			goto ASIO_fail;
		}

		dev->numbufs = MIN(maxchn, desired->channels);
	}

	if (!dev->numbufs)
		goto ASIO_fail;

	dev->buffers = mem_calloc(dev->numbufs, sizeof(*dev->buffers));
	for (i = 0; i < dev->numbufs; i++) {
		struct AsioChannelInfo chninfo;

		chninfo.index = i;
		chninfo.input = 0;

		err = IAsio_GetChannelInfo(dev->asio, &chninfo);
		if (err < 0) {
			log_appendf(4, "[ASIO] GetChannelInfo ERROR: %" PRId32, err);
			goto ASIO_fail;
		}

		switch (chninfo.sample_type) {
		case ASIO_SAMPLE_TYPE_INT16LE:
			obtained->bits = 16;
			obtained->fp = 0;
			dev->bps = 2;
			break;
		case ASIO_SAMPLE_TYPE_INT24LE:
			obtained->bits = 24;
			obtained->fp = 0;
			dev->bps = 3;
			break;
		case ASIO_SAMPLE_TYPE_INT32LE:
			obtained->bits = 32;
			obtained->fp = 0;
			dev->bps = 4;
			break;
		case ASIO_SAMPLE_TYPE_FLOAT32LE:
			obtained->bits = 32;
			obtained->fp = 1;
			dev->bps = 4;
			break;
		default:
			log_appendf(4, "[ASIO] unknown sample type %" PRIx32,
				chninfo.sample_type);
			goto ASIO_fail;
		}

		dev->buffers[i].input = 0;
		dev->buffers[i].channel = i;
	}

	{
		dev->callbacks.buffer_flip = asio_buffer_flip;
		dev->callbacks.unk2 = asio_dummy2;
		dev->callbacks.msg = asio_msg;
		dev->callbacks.buffer_flip_ex = asio_buffer_flip_ex;

		err = IAsio_CreateBuffers(dev->asio, dev->buffers, dev->numbufs,
			dev->bufsmps, &dev->callbacks);
		if (err < 0) {
			log_appendf(4, "[ASIO] IAsio_CreateBuffers error %" PRId32, err);
			goto ASIO_fail;
		}
	}

	dev->mutex = mt_mutex_create();
	if (!dev->mutex)
		goto ASIO_fail;

	obtained->channels = dev->numbufs;

	dev->buflen = dev->bufsmps;
	dev->buflen *= (obtained->bits / 8);
	dev->buflen *= dev->numbufs;

	dev->membuf = mem_alloc(dev->buflen);
	dev->callback = desired->callback;

	/* set this for the ASIO callbacks */
	current_device = dev;

	return dev;

ASIO_fail:
	asio_close_device(dev);
	return NULL;
}

static void asio_close_device(schism_audio_device_t *dev)
{
	if (dev) {
		/* stop playing */
		if (dev->asio) {
			IAsio_Stop(dev->asio);

			if (dev->buffers)
				IAsio_DestroyBuffers(dev->asio);

			IAsio_Release(dev->asio);
		}

		if (dev->mutex)
			mt_mutex_delete(dev->mutex);

		free(dev->membuf);
		free(dev->buffers);
		free(dev);
	}

	current_device = NULL;
}

static void asio_lock_device(schism_audio_device_t *dev)
{
	mt_mutex_lock(dev->mutex);
}

static void asio_unlock_device(schism_audio_device_t *dev)
{
	mt_mutex_unlock(dev->mutex);
}

static void asio_pause_device(schism_audio_device_t *dev, int paused)
{
	mt_mutex_lock(dev->mutex);
	(paused ? IAsio_Stop : IAsio_Start)(dev->asio);
	mt_mutex_unlock(dev->mutex);
}

/* ------------------------------------------------------------------------ */

static int asio_init(void)
{
	lib_ole32 = loadso_object_load("OLE32.DLL");
	if (!lib_ole32)
		return 0;

	OLE32_CoCreateInstance = (OLE32_CoCreateInstanceSpec)
		loadso_function_load(lib_ole32, "CoCreateInstance");
	OLE32_CoInitializeEx = (OLE32_CoInitializeExSpec)
		loadso_function_load(lib_ole32, "CoInitializeEx");
	OLE32_CoUninitialize = (OLE32_CoUninitializeSpec)
		loadso_function_load(lib_ole32, "CoUninitialize");

	if (!OLE32_CoInitializeEx || !OLE32_CoUninitialize
		|| !OLE32_CoCreateInstance) {
		loadso_object_unload(lib_ole32);
		return 0;
	}

	switch (OLE32_CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)) {
	case S_OK:
	case S_FALSE:
	case RPC_E_CHANGED_MODE:
		break;
	default:
		loadso_object_unload(lib_ole32);
		return 0;
	}

	return 1;
}

static void asio_quit(void)
{
	OLE32_CoUninitialize();

	if (lib_ole32)
		loadso_object_unload(lib_ole32);
}

/* ---------------------------------------------------------------------------- */

const schism_audio_backend_t schism_audio_backend_asio = {
	.init = asio_init,
	.quit = asio_quit,

	.driver_count = asio_driver_count,
	.driver_name = asio_driver_name,

	.device_count = asio_device_count,
	.device_name = asio_device_name,

	.init_driver = asio_init_driver,
	.quit_driver = asio_quit_driver,

	.open_device = asio_open_device,
	.close_device = asio_close_device,
	.lock_device = asio_lock_device,
	.unlock_device = asio_unlock_device,
	.pause_device = asio_pause_device,

	.control_panel = asio_control_panel,
};
