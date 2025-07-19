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

/* callback spec. this is called from http_send_request,
 * and receives the data from the server.
 *
 * 'data' is the actual data itself. this is always padding with 4 trailing
 * NUL bytes, so you can use them with string functions assuming a NUL
 * termination sequence (up to 4 bytes). do note that previous NUL bytes
 * will *not* be stripped.
 * 'len' is the length of the data, not including the trailing padding bytes.
 * 'userdata' is the same pointer passed to http_send_request. */
typedef void (*http_request_cb_spec)(const void *data, size_t len, void *userdata);

/* this is the user agent...
 * Ideally, we would want to set this up with some system info, maybe
 * architecture, OS, etc. if we really need those statistics. */
#define HTTP_USER_AGENT "Schism Tracker/" VERSION

/* these are well-defined default ports for HTTP and HTTPS. */
#define HTTP_PORT_HTTP (80)
#define HTTP_PORT_HTTPS (443)

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
	int (*send_request_)(struct http *r, const char *host, const char *path,
		uint16_t port, uint32_t reqflags, http_request_cb_spec cb,
		void *userdata);

	/* destructor */
	void (*destroy_)(struct http *r);

	/* backend data */
	void *data;
};

int http_init(struct http *r);
void http_quit(struct http *r);

int http_send_request(struct http *r, const char *host, const char *path,
	uint16_t port, uint32_t reqflags, http_request_cb_spec cb, void *userdata);

int http_wininet_init(struct http *r); /* SCHISM_WIN32 */
int http_macosx_init(struct http *r);  /* SCHISM_MACOSX */

#endif /* SCHISM_HTTP_H_ */
