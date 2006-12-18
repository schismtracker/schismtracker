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

#ifdef USE_NETWORK

#ifdef WIN32
#include <windows.h>
#include <ws2tcpip.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#endif

#include <errno.h>

#define DEFAULT_IP_PORT_COUNT	5
#define MIDI_IP_BASE	21928
#define MAX_DGRAM_SIZE	1280

static int real_num_ports = 0;
static int num_ports = 0;
static int out_fd = -1;
static int *port_fd = 0;
static int *state = 0;



static int _get_fd(int pb, int isout)
{
	struct ip_mreq mreq;
	struct sockaddr_in sin;
	unsigned char *ipcopy;
	int fd, opt;
#if !defined(PF_INET) && defined(AF_INET)
#define PF_INET AF_INET
#endif
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == -1) return -1;

	opt = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(int));

	/* don't loop back what we generate */
	opt = !isout;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, &opt, sizeof(opt)) < 0) {
		(void)close(fd);
		return -1;
	}

	opt = 31;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, &opt, sizeof(opt)) < 0) {
		(void)close(fd);
		return -1;
	}

	memset(&mreq, 0, sizeof(mreq));
	ipcopy = (unsigned char *)&mreq.imr_multiaddr;
	ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, &mreq, sizeof(mreq)) < 0) {
		(void)close(fd);
		return -1;
	}

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &opt, sizeof(opt)) < 0) {
		(void)close(fd);
		return -1;
	}

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	ipcopy = (unsigned char *)&sin.sin_addr;
	if (!isout) {
		/* all 0s is inaddr_any; but this is for listening */
		ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
		sin.sin_port = htons(MIDI_IP_BASE+pb);
	}
	if (bind(fd, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		(void)close(fd);
		return -1;
	}

	return fd;
}
int ip_midi_setports(int n)
{
	int *tmp, i;

	if (n > -1) {
		if (n > num_ports) {
			tmp = (int *)realloc(port_fd, n * sizeof(int));
			if (!tmp) return real_num_ports;
			port_fd = tmp; /* err... */

			tmp = (int *)realloc(state, n * sizeof(int));
			if (!tmp) return real_num_ports;
			state = tmp;

			for (i = num_ports; i < n; i++) {
				state[i] = 0;
				if ((port_fd[i] = _get_fd(i,0)) == -1) {
					while (i >= num_ports) {
						(void)close(tmp[i]);
						i--;
					}
					return real_num_ports;
				}
			}
			real_num_ports = num_ports = n;

			return n;
		} else if (n < num_ports) {
			for (i = n; i < real_num_ports; i++) {
				(void)close(port_fd[i]);
				port_fd[i] = -1;
			}
			real_num_ports = n;
			return n;
		}
	}
	return real_num_ports;
}

static void _readin(struct midi_provider *p, int en, int fd)
{
	struct midi_port *ptr, *src;
	static unsigned char buffer[65536];
	static struct sockaddr_in sin;
	unsigned slen = sizeof(sin);
	int r;

	r = recvfrom(fd, buffer, sizeof(buffer), 0,
		(struct sockaddr *)&sin, &slen);
	if (r > 0) {
		ptr = src = 0;
		while (midi_port_foreach(p, &ptr)) {
			int n = ((int*)ptr->userdata) - port_fd;
			if (n == en) src = ptr;
		}
		midi_received_cb(src, buffer, r);
	}
}
static int _ip_thread(struct midi_provider *p)
{
	fd_set rfds;
	int i, m;

	for (;;) {
		FD_ZERO(&rfds);
		m = 0;
		for (i = 0; i < real_num_ports; i++) {
			FD_SET(port_fd[i], &rfds);
			if (port_fd[i] > m) m = port_fd[i];
		}
		do {
			i = select(m+1, &rfds, 0, 0, 0);
		} while (i == -1 && errno == EINTR);

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
	int n = ((int*)p->userdata) - port_fd;
	if (p->io & MIDI_INPUT)
		state[n] |= 1;
	if (p->io & MIDI_OUTPUT)
		state[n] |= 2;
	return 1;
}
static int _ip_stop(struct midi_port *p)
{
	int n = ((int*)p->userdata) - port_fd;
	if (p->io & MIDI_INPUT)
		state[n] &= (~1);
	if (p->io & MIDI_OUTPUT)
		state[n] &= (~2);
	return 1;
}

static void _ip_send(struct midi_port *p, unsigned char *data, unsigned int len,
				UNUSED unsigned int delay)
{
	struct sockaddr_in sin;
	unsigned char *ipcopy;
	int n = ((int*)p->userdata) - port_fd;
	int ss;

	if (len == 0) return;
	if (!(state[n] & 2)) return; /* blah... */

	memset(&sin, 0, sizeof(sin));
	sin.sin_family = AF_INET;
	ipcopy = (unsigned char *)&sin.sin_addr.s_addr;
	ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
	sin.sin_port = htons(MIDI_IP_BASE+n);

	while (len) {
		ss = (len > MAX_DGRAM_SIZE) ?  MAX_DGRAM_SIZE : len;
		if (sendto(out_fd, data, ss, 0,
				(struct sockaddr *)&sin,sizeof(sin)) < 0) {
			state[n] &= (~2); /* turn off output */
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

	if (real_num_ports < last_buildout) {
		ptr = 0;
		while (midi_port_foreach(p, &ptr)) {
			i = ((int*)ptr->userdata) - port_fd;
			if (i >= real_num_ports) midi_port_unregister(ptr->num);
		}
	} else {
		for (i = last_buildout; i < real_num_ports; i++) {
			buffer = 0;
			asprintf(&buffer, " Multicast/IP MIDI %u", i+1);
			midi_port_register(p, MIDI_INPUT | MIDI_OUTPUT, buffer,
					&port_fd[i], 0);
		}
	}
	last_buildout = i;
}

int ip_midi_setup(void)
{
	struct midi_driver driver;
#ifdef WIN32
	WSADATA ignored;
	if (WSAStartup(0x202, &ignored) == SOCKET_ERROR) {
		WSACleanup(); /* ? */
		return 0;
	}
#endif
	if (out_fd == -1) {
		out_fd = _get_fd(-1, 1);
	}

	if (ip_midi_setports(DEFAULT_IP_PORT_COUNT) != DEFAULT_IP_PORT_COUNT) return 0;

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

int ip_midi_setports(int n)
{
	return 0;
}

#endif

int ip_midi_getports(void) { return ip_midi_setports(-1); }
