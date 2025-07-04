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

#import <Foundation/Foundation.h>

/* Mac OS X HTTP backend (requires 10.3+) */

static void http_macosx_destroy_(struct http *r)
{
	/* nothing */
}

static int http_macosx_send_request_(struct http *r, const char *domain,
	const char *path, uint32_t reqflags, http_request_cb_spec cb,
	void *userdata)
{
	/* this API sucks */
	NSURL *url;
	NSMutableURLRequest *req;
	NSURLResponse *res;
	NSError *err;
	NSData *data;
	disko_t ds;

	if (disko_memopen(&ds) < 0)
		return -1; /* oops! */

	url = [NSURL URLWithString: [NSString stringWithFormat: @"%s://%s%s",
		(reqflags & HTTP_REQ_SSL) ? "https" : "http",
		domain, path]];

	req = [NSMutableURLRequest requestWithURL: url];
	[req setValue: [NSString stringWithUTF8String: HTTP_USER_AGENT] forHTTPHeaderField: @"User-Agent"];

	/* This API is deprecated but it's really old and I don't see it
	 * TOTALLY going away any time soon... */
	data = [NSURLConnection sendSynchronousRequest: req
		returningResponse: &res error: &err];
	if (!data) {
		disko_memclose(&ds, 0);
		return -1;
	}

	disko_write(&ds, data.bytes, data.length);
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
