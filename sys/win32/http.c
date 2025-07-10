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

#include "http.h"
#include "mem.h"
#include "disko.h"
#include "loadso.h"

#ifdef SCHISM_WIN32
# include <windows.h>
# include <wininet.h>
#endif

/* WinINet HTTP backend
 *
 * This library is ancient and available since Win95(?).
 * Even so, I'm going to dynamically load it so that there's no
 * explicit dependency on it. */

typedef HINTERNET (WINAPI *WININET_InternetOpenASpec)(LPCSTR,DWORD,LPCSTR,LPCSTR,DWORD);
typedef HINTERNET (WINAPI *WININET_InternetConnectASpec)(HINTERNET,LPCSTR,INTERNET_PORT,LPCSTR,LPCSTR,DWORD,DWORD,DWORD_PTR);
typedef HINTERNET (WINAPI *WININET_HttpOpenRequestASpec)(HINTERNET,LPCSTR,LPCSTR,LPCSTR,LPCSTR,LPCSTR,DWORD,DWORD_PTR);
typedef BOOL (WINAPI *WININET_HttpSendRequestASpec)(HINTERNET,LPCSTR,DWORD,LPVOID,DWORD);
typedef BOOL (WINAPI *WININET_InternetQueryDataAvailableSpec)(HINTERNET, LPDWORD, DWORD, DWORD_PTR);
typedef BOOL (WINAPI *WININET_InternetCloseHandleSpec)(HINTERNET);
typedef BOOL (WINAPI *WININET_InternetReadFileSpec)(HINTERNET, LPVOID, DWORD, LPDWORD);

struct http_wininet_data {
	HINTERNET io; /* InternetOpen() handle */

	void *lib_wininet;
	WININET_InternetOpenASpec WININET_InternetOpenA;
	WININET_InternetConnectASpec WININET_InternetConnectA;
	WININET_HttpOpenRequestASpec WININET_HttpOpenRequestA;
	WININET_HttpSendRequestASpec WININET_HttpSendRequestA;
	WININET_InternetQueryDataAvailableSpec WININET_InternetQueryDataAvailable;
	WININET_InternetCloseHandleSpec WININET_InternetCloseHandle;
	WININET_InternetReadFileSpec WININET_InternetReadFile;
};

static void http_wininet_destroy_(struct http *r)
{
	struct http_wininet_data *wi;

	wi = r->data;
	if (!wi)
		return;

	if (wi->io)
		wi->WININET_InternetCloseHandle(wi->io);
}

static int http_wininet_send_request_(struct http *r, const char *domain,
	const char *path, uint32_t reqflags, http_request_cb_spec cb,
	void *userdata)
{
	HINTERNET ic;
	HINTERNET hreq;
	disko_t ds;
	DWORD size;
	struct http_wininet_data *wi;

	wi = r->data;
	if (!wi)
		return -5;

	/* I'm not gonna bother handling UTF-8 correctly here.
	 * HTTPS is on port 443, HTTP is on port 80 (usually...) */
	ic = wi->WININET_InternetConnectA(wi->io, domain,
		(reqflags & HTTP_REQ_SSL) ? 443 : 80, NULL, NULL,
		INTERNET_SERVICE_HTTP, 0, 0);
	if (!ic)
		return -1;

	hreq = wi->WININET_HttpOpenRequestA(ic, "GET", path, "HTTP/1.1", NULL,
		NULL, (reqflags & HTTP_REQ_SSL) ? INTERNET_FLAG_SECURE : 0, 0);
	if (!hreq) {
		wi->WININET_InternetCloseHandle(ic);
		return -2;
	}

	if (!wi->WININET_HttpSendRequestA(hreq, NULL, 0, NULL, 0)) {
		wi->WININET_InternetCloseHandle(hreq);
		wi->WININET_InternetCloseHandle(ic);
		return -3;
	}

	if (wi->WININET_InternetQueryDataAvailable
		&& wi->WININET_InternetQueryDataAvailable(hreq, &size, 0, 0)) {
		disko_memopen_estimate(&ds, size + 4); /* NUL bytes at the end */
	} else {
		disko_memopen(&ds);
	}

	for (;;) {
		DWORD wrt;
		char buffer[4096]; /* this should be big enough */

		if (!wi->WININET_InternetReadFile(hreq, buffer, sizeof(buffer),
				&wrt)) {
			disko_memclose(&ds, 0);
			wi->WININET_InternetCloseHandle(hreq);
			wi->WININET_InternetCloseHandle(ic);
			return -4;
		}

		if (!wrt)
			break;

		disko_write(&ds, buffer, sizeof(buffer));
	}

	disko_write(&ds, "\0\0\0\0", 4);

	cb(ds.data, ds.length - 4, userdata);

	disko_memclose(&ds, 0);

	wi->WININET_InternetCloseHandle(hreq);
	wi->WININET_InternetCloseHandle(ic);

	return 0;
}

int http_wininet_init(struct http *r)
{
	struct http_wininet_data *data;

	data = mem_calloc(1, sizeof(*data));

#define LOAD_OBJECT(x) \
	data->lib_##x = loadso_object_load(#x ".dll"); \
	if (!data->lib_##x) goto err

#define LOAD_SYMBOL(libl, libu, x) \
	data->libu##_##x = (libu##_##x##Spec)loadso_function_load(data->lib_##libl, #x); \
	if (!data->libu##_##x) goto err

#define LOAD_SYMBOL_OPT(libl, libu, x) \
	data->libu##_##x = (libu##_##x##Spec)loadso_function_load(data->lib_##libl, #x);

	LOAD_OBJECT(wininet);

	LOAD_SYMBOL(wininet, WININET, InternetOpenA);
	LOAD_SYMBOL(wininet, WININET, InternetConnectA);
	LOAD_SYMBOL(wininet, WININET, HttpOpenRequestA);
	LOAD_SYMBOL(wininet, WININET, HttpSendRequestA);
	LOAD_SYMBOL(wininet, WININET, InternetCloseHandle);
	LOAD_SYMBOL(wininet, WININET, InternetReadFile);
	/* don't need this but it's nice to have */
	LOAD_SYMBOL_OPT(wininet, WININET, InternetQueryDataAvailable);

#undef LOAD_OBJECT
#undef LOAD_SYMBOL

	data->io = data->WININET_InternetOpenA(HTTP_USER_AGENT,
		INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
	if (!data->io)
		goto err;

	r->data = data;
	r->send_request_ = http_wininet_send_request_;
	r->destroy_ = http_wininet_destroy_;

	return 0;

err:
	if (data->lib_wininet)
		loadso_object_unload(data->lib_wininet);

	free(data);

	return -1;
}
