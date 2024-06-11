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

/* Target Windows 7 */
#define WINVER 0x0601
#define _WIN32_WINNT 0x0601

#include "headers.h"
#include "charset.h"
#include "fmt.h"
#include "util.h"

#include <mfapi.h>
#include <mfidl.h>
#include <mfreadwrite.h>
#include <mferror.h>
#include <propvarutil.h>

/* declaration for mingw headers that don't have this for some reason */
HRESULT WINAPI MFCreateMFByteStreamOnStream(IStream *pStream, IMFByteStream **ppByteStream);

/* --------------------------------------------------------------------- */
/* stupid COM stuff incoming */

static char* get_media_type_description(IMFMediaType* media_type) {
	if (!media_type)
		goto unknown;

	GUID major_type;
	if (!SUCCEEDED(media_type->lpVtbl->GetMajorType(media_type, &major_type)))
		goto unknown;

	/* eventually we can handle video, but this is a good start */
	if (IsEqualGUID(&major_type, &MFMediaType_Audio)) {
		return "Some audio format...";
	} else if (IsEqualGUID(&major_type, &MFMediaType_Video)) {
		return "Some video format...";
	}

unknown:
	/* welp */
	return "Unknown format";
}

static int convert_media_foundation_metadata(IMFMediaSource* source, dmoz_file_t* file) {
	/* this API is actually the worst */
	IMFPresentationDescriptor* descriptor = NULL;
	IMFMetadataProvider* provider = NULL;
	IMFMetadata* metadata = NULL;
	PROPVARIANT propnames = {0};
	DWORD streams = 0;
	int found = 0;

	/* do this before anything else... */
	PropVariantInit(&propnames);

	if (!SUCCEEDED(source->lpVtbl->CreatePresentationDescriptor(source, &descriptor)))
		goto cleanup;

	if (!SUCCEEDED(MFGetService((IUnknown*)source, &MF_METADATA_PROVIDER_SERVICE, &IID_IMFMetadataProvider, (void**)&provider)))
		goto cleanup;

	if (!SUCCEEDED(provider->lpVtbl->GetMFMetadata(provider, descriptor, 0, 0, &metadata)))
		goto cleanup;

	if (!SUCCEEDED(metadata->lpVtbl->GetAllPropertyNames(metadata, &propnames)))
		goto cleanup;

	for (DWORD prop_index = 0; prop_index < propnames.calpwstr.cElems; prop_index++) {
		LPWSTR prop_name = propnames.calpwstr.pElems[prop_index];
		if (!prop_name)
			continue; /* ... */

		const int title = !wcscmp(prop_name, L"Title");
		const int artist = !wcscmp(prop_name, L"Artist");

		if (title || artist) {
			PROPVARIANT propval = {0};
			if (metadata->lpVtbl->GetProperty(metadata, prop_name, &propval) != S_OK)
				continue; /* ... */

			LPWSTR prop_val_str = NULL;
			if (!SUCCEEDED(PropVariantToStringAlloc(&propval, &prop_val_str))) {
				PropVariantClear(&propval);
				continue;
			}

			found = 1;

			if (title)
				charset_iconv((uint8_t*)prop_val_str, (uint8_t**)file->title, CHARSET_WCHAR_T, CHARSET_CP437);
			else if (artist)
				charset_iconv((uint8_t*)prop_val_str, (uint8_t**)file->artist, CHARSET_WCHAR_T, CHARSET_CP437);

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

/* creates a byte stream. eventually this will be shared among the
 * read info and load sample functions */
#define MEDIA_FOUNDATION_START(data, len) \
	IStream* strm = NULL; \
	IMFByteStream* bstrm = NULL; \
	strm = SHCreateMemStream(data, len); \
	if (!strm) \
		goto cleanup; \
	if (!SUCCEEDED(MFCreateMFByteStreamOnStream(strm, &bstrm))) \
		goto cleanup;

#define MEDIA_FOUNDATION_CLEANUP() \
	if (strm) \
		strm->lpVtbl->Release(strm); \
	if (bstrm) \
		bstrm->lpVtbl->Release(bstrm);

int fmt_win32mf_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
	int success = 0;
	IMFSourceResolver* resolver = NULL;
	MF_OBJECT_TYPE object_type = MF_OBJECT_INVALID;
	IUnknown* unknown_media_source = NULL;
	IMFMediaSource* real_media_source = NULL;

	MEDIA_FOUNDATION_START(data, length);

	if (!SUCCEEDED(MFCreateSourceResolver(&resolver)))
		goto cleanup;

	if (!SUCCEEDED(resolver->lpVtbl->CreateObjectFromByteStream(resolver, bstrm, NULL, MF_RESOLUTION_MEDIASOURCE | MF_RESOLUTION_CONTENT_DOES_NOT_HAVE_TO_MATCH_EXTENSION_OR_MIME_TYPE | MF_RESOLUTION_READ, NULL, &object_type, &unknown_media_source)))
		goto cleanup;

	if (object_type != MF_OBJECT_MEDIASOURCE)
		goto cleanup;

	if (!SUCCEEDED(unknown_media_source->lpVtbl->QueryInterface(unknown_media_source, &IID_IMFMediaSource, (void**)&real_media_source)))
		goto cleanup;

	convert_media_foundation_metadata(real_media_source, file);
	file->description = "Media Foundation";
	file->type = TYPE_SAMPLE_COMPR; /* not really true */
	success = 1;

cleanup:
	if (resolver)
		resolver->lpVtbl->Release(resolver);

	if (unknown_media_source)
		unknown_media_source->lpVtbl->Release(unknown_media_source);

	if (real_media_source)
		real_media_source->lpVtbl->Release(real_media_source);

	MEDIA_FOUNDATION_CLEANUP();

	return success;
}
