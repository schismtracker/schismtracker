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

/* Target Windows 10, so that we can get the extra audio codec
 * GUIDs it provides */
#define WINVER 0x1000
#define _WIN32_WINNT 0x1000

#include "headers.h"
#include "charset.h"
#include "fmt.h"
#include "util.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>

/* Dynamically loaded Media Foundation functions, to keep Schism working with Windows XP */

typedef HRESULT WINAPI (*MF_MFStartupSpec)(ULONG version, DWORD flags);
typedef HRESULT WINAPI (*MF_MFShutdownSpec)(void);
typedef HRESULT WINAPI (*MF_MFCreateSourceResolverSpec)(IMFSourceResolver **ppISourceResolver);
typedef HRESULT WINAPI (*MF_MFGetServiceSpec)(IUnknown *punkObject, REFGUID guidService, REFIID riid, LPVOID *ppvObject);
typedef HRESULT WINAPI (*MF_PropVariantToStringAllocSpec)(REFPROPVARIANT propvar, PWSTR *ppszOut);
typedef HRESULT WINAPI (*MF_PropVariantToUInt64Spec)(REFPROPVARIANT propvar, ULONGLONG *pullRet);
typedef HRESULT WINAPI (*MF_MFCreateSourceReaderFromMediaSourceSpec)(IMFMediaSource *pMediaSource, IMFAttributes *pAttributes, IMFSourceReader **ppSourceReader);
typedef HRESULT WINAPI (*MF_MFCreateMFByteStreamOnStreamSpec)(IStream *pStream, IMFByteStream **ppByteStream);
typedef HRESULT WINAPI (*MF_MFCreateMediaTypeSpec)(IMFMediaType **ppMFType);

typedef IStream * WINAPI (*MF_SHCreateMemStreamSpec)(const BYTE *pInit, UINT cbInit);

static MF_MFStartupSpec MF_MFStartup;
static MF_MFShutdownSpec MF_MFShutdown;
static MF_MFCreateSourceResolverSpec MF_MFCreateSourceResolver;
static MF_MFGetServiceSpec MF_MFGetService;
static MF_PropVariantToStringAllocSpec MF_PropVariantToStringAlloc;
static MF_PropVariantToUInt64Spec MF_PropVariantToUInt64;
static MF_MFCreateSourceReaderFromMediaSourceSpec MF_MFCreateSourceReaderFromMediaSource;
static MF_MFCreateMFByteStreamOnStreamSpec MF_MFCreateMFByteStreamOnStream;
static MF_MFCreateMediaTypeSpec MF_MFCreateMediaType;

static MF_SHCreateMemStreamSpec MF_SHCreateMemStream;

/* --------------------------------------------------------------------- */
/* this is what C++ nonsense looks like in plain C, btw. this api is
 * absolutely diabolical and I yearn to just use function pointers again */

struct slurp_async_op {
	/* vtable */
	CONST_VTBL IUnknownVtbl *lpvtbl;

	/* things we have to free, or whatever */
	IMFByteStream *bs;
	IMFAsyncCallback *cb;

	/* agh */
	BYTE *buffer;
	ULONG req_length;
	ULONG actual_length;

	//int64_t pos; TODO

	long ref_cnt;
};

static HRESULT STDMETHODCALLTYPE slurp_async_op_QueryInterface(IUnknown *This, REFIID riid, void **ppvobj)
{
	static const QITAB qit[] = {
		{&IID_IUnknown, 0},
		{NULL, 0},
	};

	return QISearch(This, qit, riid, ppvobj);
}

static ULONG STDMETHODCALLTYPE slurp_async_op_AddRef(IUnknown *This)
{
	return InterlockedIncrement(&(((struct slurp_async_op *)This)->ref_cnt));
}

static ULONG STDMETHODCALLTYPE slurp_async_op_Release(IUnknown *This)
{
	long ref = InterlockedDecrement(&(((struct slurp_async_op *)This)->ref_cnt));
	if (!ref)
		free(This);

	return ref;
}

static const IUnknownVtbl slurp_async_op_vtbl = {
	.QueryInterface = slurp_async_op_QueryInterface,
	.AddRef = slurp_async_op_AddRef,
	.Release = slurp_async_op_Release,
};

static inline int slurp_async_op_new(IUnknown **This, IMFByteStream *bs, IMFAsyncCallback *cb, BYTE *buffer, ULONG req_length)
{
	struct slurp_async_op *op = calloc(1, sizeof(struct slurp_async_op));
	if (!op)
		return 0;

	op->vtbl = &slurp_async_op_vtbl;

	/* user provided info... */
	op->bs = bs;
	op->cb = cb;
	op->buffer = buffer;
	op->req_length = req_length;

	/* hmph */
	op->ref_cnt = 1;

	*This = (IUnknown *)op;
	return 1;
}

/* ---------------------------------------------------------------------- */

/* IMFAsyncCallback implementation; I really wish I could just use the regular
 * IMFByteStream implementation because there isn't really anything special about
 * this :/ */
struct slurp_async_callback {
	/* vtable */
	CONST_VTBL IMFAsyncCallbackVtbl *lpvtbl;

	long ref_cnt;
};

/* IUnknown */
static HRESULT STDMETHODCALLTYPE slurp_async_callback_QueryInterface(IMFAsyncCallback *This, REFIID riid, void **ppobj)
{
	static const QITAB qit[] = {
		{&IID_IMFAsyncCallback, 0},
		{NULL, 0},
	};

	return QISearch(This, qit, IID_PPV_ARGS(&ppobj));
}

static ULONG STDMETHODCALLTYPE slurp_async_callback_AddRef(IMFAsyncCallback *This)
{
	return InterlockedIncrement(&(((struct slurp_async_callback *)This)->ref_cnt));
}

static ULONG STDMETHODCALLTYPE slurp_async_callback_Release(IMFAsyncCallback *This)
{
	long ref = InterlockedDecrement(&(((struct slurp_async_callback *)This)->ref_cnt));
	if (!ref)
		free(This);

	return ref;
}

/* IMFAsyncCallback */
static HRESULT STDMETHODCALLTYPE slurp_async_callback_GetParameters(IMFAsyncCallback *This, DWORD *pdwFlags, DWORD *pdwQueue)
{
	/* optional apparently, don't care enough to implement */
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE slurp_async_callback_Invoke(IMFAsyncCallback *This, IMFAsyncResult *pAsyncResult)
{
	IUnknown *pState = NULL;
	IUnknown *pUnk = NULL;
	IMFAsyncResult *caller = NULL;
	HRESULT hr = S_OK;

	hr = pAsyncResult->lpVtbl->GetState(pAsyncResult, &pState);
	if (FAILED(hr))
		goto done;

	hr = pState->lpVtbl->QueryInterface(pState, IID_PPV_ARGS(&caller));
	if (FAILED(hr))
		goto done;

	hr = caller->lpVtbl->GetObject(&pUnk);
	if (FAILED(hr))
		goto done;

	op = (struct slurp_async_op *)pUnk;

	/* now we can actually do the work */
	hr = op->bs->lpVtbl->Read(op->bs, op->buffer, op->req_length, &op->actual_length);

done:
	if (op->caller) {
		op->caller->lpVtbl->SetStatus(op->caller, hr);
		MFInvokeCallback(op->caller);
	}

	if (pState)
		pState->lpVtbl->Release(pState);

	if (caller)
		caller->lpVtbl->Release(caller);

	if (pUnk)
		pUnk->lpVtbl->Release(pUnk);

	return S_OK;
}

static const IMFASyncCallbackVtbl = {
	/* IUnknown */
	.QueryInterface = slurp_async_callback_QueryInterface,
	.AddRef = slurp_async_callback_AddRef,
	.Release = slurp_async_callback_Release,

	/* IMFASyncCallback */
	.GetParameters = slurp_async_callback_GetParameters,
	.Invoke = slurp_async_callback_Invoke,
};

static inline int slurp_async_callback_new(IMFASyncCallback **This)
{
	struct slurp_async_callback *cb = calloc(1, sizeof(struct slurp_async_callback));
	if (!cb)
		return 0;

	cb->ref_cnt = 1;

	*This = (IMFAsyncCallback *)cb;
	return 1;
}

/* ----------------------------------------------------------------------------------------- */

/* IMFByteStream implementation for slurp */
struct mfbytestream {
	/* IMFByteStream vtable */
	CONST_VTBL IMFByteStreamVtbl *lpvtbl;

	slurp_t *fp;
	SDL_mutex *mutex;
	long ref_cnt;
};

/* IUnknown methods */
static HRESULT STDMETHODCALLTYPE mfbytestream_QueryInterface(IMFByteStream *This, REFIID riid, void **ppobj)
{
	/* stupid fucking hack */
	static const QITAB qit[] = {
		{&IID_IMFByteStream, 0},
		{NULL, 0},
	};

	return QISearch(This, qit, IID_PPV_ARGS(&ppobj));
}

static ULONG STDMETHODCALLTYPE mfbytestream_AddRef(IMFByteStream *This)
{
	struct mfbytestream *mfb = (struct mfbytestream *)This;

	return InterlockedIncrement(&mfb->ref_cnt);
}

static ULONG STDMETHODCALLTYPE mfbytestream_Release(IMFByteStream *This)
{
	struct mfbytestream *mfb = (struct mfbytestream *)This;

	long ref = InterlockedDecrement(&mfb->ref_cnt);
	if (!ref) {
		SDL_DestroyMutex(mfb->mutex);
		free(mfb);
	}

	return ref;
}

/* IMFByteStream methods */
static HRESULT STDMETHODCALLTYPE mfbytestream_GetCapabilities(UNUSED IMFByteStream* This, DWORD *pdwCapabilities)
{
	*pdwCapabilities = MFBYTESTREAM_IS_READABLE | MFBYTESTREAM_IS_SEEKABLE | MFBYTESTREAM_DOES_NOT_USE_NETWORK;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_GetLength(IMFByteStream *This, QWORD *pqwLength)
{
	struct mfbytestream *mfb = (struct mfbytestream *)This;
	*pqwLength = slurp_length(mfb->fp);
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_SetLength(IMFByteStream *This, QWORD qwLength)
{
	return E_NOTIMPL;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_SetCurrentPosition(IMFByteStream *This, QWORD qwPosition)
{
	struct mfbytestream *mfb = (struct mfbytestream *)this;

	SDL_LockMutex(mfb->mutex);

	slurp_seek(mfb->fp, qwPosition, SEEK_SET);

	SDL_UnlockMutex(mfb->mutex);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_GetCurrentPosition(IMFByteStream *This, QWORD *pqwPosition)
{
	struct mfbytestream *mfb = (struct mfbytestream *)this;

	SDL_LockMutex(mfb->mutex);

	int64_t pos = slurp_tell(mfb->fp);

	SDL_UnlockMutex(mfb->mutex);

	if (pos < 0)
		return E_FAIL;

	*pqwPosition = pos;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_IsEndOfStream(IMFByteStream *This, WINBOOL *pfEndOfStream)
{
	struct mfbytestream *mfb = (struct mfbytestream *)this;

	SDL_LockMutex(mfb->mutex);

	*pfEndOfStream = slurp_eof(mfb->fp);

	SDL_UnlockMutex(mfb->mutex);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_Read(IMFByteStream *This, BYTE *pb, ULONG cb, ULONG *pcbRead)
{
	struct mfbytestream *mfb = (struct mfbytestream *)this;

	SDL_LockMutex(mfb->mutex);

	*pcbRead = slurp_read(mfb->fp, pb, cb);

	SDL_UnlockMutex(mfb->mutex);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_BeginRead(IMFByteStream *This, BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState)
{
	IUnknown *op = NULL;
	IMFASyncCallback* cb = NULL;
	IMFAsyncResult *result = NULL;
	HRESULT hr = S_OK;

	if (!slurp_async_callback_new(&cb))
		return E_OUTOFMEMORY;

	if (!slurp_async_op_new(&op, This, cb, pb, cb))
		return E_OUTOFMEMORY;

	if (FAILED(hr = MFCreateAsyncResult(op, pCallback, punkState, &result)))
		goto fail;

	hr = MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, cb, result);

	result->lpVtbl->Release(result);

	return hr;

fail:
	if (cb)
		cb->lpVtbl->Release(cb);

	if (result)
		result->lpVtbl->Release(result);

	return hr;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_EndRead(IMFByteStream *This, IMFAsyncResult *pResult, ULONG *pcbRead)
{
	*pcbRead = 0;

	struct async_slurp_op *op = NULL;
	IUnknown *unk = NULL;

	HRESULT hr = pResult->lpVtbl->GetStatus(pResult);
	if (FAILED(hr))
		goto done;

	hr = pResult->lpVtbl->GetObject(pResult, &unk);
	if (FAILED(hr))
		goto done;

	op = (struct async_slurp_op *)unk;

	*pcbRead = op->actual_length;

done:
	if (op) {
		/* need to deal with this crap */
		op->cb->lpvtbl->Release(op->cb);
		op->lpvtbl->Release(op);
	}

	return hr;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_Write(IMFByteStream *This, const BYTE *pb, ULONG cb, ULONG *pcbWritten)
{
	return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_BeginWrite(IMFByteStream *This, const BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState)
{
	return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_EndWrite(IMFByteStream *This, IMFAsyncResult *pResult, ULONG *pcbWritten)
{
	return E_NOINTERFACE;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_Seek(IMFByteStream *This, MFBYTESTREAM_SEEK_ORIGIN SeekOrigin, LONGLONG llSeekOffset, DWORD dwSeekFlags, QWORD *pqwCurrentPosition)
{
	struct mfbytestream *mfb = (struct mfbytestream *)this;
	int whence;

	switch (SeekOrigin) {
	case msoBegin:
		whence = SEEK_SET;
		break;
	case msoCurrent:
		whence = SEEK_CUR;
		break;
	default:
		return E_NOINTERFACE;
	}

	// XXX MFBYTESTREAM_SEEK_FLAG_CANCEL_PENDING_IO wtf?

	slurp_seek(mfb->fp, llSeekOffset, whence);

	*pqwCurrentPosition = slurp_tell(mfb->fp);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_Flush(IMFByteStream *This)
{
	/* we don't have anything to flush... */
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_Close(IMFByteStream *This)
{
	/* do nothing */
	return S_OK;
}

static const IMFByteStreamVtbl mfbytestream_vtbl = {
	/* IUnknown */
	.QueryInterface = mfbytestream_QueryInterface,
	.AddRef = mfbytestream_AddRef,
	.Release = mfbytestream_Release,

	/* IMFByteStream */
	.GetCapabilities = mfbytestream_GetCapabilities,
	.GetLength = mfbytestream_GetLength,
	.SetLength = mfbytestream_SetLength,
	.SetCurrentPosition = mfbytestream_SetCurrentPosition,
	.GetCurrentPosition = mfbytestream_GetCurrentPosition,
	.IsEndOfStream = mfbytestream_IsEndOfStream,
	.Read = mfbytestream_Read,
	.BeginRead = mfbytestream_BeginRead,
	.EndRead = mfbytestream_EndRead,
	.Write = mfbytestream_Write,
	.BeginWrite = mfbytestream_BeginWrite,
	.EndWrite = mfbytestream_EndWrite,
	.Seek = mfbytestream_Seek,
	.Flush = mfbytestream_Flush,
	.Close = mfbytestream_Close,
};

static inline int mfbytestream_new(IMFByteStream **imf, slurp_t *fp)
{
	struct mfbytestream *mfb = calloc(1, sizeof(*mfb));
	if (!mfb)
		return 0;

	/* put in the vtable */
	mfb->vtbl = mfbytestream_vtbl;
	mfb->fp = fp;
	mfb->mutex = SDL_CreateMutex();
	mfb->ref_cnt = 1;

	/* owned */
	*imf = (IMFByteStream *)mfb;
	return 1;
}

/* -------------------------------------------------------------- */

/* I really have no idea what (apartment/multi)threading does. I don't
 * think it really has an impact, either */
#define COM_INITFLAGS (COINIT_MULTITHREADED | COINIT_DISABLE_OLE1DDE)

static const char* get_media_type_description(IMFMediaType* media_type)
{
	GUID subtype;

	if (SUCCEEDED(media_type->lpVtbl->GetGUID(media_type, &MF_MT_SUBTYPE, &subtype))) {
#define SUBTYPE(x, n) do { if (IsEqualGUID(&subtype, &x)) return n; } while (0)
		SUBTYPE(MFAudioFormat_AAC, "Advanced Audio Coding");
		/* SUBTYPE(MFAudioFormat_ADTS, "Not used"); */
		SUBTYPE(MFAudioFormat_ALAC, "Apple Lossless Audio Codec");
		SUBTYPE(MFAudioFormat_AMR_NB, "Adaptive Multi-Rate");
		SUBTYPE(MFAudioFormat_AMR_WB, "Adaptive Multi-Rate Wideband");
		SUBTYPE(MFAudioFormat_Dolby_AC3, "Dolby Digital (AC-3)");
		SUBTYPE(MFAudioFormat_Dolby_AC3_SPDIF, "Dolby AC-3 over S/PDIF");
		SUBTYPE(MFAudioFormat_Dolby_DDPlus, "Dolby Digital Plus");
		SUBTYPE(MFAudioFormat_DRM, "Encrypted audio data"); /* ???? */
		SUBTYPE(MFAudioFormat_DTS, "Digital Theater Systems"); /* yes, "theater", not "theatre" */
		SUBTYPE(MFAudioFormat_FLAC, "Free Lossless Audio Codec");
		SUBTYPE(MFAudioFormat_Float, "Uncompressed floating-point audio");
		SUBTYPE(MFAudioFormat_Float_SpatialObjects, "Uncompressed floating-point audio");
		SUBTYPE(MFAudioFormat_MP3, "MPEG Layer-3 (MP3)");
		SUBTYPE(MFAudioFormat_MPEG, "MPEG-1 audio payload");
		SUBTYPE(MFAudioFormat_MSP1, "Windows Media Audio 9 Voice");
		SUBTYPE(MFAudioFormat_Opus, "Opus");
		SUBTYPE(MFAudioFormat_PCM, "Uncompressed PCM");
		/* SUBTYPE(MFAudioFormat_QCELP, "QCELP audio"); -- ...can't get this to compile */
		SUBTYPE(MFAudioFormat_WMASPDIF, "Windows Media Audio 9 over S/PDIF");
		SUBTYPE(MFAudioFormat_WMAudio_Lossless, "Windows Media Audio 9 Lossless");
		SUBTYPE(MFAudioFormat_WMAudioV8, "Windows Media Audio 8");
		SUBTYPE(MFAudioFormat_WMAudioV9, "Windows Media Audio 8");
#undef SUBTYPE
#ifdef SCHISM_MF_DEBUG
		log_appendf(1, "Unknown Media Foundation subtype found:");
		log_appendf(1, "{%08lX-%04hX-%04hX-%02hhX%02hhX-%02hhX%02hhX%02hhX%02hhX%02hhX%02hhX}", 
			subtype.Data1, subtype.Data2, subtype.Data3, 
			subtype.Data4[0], subtype.Data4[1], subtype.Data4[2], subtype.Data4[3],
			subtype.Data4[4], subtype.Data4[5], subtype.Data4[6], subtype.Data4[7]);
#endif
	}

	/* welp */
	return "Media Foundation";
}

static int get_source_reader_information(IMFSourceReader *reader, dmoz_file_t *file) {
	IMFMediaType *media_type = NULL;
	uint64_t length = 0;
	BOOL compressed = FALSE;

	if (SUCCEEDED(reader->lpVtbl->GetNativeMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, MF_SOURCE_READER_CURRENT_TYPE_INDEX, &media_type))) {
		file->type = (SUCCEEDED(media_type->lpVtbl->IsCompressedFormat(media_type, &compressed)) && compressed)
			? TYPE_SAMPLE_COMPR
			: TYPE_SAMPLE_PLAIN;

		file->description = get_media_type_description(media_type);

		media_type->lpVtbl->Release(media_type);
	}

	return 1;
}

static int convert_media_foundation_metadata(IMFMediaSource* source, dmoz_file_t* file)
{
	IMFPresentationDescriptor* descriptor = NULL;
	IMFMetadataProvider* provider = NULL;
	IMFMetadata* metadata = NULL;
	PROPVARIANT propnames = {0};
	DWORD streams = 0;
	int found = 0;
	uint64_t duration = 0;

	/* do this before anything else... */
	PropVariantInit(&propnames);

	if (FAILED(source->lpVtbl->CreatePresentationDescriptor(source, &descriptor)))
		goto cleanup;

	if (SUCCEEDED(descriptor->lpVtbl->GetUINT64(descriptor, &MF_PD_DURATION, &duration)))
		file->smp_length = (double)duration * file->smp_speed / (10.0 * 1000.0 * 1000.0);

	if (FAILED(MF_MFGetService((IUnknown*)source, &MF_METADATA_PROVIDER_SERVICE, &IID_IMFMetadataProvider, (void**)&provider)))
		goto cleanup;

	if (FAILED(provider->lpVtbl->GetMFMetadata(provider, descriptor, 0, 0, &metadata)))
		goto cleanup;

	if (FAILED(metadata->lpVtbl->GetAllPropertyNames(metadata, &propnames)))
		goto cleanup;

	for (DWORD prop_index = 0; prop_index < propnames.calpwstr.cElems; prop_index++) {
		LPWSTR prop_name = propnames.calpwstr.pElems[prop_index];
		if (!prop_name)
			continue; /* ... */

		if (!wcscmp(prop_name, L"Title")) {
			PROPVARIANT propval = {0};
			PropVariantInit(&propval);

			if (FAILED(metadata->lpVtbl->GetProperty(metadata, prop_name, &propval))) {
				PropVariantClear(&propval);
				continue;
			}

			LPWSTR prop_val_str = NULL;
			if (FAILED(MF_PropVariantToStringAlloc(&propval, &prop_val_str))) {
				PropVariantClear(&propval);
				continue;
			}

			found = 1;

			charset_iconv((uint8_t*)prop_val_str, (uint8_t**)file->title, CHARSET_WCHAR_T, CHARSET_CP437);

			CoTaskMemFree(prop_val_str);
			PropVariantClear(&propval);
		}
	}

cleanup:
	PropVariantClear(&propnames);

	if (descriptor)
		descriptor->lpVtbl->Release(descriptor);

	if (provider)
		provider->lpVtbl->Release(provider);

	if (metadata)
		metadata->lpVtbl->Release(metadata);

	return found;
}

static int media_foundation_initialized = 0;

/* needs to be called once on startup */
int win32mf_init(void)
{
	HMODULE shlwapi = NULL;
	HMODULE mf = NULL;
	HMODULE mfplat = NULL;
	HMODULE mfreadwrite = NULL;
	HMODULE propsys = NULL;

#ifdef SCHISM_MF_DEBUG
#define DEBUG_PUTS(x) puts(x)
#else
#define DEBUG_PUTS(x)
#endif

#define LOAD_MF_LIBRARY(o) \
	do { \
		o = LoadLibraryW(L ## #o L".dll"); \
		if (!(o)) { DEBUG_PUTS("Failed to load library " #o "!"); goto fail; } \
	} while (0)

#define LOAD_MF_OBJECT(o, x) \
	do { \
		MF_##x = (MF_##x##Spec)GetProcAddress(o, #x); \
		if (!MF_##x) { DEBUG_PUTS("Failed to load " #x " from library " #o ".dll !"); goto fail; } \
	} while (0)

	HRESULT com_init = CoInitializeEx(NULL, COM_INITFLAGS);
	if (com_init != S_OK && com_init != S_FALSE && com_init != RPC_E_CHANGED_MODE) {
		DEBUG_PUTS("Failed to initialize COM!");
		goto fail;
	}

	LOAD_MF_LIBRARY(shlwapi);

	MF_SHCreateMemStream = (MF_SHCreateMemStreamSpec)GetProcAddress(shlwapi, MAKEINTRESOURCE(12));
	if (!MF_SHCreateMemStream) {
		DEBUG_PUTS("Failed to load SHCreateMemStream from library shlwapi.dll!");
		goto fail;
	}

	LOAD_MF_LIBRARY(mf);

	LOAD_MF_OBJECT(mf, MFGetService);
	LOAD_MF_OBJECT(mf, MFCreateSourceResolver);

	LOAD_MF_LIBRARY(mfplat);

	LOAD_MF_OBJECT(mfplat, MFCreateMFByteStreamOnStream);
	LOAD_MF_OBJECT(mfplat, MFCreateMediaType);
	LOAD_MF_OBJECT(mfplat, MFStartup);
	LOAD_MF_OBJECT(mfplat, MFShutdown);

	LOAD_MF_LIBRARY(mfreadwrite);

	LOAD_MF_OBJECT(mfreadwrite, MFCreateSourceReaderFromMediaSource);

	LOAD_MF_LIBRARY(propsys);

	LOAD_MF_OBJECT(propsys, PropVariantToStringAlloc);
	LOAD_MF_OBJECT(propsys, PropVariantToUInt64);

	/* MFSTARTUP_LITE == no sockets */
	media_foundation_initialized = SUCCEEDED(MF_MFStartup(MF_VERSION, MFSTARTUP_LITE));

	return media_foundation_initialized;

fail:
	if (shlwapi)
		FreeLibrary(shlwapi);

	if (mfplat)
		FreeLibrary(mfplat);

	if (mf)
		FreeLibrary(mf);

	if (mfreadwrite)
		FreeLibrary(mfreadwrite);

	if (propsys)
		FreeLibrary(propsys);

	CoUninitialize();

	return 0;
}

void win32mf_quit(void)
{
	if (!media_foundation_initialized)
		return;

	MF_MFShutdown();
	CoUninitialize();
}

/* implementation for both READ_INFO and LOAD_SAMPLE
 * TODO make this a function with a structure pls */
#define MEDIA_FOUNDATION_START(fp, url, cleanup) \
	IMFSourceResolver* resolver = NULL; \
	MF_OBJECT_TYPE object_type = MF_OBJECT_INVALID; \
	IUnknown* unknown_media_source = NULL; \
	IMFMediaSource* real_media_source = NULL; \
	IMFSourceReader* reader = NULL; \
	IMFByteStream *byte_stream = NULL; \
	uint32_t channels = 0, sps = 0, bps = 0, flags; \
	IMFMediaType *uncompressed_type = NULL; \
\
	if (FAILED(MF_MFCreateSourceResolver(&resolver))) \
		goto cleanup; \
\
	if (!mfbytestream_new(&byte_stream, fp)) \
		goto cleanup; \
\
	if (FAILED(resolver->lpVtbl->CreateObjectFromByteStream(resolver, byte_stream, url, MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE | MF_RESOLUTION_READ, NULL, &object_type, &unknown_media_source))) \
		goto cleanup; \
\
	if (object_type != MF_OBJECT_MEDIASOURCE) \
		goto cleanup; \
\
	if (FAILED(unknown_media_source->lpVtbl->QueryInterface(unknown_media_source, &IID_IMFMediaSource, (void**)&real_media_source))) \
		goto cleanup; \
\
	if (FAILED(MF_MFCreateSourceReaderFromMediaSource(real_media_source, NULL, &reader))) \
		goto cleanup; \
\
	if (FAILED(MF_MFCreateMediaType(&uncompressed_type))) \
		goto cleanup; \
\
	if (FAILED(uncompressed_type->lpVtbl->SetGUID(uncompressed_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio))) \
		goto cleanup; \
\
	if (FAILED(uncompressed_type->lpVtbl->SetGUID(uncompressed_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM))) \
		goto cleanup; \
\
	if (FAILED(reader->lpVtbl->SetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, uncompressed_type))) \
		goto cleanup; \
\
	uncompressed_type->lpVtbl->Release(uncompressed_type); \
\
	if (FAILED(reader->lpVtbl->GetCurrentMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, &uncompressed_type))) \
		goto cleanup; \
\
	if (FAILED(reader->lpVtbl->SetStreamSelection(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE))) \
		goto cleanup; \
\
	if (FAILED(uncompressed_type->lpVtbl->GetUINT32(uncompressed_type, &MF_MT_AUDIO_NUM_CHANNELS, &channels))) \
		goto cleanup; \
\
	if (FAILED(uncompressed_type->lpVtbl->GetUINT32(uncompressed_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &sps))) \
		goto cleanup; \
\
	if (FAILED(uncompressed_type->lpVtbl->GetUINT32(uncompressed_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &bps))) \
		goto cleanup; \
\
	if (sps <= 0) \
		goto cleanup; \
\
	flags = SF_LE | SF_PCMS; \
\
	switch (channels) { \
	case 1: flags |= SF_M; \
	case 2: flags |= SF_SI; \
	default: goto cleanup; \
	} \
\
	switch (bps) { \
	case 8: flags |= SF_8; \
	case 16: flags |= SF_16; \
	case 24: flags |= SF_24; \
	case 32: flags |= SF_32; \
	default: goto cleanup; \
	}

#define MEDIA_FOUNDATION_END() \
	if (resolver) \
		resolver->lpVtbl->Release(resolver); \
\
	if (unknown_media_source) \
		unknown_media_source->lpVtbl->Release(unknown_media_source); \
\
	if (real_media_source) \
		real_media_source->lpVtbl->Release(real_media_source); \
\
	if (reader) \
		reader->lpVtbl->Release(reader); \
\
	if (byte_stream) \
		byte_stream->lpVtbl->Release(byte_stream); \
\
	if (uncompressed_type) \
		uncompressed_type->lpVtbl->Release(uncompressed_type);

int fmt_win32mf_read_info(dmoz_file_t *file, slurp_t *fp)
{
	if (!media_foundation_initialized)
		return 0;

	int success = 0;

	wchar_t *url = NULL;
	if (!file->path || charset_iconv(file->path, (uint8_t **)&url, CHARSET_UTF8, CHARSET_WCHAR_T))
		url = NULL;

	MEDIA_FOUNDATION_START(fp->data, fp->length, url, cleanup)

	file->smp_flags = flags;
	file->smp_speed = sps;

	convert_media_foundation_metadata(real_media_source, file);
	get_source_reader_information(reader, file);

	success = 1;

cleanup:
	MEDIA_FOUNDATION_END()

	return success;
}

enum {
	READER_LOAD_DONE,
	READER_LOAD_ERROR,
	READER_LOAD_MORE
};

/* uncompressed MUST point to NULL (or some other allocated memory) before the first pass! */
static int reader_load_sample(IMFSourceReader *reader, uint8_t **uncompressed, size_t *size)
{
	if (!reader)
		return READER_LOAD_ERROR;

	int success = READER_LOAD_MORE;

	IMFSample *sample = NULL;
	DWORD sample_flags = 0;

	IMFMediaBuffer *buffer = NULL;
	if (FAILED(reader->lpVtbl->ReadSample(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &sample_flags, NULL, &sample))) {
		success = READER_LOAD_ERROR;
		goto cleanup;
	}

	if (sample_flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED || sample_flags & MF_SOURCE_READERF_ENDOFSTREAM) {
		success = READER_LOAD_DONE;
		goto cleanup;
	}

	if (FAILED(sample->lpVtbl->ConvertToContiguousBuffer(sample, &buffer))) {
		success = READER_LOAD_ERROR;
		goto cleanup;
	}

	BYTE *buffer_data = NULL;
	DWORD buffer_data_size = 0;

	if (FAILED(buffer->lpVtbl->Lock(buffer, &buffer_data, NULL, &buffer_data_size))) {
		success = READER_LOAD_ERROR;
		goto cleanup;
	}

	{
		uint8_t *orig = *uncompressed;
		*uncompressed = realloc(orig, *size + buffer_data_size);
		if (!*uncompressed) {
			free(orig);
			success = READER_LOAD_ERROR;
			goto cleanup;
		}

		memcpy(*uncompressed + *size, buffer_data, buffer_data_size);

		*size += buffer_data_size;
	}

	if (FAILED(buffer->lpVtbl->Unlock(buffer))) {
		success = READER_LOAD_ERROR;
		goto cleanup;
	}

cleanup:
	if (sample)
		sample->lpVtbl->Release(sample);

	if (buffer)
		buffer->lpVtbl->Release(buffer);

	return success;
}

int fmt_win32mf_load_sample(slurp_t *fp, song_sample_t *smp)
{
	if (!media_foundation_initialized)
		return 0;

	int success = 0;
	uint8_t *uncompressed = NULL;
	size_t uncompressed_size = 0, sample_length = 0;

	MEDIA_FOUNDATION_START(fp->data, fp->length, NULL, cleanup)

	for (;;) {
		int loop_success = reader_load_sample(reader, &uncompressed, &uncompressed_size);
		if (loop_success == READER_LOAD_ERROR)
			goto cleanup;

		if (loop_success == READER_LOAD_DONE)
			break;

		if (uncompressed_size / channels / (bps / 8) > MAX_SAMPLE_LENGTH)
			break; /* punt */
	}

	sample_length = uncompressed_size / channels / (bps / 8);
	if (sample_length < 1 || sample_length > MAX_SAMPLE_LENGTH)
		goto cleanup;

	smp->volume        = 64 * 4;
	smp->global_volume = 64;
	smp->c5speed       = sps;
	smp->length        = sample_length;

	success = csf_read_sample(smp, flags, uncompressed, uncompressed_size);

cleanup:
	MEDIA_FOUNDATION_END()

	if (uncompressed)
		free(uncompressed);

	return success;
}