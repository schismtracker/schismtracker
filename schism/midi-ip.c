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

/* TODO:
 * There are a few variations of MIDI over IP.
 *
 * One of which is RTP-MIDI (originally AppleMIDI), introduced in Mac
 * OS X 10.4 (e.g. 2005 or so). I'm not sure what we could do to be able
 * to support that (or if it would even be useful), but it seems to be
 * more useful than multicast MIDI these days. */

#ifdef USE_NETWORK

#ifdef SCHISM_WIN32
# include <ws2tcpip.h>
# include <windows.h>

typedef UINT_PTR sock_t;

# define SOCKET_INVALID INVALID_SOCKET
#else
# include <sys/select.h>
# include <sys/socket.h>
# include <sys/types.h>
# include <arpa/inet.h>
# include <fcntl.h>
# include <netinet/in.h>

# define SOCKET_INVALID (-1)
# define SOCKET_ERROR (-1)
# define closesocket close

typedef int sock_t;
#endif

#define DEFAULT_IP_PORT_COUNT   (5)
#define MIDI_IP_BASE    (21928)
#define MAX_DGRAM_SIZE  (1280)

static const unsigned char midi_ip_multicast_addr[4] = {
	/* network byte order (big endian) */
	225, 0, 0, 37
};

struct midi_ip {
	sock_t fd;

	int pb;
};

struct midi_ip_provider {
	/* this is read by the midi-poll function,
	 * and set in midi_ip_setports */
	volatile int num_ports;
	/* socket used for output */
	sock_t out_fd;
#define MIDIBUF_SIZE (65536)
	/* midi buffer */
	unsigned char midibuf[MIDIBUF_SIZE];
};

/* tells the main function to poll for midi devices */
static void do_wake_main(void)
{
	schism_event_t e = {
		.type = SCHISM_EVENT_UPDATE_IPMIDI,
	};

	events_push_event(&e);
}

static sock_t _get_fd(int pb, int isout)
{
	struct ip_mreq mreq = {0};
	struct sockaddr_in asin = {0};
	sock_t fd;
	int opt;

#if !defined(PF_INET) && defined(AF_INET)
#define PF_INET AF_INET
#endif
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == SOCKET_INVALID)
		return SOCKET_INVALID;

	/* XXX why are we setting this? where are we reusing ports? */
	opt = 1;
	setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(int));

	/* don't loop back what we generate */
	opt = !isout;
#ifdef SCHISM_WIN32
	/* apparently this is reversed on windows?
	 * drumstick sources link to msdn:
	 * https://learn.microsoft.com/en-us/windows/win32/winsock/ip-multicast-2 */
	opt = !opt;
#endif
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (void*)&opt, sizeof(opt)) == SOCKET_ERROR) {
		closesocket(fd);
		return SOCKET_INVALID;
	}

	/* this USED to get set to 31.
	 * HOWEVER, all other midi-ip implementations that I'm aware of explicitly
	 * set this to 1. Linux manpages even say that it's important that this be
	 * set to the smallest TTL possible, so I'm changing it :) */
	opt = 1;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&opt, sizeof(opt)) == SOCKET_ERROR) {
		closesocket(fd);
		return SOCKET_INVALID;
	}

	memcpy(&mreq.imr_multiaddr, midi_ip_multicast_addr, sizeof(midi_ip_multicast_addr));
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) == SOCKET_ERROR) {
		closesocket(fd);
		return SOCKET_INVALID;
	}

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void*)&opt, sizeof(opt)) == SOCKET_ERROR) {
		closesocket(fd);
		return SOCKET_INVALID;
	}

	asin.sin_family = AF_INET;
	if (!isout) {
		/* all 0s is inaddr_any; but this is for listening */
#if 1
		//JosepMa:On my machine, using the 225.0.0.37 address caused bind to fail.
		//Didn't look too much to find why.
		memset(&asin.sin_addr, 0, 4);
#else
		memcpy(&asin.sin_addr, midi_ip_multicast_addr, sizeof(midi_ip_multicast_addr));
#endif
		asin.sin_port = htons(MIDI_IP_BASE+pb);
	}
	if (bind(fd, (struct sockaddr *)&asin, sizeof(asin)) == SOCKET_ERROR) {
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
#else
		perror("bind");
#endif
		closesocket(fd);
		return SOCKET_INVALID;
	}



	return fd;
}

static void _readin(struct midi_provider *p, struct midi_port *q)
{
	/* FIXME move this into midi_provider? */
	struct midi_ip_provider *mip;
	struct midi_ip *data;
	int r;

	mip = p->userdata;
	data = q->userdata;

	/* NOTE: original code did recvfrom, and got the address,
	 * but did literally nothing with it.  --paper */
	r = recv(data->fd, (char *)mip->midibuf, sizeof(mip->midibuf), 0);
	if (r > 0) {
		midi_received_cb(q, mip->midibuf, r);
	}
}

static int _ip_work(struct midi_provider *p)
{
	/* this seems to not work on linux? or VMPK's implementation
	 * is wrong there maybe. it works fine on windows.  --paper */
	struct timeval tv;
	fd_set rfds;
	int i, m;
	struct midi_port *q;
	struct midi_ip *data;
	ssize_t r;

	FD_ZERO(&rfds);

	for (m = 0, q = NULL; midi_port_foreach(p, &q); /* nothing */) {
		data = q->userdata;

		FD_SET(data->fd, &rfds);
		if (data->fd > m) m = data->fd;
	}

	/* no waiting, just poll */
	memset(&tv, 0, sizeof(tv));

	/* TODO use non-blocking sockets instead of select */
	r = select(m + 1, &rfds, NULL, NULL, &tv);
	if (r == SOCKET_ERROR) {
		return -1; /* what? */
	} else if (r == 0) {
		return 0; /* nothing to process */
	}

	for (i = 0, q = NULL; midi_port_foreach(p, &q); i++) {
		data = q->userdata;

		if ((q->io & MIDI_INPUT) && FD_ISSET(data->fd, &rfds))
			_readin(p, q);
	}

	return 0;
}

static int _ip_start(SCHISM_UNUSED struct midi_port *p)
{
	/* no-op */
	return 1;
}
static int _ip_stop(SCHISM_UNUSED struct midi_port *p)
{
	/* no-op */
	return 1;
}

static void _ip_send(struct midi_port *p, const unsigned char *data, uint32_t len,
	SCHISM_UNUSED uint32_t delay)
{
	struct sockaddr_in asin = {0};
	struct midi_ip *userdata;
	struct midi_ip_provider *mip;

	mip = p->provider->userdata;
	userdata = p->userdata;

	if (len == 0) return;

	asin.sin_family = AF_INET;
	/* 255.0.0.37 -- UDP multicast MIDI IP */
	memcpy(&asin.sin_addr, midi_ip_multicast_addr, sizeof(midi_ip_multicast_addr));
	asin.sin_port = htons(MIDI_IP_BASE + userdata->pb);

	while (len) {
		int ss;

		ss = (len > MAX_DGRAM_SIZE) ?  MAX_DGRAM_SIZE : len;
		if (sendto(mip->out_fd, (const char*)data, ss, 0,
				(struct sockaddr *)&asin,sizeof(asin)) < 0) {
			/* nah */
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
	struct midi_port *q;
	struct midi_ip *data;
	struct midi_ip_provider *mip;
	long i, m;

	mip = p->userdata;

	midi_provider_mark_ports(p);

	m = mip->num_ports;

	q = NULL;
	for (i = 0; midi_port_foreach(p, &q) && i < m; i++)
		q->mark = 0;

	midi_provider_remove_marked_ports(p);

	for (; i < m; i++) {
		char buf[256];

		data = mem_alloc(sizeof(struct midi_ip));
		data->fd = _get_fd(i, 0);
		if (data->fd == SOCKET_INVALID) {
			free(data);
			break; /* what? */
		}
		data->pb = i;

		if (snprintf(buf, sizeof(buf), " Multicast/IP MIDI %ld", i+1) == -1) {
			perror("snprintf");
			exit(255);
		}

		midi_port_register(p, MIDI_INPUT|MIDI_OUTPUT, buf,
			data, _ip_userdata_destroy);
	}
}

static void _ip_destroy_userdata(void *x)
{
	struct midi_ip_provider *mip = x;

	if (mip->out_fd != SOCKET_INVALID)
		closesocket(mip->out_fd);

	free(mip);
}

/* dumb hack for ip_midi_getports */
static struct midi_provider *midi_ip_prov;

int ip_midi_setup(void)
{
	static struct midi_driver driver;
	struct midi_ip_provider *mip;

	if (status.flags & NO_NETWORK) return 0;

	mip = mem_calloc(1, sizeof(*mip));

	mip->out_fd = _get_fd(-1, 1);
	if (mip->out_fd == SOCKET_INVALID) {
		free(mip);
		return 0;
	}

	driver.flags = 0;
	driver.poll = _ip_poll;
	driver.work = _ip_work;
	driver.send = _ip_send;
	driver.enable = _ip_start;
	driver.disable = _ip_stop;
	driver.destroy_userdata = _ip_destroy_userdata;

	midi_ip_prov = midi_provider_register("IP", &driver, mip);
	if (!midi_ip_prov)
		return 0;

	//TODO: Save number of MIDI-IP ports
	ip_midi_setports(DEFAULT_IP_PORT_COUNT);

	return 1;
}

int ip_midi_getports(void)
{
	struct midi_ip_provider *mip;
	struct midi_port *q;
	int i;

	if (!midi_ip_prov) return 0;

	mip = midi_ip_prov->userdata;

	if (mip->out_fd == SOCKET_INVALID) return 0;
	if (status.flags & NO_NETWORK) return 0;

	for (i = 0, q = NULL; midi_port_foreach(midi_ip_prov, &q); i++);

	return i;
}

void ip_midi_setports(int n)
{
	struct midi_ip_provider *mip;

	if (!midi_ip_prov) return;

	mip = midi_ip_prov->userdata;

	if (mip->out_fd == SOCKET_INVALID) return;
	if (status.flags & NO_NETWORK) return;

	mip->num_ports = n;

	/* is this still necessary? */
	do_wake_main();
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

