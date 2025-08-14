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
#include "midi.h"

#include "it.h"
#include "util.h"
#include "events.h"
#include "mem.h"

#include "mt.h" // mutexes

#ifdef USE_NETWORK

#ifdef SCHISM_WIN32
#include <ws2tcpip.h>
#include <windows.h>
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <fcntl.h>
#include <arpa/inet.h>

# define SOCKET_ERROR (-1)
# define closesocket close
#endif

#define DEFAULT_IP_PORT_COUNT   5
#define MIDI_IP_BASE    21928
#define MAX_DGRAM_SIZE  1280

struct midi_ip {
	/* currently only stores fd state, maybe we
	 * could save IP or something ... idk.
	 *
	 * from what I can tell, other programs simply
	 * dump out the port number -- maybe we should
	 * do that too? */
	int fd;
};

/* TODO all of these statics, for every midi driver,
 * should be inside of midi_provider. */

/* this is for ip_midi_setports, so it can set the
 * number of ports to poll. */
static int num_ports = 0;
/* XXX this should be in midi_provider */
static int out_fd = -1;
/* no idea if this is still needed, since I converted
 * everything to use the normal MIDI stuff like all of
 * the other drivers  --paper */
static mt_mutex_t *blocker = NULL;

/* tells the main function to poll for midi devices */
static void do_wake_main(void)
{
	schism_event_t e = {
		.type = SCHISM_EVENT_UPDATE_IPMIDI,
	};

	events_push_event(&e);
}

static int _get_fd(int pb, int isout)
{
	struct ip_mreq mreq = {0};
	struct sockaddr_in asin = {0};
	unsigned char *ipcopy;
	int fd, opt;

#if !defined(PF_INET) && defined(AF_INET)
#define PF_INET AF_INET
#endif
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == -1) return -1;

	opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(int));

	/* don't loop back what we generate */
	opt = !isout;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (void*)&opt, sizeof(opt)) < 0) {
		closesocket(fd);
		return -1;
	}

	opt = 31;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&opt, sizeof(opt)) < 0) {
		closesocket(fd);
		return -1;
	}

	ipcopy = (unsigned char *)&mreq.imr_multiaddr;
	ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) < 0) {
		closesocket(fd);
		return -1;
	}

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void*)&opt, sizeof(opt)) < 0) {
		closesocket(fd);
		return -1;
	}

	asin.sin_family = AF_INET;
	ipcopy = (unsigned char *)&asin.sin_addr;
	if (!isout) {
		/* all 0s is inaddr_any; but this is for listening */
#ifdef SCHISM_WIN32
		//JosepMa:On my machine, using the 225.0.0.37 address caused bind to fail.
		//Didn't look too much to find why.
		memset(ipcopy, 0, 4);
#else
		ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
#endif
		asin.sin_port = htons(MIDI_IP_BASE+pb);
	}
	if (bind(fd, (struct sockaddr *)&asin, sizeof(asin)) < 0) {
#ifdef SCHISM_WIN32
		int asdf = WSAGetLastError();
		perror("binderror");
		switch(asdf) {
			case WSANOTINITIALISED: perror("WSANOTINITIALISED");break;
			case WSAENETDOWN: perror("WSAENETDOWN");break;
			case WSAEFAULT: perror("WSAEFAULT");break;
			case WSAEINVAL: perror("WSAEINVAL");break;
			case WSAEINPROGRESS: perror("WSAEINPROGRESS");break;
			case WSAENOTSOCK: perror("WSAENOTSOCK");break;
			case WSAEACCES: perror("WSAEACCES");break;
			case WSAEADDRINUSE: perror("WSAEADDRINUSE");break;
			case WSAEADDRNOTAVAIL: perror("WSAEADDRNOTAVAIL");break;
			case WSAENOBUFS: perror("WSAENOBUFS");break;
			default: perror("default");break;
		}
#endif
		closesocket(fd);
		return -1;
	}

	return fd;
}

void ip_midi_setports(int n)
{
	if (out_fd == -1) return;
	if (status.flags & NO_NETWORK) return;

	mt_mutex_lock(blocker);
	num_ports = n;
	mt_mutex_unlock(blocker);
}

static void _readin(struct midi_provider *p, struct midi_port *q)
{
	/* FIXME move this into midi_provider? */
	static unsigned char buffer[65536];
	struct midi_ip *data;
	int r;

	data = q->userdata;

	/* NOTE: original code did recvfrom, and got the address,
	 * but did literally nothing with it.  --paper */
	r = recv(data->fd, (char *)buffer, sizeof(buffer), 0);
	if (r > 0) {
		midi_received_cb(q, buffer, r);
	}
}

static int _ip_work(struct midi_provider *p)
{
	/* this function should be called periodically */
	struct timeval tv;
	fd_set rfds;
	int i, m, r;
	int *tmp2, *tmp;
	struct midi_port *q;
	struct midi_ip *data;

	FD_ZERO(&rfds);
	m = 0;
	q = NULL;
	while (midi_port_foreach(p, &q)) {
		data = q->userdata;

		FD_SET(data->fd, &rfds);
		if (data->fd > m) data->fd;
	}

	/* no waiting, just poll */
	memset(&tv, 0, sizeof(tv));

	/* TODO use non-blocking sockets instead of select */
	r = select(m + 1, &rfds, NULL, NULL, &tv);
	if (r == SOCKET_ERROR)
		return -1; /* what? */
	else if (r == 0)
		return 0; /* nothing to process */

	q = NULL;
	for (i = 0; midi_port_foreach(p, &q); i++) {
		data = q->userdata;

		if ((q->io & MIDI_INPUT) && FD_ISSET(data->fd, &rfds))
			_readin(p, q);
	}

	return 0;
}

static int _ip_thread(struct midi_provider *p)
{
	while (!p->cancelled) {
		_ip_work(p);
		timer_msleep(1);
	}

	return 0;
}

static int _ip_start(struct midi_port *p)
{
	/* no-op */
	return 1;
}
static int _ip_stop(struct midi_port *p)
{
	/* no-op */
	return 1;
}

static void _ip_send(struct midi_port *p, const unsigned char *data, uint32_t len,
	SCHISM_UNUSED uint32_t delay)
{
	struct sockaddr_in asin = {0};
	struct midi_ip *userdata;
	unsigned char *ipcopy;
	int n;
	int ss;

	userdata = p->userdata;
	n = userdata->fd;

	if (len == 0) return;
	if (!(p->io & MIDI_OUTPUT)) return; /* is this still needed ?  --paper */

	asin.sin_family = AF_INET;
	ipcopy = (unsigned char *)&asin.sin_addr.s_addr;
	/* 255.0.0.37 -- UDP multicast MIDI IP */
	ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
	asin.sin_port = htons(MIDI_IP_BASE+n);

	while (len) {
		ss = (len > MAX_DGRAM_SIZE) ?  MAX_DGRAM_SIZE : len;
		if (sendto(out_fd, (const char*)data, ss, 0,
				(struct sockaddr *)&asin,sizeof(asin)) < 0) {
			p->io &= ~(MIDI_OUTPUT);
			break;
		}
		len -= ss;
		data += ss;
	}
}

static void _ip_userdata_destroy(void *userdata)
{
	struct midi_ip *data = userdata;

	closesocket(data->fd);
	free(data);
}

static void _ip_poll(struct midi_provider *p)
{
	static int last_buildout = 0;
	struct midi_port *q;
	struct midi_ip *data;
	char *buffer;
	long i, m;

	midi_provider_mark_ports(p);

	mt_mutex_lock(blocker);
	m = (volatile int)num_ports;

	q = NULL;
	i = 0;
	for (i = 0; midi_port_foreach(p, &q) && i < m; i++)
		q->mark = 0;

	midi_provider_remove_marked_ports(p);

	for (; i < m; i++) {
		char buf[256];

		data = mem_alloc(sizeof(struct midi_ip));
		data->fd = _get_fd(i, 0);
		if (data->fd == -1) {
			free(data);
			break; /* what? */
		}

		if (snprintf(buf, sizeof(buf), " Multicast/IP MIDI %ld", i+1) == -1) {
			perror("snprintf");
			exit(255);
		}

		midi_port_register(p, MIDI_INPUT|MIDI_OUTPUT, buf,
			data, _ip_userdata_destroy);
	}

	mt_mutex_unlock(blocker);
}

/* dumb hack for ip_midi_getports */
static struct midi_provider *midi_ip_prov;

int ip_midi_setup(void)
{
	static struct midi_driver driver;

	if (status.flags & NO_NETWORK) return 0;

	blocker = mt_mutex_create();
	if (!blocker)
		return 0;

	if (out_fd == -1) {
		out_fd = _get_fd(-1, 1);
		if (out_fd == -1) return 0;
	}

	driver.flags = 0;
	driver.poll = _ip_poll;
	driver.thread = _ip_thread;
	driver.send = _ip_send;
	driver.enable = _ip_start;
	driver.disable = _ip_stop;

	midi_ip_prov = midi_provider_register("IP", &driver);
	if (!midi_ip_prov)
		return 0;

	//TODO: Save number of MIDI-IP ports
	ip_midi_setports(DEFAULT_IP_PORT_COUNT);

	return 1;
}

int ip_midi_getports(void)
{
	struct midi_port *q;
	int i;

	if (out_fd == -1) return 0;
	if (status.flags & NO_NETWORK) return 0;

	mt_mutex_lock(blocker);
	for (i = 0, q = NULL; midi_port_foreach(midi_ip_prov, &q); i++);
	mt_mutex_unlock(blocker);

	return i;
}

#else

int ip_midi_getports(void)
{
	return 0;
}

void ip_midi_setports(SCHISM_UNUSED int n)
{
}

#endif

