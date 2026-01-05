/**
 * Incomplete ASIO interface definitions -- reverse-engineered from existing
 * binaries and various public sources.
 * 
 * Copyright (C) 2025-2026 Paper
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
**/

#include "audio-asio.h"

#include <windows.h>

/* Toggle #define for ANSI support (i.e. non-WinNT) */
#define ASIO_WIN32_ANSI 1

/* ------------------------------------------------------------------------ */
/* IAsio definition for Windows */

typedef struct IAsioVtbl IAsioVtbl;

struct IAsioVtbl {
	HRESULT (__stdcall *QueryInterface)(IAsio *This, REFIID riid, void **ppvObject);
	ULONG (__stdcall *AddRef)(IAsio *This);
	ULONG (__stdcall *Release)(IAsio *This);
	AsioError (__thiscall *Init)(IAsio *This, void *unk1);
	void (__thiscall *GetDriverName)(IAsio *This, char name[32]);
	uint32_t (__thiscall *GetDriverVersion)(IAsio *This);
	void (__thiscall *GetErrorMessage)(IAsio *This, char msg[128]);
	AsioError (__thiscall *Start)(IAsio *This);
	AsioError (__thiscall *Stop)(IAsio *This);
	AsioError (__thiscall *GetChannels)(IAsio *This, uint32_t *pinchns, uint32_t *poutchns);
	AsioError (__thiscall *GetLatencies)(IAsio *This, uint32_t *pinlatency, uint32_t *poutlatency);
	AsioError (__thiscall *GetBufferSize)(IAsio *This, uint32_t *pmin, uint32_t *pmax, uint32_t *pwanted, uint32_t *punknown);
	AsioError (__thiscall *SupportsSampleRate)(IAsio *This, double rate);
	AsioError (__thiscall *GetSampleRate)(IAsio *This, double *prate);
	AsioError (__thiscall *SetSampleRate)(IAsio *This, double rate);
	AsioError (__thiscall *GetClockSources)(IAsio *This, struct AsioClockSource *srcs, uint32_t *size);
	AsioError (__thiscall *SetClockSource)(IAsio *This, uint32_t src);
	AsioError (__thiscall *GetSamplePosition)(IAsio *This, uint64_t *unk1, uint64_t *unk2);
	AsioError (__thiscall *GetChannelInfo)(IAsio *This, struct AsioChannelInfo *pinfo);
	AsioError (__thiscall *CreateBuffers)(IAsio *This, struct AsioBuffers *bufs, uint32_t numbufs, uint32_t buffer_size, struct AsioCreateBufferCallbacks *cbs);
	AsioError (__thiscall *DestroyBuffers)(IAsio *This);
	AsioError (__thiscall *ControlPanel)(IAsio *This);
	AsioError (__thiscall *Future)(IAsio *This, uint32_t which);
	AsioError (__thiscall *OutputReady)(IAsio *This);
};

struct IAsio {
	/* Win32 ASIO interface */
	CONST_VTBL IAsioVtbl *lpVtbl;
};

/* ------------------------------------------------------------------------ */
/* Helper functions */

static char *Asio_Win32_UNICODEtoUTF8(const wchar_t *in)
{
	int x;
	char *r;

	/* note: this doesn't work on win95, because it doesn't
	 * have support for UTF-8 strings. boohoo, I don't care */
	x = WideCharToMultiByte(CP_UTF8, 0, in, -1, NULL, 0, NULL, NULL);
	if (x <= 0)
		return NULL;

	r = malloc(x + 1);
	if (!r)
		return NULL;

	if (WideCharToMultiByte(CP_UTF8, 0, in, -1, r, x, NULL, NULL) <= 0) {
		free(r);
		return NULL;
	}

	/* I don't think this is really necessary */
	r[x] = 0;
	return r;
}

#ifdef ASIO_WIN32_ANSI
static wchar_t *Asio_Win32_ANSItoUNICODE(const char *in)
{
	int x;
	wchar_t *r;

	x = MultiByteToWideChar(CP_ACP, 0, in, -1, NULL, 0);
	if (x <= 0)
		return NULL;

	r = malloc((x + 1) * 2);
	if (!r)
		return NULL;

	if (MultiByteToWideChar(CP_ACP, 0, in, -1, r, x) <= 0) {
		free(r);
		return NULL;
	}

	r[x] = 0;
	return r;
}

static char *Asio_Win32_ANSItoUTF8(const char *in)
{
	/* This sucks */
	wchar_t *u;
	char *r;

	u = Asio_Win32_ANSItoUNICODE(in);
	if (!u)
		return NULL;

	r = Asio_Win32_UNICODEtoUTF8(u);

	free(u);

	/* 'r' is either a valid pointer or NULL */
	return r;
}

static int Asio_Win32_UseANSI(void)
{
	return !!(GetVersion() & 0x80000000);
}

# define ASIO_ANSI_UNICODE(x, y) \
	if (Asio_Win32_UseANSI()) { \
		x \
	} else { \
		y \
	}
#else
# define ASIO_ANSI_UNICODE(x, y) y
#endif

typedef HRESULT (WINAPI *OLE32_CLSIDFromStringSpec)(LPCOLESTR,LPCLSID);
typedef BOOL (WINAPI *SHELL32_GUIDFromStringASpec)(LPCSTR,LPGUID);
typedef BOOL (WINAPI *SHELL32_GUIDFromStringWSpec)(LPCWSTR,LPGUID);
static OLE32_CLSIDFromStringSpec OLE32_CLSIDFromString;
static SHELL32_GUIDFromStringASpec SHELL32_GUIDFromStringA;
static SHELL32_GUIDFromStringWSpec SHELL32_GUIDFromStringW;

static struct asio_driver {
	CLSID clsid;
	char *description; /* UTF-8 */
} *drivers = NULL;
static size_t drivers_size = 0;

static void Asio_Win32_FreeDrivers(void)
{
	size_t i;

	for (i = 0; i < drivers_size; i++)
		free(drivers[i].description);
	free(drivers);

	drivers_size = 0;
	drivers = NULL;
}

void Asio_DriverPoll(void)
{
	/* this function is one of the unholiest of spaghettis ive ever written */
	LONG lstatus; /* LSTATUS isn't defined in older MinGW headers */
	HKEY hkey;
	DWORD i;
	DWORD max_subkey_len, max_subkey;
	union {
#ifdef ASIO_WIN32_ANSI
		CHAR *a;
#endif
		WCHAR *w;
		void *v; /* for allocation */
	} subkey, dev_desc;

	Asio_Win32_FreeDrivers();

	ASIO_ANSI_UNICODE({
		lstatus = RegOpenKeyExA(HKEY_LOCAL_MACHINE, "SOFTWARE\\ASIO", 0,
			KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE, &hkey);
	}, {
		lstatus = RegOpenKeyExW(HKEY_LOCAL_MACHINE, L"SOFTWARE\\ASIO", 0,
			KEY_ENUMERATE_SUB_KEYS|KEY_QUERY_VALUE, &hkey);
	})

	/* WHAT'S IN THE BOX? */
	if (lstatus != ERROR_SUCCESS)
		return;

	lstatus = RegQueryInfoKeyA(hkey, NULL, NULL, NULL, &max_subkey,
		&max_subkey_len, NULL, NULL, NULL, NULL, NULL, NULL);
	if (lstatus != ERROR_SUCCESS) {
		RegCloseKey(hkey);
		return;
	}

	ASIO_ANSI_UNICODE({
		subkey.a = malloc(max_subkey_len + 1);
	}, {
		subkey.w = malloc((max_subkey_len + 1) * sizeof(WCHAR));
	})

	drivers = malloc(sizeof(*drivers) * max_subkey);

	if (!subkey.v || !drivers) {
		free(drivers);
		free(subkey.v);
		RegCloseKey(hkey);
		return;
	}

	for (i = 0; i < max_subkey; i++) {
		DWORD subkey_len;
		DWORD desc_sz;
		DWORD type;
		union {
#ifdef ASIO_WIN32_ANSI
			CHAR a[39];
#endif
			WCHAR w[39];
		} clsid_s;
		CLSID clsid;
		HKEY hsubkey;

		/* MSDN: "This size should include the terminating [NUL] character." */
		subkey_len = max_subkey_len + 1;

		ASIO_ANSI_UNICODE({
			lstatus = RegEnumKeyExA(hkey, i, subkey.a, &subkey_len, NULL,
				NULL, NULL, NULL);
		}, {
			lstatus = RegEnumKeyExW(hkey, i, subkey.w, &subkey_len, NULL,
				NULL, NULL, NULL);
		})

		if (lstatus != ERROR_SUCCESS)
			continue; /* ??? */

		ASIO_ANSI_UNICODE({
			lstatus = RegOpenKeyExA(hkey, subkey.a, 0, KEY_READ, &hsubkey);
		}, {
			lstatus = RegOpenKeyExW(hkey, subkey.w, 0, KEY_READ, &hsubkey);
		})

		if (lstatus != ERROR_SUCCESS)
			continue;

		ASIO_ANSI_UNICODE({
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

			/* There are no platforms that use ANSI and have CLSIDFromString,
			 * but I'm paranoid. :)
			 *
			 * It would probably make more sense to hand-roll something with
			 * sscanf instead of relying on this functionality. */
			if (OLE32_CLSIDFromString && !Asio_Win32_UseANSI()) {
				success = SUCCEEDED(OLE32_CLSIDFromString(clsid_s.w, &clsid));
			} else {
				ASIO_ANSI_UNICODE({
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

		ASIO_ANSI_UNICODE({
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

		dev_desc.v = malloc(desc_sz);
		if (!dev_desc.v) {
			RegCloseKey(hsubkey);
			continue;
		}

		ASIO_ANSI_UNICODE({
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

		memcpy(&drivers[drivers_size].clsid, &clsid, sizeof(CLSID));
		ASIO_ANSI_UNICODE({
			drivers[drivers_size].description = Asio_Win32_ANSItoUTF8(dev_desc.a);
		}, {
			drivers[drivers_size].description = Asio_Win32_UNICODEtoUTF8(dev_desc.w);
		})

		free(dev_desc.v);

		if (!drivers[drivers_size].description) {
			free(drivers[drivers_size].description);
			continue;
		}

		drivers_size++;
	}
}

uint32_t Asio_DriverCount(void)
{
	return drivers_size;
}

const char *Asio_DriverDescription(uint32_t x)
{
	if (x >= drivers_size)
		return NULL;

	return drivers[x].description;
}

/* initialized in Asio_Init() */
typedef HRESULT (WINAPI *OLE32_CoCreateInstanceSpec)(REFCLSID rclsid,
	LPUNKNOWN pUnkOuter, DWORD dwClsContext, REFIID riid, LPVOID *ppv);
static OLE32_CoCreateInstanceSpec OLE32_CoCreateInstance;

IAsio *Asio_DriverGet(uint32_t x)
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

	if (x >= drivers_size)
		return NULL;

	hres = OLE32_CoCreateInstance(&drivers[x].clsid, NULL,
		CLSCTX_INPROC_SERVER, &drivers[x].clsid, (LPVOID *)&asio);

	if (FAILED(hres))
		return NULL;

	return asio;
}

/* ------------------------------------------------------------------------ */
/* IAsio functions */

AsioError IAsio_Init(IAsio *This)
{
	return This->lpVtbl->Init(This, NULL);
}

void IAsio_Quit(IAsio *This)
{
	This->lpVtbl->Release(This);
}

void IAsio_GetDriverName(IAsio *This, char name[32])
{
	This->lpVtbl->GetDriverName(This, name);
}

uint32_t IAsio_GetDriverVersion(IAsio *This)
{
	return This->lpVtbl->GetDriverVersion(This);
}

void IAsio_GetErrorMessage(IAsio *This, char msg[128])
{
	This->lpVtbl->GetErrorMessage(This, msg);
}

AsioError IAsio_Start(IAsio *This)
{
	return This->lpVtbl->Start(This);
}

AsioError IAsio_Stop(IAsio *This)
{
	return This->lpVtbl->Stop(This);
}

AsioError IAsio_GetChannels(IAsio *This, uint32_t *pinchns, uint32_t *poutchns)
{
	return This->lpVtbl->GetChannels(This, pinchns, poutchns);
}

AsioError IAsio_GetLatencies(IAsio *This, uint32_t *pinlatency, uint32_t *poutlatency)
{
	return This->lpVtbl->GetLatencies(This, pinlatency, poutlatency);
}

AsioError IAsio_GetBufferSize(IAsio *This, uint32_t *pmin, uint32_t *pmax, uint32_t *pwanted, uint32_t *punknown)
{
	return This->lpVtbl->GetBufferSize(This, pmin, pmax, pwanted, punknown);
}

AsioError IAsio_SupportsSampleRate(IAsio *This, double rate)
{
	return This->lpVtbl->SupportsSampleRate(This, rate);
}

AsioError IAsio_GetSampleRate(IAsio *This, double *prate)
{
	return This->lpVtbl->GetSampleRate(This, prate);
}

AsioError IAsio_SetSampleRate(IAsio *This, double rate)
{
	return This->lpVtbl->SetSampleRate(This, rate);
}

AsioError IAsio_GetClockSources(IAsio *This, struct AsioClockSource *srcs, uint32_t *size)
{
	return This->lpVtbl->GetClockSources(This, srcs, size);
}

AsioError IAsio_SetClockSource(IAsio *This, uint32_t src)
{
	return This->lpVtbl->SetClockSource(This, src);
}

AsioError IAsio_GetSamplePosition(IAsio *This, uint64_t *unk1, uint64_t *unk2)
{
	return This->lpVtbl->GetSamplePosition(This, unk1, unk2);
}

AsioError IAsio_GetChannelInfo(IAsio *This, struct AsioChannelInfo *pinfo)
{
	return This->lpVtbl->GetChannelInfo(This, pinfo);
}

AsioError IAsio_CreateBuffers(IAsio *This, struct AsioBuffers *bufs, uint32_t numbufs, uint32_t buffer_size, struct AsioCreateBufferCallbacks *cbs)
{
	return This->lpVtbl->CreateBuffers(This, bufs, numbufs, buffer_size, cbs);
}

AsioError IAsio_DestroyBuffers(IAsio *This)
{
	return This->lpVtbl->DestroyBuffers(This);
}

AsioError IAsio_ControlPanel(IAsio *This)
{
	return This->lpVtbl->ControlPanel(This);
}

AsioError IAsio_Future(IAsio *This, uint32_t which)
{
	return This->lpVtbl->Future(This, which);
}

AsioError IAsio_OutputReady(IAsio *This)
{
	return This->lpVtbl->OutputReady(This);
}

static HMODULE lib_ole32;
static HMODULE lib_shell32;

typedef HRESULT (WINAPI *OLE32_CoInitializeExSpec)(LPVOID, DWORD);
typedef void (WINAPI *OLE32_CoUninitializeSpec)(void);

static OLE32_CoInitializeExSpec OLE32_CoInitializeEx;
static OLE32_CoUninitializeSpec OLE32_CoUninitialize;

int32_t Asio_Init(void)
{
	lib_ole32 = LoadLibraryA("OLE32.DLL");
	if (!lib_ole32)
		goto fail;

	lib_shell32 = LoadLibraryA("SHELL32.DLL");
	if (!lib_shell32)
		goto fail;

	OLE32_CoCreateInstance = (OLE32_CoCreateInstanceSpec)
		GetProcAddress(lib_ole32, "CoCreateInstance");
	OLE32_CoInitializeEx = (OLE32_CoInitializeExSpec)
		GetProcAddress(lib_ole32, "CoInitializeEx");
	OLE32_CoUninitialize = (OLE32_CoUninitializeSpec)
		GetProcAddress(lib_ole32, "CoUninitialize");
	OLE32_CLSIDFromString = (OLE32_CLSIDFromStringSpec)
		GetProcAddress(lib_ole32, "CLSIDFromString");
	SHELL32_GUIDFromStringA = (SHELL32_GUIDFromStringASpec)
		GetProcAddress(lib_shell32, MAKEINTRESOURCEA(703));
	SHELL32_GUIDFromStringW = (SHELL32_GUIDFromStringWSpec)
		GetProcAddress(lib_shell32, MAKEINTRESOURCEA(704));

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

	return 0;

fail:
	if (lib_ole32)
		FreeLibrary(lib_ole32);

	if (lib_shell32)
		FreeLibrary(lib_shell32);

	return -1;
}

void Asio_Quit(void)
{
	/* clean up driver count, if any */
	Asio_Win32_FreeDrivers();

	OLE32_CoUninitialize();

	if (lib_ole32)
		FreeLibrary(lib_ole32);

	if (lib_shell32)
		FreeLibrary(lib_shell32);
}
