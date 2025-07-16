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

#include "update.h"
#include "mt.h"
#include "http.h"
#include "version.h"
#include "config-parser.h"
#include "log.h"

/* An example of the contents of phone_home.ini is below: */

/*
[Latest]
version=20250704

... maybe more in the future?
*/

static void update_thread_http_cb(const void *data, size_t len, void *userdata)
{
	cfg_file_t cfg = {0};
	const char *ptr;

	cfg_read_mem(data, len, &cfg);

	ptr = cfg_get_string(&cfg, "Latest", "version", NULL, 0, NULL);
	if (ptr) {
		int syear, smon, sday;

		if (ver_parse(ptr, &syear, &smon, &sday)) {
			uint32_t sver = ver_mktime(syear, smon, sday);

			if (sver > ver_reserved) {
				log_nl();
				log_appendf(1,
					"A new version of Schism Tracker has been released!");
				log_appendf(1,
					"Download version %s from https://schismtracker.org/",
					ptr);
			}
		}
	}

	cfg_free(&cfg);
}

static int update_thread(void *userdata)
{
	struct http r;
	int ret;

	if (http_init(&r) < 0)
		return 1;

	ret = http_send_request(&r, "www.schismtracker.org", "/phone_home.ini",
		HTTP_PORT_HTTPS, HTTP_REQ_SSL, update_thread_http_cb, NULL);

	http_quit(&r);

	return (ret < 0) ? 1 : 0;
}

/* this launches an HTTP thread that checks for a new update */
void update_check(void)
{
	mt_thread_t *thread = mt_thread_create(update_thread, "http-update", NULL);
	if (!thread)
		return; /* DOH! */

	mt_thread_detach(thread);
}