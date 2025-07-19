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
#include "disko.h"

#include <CoreFoundation/CoreFoundation.h>
#include <CFNetwork/CFNetwork.h>

/* We use Core Foundation here, simply because the Objective-C APIs require us
 * to build a delegator to do things asynchronously.
 *
 * NOTE: This does not properly handle HTTP redirect codes! */

static void http_macosx_destroy_(struct http *r)
{
	/* nothing */
}

static int http_macosx_send_request_(struct http *r, const char *host,
	const char *path, uint16_t port, uint32_t reqflags,
	http_request_cb_spec cb, void *userdata)
{
	CFURLRef url;
	CFHTTPMessageRef msg;
	CFReadStreamRef stream;
	disko_t ds;
	UInt8 buf[4096];
	CFIndex bytes_read;

	if (disko_memopen(&ds) < 0)
		return -1; /* oops! */

	{
		CFStringRef surl;

		surl = CFStringCreateWithFormat(kCFAllocatorDefault, NULL,
			CFSTR("http%s://%s:%u%s"), (reqflags & HTTP_REQ_SSL) ? "s" : "",
			host, (unsigned int)port, path);
		if (!surl) {
			disko_memclose(&ds, 0);
			return -1;
		}

		url = CFURLCreateWithString(kCFAllocatorDefault, surl, NULL);

		CFRelease(surl);
	}

	if (!url) {
		disko_memclose(&ds, 0);
		return -1;
	}

	msg = CFHTTPMessageCreateRequest(kCFAllocatorDefault, CFSTR("GET"),
		url, kCFHTTPVersion1_1);

	/* we don't need the URL anymore */
	CFRelease(url);

	if (!msg) {
		disko_memclose(&ds, 0);
		return -1;
	}

	{
		CFStringRef user_agent;

		user_agent = CFStringCreateWithCString(kCFAllocatorDefault,
			HTTP_USER_AGENT, kCFStringEncodingUTF8);
		if (user_agent)
			CFHTTPMessageSetHeaderFieldValue(msg, CFSTR("User-Agent"),
				user_agent);
	}

	/* Why the fuck does Apple always deprecate their good APIs? */
	stream = CFReadStreamCreateForHTTPRequest(kCFAllocatorDefault, msg);
	if (!stream) {
		CFRelease(msg);
		disko_memclose(&ds, 0);
		return -1;
	}

	if (!CFReadStreamOpen(stream)) {
		CFReadStreamClose(stream);
		CFRelease(msg);
		disko_memclose(&ds, 0);
		return -1;
	}

	while ((bytes_read = CFReadStreamRead(stream, buf, sizeof(buf))) > 0)
		disko_write(&ds, buf, sizeof(buf));

	CFReadStreamClose(stream);
	CFRelease(msg);

	if (bytes_read < 0) {
		/* WEIRD! */
		disko_memclose(&ds, 0);
		return -1;
	}

	disko_write(&ds, "\0\0\0\0", 4);

	cb(ds.data, ds.length, userdata);

	disko_memclose(&ds, 0);

	return 0;
}

int http_macosx_init(struct http *r)
{
	r->send_request_ = http_macosx_send_request_;
	r->destroy_ = http_macosx_destroy_;

	return 0;
}
