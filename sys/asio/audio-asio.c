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
#include "str.h"

#ifdef SCHISM_WIN32
# include <windows.h>
#elif defined(SCHISM_MACOS)
/* eh, okay */
# include <Files.h>
# include <CodeFragments.h>
# include <Resources.h>

# include "macos-dirent.h"
#endif

#define ASIO_C_WRAPPERS

#include "audio-asio.h"

/* ------------------------------------------------------------------------ */

#ifdef SCHISM_WIN32
typedef HRESULT (WINAPI *OLE32_CLSIDFromStringSpec)(LPCOLESTR,LPCLSID);
typedef BOOL (WINAPI *SHELL32_GUIDFromStringASpec)(LPCSTR,LPGUID);
typedef BOOL (WINAPI *SHELL32_GUIDFromStringWSpec)(LPCWSTR,LPGUID);
static OLE32_CLSIDFromStringSpec OLE32_CLSIDFromString;
static SHELL32_GUIDFromStringASpec SHELL32_GUIDFromStringA;
static SHELL32_GUIDFromStringWSpec SHELL32_GUIDFromStringW;
#endif

/* this is actually drivers, but OH WELL */
static struct asio_device {
#ifdef SCHISM_WIN32
	CLSID clsid;
#elif defined(SCHISM_MACOS)
	FSSpec spec;
#else
# error Ooops, your files have been encrypted!
#endif
	char *description; /* UTF-8 */
} *devices = NULL;
static uint32_t devices_size = 0;
#ifdef SCHISM_MACOS
static uint32_t devices_alloc = 0;
#endif

static void asio_free_devices(void)
{
	uint32_t i;
	for (i = 0; i < devices_size; i++)
		free(devices[i].description);

	devices_size = 0;
#ifdef SCHISM_MACOS
	devices_alloc = 0;
#endif
	free(devices);
	devices = NULL;
}

/* "drivers" are refreshed every time this is called */
static uint32_t asio_device_count(uint32_t flags)
{
#ifdef SCHISM_WIN32
	/* this function is one of the unholiest of spaghettis ive ever written */
	LONG lstatus; /* LSTATUS isn't defined in older MinGW headers */
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

	if (flags & AUDIO_BACKEND_CAPTURE)
		return 0;

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
		DWORD type;
		union {
#ifdef SCHISM_WIN32_COMPILE_ANSI
			CHAR a[39];
#endif
			WCHAR w[39];
		} clsid_s;
		CLSID clsid;
		HKEY hsubkey;

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
			lstatus = RegOpenKeyExA(hkey, subkey.a, 0, KEY_READ, &hsubkey);
		}, {
			lstatus = RegOpenKeyExW(hkey, subkey.w, 0, KEY_READ, &hsubkey);
		})

		if (lstatus != ERROR_SUCCESS)
			continue;

		SCHISM_ANSI_UNICODE({
			DWORD x = sizeof(clsid_s.a);
			lstatus = RegQueryValueExA(hsubkey, "CLSID", NULL, &type,
				clsid_s.a, &x);
			/* NUL terminate */
			clsid_s.a[38] = 0;
		}, {
			DWORD x = sizeof(clsid_s.w);
			lstatus = RegQueryValueExW(hsubkey, L"CLSID", NULL, &type,
				(LPBYTE)clsid_s.w, &x);
			/* NUL terminate */
			clsid_s.w[38] = 0;
		})

		if (lstatus != ERROR_SUCCESS || type != REG_SZ) {
			RegCloseKey(hsubkey);
			continue;
		}

		/* before we grab the description, lets parse the CLSID */
		{
			int success;

			if (OLE32_CLSIDFromString) {
				/* this function is actually supported past XP/Vista */
				SCHISM_ANSI_UNICODE({
					WCHAR *s;

					s = charset_iconv_easy(clsid_s.a, CHARSET_ANSI, CHARSET_WCHAR_T);
					success = SUCCEEDED(OLE32_CLSIDFromString(s, &clsid));
					free(s);
				}, {
					success = SUCCEEDED(OLE32_CLSIDFromString(clsid_s.w, &clsid));
				})
			} else {
				/* these are only exported via ordinal and are unsupported,
				 * but available since Win95 (supposedly...) */
				SCHISM_ANSI_UNICODE({
					success = SHELL32_GUIDFromStringA(clsid_s.a, &clsid);
				}, {
					success = SHELL32_GUIDFromStringW(clsid_s.w, &clsid);
				})
			}

			if (!success) {
				RegCloseKey(hsubkey);
				continue;
			}
		}

		SCHISM_ANSI_UNICODE({
			lstatus = RegQueryValueExA(hsubkey, "Description",
				NULL, &type, NULL, &desc_sz);
			desc_sz += 1; /* NUL terminator */
		}, {
			lstatus = RegQueryValueExW(hsubkey, L"Description",
				NULL, &type, NULL, &desc_sz);
			desc_sz += 2; /* NUL terminator */
		})

		if (lstatus != ERROR_SUCCESS || type != REG_SZ) {
			RegCloseKey(hsubkey);
			continue;
		}

		dev_desc.v = mem_alloc(desc_sz);

		SCHISM_ANSI_UNICODE({
			lstatus = RegQueryValueExA(hsubkey, "Description",
				NULL, &type, dev_desc.a, &desc_sz);
			dev_desc.a[desc_sz - 1] = 0;
		}, {
			lstatus = RegQueryValueExW(hsubkey, L"Description",
				NULL, &type, (LPBYTE)dev_desc.w, &desc_sz);
			dev_desc.w[(desc_sz >> 1) - 1] = 0;
		})

		/* done with this */
		RegCloseKey(hsubkey);

		if (lstatus != ERROR_SUCCESS || type != REG_SZ) {
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
#elif defined(SCHISM_MACOS)
	/* we do a little song-and-dance between these two char pointers */
	char *ptr, *ptr2;
	DIR *dir;
	struct dirent *ent;

	/* free any existing devices */
	asio_free_devices();

	/* get current exe directory */
	ptr = dmoz_get_exe_directory();

	ptr2 = dmoz_path_concat(ptr, "ASIO Drivers");
	free(ptr);

	log_appendf(1, "%s", ptr2);

	dir = opendir(ptr2);
	if (!dir) {
		log_appendf(4, "[ASIO] failed to load ASIO driver directory");
		return 0;
	}

	/* ptr2 is not free'd here; it's needed to convert
	 * the asio driver path to an fsspec */

	while ((ent = readdir(dir)) != NULL) {
		struct asio_device *dev;

		/* allocate more devices if needed.
		 * if we are uninitialized (i.e. both sizes are 0),
		 * then this will simply allocate what we need */
		if (devices_size >= devices_alloc) {
			devices_alloc = (devices_alloc)
				? (devices_alloc * 2)
				/* sane default value I guess */
				: 4;

			/* reallocate */
			devices = mem_realloc(devices, devices_alloc * sizeof(struct asio_device));
		}

		dev = devices + devices_size;

		/* Store the FSSpec */
		ptr = dmoz_path_concat(ptr2, ent->d_name);
		if (!dmoz_path_to_fsspec(ptr, &dev->spec)) {
			/* ??? */
			free(ptr);
			continue;
		}
		free(ptr);

		/* FIXME: To do this properly, we have to
		 *  1. open the resource fork
		 *  2. read in the whole 'Asio' resource
		 *  3. call GetResInfo to get the name
		 *  4. convert that name to UTF-8
		 * This will be okay for now though. */
		dev->description = str_dup(ent->d_name);

		devices_size++;
	}

	closedir(dir);
	free(ptr2);

	return devices_size;
#endif
}

static const char *asio_device_name(uint32_t i)
{
	SCHISM_RUNTIME_ASSERT(i < devices_size, "overflow");

	return devices[i].description;
}

/* ---------------------------------------------------------------------------- */

static int asio_driver_count(void)
{
	return 1;
}

static const char *asio_driver_name(int i)
{
	switch (i) {
	case 0: return "asio";
	default: return NULL;
	}
}

static int asio_init_driver(const char *driver)
{
	if (strcmp("asio", driver))
		return -1;

	(void)asio_device_count(0);
	return 0;
}

static void asio_quit_driver(void)
{
	asio_free_devices();
}

/* ---------------------------------------------------------------------------- */

#ifdef SCHISM_WIN32
/* initialized in asio_init() */
typedef HRESULT (WINAPI *OLE32_CoCreateInstanceSpec)(REFCLSID rclsid,
	LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv);
static OLE32_CoCreateInstanceSpec OLE32_CoCreateInstance;

static IAsio *asio_get_device(struct asio_device *dev)
{
	/* "ASIO under Windows is accessed as a COM in-process server object"
	 *  - ASIO4ALL Private API specification.
	 *
	 * https://asio4all.org/about/developer-resources/private-api-v2-0/
	 *
	 * Yet despite this fact, it is using the same value for CLSID as IID.
	 * If it were truly COM, the IAsio interface would have its own IID.
	 * But I digress. */
	HRESULT hres;
	IAsio *asio;

	hres = OLE32_CoCreateInstance(&dev->clsid, NULL,
		CLSCTX_INPROC_SERVER, &dev->clsid, (LPVOID *)&asio);

	return SUCCEEDED(hres) ? asio : NULL;
}
#elif defined(SCHISM_MACOS)
/* Mac OS 9 ASIO isn't even COM; have to fake it */
struct CAsioFake {
	IAsioVtbl *lpVtbl;

	ULONG refcnt;
	Handle handle;
	CFragConnectionID conn;
	short resfile;

	/* This is our ACTUAL vtable.
	 * TODO store all of this data in an audio-asio-mac-vtable.h or something */
	AsioError (*Init)(void *unk1);
	AsioError (*Quit)(void);
	AsioError (*Start)(void);
	AsioError (*Stop)(void);
	AsioError (*GetChannels)(uint32_t *pinchns, uint32_t *poutchns);
	AsioError (*GetLatencies)(uint32_t *pinlatency, uint32_t *poutlatency);
	AsioError (*GetBufferSize)(uint32_t *pmin, uint32_t *pmax, uint32_t *pwanted, uint32_t *punknown);
	AsioError (*CheckSampleRate)(double rate);
	AsioError (*GetSampleRate)(double *prate);
	AsioError (*SetSampleRate)(double rate);
	AsioError (*GetClockSources)(struct AsioClockSource *srcs, uint32_t *size);
	AsioError (*SetClockSource)(uint32_t src);
	AsioError (*GetSamplePosition)(uint64_t *unk1, uint64_t *unk2);
	AsioError (*GetChannelInfo)(struct AsioChannelInfo *pinfo);
	AsioError (*CreateBuffers)(struct AsioBuffers *bufs, uint32_t numbufs, uint32_t buffer_size, struct AsioCreateBufferCallbacks *cbs);
	AsioError (*DestroyBuffers)(void);
	AsioError (*ControlPanel)(void);
	AsioError (*Future)(uint32_t which);
	AsioError (*OutputReady)(void);
};

#define ASIO_MAC_EASY_EX(retval, name, params, call, RETURN) \
	static retval asio_mac_##name params \
	{ \
		struct CAsioFake *asio_fake = (struct CAsioFake *)This; \
	\
		RETURN asio_fake->name call; \
	}

#define ASIO_MAC_EASY(retval, name, params, call) \
	ASIO_MAC_EASY_EX(retval, name, params, call, return)
#define ASIO_MAC_EASY_VOID(name, params, call) \
	ASIO_MAC_EASY_EX(void, name, params, call, /* nothing */)

static ULONG asio_mac_AddRef(IAsio *This)
{
	struct CAsioFake *asio_fake = (struct CAsioFake *)This;

	return ++asio_fake->refcnt;
}

static ULONG asio_mac_Release(IAsio *This)
{
	ULONG ref;
	struct CAsioFake *asio_fake = (struct CAsioFake *)This;

	ref = --asio_fake->refcnt;

	if (ref) return ref;

	/* kill it all off! */
	asio_fake->Quit();
	CloseConnection(&asio_fake->conn);
	ReleaseResource(asio_fake->handle);
	CloseResFile(asio_fake->resfile);
	free(asio_fake);

	return ref;
}

static void asio_mac_GetDriverName(IAsio *This, char name[32])
{
	/* dummy.
	 *
	 * TODO: ASIO_Init on mac os 9 takes in a ASIODriverInfo
	 * structure (according to name mangling) so this info
	 * is probably stored in there somewhere. */
	name[0] = 0;
}

static uint32_t asio_mac_GetDriverVersion(IAsio *This)
{
	return 0;
}

static void asio_mac_GetErrorMessage(IAsio *This, char msg[128])
{
	/* ??? */
	msg[0] = 0;
}

static AsioError asio_mac_Init(IAsio *This, void *unk1)
{
	/* Fake an ASIODriverInfo structure. Align it onto
	 * 8 bytes, just to be absolutely sure we're not
	 * violating anything
	 *
	 * TODO: Need to make a file dump of whatever is
	 * in this structure; it's bound to be useful */
	unsigned char dummy[512] __attribute__((__aligned__(8)));
	struct CAsioFake *fake = (struct CAsioFake *)This;

	memset(dummy, 0, 512);

	return fake->Init(dummy);
}

ASIO_MAC_EASY(AsioError, Start, (IAsio *This), ())
ASIO_MAC_EASY(AsioError, Stop, (IAsio *This), ())
ASIO_MAC_EASY(AsioError, GetChannels, (IAsio *This, uint32_t *pinchns, uint32_t *poutchns), (pinchns, poutchns))
ASIO_MAC_EASY(AsioError, GetLatencies, (IAsio *This, uint32_t *pinlatency, uint32_t *poutlatency), (pinlatency, poutlatency))
ASIO_MAC_EASY(AsioError, GetBufferSize, (IAsio *This, uint32_t *pmin, uint32_t *pmax, uint32_t *pwanted, uint32_t *punknown), (pmin, pmax, pwanted, punknown))
ASIO_MAC_EASY(AsioError, CheckSampleRate, (IAsio *This, double rate), (rate))
ASIO_MAC_EASY(AsioError, GetSampleRate, (IAsio *This, double *prate), (prate))
ASIO_MAC_EASY(AsioError, SetSampleRate, (IAsio *This, double rate), (rate))
ASIO_MAC_EASY(AsioError, GetClockSources, (IAsio *This, struct AsioClockSource *srcs, uint32_t *size), (srcs, size))
ASIO_MAC_EASY(AsioError, SetClockSource, (IAsio *This, uint32_t src), (src))
ASIO_MAC_EASY(AsioError, GetSamplePosition, (IAsio *This, uint64_t *unk1, uint64_t *unk2), (unk1, unk2))
ASIO_MAC_EASY(AsioError, GetChannelInfo, (IAsio *This, struct AsioChannelInfo *pinfo), (pinfo))
ASIO_MAC_EASY(AsioError, CreateBuffers, (IAsio *This, struct AsioBuffers *bufs, uint32_t numbufs, uint32_t buffer_size, struct AsioCreateBufferCallbacks *cbs), (bufs, numbufs, buffer_size, cbs))
ASIO_MAC_EASY(AsioError, DestroyBuffers, (IAsio *This), ())
ASIO_MAC_EASY(AsioError, ControlPanel, (IAsio *This), ())
ASIO_MAC_EASY(AsioError, Future, (IAsio *This, uint32_t which), (which))
ASIO_MAC_EASY(AsioError, OutputReady, (IAsio *This), ())

/* this is never even used anywhere. it's a COM-specific thing.
 * TODO: need to de-COM-ify audio-asio.h (and make it more complete) */
#define asio_mac_QueryInterface NULL

IAsioVtbl asio_mac_vtable = {
#define ASIO_FUNC(type, name, paramswtype, params, calltype) \
	asio_mac_##name,
#include "audio-asio-vtable.h"
};

#undef asio_mac_QueryInterface
#undef ASIO_MAC_EASY
#undef ASIO_MAC_EASY_EX
#undef ASIO_MAC_EASY_VOID

static IAsio *asio_get_device(struct asio_device *dev)
{
	struct CAsioFake *asio;
	OSErr err;
	short resfile;
	short oldresfile;
	Handle res;
	uint32_t (*mainfunc)(void);
	Str255 errname;
	CFragConnectionID conn;

	resfile = FSpOpenResFile(&dev->spec, fsRdPerm);
	if (resfile < 0)
		return NULL;

	/* back this up... */
	oldresfile = CurResFile();

	UseResFile(resfile);

	res = GetResource('Asio', 0);

	/* restore it before doing anytihng else */
	UseResFile(oldresfile);

	if (!res) {
		/* handle it */
		CloseResFile(resfile);
		return NULL;
	}
	HLock(res);
	err = GetMemFragment(*res, GetHandleSize(res),
		"\xd" "ASIO fragment", kReferenceCFrag, &conn,
		(Ptr *)&mainfunc, errname);
	HUnlock(res);

	if (err != noErr) {
		log_appendf(4, " Failed to load ASIO code: %.*s", (int)errname[0], errname + 1);
		CloseResFile(resfile);
		return NULL;
	}

	if (mainfunc && mainfunc() != 0x4153494F /* 'ASIO' */)
		log_appendf(4, "[ASIO]: Main function did not return 'ASIO' ?!?!");

	asio = mem_calloc(1, sizeof(*asio));

#define LOAD_SYMBOL(name, namemangled) \
do { \
	SCHISM_STATIC_ASSERT(sizeof(#namemangled) < 256, "function name must not exceed 255 chars"); \
\
	unsigned char pname[256]; \
	CFragSymbolClass cls; \
\
	/* hmm, this might actually be a data symbol; in the object file it's a pointer */ \
	str_to_pascal(#namemangled, pname, NULL); \
	err = FindSymbol(conn, pname, (Ptr *)&asio->name, &cls); \
\
	if (err != noErr) { \
		log_appendf(4, "[ASIO]: Function %s missing from driver", #name); \
		free(asio); \
		CloseConnection(&conn); \
		ReleaseResource(res); \
		CloseResFile(resfile); \
		return NULL; \
	} \
} while (0)

	LOAD_SYMBOL(OutputReady, ASIOOutputReady__Fv);
	LOAD_SYMBOL(Future, ASIOFuture__FlPv);
	LOAD_SYMBOL(ControlPanel, ASIOControlPanel__Fv);
	LOAD_SYMBOL(DestroyBuffers, ASIODisposeBuffers__Fv);
	LOAD_SYMBOL(CreateBuffers, ASIOCreateBuffers__FP14ASIOBufferInfollP13ASIOCallbacks);
	LOAD_SYMBOL(GetChannelInfo, ASIOGetChannelInfo__FP15ASIOChannelInfo);
	LOAD_SYMBOL(GetSamplePosition, ASIOGetSamplePosition__FP11ASIOSamplesP13ASIOTimeStamp);
	LOAD_SYMBOL(SetClockSource, ASIOSetClockSource__Fl);
	LOAD_SYMBOL(GetClockSources, ASIOGetClockSources__FP15ASIOClockSourcePl);
	LOAD_SYMBOL(SetSampleRate, ASIOSetSampleRate__Fd);
	LOAD_SYMBOL(GetSampleRate, ASIOGetSampleRate__FPd);
	LOAD_SYMBOL(CheckSampleRate, ASIOCanSampleRate__Fd);
	LOAD_SYMBOL(GetBufferSize, ASIOGetBufferSize__FPlPlPlPl);
	LOAD_SYMBOL(GetLatencies, ASIOGetLatencies__FPlPl);
	LOAD_SYMBOL(GetChannels, ASIOGetChannels__FPlPl);
	LOAD_SYMBOL(Stop, ASIOStop__Fv);
	LOAD_SYMBOL(Start, ASIOStart__Fv);
	LOAD_SYMBOL(Quit, ASIOExit__Fv);
	LOAD_SYMBOL(Init, ASIOInit__FP14ASIODriverInfo);

#undef LOAD_SYMBOL

	/* all of our symbols are loaded now, and we have no more work to do
	 * so fill in data in the structure... */

	asio->lpVtbl = &asio_mac_vtable;
	asio->refcnt = 1;
	asio->handle = res;
	asio->conn = conn;
	asio->resfile = resfile;

	return (IAsio *)asio;
}
#endif

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
	unsigned int swap : 1; /* need to byteswap? */

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

static uint32_t ASIO_CDECL asio_msg(uint32_t class, uint32_t msg,
	SCHISM_UNUSED void *unk3, SCHISM_UNUSED void *unk4)
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

static void ASIO_CDECL asio_dummy2(void)
{
	/* dunno what this is */
}

static void ASIO_CDECL asio_buffer_flip(uint32_t buf,
	SCHISM_UNUSED uint32_t unk1)
{
	schism_audio_device_t *dev = current_device;

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

		/* swap the bytes if necessary */
		if (dev->swap) {
			switch (dev->bps) {
#define BSWAP_EX(BITS, INLOOP) \
	do { \
		uint32_t j; \
	\
		for (j = 0; j < dev->numbufs; j++) { \
			uint##BITS##_t *xx = dev->buffers[j].ptrs[buf]; \
			for (i = 0; i < dev->bufsmps; i++) { \
				INLOOP \
			} \
		} \
	} while (0)

#define BSWAP(BITS) BSWAP_EX(BITS, { xx[i] = bswap_##BITS(xx[i]); } )

			case 2: BSWAP(16); break;
			case 4: BSWAP(32); break;
			case 8: BSWAP(64); break;

#undef BSWAP

			case 3: BSWAP_EX(8, { uint8_t tmp = xx[0]; xx[0] = xx[2]; xx[2] = tmp; } ); break;

#undef BSWAP_EX

			}
		}
	}

	IAsio_OutputReady(dev->asio);
}

static void *ASIO_CDECL asio_buffer_flip_ex(SCHISM_UNUSED void *unk1,
	uint32_t buf, SCHISM_UNUSED uint32_t unk2)
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
	AsioError err;
	uint32_t i;

	if (current_device || !devices_size)
		return NULL; /* ASIO only supports one device at a time */

	/* Special handling for "default" ASIO device:
	 *
	 * Try to open each driver, and return the first one that works.
	 * It may be better to initialize a dummy device that then allows
	 * the user to select which ASIO driver they actually want to use. */
	if (id == AUDIO_BACKEND_DEFAULT) {
		for (i = 0; i < devices_size; i++) {
			/* zero out the obtained structure, as the previous call
			 * might have messed up the values */
			memset(obtained, 0, sizeof(*obtained));

			dev = asio_open_device(i, desired, obtained);
			if (dev)
				return dev;
		}

		/* no working device found */
		return NULL;
	}

	dev = mem_calloc(1, sizeof(struct schism_audio_device));

	/* "ASIO under Windows is accessed as a COM in-process server object"
	 *  - ASIO4ALL Private API specification.
	 *
	 * https://asio4all.org/about/developer-resources/private-api-v2-0/
	 *
	 * Yet despite this fact, it is using the same value for CLSID as IID.
	 * If it were truly COM, the IAsio interface would have its own IID.
	 * But I digress. */
	dev->asio = asio_get_device(devices + id);
	if (!dev->asio)
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
		int be;

		chninfo.index = i;
		chninfo.input = 0;

		err = IAsio_GetChannelInfo(dev->asio, &chninfo);
		if (err < 0) {
			log_appendf(4, "[ASIO] GetChannelInfo ERROR: %" PRId32, err);
			goto ASIO_fail;
		}

		switch (chninfo.sample_type) {
		case ASIO_SAMPLE_TYPE_INT16BE:
			obtained->fp = 0;
			dev->bps = 2;
			be = 1;
			break;
		case ASIO_SAMPLE_TYPE_INT24BE:
			obtained->fp = 0;
			dev->bps = 3;
			be = 1;
			break;
		case ASIO_SAMPLE_TYPE_INT32BE:
			obtained->fp = 0;
			dev->bps = 4;
			be = 1;
			break;
		case ASIO_SAMPLE_TYPE_INT16LE:
			obtained->fp = 0;
			dev->bps = 2;
			be = 0;
			break;
		case ASIO_SAMPLE_TYPE_INT24LE:
			obtained->fp = 0;
			dev->bps = 3;
			be = 0;
			break;
		case ASIO_SAMPLE_TYPE_INT32LE:
			obtained->fp = 0;
			dev->bps = 4;
			be = 0;
			break;
		case ASIO_SAMPLE_TYPE_FLOAT32LE:
			obtained->fp = 1;
			dev->bps = 4;
			be = 0;
			break;
		default:
			log_appendf(4, "[ASIO] unknown sample type %" PRIx32,
				chninfo.sample_type);
			goto ASIO_fail;
		}

		obtained->bits = (dev->bps << 3);

#ifdef WORDS_BIGENDIAN
		/* toggle byteswapping */
		if (!be) dev->swap = 1;
#else
		if (be) dev->swap = 1;
#endif

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

#ifdef SCHISM_WIN32

static void *lib_ole32;
static void *lib_shell32;

typedef HRESULT (WINAPI *OLE32_CoInitializeExSpec)(LPVOID, DWORD);
typedef void (WINAPI *OLE32_CoUninitializeSpec)(void);

static OLE32_CoInitializeExSpec OLE32_CoInitializeEx;
static OLE32_CoUninitializeSpec OLE32_CoUninitialize;

static int asio_init(void)
{
	lib_ole32 = loadso_object_load("OLE32.DLL");
	if (!lib_ole32)
		goto fail;

	lib_shell32 = loadso_object_load("SHELL32.DLL");
	if (!lib_shell32)
		goto fail;

	OLE32_CoCreateInstance = (OLE32_CoCreateInstanceSpec)
		loadso_function_load(lib_ole32, "CoCreateInstance");
	OLE32_CoInitializeEx = (OLE32_CoInitializeExSpec)
		loadso_function_load(lib_ole32, "CoInitializeEx");
	OLE32_CoUninitialize = (OLE32_CoUninitializeSpec)
		loadso_function_load(lib_ole32, "CoUninitialize");
	OLE32_CLSIDFromString = (OLE32_CLSIDFromStringSpec)
		loadso_function_load(lib_ole32, "CLSIDFromString");
	SHELL32_GUIDFromStringA = (SHELL32_GUIDFromStringASpec)
		loadso_function_load(lib_shell32, MAKEINTRESOURCEA(703));
	SHELL32_GUIDFromStringW = (SHELL32_GUIDFromStringWSpec)
		loadso_function_load(lib_shell32, MAKEINTRESOURCEA(704));

	if (!OLE32_CoInitializeEx || !OLE32_CoUninitialize
		|| !OLE32_CoCreateInstance
		|| (!OLE32_CLSIDFromString
			|| (!SHELL32_GUIDFromStringA && !SHELL32_GUIDFromStringW))) {
		goto fail;
	}

	switch (OLE32_CoInitializeEx(NULL, COINIT_APARTMENTTHREADED)) {
	case S_OK:
	case S_FALSE:
	case RPC_E_CHANGED_MODE:
		break;
	default:
		goto fail;
	}

	return 1;

fail:
	if (lib_ole32)
		loadso_object_unload(lib_ole32);

	if (lib_shell32)
		loadso_object_unload(lib_shell32);

	return 0;
}

static void asio_quit(void)
{
	OLE32_CoUninitialize();

	if (lib_ole32)
		loadso_object_unload(lib_ole32);

	if (lib_shell32)
		loadso_object_unload(lib_shell32);
}

#elif defined(SCHISM_MACOS)

/* just stubs for now; we don't have any big initialization to do.
 * the most we'd need to do is call into Gestalt I think */
static int asio_init(void) { return 1; }
static void asio_quit(void) { }

#endif

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
