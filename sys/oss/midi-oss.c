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

#include <config.h>

#include "headers.h"

#include "midi.h"

#include "util.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <sys/soundcard.h>

#ifdef HAVE_POLL_H
# include <poll.h>
#elif HAVE_SYS_POLL_H
# include <sys/poll.h>
#endif

#include <errno.h>
#include <fcntl.h>

/* this is stupid; oss doesn't have a concept of ports... */
#define MAX_OSS_MIDI    64

#define MAX_MIDI_PORTS  (MAX_OSS_MIDI + 2)
static int opened[MAX_MIDI_PORTS];

static void _oss_send(struct midi_port *p, const unsigned char *data, unsigned int len,
	SCHISM_UNUSED unsigned int delay)
{
	int fd, r, n;
	fd = opened[ n = INT_SHAPED_PTR(p->userdata) ];
	if (fd < 0) return;
	while (len > 0) {
		r = write(fd, data, len);
		if (r < -1 && errno == EINTR) continue;
		if (r < 1) {
			/* err, can't happen? */
			(void)close(opened[n]);
			opened[n] = -1;
			p->userdata = PTR_SHAPED_INT(-1);
			p->io = 0; /* failure! */
			return;
		}
		data += r;
		len -= r;
	}
}

static int _oss_start_stop(SCHISM_UNUSED struct midi_port *p) { return 1; /* do nothing */ }

static int _oss_thread(struct midi_provider *p)
{
	struct pollfd pfd[MAX_MIDI_PORTS];
	struct midi_port *ptr, *src;
	unsigned char midi_buf[4096];
	int i, j, r;

	while (!p->cancelled) {
		ptr = NULL;
		j = 0;
		while (midi_port_foreach(p, &ptr)) {
			i = INT_SHAPED_PTR(ptr->userdata);
			if (i == -1) continue; /* err... */
			if (!(ptr->io & MIDI_INPUT)) continue;
			pfd[j].fd = i;
			pfd[j].events = POLLIN;
			pfd[j].revents = 0; /* RH 5 bug */
			j++;
		}
		if (!j || poll(pfd, j, -1) < 1) {
			sleep(1);
			continue;
		}
		for (i = 0; i < j; i++) {
			if (!(pfd[i].revents & POLLIN)) continue;
			do {
				r = read(pfd[i].fd, midi_buf, sizeof(midi_buf));
			} while (r == -1 && errno == EINTR);
			if (r > 0) {
				ptr = src = NULL;
				while (midi_port_foreach(p, &ptr)) {
					if (INT_SHAPED_PTR(ptr->userdata) == pfd[i].fd) {
						src = ptr;
					}
				}
				midi_received_cb(src, midi_buf, r);
			}
		}
	}
	/* stupid gcc */
	return 0;
}


static int _tryopen(int n, const char *name, struct midi_provider *_oss_provider)
{
	int io;
	char *ptr;

	if (opened[n] != -1)
		return 1;

	if ((opened[n] = open(name, O_RDWR|O_NOCTTY|O_NONBLOCK)) != -1) {
		io = MIDI_INPUT | MIDI_OUTPUT;
	} else if ((opened[n] = open(name, O_RDONLY|O_NOCTTY|O_NONBLOCK)) != -1) {
		io = MIDI_INPUT;
	} else if ((opened[n] = open(name, O_WRONLY|O_NOCTTY|O_NONBLOCK)) != -1) {
		io = MIDI_OUTPUT;
	} else {
		return 2;
	}

	ptr = NULL;
	if (asprintf(&ptr, " %-16s (OSS)", name) == -1)
		return 3;

	midi_port_register(_oss_provider, io, ptr, PTR_SHAPED_INT((long)n), 0);
	free(ptr);
	return 0;
}

static void _oss_poll(struct midi_provider *_oss_provider)
{
	char sbuf[64];
	int i;

	_tryopen(0, "/dev/midi", _oss_provider);
	for (i = 0; i < MAX_OSS_MIDI; i++) {
		sprintf(sbuf, "/dev/midi%d", i);
		if (!_tryopen(i + 1, sbuf, _oss_provider))
			continue;

		sprintf(sbuf, "/dev/midi%02d", i);
		if (!_tryopen(i + 1, sbuf, _oss_provider))
			continue;
	}
}
int oss_midi_setup(void)
{
	static struct midi_driver driver;
	int i;

	driver.flags = 0;
	driver.poll = _oss_poll;
	driver.thread = _oss_thread;
	driver.enable = _oss_start_stop;
	driver.disable = _oss_start_stop;
	driver.send = _oss_send;

	for (i = 0; i < MAX_MIDI_PORTS; i++) opened[i] = -1;
	if (!midi_provider_register("OSS", &driver)) return 0;
	return 1;
}
