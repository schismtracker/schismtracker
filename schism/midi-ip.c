/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/
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
#include "midi.h"

#include "util.h"
#include "sdlmain.h"
#include "event.h"

#ifdef USE_NETWORK

#ifdef WIN32
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#endif

#include <errno.h>

#define DEFAULT_IP_PORT_COUNT	5
#define MIDI_IP_BASE	21928
#define MAX_DGRAM_SIZE	1280

static int wakeup[2];
static int real_num_ports = 0;
static int num_ports = 0;
static int out_fd = -1;
static int *port_fd = 0;
static int *state = 0;
static SDL_mutex *blocker = 0;

static void do_wake_main(void)
{
	/* send at end */
	SDL_Event e;
	e.user.type = SCHISM_EVENT_UPDATE_IPMIDI;
	e.user.code = 0;
	e.user.data1 = 0;
	e.user.data2 = 0;
	SDL_PushEvent(&e);
}
static void do_wake_midi(void)
{
#ifdef WIN32
	/* anyone want to suggest how this is done? XXX */
#else
	(void)write(wakeup[1], "\x1", 1);
#endif
}

static int _get_fd(int pb, int isout)
{
	struct ip_mreq mreq;
	struct sockaddr_in asin;
	unsigned char *ipcopy;
	int fd, opt;

#if !defined(PF_INET) && defined(AF_INET)
#define PF_INET AF_INET
#endif
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == -1) return -1;

	opt = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(int));

	/* don't loop back what we generate */
	opt = !isout;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (void*)&opt, sizeof(opt)) < 0) {
		(void)close(fd);
		return -1;
	}

	opt = 31;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&opt, sizeof(opt)) < 0) {
		(void)close(fd);
		return -1;
	}

	memset(&mreq, 0, sizeof(mreq));
	ipcopy = (unsigned char *)&mreq.imr_multiaddr;
	ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) < 0) {
		(void)close(fd);
		return -1;
	}

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void*)&opt, sizeof(opt)) < 0) {
		(void)close(fd);
		return -1;
	}

	memset(&asin, 0, sizeof(asin));
	asin.sin_family = AF_INET;
	ipcopy = (unsigned char *)&asin.sin_addr;
	if (!isout) {
		/* all 0s is inaddr_any; but this is for listening */
		ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
		asin.sin_port = htons(MIDI_IP_BASE+pb);
	}
	if (bind(fd, (struct sockaddr *)&asin, sizeof(asin)) < 0) {
		(void)close(fd);
		return -1;
	}

	return fd;
}
void ip_midi_setports(int n)
{
	SDL_mutexP(blocker);
	num_ports = n;
	SDL_mutexV(blocker);
	do_wake_midi();
}
static void _readin(struct midi_provider *p, int en, int fd)
{
	struct midi_port *ptr, *src;
	static unsigned char buffer[65536];
	static struct sockaddr_in asin;
	unsigned slen = sizeof(asin);
	int r;

	r = recvfrom(fd, buffer, sizeof(buffer), 0,
		(struct sockaddr *)&asin, &slen);
	if (r > 0) {
		ptr = src = 0;
		while (midi_port_foreach(p, &ptr)) {
			int n = (int)ptr->userdata;
			if (n == en) src = ptr;
		}
		midi_received_cb(src, buffer, r);
	}
}
static int _ip_thread(struct midi_provider *p)
{
	static unsigned char buffer[4096];
#ifdef WIN32
	struct timeval tv;
#endif
	fd_set rfds;
	int *tmp2, *tmp;
	int i, m;

	for (;;) {
		SDL_mutexP(blocker);
		m = (volatile int)num_ports;
		if (m > real_num_ports) {
			/* need more ports */
			tmp = (int *)malloc(2 * (m * sizeof(int)));
			if (tmp) {
				tmp2 = tmp + m;
				for (i = 0; i < real_num_ports; i++) {
					tmp[i] = port_fd[i];
					tmp2[i] = state[i];
				}
				free(port_fd);

				port_fd = tmp;
				state = tmp2;

				for (i = real_num_ports; i < m; i++) {
					state[i] = 0;
					if ((port_fd[i] = _get_fd(i,0)) == -1) {
						m = i+1;
						break;
					}
				}
				real_num_ports = num_ports = m;
			}
			SDL_mutexV(blocker);
			do_wake_main();

		} else if (m < real_num_ports) {
			for (i = m; i < real_num_ports; i++) {
				(void)close(port_fd[i]);
				port_fd[i] = -1;
			}
			real_num_ports = num_ports = m;

			SDL_mutexV(blocker);
			do_wake_main();
		} else {
			SDL_mutexV(blocker);
		}

		FD_ZERO(&rfds);
		m = 0;
		for (i = 0; i < real_num_ports; i++) {
			FD_SET(port_fd[i], &rfds);
			if (port_fd[i] > m) m = port_fd[i];
		}

#ifdef WIN32
		tv.tv_sec = 1;
		tv.tv_usec = 0;
#else
		FD_SET(wakeup[0], &rfds);
		if (wakeup[0] > m) m = wakeup[0];
#endif
		do {
			i = select(m+1, &rfds, 0, 0, 
#ifdef WIN32
				&tv
#else
				(struct timeval *)0
#endif
				);
		} while (i == -1 && errno == EINTR);

#ifndef WIN32
		if (FD_ISSET(wakeup[0], &rfds)) {
			(void)read(wakeup[0], buffer, sizeof(buffer));
		}
#endif
		for (i = 0; i < real_num_ports; i++) {
			if (FD_ISSET(port_fd[i], &rfds)) {
				if (state[i] & 1) _readin(p, i, port_fd[i]);
			}
		}
	}
	return 0;
}

static int _ip_start(struct midi_port *p)
{
	int n = (int)p->userdata;
	SDL_mutexP(blocker);
	if (p->io & MIDI_INPUT)
		state[n] |= 1;
	if (p->io & MIDI_OUTPUT)
		state[n] |= 2;
	SDL_mutexV(blocker);
	do_wake_midi();
	return 1;
}
static int _ip_stop(struct midi_port *p)
{
	int n = (int)p->userdata;
	SDL_mutexP(blocker);
	if (p->io & MIDI_INPUT)
		state[n] &= (~1);
	if (p->io & MIDI_OUTPUT)
		state[n] &= (~2);
	SDL_mutexV(blocker);
	do_wake_midi();
	return 1;
}

static void _ip_send(struct midi_port *p, unsigned char *data, unsigned int len,
				UNUSED unsigned int delay)
{
	struct sockaddr_in asin;
	unsigned char *ipcopy;
	int n = (int)p->userdata;
	int ss;

	if (len == 0) return;
	if (!(state[n] & 2)) return; /* blah... */

	memset(&asin, 0, sizeof(asin));
	asin.sin_family = AF_INET;
	ipcopy = (unsigned char *)&asin.sin_addr.s_addr;
	ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
	asin.sin_port = htons(MIDI_IP_BASE+n);

	while (len) {
		ss = (len > MAX_DGRAM_SIZE) ?  MAX_DGRAM_SIZE : len;
		if (sendto(out_fd, data, ss, 0,
				(struct sockaddr *)&asin,sizeof(asin)) < 0) {
			state[n] &= (~2); /* turn off output */
			break;
		}
		len -= ss;
		data += ss;
	}
}
static void _ip_poll(struct midi_provider *p)
{
	static int last_buildout = 0;
	struct midi_port *ptr;
	char *buffer;
	int i = 0;
	int m;

	SDL_mutexP(blocker);
	m = (volatile int)real_num_ports;
	if (m < last_buildout) {
		ptr = 0;
		while (midi_port_foreach(p, &ptr)) {
			i = (int)ptr->userdata;
			if (i >= m) midi_port_unregister(ptr->num);
		}
		last_buildout = m;
	} else if (m > last_buildout) {
		for (i = last_buildout; i < m; i++) {
			buffer = 0;
			asprintf(&buffer, " Multicast/IP MIDI %u", i+1);
			midi_port_register(p, MIDI_INPUT | MIDI_OUTPUT, buffer,
					(void*)i, 0);
		}
		last_buildout = m;
	}
	SDL_mutexV(blocker);
}

int ip_midi_setup(void)
{
	static struct midi_driver driver;
#ifdef WIN32
	static WSADATA ignored;
#endif

	blocker = SDL_CreateMutex();
	if (!blocker) {
		return 0;
	}

#ifndef WIN32
	if (pipe(wakeup) == -1) {
		return 0;
	}
	(void)fcntl(wakeup[0], F_SETFL, fcntl(wakeup[0], F_GETFL, 0) | O_NONBLOCK);
	(void)fcntl(wakeup[1], F_SETFL, fcntl(wakeup[1], F_GETFL, 0) | O_NONBLOCK);
#endif

#ifdef WIN32
	memset(&ignored, 0, sizeof(ignored));
	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		return 0;
	}
#endif

	if (out_fd == -1) {
		out_fd = _get_fd(-1, 1);
	}

	ip_midi_setports(DEFAULT_IP_PORT_COUNT);

	driver.flags = 0;
	driver.poll = _ip_poll;
	driver.thread = _ip_thread;
	driver.send = _ip_send;
	driver.enable = _ip_start;
	driver.disable = _ip_stop;

	if (!midi_provider_register("IP", &driver)) return 0;
	return 1;
}

#else

void ip_midi_setports(int n)
{
}

#endif

int ip_midi_getports(void) {
	int tmp;

	SDL_mutexP(blocker);
	tmp = (volatile int)real_num_ports;
	SDL_mutexV(blocker);
	return tmp;
}
