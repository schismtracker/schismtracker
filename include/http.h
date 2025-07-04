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

#ifndef SCHISM_HTTP_H_
#define SCHISM_HTTP_H_

#include "headers.h"

#include "http.h"

typedef void (*http_request_cb_spec)(const void *data, size_t len, void *userdata);

#define HTTP_USER_AGENT "Schism Tracker/" VERSION

/* reqflags for http_send_request */
enum {
	HTTP_REQ_SSL = (0x01),
};

struct http {
	/* send_request: sends an HTTP GET request; returns the results in a
	 * callback
	 *
	 * NOTE: this function blocks!
	 *
	 * For ease of string operations, `data` is always padded
	 * with four NUL bytes on the end. */
	int (*send_request_)(struct http *r, const char *domain, const char *path,
		uint32_t reqflags, http_request_cb_spec cb, void *userdata);

	/* destructor */
	void (*destroy_)(struct http *r);

	/* backend data */
	void *data;
};

int http_init(struct http *r);
void http_quit(struct http *r);

int http_send_request(struct http *r, const char *domain, const char *path,
	uint32_t reqflags, http_request_cb_spec cb, void *userdata);

int http_wininet_init(struct http *r); /* SCHISM_WIN32 */
int http_macosx_init(struct http *r);  /* SCHISM_MACOSX */

#endif /* SCHISM_HTTP_H_ */
