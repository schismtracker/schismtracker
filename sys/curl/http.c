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
#include "loadso.h"
#include "mem.h"
#include "disko.h"

#include <curl/curl.h>

struct curl_http {
#ifdef CURL_DYNAMIC_LOAD
# define LIB(name) void *dyn_##name;
#endif
#define FUNC(lib, ret, name, params) ret (*dyn_##name) params;
#include "http-funcs.h"

	CURL *c;
};

static void http_curl_destroy_(struct http *r)
{
	struct curl_http *c;

	if (!r) return;

	c = r->data;

	if (c->c && c->dyn_curl_easy_cleanup)
		c->dyn_curl_easy_cleanup(c->c);

#ifdef CURL_DYNAMIC_LOAD
# define LIB(name) if (c->dyn_##name) loadso_object_unload(c->dyn_##name);
# include "http-funcs.h"
#endif

	free(c);
}

static size_t write_callback_(char *ptr, size_t size, size_t nmemb, void *data)
{
	disko_t *ds;

	ds = data;

	if (size != 1)
		return 0; /* broken libcurl */

	disko_write(ds, ptr, nmemb);

	return nmemb;
}

static int http_curl_send_request_(struct http *r, const char *domain,
	const char *path, uint32_t reqflags, http_request_cb_spec cb,
	void *data)
{
	disko_t ds;
	CURLcode cc;
	struct curl_http *c;

	if (disko_memopen(&ds) < 0)
		return -1;

	c = r->data;

	{
		char *url;

		url = http_build_url(domain, path, reqflags);
		if (!url) goto err;

		c->dyn_curl_easy_setopt(c->c, CURLOPT_URL, url);
		free(url);
	}

	c->dyn_curl_easy_setopt(c->c, CURLOPT_USERAGENT, HTTP_USER_AGENT);

	/* Old versions of curl don't define these */
# ifndef CURLOPT_SSL_OPTIONS
#  define CURLOPT_SSL_OPTIONS 216
# endif
# ifndef CURLSSLOPT_NATIVE_CA
#  define CURLSSLOPT_NATIVE_CA 0x10
# endif
	c->dyn_curl_easy_setopt(c->c, CURLOPT_SSL_OPTIONS, (long)CURLSSLOPT_NATIVE_CA);
	/* for threading */
	c->dyn_curl_easy_setopt(c->c, CURLOPT_NOSIGNAL, 1L);

	c->dyn_curl_easy_setopt(c->c, CURLOPT_WRITEFUNCTION, &write_callback_);
	c->dyn_curl_easy_setopt(c->c, CURLOPT_WRITEDATA, &ds);

	cc = c->dyn_curl_easy_perform(c->c);
	if (cc != CURLE_OK) goto err;

	cb(ds.data, ds.length, data);

	disko_memclose(&ds, 0);

	return 0;

err:
	disko_memclose(&ds, 0);
	return -1;
}

int http_curl_init(struct http *r)
{
	struct curl_http *c;

	c = mem_calloc(1, sizeof(*c));

#ifdef CURL_DYNAMIC_LOAD
# define LIB(name) \
	c->dyn_##name = library_load(#name, 4, 0); \
	if (!c->dyn_##name) goto err;
# define FUNC(lib, ret, name, params) \
	c->dyn_##name = (ret (*) params)loadso_function_load(c->dyn_##lib, #name); \
	if (!c->dyn_##name) goto err;
#else
# define FUNC(lib, ret, name, params) \
	c->dyn_##name = name;
#endif
#include "http-funcs.h"

	/* get our CURL structure */
	c->c = c->dyn_curl_easy_init();
	if (!c->c) goto err;

	/* yay */

	r->destroy_ = http_curl_destroy_;
	r->send_request_ = http_curl_send_request_;
	r->data = c;

	return 0;

err:
	http_curl_destroy_(r);
	return -1;
}
