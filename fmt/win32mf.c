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

#define COBJMACROS // Get C object macros

#include "headers.h"

#include "threads.h"
#include "charset.h"
#include "fmt.h"
#include "util.h"

/* we want constant vtables */
#define CONST_VTABLE

/* include these first */
#include <windows.h>
#include <propsys.h>
#include <mediaobj.h>

/* define the GUIDs here; we don't use stuff outside here anyway */
#include <initguid.h>
#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <shlwapi.h>
#include <propvarutil.h>

/* Dynamically loaded Media Foundation functions, to keep Schism working with Windows XP */

typedef HRESULT (WINAPI *MF_MFStartupSpec)(ULONG version, DWORD flags);
typedef HRESULT (WINAPI *MF_MFShutdownSpec)(void);
typedef HRESULT (WINAPI *MF_MFCreateSourceResolverSpec)(IMFSourceResolver **ppISourceResolver);
typedef HRESULT (WINAPI *MF_MFGetServiceSpec)(IUnknown *punkObject, REFGUID guidService, REFIID riid, LPVOID *ppvObject);
typedef HRESULT (WINAPI *MF_PropVariantToStringAllocSpec)(REFPROPVARIANT propvar, PWSTR *ppszOut);
typedef HRESULT (WINAPI *MF_PropVariantToUInt64Spec)(REFPROPVARIANT propvar, ULONGLONG *pullRet);
typedef HRESULT (WINAPI *MF_PropVariantClearSpec)(PROPVARIANT *pvar);
typedef HRESULT (WINAPI *MF_MFCreateSourceReaderFromMediaSourceSpec)(IMFMediaSource *pMediaSource, IMFAttributes *pAttributes, IMFSourceReader **ppSourceReader);
typedef HRESULT (WINAPI *MF_MFCreateMediaTypeSpec)(IMFMediaType **ppMFType);
typedef HRESULT (WINAPI *MF_MFCreateAsyncResultSpec)(IUnknown *punkObject, IMFAsyncCallback *pCallback, IUnknown *punkState, IMFAsyncResult **ppAsyncResult);
typedef HRESULT (WINAPI *MF_MFPutWorkItemSpec)(DWORD dwQueue, IMFAsyncCallback *pCallback, IUnknown *pState);
typedef HRESULT (WINAPI *MF_MFInvokeCallbackSpec)(IMFAsyncResult *pAsyncResult);
typedef HRESULT (WINAPI *MF_QISearchSpec)(void     *that,LPCQITAB pqit,REFIID   riid,void     **ppv);
typedef HRESULT (WINAPI *MF_CoInitializeExSpec)(LPVOID pvReserved, DWORD  dwCoInit);
typedef void (WINAPI *MF_CoUninitializeSpec)(void);
typedef void (WINAPI *MF_CoTaskMemFreeSpec)(LPVOID pv);

static MF_MFStartupSpec MF_MFStartup;
static MF_MFShutdownSpec MF_MFShutdown;
static MF_MFCreateSourceResolverSpec MF_MFCreateSourceResolver;
static MF_MFGetServiceSpec MF_MFGetService;
static MF_PropVariantToStringAllocSpec MF_PropVariantToStringAlloc;
static MF_PropVariantToUInt64Spec MF_PropVariantToUInt64;
static MF_PropVariantClearSpec MF_PropVariantClear;
static MF_MFCreateSourceReaderFromMediaSourceSpec MF_MFCreateSourceReaderFromMediaSource;
static MF_MFCreateMediaTypeSpec MF_MFCreateMediaType;
static MF_MFCreateAsyncResultSpec MF_MFCreateAsyncResult;
static MF_MFPutWorkItemSpec MF_MFPutWorkItem;
static MF_MFInvokeCallbackSpec MF_MFInvokeCallback;
static MF_QISearchSpec MF_QISearch;
static MF_CoInitializeExSpec MF_CoInitializeEx;
static MF_CoUninitializeSpec MF_CoUninitialize;
static MF_CoTaskMemFreeSpec MF_CoTaskMemFree;

static int media_foundation_initialized = 0;

/* forward declare this */
static HRESULT STDMETHODCALLTYPE mfbytestream_ReadAtPosition(IMFByteStream *This, BYTE *pb, ULONG cb, ULONG *pcbRead, int64_t pos);

/* --------------------------------------------------------------------- */
/* Florida man uses C++ constructs in C and gets brutally murdered by
 * Linus Torvalds himself */

struct slurp_async_op {
	/* vtable */
	CONST_VTBL IUnknownVtbl *lpvtbl;

	/* things we have to free, or whatever */
	IMFAsyncCallback *cb;
	IMFByteStream *bs;

	/* stuff */
	BYTE *buffer;
	ULONG req_length;
	ULONG actual_length;
	int64_t pos;

	long ref_cnt;
};

static HRESULT STDMETHODCALLTYPE slurp_async_op_QueryInterface(IUnknown *This, REFIID riid, void **ppvobj)
{
	static const QITAB qit[] = {
		{&IID_IUnknown, 0},
		{NULL, 0},
	};

	return MF_QISearch(This, qit, riid, ppvobj);
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

static inline int slurp_async_op_new(IUnknown **This, IMFByteStream *bs, IMFAsyncCallback *cb, BYTE *buffer, ULONG req_length, int64_t pos)
{
	struct slurp_async_op *op = calloc(1, sizeof(struct slurp_async_op));
	if (!op)
		return 0;

	op->lpvtbl = &slurp_async_op_vtbl;

	/* user provided info... */
	op->bs = bs;
	op->cb = cb;
	op->buffer = buffer;
	op->req_length = req_length;
	op->pos = pos;

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

	return MF_QISearch(This, qit, riid, ppobj);
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
	struct slurp_async_op *op;
	IUnknown *pState = NULL;
	IUnknown *pUnk = NULL;
	IMFAsyncResult *caller = NULL;
	HRESULT hr = S_OK;

	hr = IMFAsyncResult_GetState(pAsyncResult, &pState);
	if (FAILED(hr))
		goto done;

	hr = IUnknown_QueryInterface(pState, &IID_IMFAsyncResult, (void **)&caller);
	if (FAILED(hr))
		goto done;

	hr = IMFAsyncResult_GetObject(caller, &pUnk);
	if (FAILED(hr))
		goto done;

	op = (struct slurp_async_op *)pUnk;

	/* now we can actually do the work */
	hr = mfbytestream_ReadAtPosition(op->bs, op->buffer, op->req_length, &op->actual_length, op->pos);

done:
	if (caller) {
		IMFAsyncResult_SetStatus(caller, hr);
		MF_MFInvokeCallback(caller);
	}

	if (pState)
		IUnknown_Release(pState);

	if (pUnk)
		IUnknown_Release(pUnk);

	return S_OK;
}

static const IMFAsyncCallbackVtbl slurp_async_callback_vtbl = {
	/* IUnknown */
	.QueryInterface = slurp_async_callback_QueryInterface,
	.AddRef = slurp_async_callback_AddRef,
	.Release = slurp_async_callback_Release,

	/* IMFAsyncCallback */
	.GetParameters = slurp_async_callback_GetParameters,
	.Invoke = slurp_async_callback_Invoke,
};

static inline int slurp_async_callback_new(IMFAsyncCallback **This)
{
	struct slurp_async_callback *cb = calloc(1, sizeof(struct slurp_async_callback));
	if (!cb)
		return 0;

	cb->lpvtbl = &slurp_async_callback_vtbl;

	cb->ref_cnt = 1;

	*This = (IMFAsyncCallback *)cb;
	return 1;
}

/* ----------------------------------------------------------------------------------------- */

/* IMFByteStream implementation for slurp */
struct mfbytestream {
	/* IMFByteStream vtable */
	CONST_VTBL IMFByteStreamVtbl *lpvtbl;

	/* our stuff... */
	slurp_t *fp;
	mt_mutex_t *mutex;
	long ref_cnt;
};

/* IUnknown methods */
static HRESULT STDMETHODCALLTYPE mfbytestream_QueryInterface(IMFByteStream *This, REFIID riid, void **ppobj)
{
	static const QITAB qit[] = {
		{&IID_IMFByteStream, 0},
		{NULL, 0},
	};

	return MF_QISearch(This, qit, riid, ppobj);
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
		mt_mutex_delete(mfb->mutex);
		free(mfb);
	}

	return ref;
}

/* IMFByteStream methods */
static HRESULT STDMETHODCALLTYPE mfbytestream_GetCapabilities(SCHISM_UNUSED IMFByteStream* This, DWORD *pdwCapabilities)
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
	struct mfbytestream *mfb = (struct mfbytestream *)This;

	mt_mutex_lock(mfb->mutex);

	slurp_seek(mfb->fp, qwPosition, SEEK_SET);

	mt_mutex_unlock(mfb->mutex);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_GetCurrentPosition(IMFByteStream *This, QWORD *pqwPosition)
{
	struct mfbytestream *mfb = (struct mfbytestream *)This;

	mt_mutex_lock(mfb->mutex);

	int64_t pos = slurp_tell(mfb->fp);

	mt_mutex_unlock(mfb->mutex);

	if (pos < 0)
		return E_FAIL;

	*pqwPosition = pos;
	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_IsEndOfStream(IMFByteStream *This, WINBOOL *pfEndOfStream)
{
	struct mfbytestream *mfb = (struct mfbytestream *)This;

	mt_mutex_lock(mfb->mutex);

	*pfEndOfStream = slurp_eof(mfb->fp);

	mt_mutex_unlock(mfb->mutex);

	return S_OK;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_Read(IMFByteStream *This, BYTE *pb, ULONG cb, ULONG *pcbRead)
{
	struct mfbytestream *mfb = (struct mfbytestream *)This;

	mt_mutex_lock(mfb->mutex);

	*pcbRead = slurp_read(mfb->fp, pb, cb);

	mt_mutex_unlock(mfb->mutex);

	return S_OK;
}

/* this is only used internally */
static HRESULT STDMETHODCALLTYPE mfbytestream_ReadAtPosition(IMFByteStream *This, BYTE *pb, ULONG cb, ULONG *pcbRead, int64_t pos)
{
	struct mfbytestream *mfb = (struct mfbytestream *)This;
	HRESULT hr = S_OK;

	*pcbRead = 0;

	mt_mutex_lock(mfb->mutex);

	/* cache the old position */
	int64_t old_pos = slurp_tell(mfb->fp);
	if (old_pos < 0) {
		/* what ? */
		hr = E_FAIL;
		goto done;
	}

	slurp_seek(mfb->fp, pos, SEEK_SET);

	*pcbRead = slurp_read(mfb->fp, pb, cb);

	/* seek back to the old position */
	slurp_seek(mfb->fp, old_pos, SEEK_SET);

done:
	mt_mutex_unlock(mfb->mutex);

	return hr;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_BeginRead(IMFByteStream *This, BYTE *pb, ULONG cb, IMFAsyncCallback *pCallback, IUnknown *punkState)
{
	IUnknown *op = NULL;
	IMFAsyncCallback* callback = NULL;
	IMFAsyncResult *result = NULL;
	QWORD pos;
	HRESULT hr = S_OK;

	if (FAILED(hr = mfbytestream_GetCurrentPosition(This, &pos)))
		return hr;

	if (FAILED(hr = mfbytestream_SetCurrentPosition(This, pos + cb)))
		return hr;

	if (!slurp_async_callback_new(&callback))
		return E_OUTOFMEMORY;

	if (!slurp_async_op_new(&op, This, callback, pb, cb, pos))
		return E_OUTOFMEMORY;

	if (FAILED(hr = MF_MFCreateAsyncResult(op, pCallback, punkState, &result)))
		goto fail;

	hr = MF_MFPutWorkItem(MFASYNC_CALLBACK_QUEUE_STANDARD, callback, (IUnknown *)result);

fail:
	if (result)
		IMFAsyncResult_Release(result);

	return hr;
}

static HRESULT STDMETHODCALLTYPE mfbytestream_EndRead(IMFByteStream *This, IMFAsyncResult *pResult, ULONG *pcbRead)
{
	*pcbRead = 0;

	struct slurp_async_op *op = NULL;

	HRESULT hr = IMFAsyncResult_GetStatus(pResult);
	if (FAILED(hr))
		goto done;

	hr = IMFAsyncResult_GetObject(pResult, (IUnknown **)&op);
	if (FAILED(hr))
		goto done;

	*pcbRead = op->actual_length;

done:
	if (op) {
		/* need to deal with this crap */
		if (op->cb)
			IMFAsyncCallback_Release(op->cb);
		IUnknown_Release((IUnknown *)op);
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
	struct mfbytestream *mfb = (struct mfbytestream *)This;
	int whence;

	// I don't know how to support this
	if (dwSeekFlags & MFBYTESTREAM_SEEK_FLAG_CANCEL_PENDING_IO)
		return E_NOTIMPL;

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

	mt_mutex_lock(mfb->mutex);

	slurp_seek(mfb->fp, llSeekOffset, whence);

	*pqwCurrentPosition = slurp_tell(mfb->fp);

	mt_mutex_unlock(mfb->mutex);

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
	mfb->lpvtbl = &mfbytestream_vtbl;

	/* whatever */
	mfb->fp = fp;
	mfb->mutex = mt_mutex_create();
	if (!mfb->mutex) {
		free(mfb);
		return 0;
	}
	mfb->ref_cnt = 1;

	*imf = (IMFByteStream *)mfb;
	return 1;
}

/* -------------------------------------------------------------- */

/* I really have no idea what (apartment/multi)threading does. I don't
 * think it really has an impact, either */
#define COM_INITFLAGS (COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE)

static const char* get_media_type_description(IMFMediaType* media_type)
{
	static const struct {
		const GUID *guid;
		const char *description;
	} guids[] = {
		// no RealAudio support? for shame, Microsoft.
		{&MFAudioFormat_AAC, "Advanced Audio Coding"},
		// {&MFAudioFormat_ADTS, "Not used"},
		{&MFAudioFormat_ALAC, "Apple Lossless Audio Codec"},
		{&MFAudioFormat_AMR_NB, "Adaptive Multi-Rate"},
		{&MFAudioFormat_AMR_WB, "Adaptive Multi-Rate Wideband"},
		{&MFAudioFormat_Dolby_AC3, "Dolby Digital (AC-3)"},
		{&MFAudioFormat_Dolby_AC3_SPDIF, "Dolby AC-3 over S/PDIF"},
		{&MFAudioFormat_Dolby_DDPlus, "Dolby Digital Plus"},
		{&MFAudioFormat_DRM, "Encrypted audio data"}, // ????
		{&MFAudioFormat_DTS, "Digital Theater Systems"}, // yes, "theater", not "theatre"
		{&MFAudioFormat_FLAC, "Free Lossless Audio Codec"},
		{&MFAudioFormat_Float, "Uncompressed floating-point audio"},
		{&MFAudioFormat_Float_SpatialObjects, "Uncompressed floating-point audio"},
		{&MFAudioFormat_MP3, "MPEG Layer-3 (MP3)"},
		{&MFAudioFormat_MPEG, "MPEG-1 audio payload"},
		{&MFAudioFormat_MSP1, "Windows Media Audio 9 Voice"},
		{&MFAudioFormat_Opus, "Opus"},
		{&MFAudioFormat_PCM, "Uncompressed PCM"},
		// {&MFAudioFormat_QCELP, "QCELP audio"}, // can't get this to compile... 
		{&MFAudioFormat_WMASPDIF, "Windows Media Audio 9 over S/PDIF"},
		{&MFAudioFormat_WMAudio_Lossless, "Windows Media Audio 9 Lossless"},
		{&MFAudioFormat_WMAudioV8, "Windows Media Audio 8"},
		{&MFAudioFormat_WMAudioV9, "Windows Media Audio 8"},
	};

	GUID subtype;
	if (SUCCEEDED(IMFMediaType_GetGUID(media_type, &MF_MT_SUBTYPE, &subtype))) {
		for (size_t i = 0; i < ARRAY_SIZE(guids); i++)
			if (IsEqualGUID(&subtype, guids[i].guid))
				return guids[i].description;

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

	if (SUCCEEDED(IMFSourceReader_GetNativeMediaType(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, MF_SOURCE_READER_CURRENT_TYPE_INDEX, &media_type))) {
		file->type = (SUCCEEDED(IMFMediaType_IsCompressedFormat(media_type, &compressed)) && compressed)
			? TYPE_SAMPLE_COMPR
			: TYPE_SAMPLE_PLAIN;

		file->description = get_media_type_description(media_type);

		IMFMediaType_Release(media_type);
	}

	return 1;
}

static int convert_media_foundation_metadata(IMFMediaSource* source, dmoz_file_t* file)
{
	IMFPresentationDescriptor *descriptor = NULL;
	IMFMetadataProvider *provider = NULL;
	IMFMetadata *metadata = NULL;
	PROPVARIANT propnames = {0};
	DWORD streams = 0;
	int found = 0;
	uint64_t duration = 0;

	/* do this before anything else... */
	PropVariantInit(&propnames);

	if (FAILED(IMFMediaSource_CreatePresentationDescriptor(source, &descriptor)))
		goto cleanup;

	if (SUCCEEDED(IMFPresentationDescriptor_GetUINT64(descriptor, &MF_PD_DURATION, &duration)))
		file->smp_length = (double)duration * file->smp_speed / (10.0 * 1000.0 * 1000.0);

	if (FAILED(MF_MFGetService((IUnknown*)source, &MF_METADATA_PROVIDER_SERVICE, &IID_IMFMetadataProvider, (void**)&provider)))
		goto cleanup;

	if (FAILED(IMFMetadataProvider_GetMFMetadata(provider, descriptor, 0, 0, &metadata)))
		goto cleanup;

	if (FAILED(IMFMetadata_GetAllPropertyNames(metadata, &propnames)))
		goto cleanup;

	for (DWORD prop_index = 0; prop_index < propnames.calpwstr.cElems; prop_index++) {
		LPWSTR prop_name = propnames.calpwstr.pElems[prop_index];
		if (!prop_name)
			continue; /* ... */

		if (!wcscmp(prop_name, L"Title")) {
			PROPVARIANT propval = {0};
			PropVariantInit(&propval);

			if (FAILED(IMFMetadata_GetProperty(metadata, prop_name, &propval))) {
				MF_PropVariantClear(&propval);
				continue;
			}

			LPWSTR prop_val_str = NULL;
			if (FAILED(MF_PropVariantToStringAlloc(&propval, &prop_val_str))) {
				MF_PropVariantClear(&propval);
				continue;
			}

			found = 1;

			charset_iconv(prop_val_str, file->title, CHARSET_WCHAR_T, CHARSET_CP437, SIZE_MAX);

			MF_CoTaskMemFree(prop_val_str);
			MF_PropVariantClear(&propval);
		}
	}

cleanup:
	MF_PropVariantClear(&propnames);

	if (descriptor)
		IMFPresentationDescriptor_Release(descriptor);

	if (provider)
		IMFMetadataProvider_Release(provider);

	if (metadata)
		IMFMetadata_Release(metadata);

	return found;
}

/* ---------------------------------------------------------------- */

struct win32mf_data {
	/* wew */
	IMFSourceReader *reader;
	IMFMediaSource *source;
	IMFByteStream *byte_stream;

	/* info about the output audio data */
	uint32_t flags, sps, bps, channels;
};

static int win32mf_start(struct win32mf_data *data, slurp_t *fp, wchar_t *url)
{
	int success = 0;
	IMFSourceResolver *resolver = NULL;
	IUnknown *unknown_media_source = NULL;
	IMFMediaType *uncompressed_type = NULL;
	MF_OBJECT_TYPE object_type = MF_OBJECT_INVALID;

	if (FAILED(MF_MFCreateSourceResolver(&resolver)))
		goto cleanup;

	if (!mfbytestream_new(&data->byte_stream, fp))
		goto cleanup;

	if (FAILED(IMFSourceResolver_CreateObjectFromByteStream(resolver, data->byte_stream, url, MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE | MF_RESOLUTION_READ, NULL, &object_type, &unknown_media_source)))
		goto cleanup;

	if (object_type != MF_OBJECT_MEDIASOURCE)
		goto cleanup;

	if (FAILED(IUnknown_QueryInterface(unknown_media_source, &IID_IMFMediaSource, (void**)&data->source)))
		goto cleanup;

	if (FAILED(MF_MFCreateSourceReaderFromMediaSource(data->source, NULL, &data->reader)))
		goto cleanup;

	if (FAILED(MF_MFCreateMediaType(&uncompressed_type)))
		goto cleanup;

	if (FAILED(IMFMediaType_SetGUID(uncompressed_type, &MF_MT_MAJOR_TYPE, &MFMediaType_Audio)))
		goto cleanup;

	if (FAILED(IMFMediaType_SetGUID(uncompressed_type, &MF_MT_SUBTYPE, &MFAudioFormat_PCM)))
		goto cleanup;

	if (FAILED(IMFSourceReader_SetCurrentMediaType(data->reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, uncompressed_type)))
		goto cleanup;

	IMFMediaType_Release(uncompressed_type);

	if (FAILED(IMFSourceReader_GetCurrentMediaType(data->reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, &uncompressed_type)))
		goto cleanup;

	if (FAILED(IMFSourceReader_SetStreamSelection(data->reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE)))
		goto cleanup;

	/* get output audio track data */
	if (FAILED(IMFMediaType_GetUINT32(uncompressed_type, &MF_MT_AUDIO_NUM_CHANNELS, &data->channels)))
		goto cleanup;

	if (FAILED(IMFMediaType_GetUINT32(uncompressed_type, &MF_MT_AUDIO_SAMPLES_PER_SECOND, &data->sps)))
		goto cleanup;

	if (FAILED(IMFMediaType_GetUINT32(uncompressed_type, &MF_MT_AUDIO_BITS_PER_SAMPLE, &data->bps)))
		goto cleanup;

	if (data->sps <= 0)
		goto cleanup;

	data->flags = SF_LE | SF_PCMS;

	switch (data->channels) {
	case 1:  data->flags |= SF_M;  break;
	case 2:  data->flags |= SF_SI; break;
	default: goto cleanup;
	}

	switch (data->bps) {
	case 8:  data->flags |= SF_8;  break;
	case 16: data->flags |= SF_16; break;
	case 24: data->flags |= SF_24; break;
	case 32: data->flags |= SF_32; break;
	default: goto cleanup;
	}

	success = 1;

cleanup:
	if (resolver)
		IMFSourceResolver_Release(resolver);

	if (unknown_media_source)
		IUnknown_Release(unknown_media_source);

	if (uncompressed_type)
		IMFMediaType_Release(uncompressed_type);

	return success;
}

static void win32mf_end(struct win32mf_data *data)
{
	if (data->reader)
		IMFSourceReader_Release(data->reader);

	if (data->source)
		IMFMediaSource_Release(data->source);

	if (data->byte_stream)
		IMFByteStream_Release(data->byte_stream);
}

int fmt_win32mf_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct win32mf_data data = {0};
	wchar_t *url = NULL;

	if (!media_foundation_initialized)
		return 0;

	if (!file->path || charset_iconv(file->path, &url, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
		url = NULL;

	if (!win32mf_start(&data, fp, url)) {
		win32mf_end(&data);
		return 0;
	}

	file->smp_flags = data.flags;
	file->smp_speed = data.sps;

	convert_media_foundation_metadata(data.source, file);
	get_source_reader_information(data.reader, file);

	win32mf_end(&data);

	return 1;
}

enum {
	READER_LOAD_DONE,
	READER_LOAD_ERROR,
	READER_LOAD_MORE
};

/* uncompressed MUST point to NULL (or some other allocated memory) before the first pass! */
static int reader_load_sample(IMFSourceReader *reader, disko_t *ds)
{
	if (!reader)
		return READER_LOAD_ERROR;

	int success = READER_LOAD_MORE;

	IMFSample *sample = NULL;
	DWORD sample_flags = 0;

	IMFMediaBuffer *buffer = NULL;
	if (FAILED(IMFSourceReader_ReadSample(reader, MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &sample_flags, NULL, &sample))) {
		success = READER_LOAD_ERROR;
		goto cleanup;
	}

	if (sample_flags & MF_SOURCE_READERF_CURRENTMEDIATYPECHANGED || sample_flags & MF_SOURCE_READERF_ENDOFSTREAM) {
		success = READER_LOAD_DONE;
		goto cleanup;
	}

	if (FAILED(IMFSample_ConvertToContiguousBuffer(sample, &buffer))) {
		success = READER_LOAD_ERROR;
		goto cleanup;
	}

	BYTE *buffer_data = NULL;
	DWORD buffer_data_size = 0;

	if (FAILED(IMFMediaBuffer_Lock(buffer, &buffer_data, NULL, &buffer_data_size))) {
		success = READER_LOAD_ERROR;
		goto cleanup;
	}

	disko_write(ds, buffer_data, buffer_data_size);

	if (FAILED(IMFMediaBuffer_Unlock(buffer))) {
		success = READER_LOAD_ERROR;
		goto cleanup;
	}

cleanup:
	if (sample)
		IMFSample_Release(sample);

	if (buffer)
		IMFMediaBuffer_Release(buffer);

	return success;
}

int fmt_win32mf_load_sample(slurp_t *fp, song_sample_t *smp)
{
	if (!media_foundation_initialized)
		return 0;

	disko_t ds = {0};
	if (disko_memopen(&ds) < 0)
		return 0;

	struct win32mf_data data = {0};

	if (!win32mf_start(&data, fp, NULL)) {
		win32mf_end(&data);
		disko_memclose(&ds, 0);
		return 0;
	}

	for (;;) {
		int loop_success = reader_load_sample(data.reader, &ds);
		if (loop_success == READER_LOAD_DONE)
			break;

		if (loop_success == READER_LOAD_ERROR ||
			(ds.length / data.channels / (data.bps / 8) > MAX_SAMPLE_LENGTH)) {
			win32mf_end(&data);
			disko_memclose(&ds, 0);
			return 0;
		}
	}

	uint32_t sample_length = ds.length / data.channels / (data.bps / 8);
	if (sample_length < 1 || sample_length > MAX_SAMPLE_LENGTH) {
		win32mf_end(&data);
		disko_memclose(&ds, 0);
		return 0;
	}

	smp->volume        = 64 * 4;
	smp->global_volume = 64;
	smp->c5speed       = data.sps;
	smp->length        = sample_length;

	slurp_t fake_fp;
	slurp_memstream(&fake_fp, ds.data, ds.length);

	int r = csf_read_sample(smp, data.flags, &fake_fp);

	disko_memclose(&ds, 0);

	win32mf_end(&data);

	return r;
}

/* ----------------------------------------------------------- */

static HMODULE lib_ole32 = NULL;
static HMODULE lib_shlwapi = NULL;
static HMODULE lib_mf = NULL;
static HMODULE lib_mfplat = NULL;
static HMODULE lib_mfreadwrite = NULL;
static HMODULE lib_propsys = NULL;

/* needs to be called once on startup */
int win32mf_init(void)
{
	int com_initialized = 0;
#ifdef SCHISM_MF_DEBUG
#define DEBUG_PUTS(x) puts(x)
#else
#define DEBUG_PUTS(x)
#endif

#define LOAD_MF_LIBRARY(o) \
	do { \
		lib_ ## o = LoadLibraryA(#o ".dll"); \
		if (!(lib_ ## o)) { DEBUG_PUTS("Failed to load library " #o "!"); goto fail; } \
	} while (0)

#define LOAD_MF_OBJECT(o, x) \
	do { \
		MF_##x = (MF_##x##Spec)GetProcAddress(lib_ ## o, #x); \
		if (!MF_##x) { DEBUG_PUTS("Failed to load " #x " from library " #o ".dll !"); goto fail; } \
	} while (0)

	LOAD_MF_LIBRARY(ole32);

	LOAD_MF_OBJECT(ole32, CoInitializeEx);
	LOAD_MF_OBJECT(ole32, CoUninitialize);
	LOAD_MF_OBJECT(ole32, PropVariantClear);
	LOAD_MF_OBJECT(ole32, CoTaskMemFree);

	{
		HRESULT com_init = MF_CoInitializeEx(NULL, COM_INITFLAGS);
		com_initialized = (com_init == S_OK || com_init == S_FALSE || com_init == RPC_E_CHANGED_MODE);
	}
	if (!com_initialized) {
		DEBUG_PUTS("Failed to initialize COM!");
		goto fail;
	}

	LOAD_MF_LIBRARY(shlwapi);

	LOAD_MF_OBJECT(shlwapi, QISearch);

	LOAD_MF_LIBRARY(mf);

	LOAD_MF_OBJECT(mf, MFGetService);
	LOAD_MF_OBJECT(mf, MFCreateSourceResolver);

	LOAD_MF_LIBRARY(mfplat);

	LOAD_MF_OBJECT(mfplat, MFCreateMediaType);
	LOAD_MF_OBJECT(mfplat, MFStartup);
	LOAD_MF_OBJECT(mfplat, MFShutdown);
	LOAD_MF_OBJECT(mfplat, MFCreateAsyncResult);
	LOAD_MF_OBJECT(mfplat, MFPutWorkItem);
	LOAD_MF_OBJECT(mfplat, MFInvokeCallback);

	LOAD_MF_LIBRARY(mfreadwrite);

	LOAD_MF_OBJECT(mfreadwrite, MFCreateSourceReaderFromMediaSource);

	LOAD_MF_LIBRARY(propsys);

	LOAD_MF_OBJECT(propsys, PropVariantToStringAlloc);
	LOAD_MF_OBJECT(propsys, PropVariantToUInt64);

	/* MFSTARTUP_LITE == no sockets */
	return (media_foundation_initialized = SUCCEEDED(MF_MFStartup(MF_VERSION, MFSTARTUP_LITE)));

fail:
	if (lib_shlwapi)
		FreeLibrary(lib_shlwapi);

	if (lib_mfplat)
		FreeLibrary(lib_mfplat);

	if (lib_mf)
		FreeLibrary(lib_mf);

	if (lib_mfreadwrite)
		FreeLibrary(lib_mfreadwrite);

	if (lib_propsys)
		FreeLibrary(lib_propsys);

	if (com_initialized && MF_CoUninitialize)
		MF_CoUninitialize();

	if (lib_ole32)
		FreeLibrary(lib_ole32);

	return 0;
}

void win32mf_quit(void)
{
	if (!media_foundation_initialized)
		return;

	if (lib_shlwapi)
		FreeLibrary(lib_shlwapi);

	if (lib_mfplat)
		FreeLibrary(lib_mfplat);

	if (lib_mf)
		FreeLibrary(lib_mf);

	if (lib_mfreadwrite)
		FreeLibrary(lib_mfreadwrite);

	if (lib_propsys)
		FreeLibrary(lib_propsys);

	if (lib_ole32)
		FreeLibrary(lib_ole32);

	MF_MFShutdown();
	MF_CoUninitialize();
}