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

#include "mt.h" // mutexes

#ifdef USE_NETWORK

#ifndef SCHISM_WIN32
// Windows uses something weird like UINT_PTR
typedef int SOCKET;
#endif


#ifdef SCHISM_WIN32
# // must precede <windows.h>
# include <ws2tcpip.h>
# include <windows.h>
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <netinet/in.h>
# include <sys/select.h>
# include <fcntl.h>
# include <arpa/inet.h>
#endif

#define DEFAULT_IP_PORT_COUNT   5
#define MIDI_IP_BASE    21928
#define MAX_DGRAM_SIZE  1280

#ifdef USE_NETWORK_WS2
static HANDLE hWakeUp;
#else
static SOCKET wakeup[2];
#endif
static int real_num_ports = 0;
static int num_ports = 0;
static SOCKET out_fd = -1;
static SOCKET *port_fd = NULL;
static SOCKET *state = NULL; /* not really sockets, but shares the same allocation as port_fd */
static mt_mutex_t *blocker = NULL;

static void do_wake_main(void)
{
	schism_event_t e = {
		.type = SCHISM_EVENT_UPDATE_IPMIDI,
	};

	events_push_event(&e);
}
static void do_wake_midi(void)
{
#ifdef SCHISM_WIN32
# ifdef USE_NETWORK_WS2
	SetEvent(hWakeUp);
# else
	if ((wakeup[1] != INVALID_SOCKET)
	 && (send(wakeup[1], "\x1", 1, 0) == 1)) {
		/* */
	}
#endif
#else
	if (write(wakeup[1], "\x1", 1) == 1) {
		/* fortify is stupid */
	}
#endif
}

static SOCKET _get_fd(int pb, int isout)
{
	struct ip_mreq mreq = {0};
	struct sockaddr_in asin = {0};
	unsigned char *ipcopy;
	SOCKET fd;
	int opt;

#if !defined(PF_INET) && defined(AF_INET)
#define PF_INET AF_INET
#endif
	fd = socket(PF_INET, SOCK_DGRAM, 0);
	if (fd == -1) return -1;

	opt = 1; setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (void*)&opt, sizeof(int));

	/* don't loop back what we generate */
	opt = !isout;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_LOOP, (void*)&opt, sizeof(opt)) < 0) {
#ifdef SCHISM_WIN32
		closesocket(fd);
#else
		close(fd);
#endif

		return -1;
	}

	opt = 31;
	if (setsockopt(fd, IPPROTO_IP, IP_MULTICAST_TTL, (void*)&opt, sizeof(opt)) < 0) {
#ifdef SCHISM_WIN32
		closesocket(fd);
#else
		close(fd);
#endif
		return -1;
	}

	mreq.imr_multiaddr.s_addr = inet_addr("225.0.0.37");
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (void*)&mreq, sizeof(mreq)) < 0) {
#ifdef SCHISM_WIN32
		closesocket(fd);
#else
		close(fd);
#endif
		return -1;
	}

	opt = 1;
	if (setsockopt(fd, SOL_SOCKET, SO_BROADCAST, (void*)&opt, sizeof(opt)) < 0) {
#ifdef SCHISM_WIN32
		closesocket(fd);
#else
		close(fd);
#endif
		return -1;
	}

	asin.sin_family = AF_INET;
	ipcopy = (unsigned char *)&asin.sin_addr;
	if (!isout) {
		asin.sin_addr.s_addr = htonl(INADDR_ANY);
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
		closesocket(fd);
#else
		close(fd);
#endif
		return -1;
	}

#ifdef USE_NETWORK_WS2
		// Translate socket events into the hWakeUp event object being set.
	if (0 != WSAEventSelect(fd, hWakeUp, FD_READ)) {
		closesocket(fd);
		return -1;
	}
#endif


	return fd;
}
void ip_midi_setports(int n)
{
	if (out_fd == -1) return;
	if (status.flags & NO_NETWORK) return;

	mt_mutex_lock(blocker);
	num_ports = n;
	mt_mutex_unlock(blocker);
	do_wake_midi();
}
static void _readin(struct midi_provider *p, int en, int fd)
{
	struct midi_port *ptr, *src;
	static unsigned char buffer[65536];
	static struct sockaddr_in asin;
	socklen_t slen = sizeof(asin);
	int r;

	r = recvfrom(fd, (char*)buffer, sizeof(buffer), 0,
		(struct sockaddr *)&asin, &slen);
	if (r > 0) {
		ptr = src = NULL;
		while (midi_port_foreach(p, &ptr)) {
			int n = INT_SHAPED_PTR(ptr->userdata);
			if (n == en) src = ptr;
		}
		midi_received_cb(src, buffer, r);
	}
}
static int _ip_thread(struct midi_provider *p)
{
#ifdef SCHISM_WIN32
	struct timeval tv;
#endif
	struct timeval *tv_ptr = NULL;
	static unsigned char buffer[4096];
	fd_set rfds;
	SOCKET *tmp2, *tmp;
	int i, m;

	while (!p->cancelled) {
		mt_mutex_lock(blocker);
		m = (volatile int)num_ports;
		//If no ports, wait and try again
		if (m > real_num_ports) {
			/* need more ports */
			tmp = malloc(2 * (m * sizeof(SOCKET)));
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
			mt_mutex_unlock(blocker);
			do_wake_main();

		} else if (m < real_num_ports) {
			for (i = m; i < real_num_ports; i++) {
#ifdef SCHISM_WIN32
				closesocket(port_fd[i]);
#else
				close(port_fd[i]);
#endif
				port_fd[i] = -1;
			}
			real_num_ports = num_ports = m;

			mt_mutex_unlock(blocker);
			do_wake_main();
		} else {
			mt_mutex_unlock(blocker);
			if (!real_num_ports) {
				//Since the thread is not finished in this case (maybe it should),
				//we put a delay to prevent the thread using all the cpu.
				timer_msleep(1);
			}
		}

#ifndef USE_NETWORK_WS2
		FD_ZERO(&rfds);
		m = 0;
		for (i = 0; i < real_num_ports; i++) {
			FD_SET(port_fd[i], &rfds);
			if (port_fd[i] > m) m = port_fd[i];
		}

		FD_SET(wakeup[0], &rfds);
		if (wakeup[0] > m) m = wakeup[0];
#endif

		do {
#ifdef USE_NETWORK_WS2
			// We have bound all of the sockets to this event object, so that waiting on
			// the event object is equivalent to the select() call.
			//
			// By coincidence, WaitForSingleObject and select both return the same value
			// on failure. WaitForSingleObject returns it as an unsigned (DWORD)0xFFFFFFFF,
			// while select returns it as a signed (int)-1. On Win32, we can assume that
			// int and DWORD are both 32-bit integers and we can just reinterpret it.
			i = (int)WaitForSingleObject(hWakeUp, INFINITE);
#else
# ifdef SCHISM_WIN32
			if (wakeup[0] == INVALID_SOCKET) {
				tv.tv_sec = 1;
				tv.tv_usec = 0;

				tv_ptr = &tv;
			}
# endif

			i = select(m+1, &rfds, NULL, NULL, tv_ptr);
#endif
			if (p->cancelled) {
				return 0;
			}
		} while (i == -1 && errno == EINTR);

#ifdef SCHISM_WIN32
# ifdef USE_NETWORK_WS2
		for (i = 0; i < real_num_ports; i++) {
			u_long avail;

			if ((0 == ioctlsocket(port_fd[i], FIONREAD, &avail))
			 && (avail != 0))
				_readin(p, i, port_fd[i]);
		}
# else
		if (wakeup[0] == INVALID_SOCKET) {
			/* if we didn't get a socket, then we can't poll on it */
		}
		else /* fall through */
 #endif
#endif

#ifndef USE_NETWORK_WS2
		if (FD_ISSET(wakeup[0], &rfds)) {
# ifdef SCHISM_WIN32
			if (recv(wakeup[0], buffer, sizeof(buffer), 0) == -1) {
				/* */
			}
# else
			if (read(wakeup[0], buffer, sizeof(buffer)) == -1) {
				/* fortify is stupid */
			}
# endif
		}
		for (i = 0; i < real_num_ports; i++) {
			if (FD_ISSET(port_fd[i], &rfds)) {
				if (state[i] & 1) _readin(p, i, port_fd[i]);
			}
		}
#endif

	}
	return 0;
}

static int _ip_start(struct midi_port *p)
{
	int n = INT_SHAPED_PTR(p->userdata);
	mt_mutex_lock(blocker);
	if (p->io & MIDI_INPUT)
		state[n] |= 1;
	if (p->io & MIDI_OUTPUT)
		state[n] |= 2;
	mt_mutex_unlock(blocker);
	do_wake_midi();
	return 1;
}
static int _ip_stop(struct midi_port *p)
{
	int n = INT_SHAPED_PTR(p->userdata);
	mt_mutex_lock(blocker);
	if (p->io & MIDI_INPUT)
		state[n] &= (~1);
	if (p->io & MIDI_OUTPUT)
		state[n] &= (~2);
	mt_mutex_unlock(blocker);
	do_wake_midi();
	return 1;
}

static void _ip_send(struct midi_port *p, const unsigned char *data, uint32_t len,
	SCHISM_UNUSED uint32_t delay)
{
	struct sockaddr_in asin = {0};
	unsigned char *ipcopy;
	int n = INT_SHAPED_PTR(p->userdata);
	int ss;

	if (len == 0) return;
	if (!(state[n] & 2)) return; /* blah... */

	asin.sin_family = AF_INET;
	ipcopy = (unsigned char *)&asin.sin_addr.s_addr;
	ipcopy[0] = 225; ipcopy[1] = ipcopy[2] = 0; ipcopy[3] = 37;
	asin.sin_port = htons(MIDI_IP_BASE+n);

	while (len) {
		ss = (len > MAX_DGRAM_SIZE) ?  MAX_DGRAM_SIZE : len;
		if (sendto(out_fd, (const char*)data, ss, 0,
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
	long i = 0;
	long m;

	mt_mutex_lock(blocker);
	m = (volatile int)real_num_ports;
	if (m < last_buildout) {
		ptr = NULL;
		while (midi_port_foreach(p, &ptr)) {
			i = INT_SHAPED_PTR(ptr->userdata);
			if (i >= m) {
				midi_port_unregister(ptr->num);
				//port_top[i] (the address where ptr points to) is freed in midi_port_unregister.
				//So clear it to avoid midi_port_foreach crashing on next round
				ptr = NULL;
			}
		}
		last_buildout = m;
	} else if (m > last_buildout) {
		for (i = last_buildout; i < m; i++) {
			buffer = NULL;
			if (asprintf(&buffer, " Multicast/IP MIDI %ld", i+1) == -1) {
				perror("asprintf");
				exit(255);
			}
			if (!buffer) {
				perror("asprintf");
				exit(255);
			}
			midi_port_register(p, MIDI_INPUT | MIDI_OUTPUT, buffer,
					PTR_SHAPED_INT((intptr_t)i), 0);
		}
		last_buildout = m;
	}
	mt_mutex_unlock(blocker);
}

static void _ip_wake(struct midi_provider* p)
{
	do_wake_midi();
}

int ip_midi_setup(void)
{
	static struct midi_driver driver;

	if (status.flags & NO_NETWORK) return 0;

	blocker = mt_mutex_create();
	if (!blocker)
		return 0;

#ifdef SCHISM_WIN32
# ifdef USE_NETWORK_WS2
	// Create an object:
	// - NULL: Do not specify the attributes needed for this to be inheritable.
	// - FALSE: The object is not manual-reset (waiting on it clears signals as they are received).
	// - FALSE: The object's initial state is not signalled.
	// - NULL: The object is not assigned a name, and thus cannot be accessed by other processes.

	hWakeUp = CreateEventA(NULL, FALSE, FALSE, NULL);
# else
	// Work-around for pre-WinSock 2 Windows: Make a loopback TCP connection. In the UNIX
	// implementation, we just call pipe() and use that. Win32, though, doesn't use file
	// descriptors and is just playing pretend for network sockets specifically. Its pipes
	// don't integrate into sockets and select(). So, use an actual socket.
	//
	// If we can't establish a connection, fall back on the old polling behaviour.

	wakeup[0] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	wakeup[1] = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if ((wakeup[0] == INVALID_SOCKET) || (wakeup[1] == INVALID_SOCKET)) {
		if (wakeup[0] != INVALID_SOCKET) {
			closesocket(wakeup[0]);
				wakeup[0] = INVALID_SOCKET;
		}
		if (wakeup[1] != INVALID_SOCKET) {
			closesocket(wakeup[1]);
			wakeup[1] = INVALID_SOCKET;
		}
	}
	else {
		int good = 0;
		struct sockaddr_in sin;
		u_long mode;

		sin.sin_family = AF_INET;
		sin.sin_addr.s_addr = inet_addr("127.0.0.1");
		sin.sin_port = 0;

		mode = 1;
		ioctlsocket(wakeup[1], FIONBIO, &mode);

		socklen_t sin_len = sizeof(sin);

		if ((bind(wakeup[0], (struct sockaddr *)&sin, sizeof(sin)) == 0)
		 && (listen(wakeup[0], 1) == 0)
		 && (getsockname(wakeup[0], (struct sockaddr *)&sin, &sin_len) == 0)
		 && (sin.sin_family == AF_INET)) {
			int result = connect(wakeup[1], (struct sockaddr *)&sin, sizeof(sin));

			if ((result == SOCKET_ERROR) && (WSAGetLastError() == WSAEWOULDBLOCK)) {
				// This is okay :-)

				SOCKET client_fd;

				sin_len = sizeof(sin);

				client_fd = accept(wakeup[0], (struct sockaddr *)&sin, &sin_len);

				closesocket(wakeup[0]);
				wakeup[0] = client_fd;

				mode = 1;
				ioctlsocket(wakeup[0], FIONBIO, &mode);

				// wakeup[0] is all good to go, make sure wakeup[1] got the message too
				{
					struct fd_set fds;
					struct timeval timeout;

					FD_ZERO(&fds);
					FD_SET(wakeup[1], &fds);

					timeout.tv_sec = 1;
					timeout.tv_usec = 0;

					good = select(1, NULL, &fds, NULL, &timeout);
				}
			}
		}

		if (!good) {
			closesocket(wakeup[0]);
			closesocket(wakeup[1]);
			wakeup[0] = INVALID_SOCKET;
			wakeup[1] = INVALID_SOCKET;
		}
	}
# endif
#else
	if (pipe(wakeup) == -1) {
		return 0;
	}
	fcntl(wakeup[0], F_SETFL, fcntl(wakeup[0], F_GETFL, 0) | O_NONBLOCK);
	fcntl(wakeup[1], F_SETFL, fcntl(wakeup[1], F_GETFL, 0) | O_NONBLOCK);
#endif

	if (out_fd == -1) {
		out_fd = _get_fd(-1, 1);
		if (out_fd == -1) return 0;
	}

	driver.flags = 0;
	driver.poll = _ip_poll;
	driver.wake = _ip_wake;
	driver.thread = _ip_thread;
	driver.send = _ip_send;
	driver.enable = _ip_start;
	driver.disable = _ip_stop;
	//TODO: Save number of MIDI-IP ports
	ip_midi_setports(DEFAULT_IP_PORT_COUNT);

	if (!midi_provider_register("IP", &driver)) return 0;
	return 1;
}

int ip_midi_getports(void)
{
	int tmp;

	if (out_fd == -1) return 0;
	if (status.flags & NO_NETWORK) return 0;

	mt_mutex_lock(blocker);
	tmp = (volatile int)real_num_ports;
	mt_mutex_unlock(blocker);
	return tmp;
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

