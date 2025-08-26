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

#include "bits.h"
#include "str.h"
#include "mem.h"
#include "disko.h"
#include "network.h"
#include "network-tcp.h"
#include "log.h" /* status_text_flash */
#include "osdefs.h" /* SCHISM_ANSI_UNICODE */

#ifdef SCHISM_WIN32
# include <ws2tcpip.h>
# include <windows.h>

# define SOCKET_EMSGSIZE    WSAEMSGSIZE
# define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
# define SOCKET_EINPROGRESS WSAEINPROGRESS
# define SOCKET_LASTERROR   (WSAGetLastError())
# define SOCKET_INVALID     INVALID_SOCKET

typedef ADDRESS_FAMILY sa_family_t;
#else
# include <sys/types.h>
# include <sys/socket.h>
# include <unistd.h>
# include <netdb.h>
# include <fcntl.h>

# define closesocket    close
# define SOCKET_ERROR   (-1)
# define SOCKET_INVALID (-1)

# define SOCKET_EMSGSIZE    EMSGSIZE
# define SOCKET_EWOULDBLOCK EWOULDBLOCK
# define SOCKET_EINPROGRESS EINPROGRESS
# ifdef EAGAIN
#  define SOCKET_EAGAIN     EAGAIN
# endif
# define SOCKET_LASTERROR   errno
#endif

#ifndef SOCKET_EAGAIN
# define SOCKET_EAGAIN SOCKET_EWOULDBLOCK
#endif

#define TCP_ISSERVER(n) ((n)->server)

/* perror(), but cross-platform */
static void network_tcp_perror(const char *prefix, int err)
{
#ifdef SCHISM_WIN32
	/* this much code just to get an error message is insane */
	char *msg = NULL;

	SCHISM_ANSI_UNICODE({
		LPSTR buf;
		FormatMessageA(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
			NULL,
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPSTR)&buf,
			0,
			NULL
		);
		charset_iconv(buf, &msg, CHARSET_ANSI, CHARSET_UTF8, SIZE_MAX);
		LocalFree(buf);
	}, {
		LPWSTR buf;
		FormatMessageW(
			FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS | FORMAT_MESSAGE_MAX_WIDTH_MASK,
			NULL,
			err,
			MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
			(LPWSTR)&buf,
			0,
			NULL
		);
		charset_iconv(buf, &msg, CHARSET_WCHAR_T, CHARSET_UTF8, SIZE_MAX);
		LocalFree(buf);
	})

	if (msg) {
		log_appendf(4, "[NETWORK] %s: %s", prefix, msg);
		free(msg);
	}
#else
	/* windows could never be this simple */
	log_appendf(4, "[NETWORK] %s: %s", prefix, strerror(err));
#endif
}

static inline SCHISM_ALWAYS_INLINE
int network_tcp_socket_nonblock(network_tcp_socket_t fd)
{
	/* Convert the socket to a non-blocking socket.
	 *
	 * We require this behavior, because if we block, then that means
	 * the GUI doesn't get updated. However, it means the logic is
	 * slightly more convoluted. (i.e. need to do stuff in the event
	 * handler)
	 *
	 * This way we don't need to spin up a thread though, so it all
	 * works out ;) */

#ifdef SCHISM_WIN32
	int r;
	u_long mode = 1;

	r = ioctlsocket(fd, FIONBIO, &mode);
	if (r != 0) {
		network_tcp_perror("ioctlsocket", r);
		return -1;
	}

	return 0;
#else
	int flags, r;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1) {
		network_tcp_perror("fcntl", errno);
		return -1;
	}
	r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (r == -1) {
		network_tcp_perror("fcntl", errno);
		return -1;
	}

	return 0;
#endif
}

/* timeout is hardcoded to 5 seconds for now
 * ideally this would be user-configurable :) */
static inline SCHISM_ALWAYS_INLINE
void network_tcp_socket_timeout(network_tcp_socket_t fd, uint64_t us)
{
#ifdef SCHISM_WIN32
	DWORD tv;

	tv = (us / 1000);
#else
	struct timeval tv;

	tv.tv_sec  = us / 1000000;
	tv.tv_usec = us % 1000000;
#endif

	/* hope this succeeds ...
	 * the stupid cast is for windows */
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, (const char *)&tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, (const char *)&tv, sizeof(tv));
}

static int network_tcp_create_socket(network_tcp_socket_t *pfd, int which)
{
	network_tcp_socket_t fd;

	fd = socket(which, SOCK_STREAM, 0);
	if (fd == SOCKET_INVALID) {
		network_tcp_perror("socket", SOCKET_LASTERROR);
		return -1;
	}

	if (network_tcp_socket_nonblock(fd) < 0)
		return -1;

	network_tcp_socket_timeout(fd, 5000000);

	*pfd = fd;
	return 0;
}

int network_tcp_init(struct network_tcp *n)
{
	int r;

	/* zero everything */
	memset(n, 0, sizeof(*n));

	/* 0 is stdin */
	n->sfd = n->cfd = n->sfd6 = SOCKET_INVALID;

	n->init = 1;

	n->s.msgsize = MSGSIZE_MAX;

	if ((network_tcp_create_socket(&n->sfd, AF_INET) < 0)
		&& (network_tcp_create_socket(&n->sfd6, AF_INET6) < 0)) {
		return -1;
	}

#ifdef IPV6_V6ONLY
	if (n->sfd != SOCKET_INVALID && n->sfd6 != SOCKET_INVALID) {
		/* disable IPv6 dual-stack shit */
		int optval;

		optval = 1;

		if (setsockopt(n->sfd6, IPPROTO_IPV6, IPV6_V6ONLY, (char *)&optval, sizeof(optval))) {
			network_tcp_perror("setsockopt IPV6_V6ONLY", SOCKET_LASTERROR);
			return -1;
		}
	}
#endif

	return 0;
}

void network_tcp_close(struct network_tcp *n)
{
	if (!n || !n->init)
		return;

	closesocket(n->sfd);
	closesocket(n->sfd6);

	/* if we're running a server, then cfd is a completely
	 * different file descriptor. */
	if (TCP_ISSERVER(n))
		closesocket(n->cfd);

	/* reset fd values */
	n->sfd = n->sfd6 = n->cfd = SOCKET_INVALID;

	n->r.msgsize = 0;
	n->r.msgrecv = 0;
	free(n->r.membuf);
	n->r.membuf = NULL;

	n->init = 0;
}

static int network_tcp_fd_bind_listen(int fd, struct sockaddr *addr, socklen_t addrlen, uint16_t port)
{
	if (bind(fd, addr, addrlen) == SOCKET_ERROR) {
		network_tcp_perror("bind", SOCKET_LASTERROR);
		return -1;
	}

	/* as of now, we only allow one peer. but, we could allow multiple
	 * peers if we really wanted to. */
	if (listen(fd, 1) == SOCKET_ERROR) {
		network_tcp_perror("listen", SOCKET_LASTERROR);
		return -1;
	}

	return 0;
}

int network_tcp_start_server(struct network_tcp *n, uint16_t port)
{
	struct sockaddr_in  addrip4;
	struct sockaddr_in6 addrip6;

	if (!n || !n->init)
		return -1;

	addrip4.sin_family = AF_INET;
	addrip4.sin_addr.s_addr = INADDR_ANY;
	addrip4.sin_port = bswapBE16(port);  /* network endian */

	addrip6.sin6_family = AF_INET6;
	addrip6.sin6_addr = in6addr_any;
	addrip6.sin6_port = bswapBE16(port); /* network endian */

	if ((network_tcp_fd_bind_listen(n->sfd, (struct sockaddr *)&addrip4, sizeof(addrip4), port) < 0)
		&& (network_tcp_fd_bind_listen(n->sfd6, (struct sockaddr *)&addrip6, sizeof(addrip6), port) < 0))
		return -1;

	log_nl();
	log_appendf(2, "[NETWORK] Starting server");
	log_underline();
	status_text_flash("[NETWORK] Starting server");

	if (n->sfd != SOCKET_INVALID && n->sfd6 != SOCKET_INVALID)
		log_appendf(5, " Using IPv4 and IPv6");
	else if (n->sfd != SOCKET_INVALID)
		log_appendf(5, " Using IPv4 only");
	else if (n->sfd6 != SOCKET_INVALID)
		log_appendf(5, " Using IPv6 only");

	log_appendf(5, " Listening on TCP port %u", (unsigned int)port);

	n->server = 1;

	return 0;
}

/* to be honest, this is probably the best way we can handle getaddrinfo()
 * and gethostbyname(), without having to repeat everything unnecessarily. */
static int network_tcp_enumerate_host_ips(const char *node, uint16_t port,
	int (*cb)(struct sockaddr *addr, size_t addrlen, void *userdata),
	void *userdata)
{
#ifdef HAVE_GETADDRINFO
	char buf[11];
	struct addrinfo *info;
	struct addrinfo hints = {0};
#endif

#ifdef HAVE_GETADDRINFO
	hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;

	str_from_num(0, port, buf);

	if (!getaddrinfo(node, buf, &hints, &info)) {
		struct addrinfo *pi;

		for (pi = info; pi; pi = pi->ai_next) {
			if (!cb(pi->ai_addr, pi->ai_addrlen, userdata))
				continue;

			freeaddrinfo(info);
			return 0;
		}

		freeaddrinfo(info);
	} else
#endif
	{
		/* ancient API from before IPv6 was a thing people actually used */
		char **s;
		struct hostent *host;

#ifdef HAVE_GETADDRINFO
		network_tcp_perror("getaddrinfo", SOCKET_LASTERROR);
#endif

		host = gethostbyname(node);
		if (!host) {
			log_nl();
			network_tcp_perror("gethostbyname", SOCKET_LASTERROR);
			return -1;
		}

		/* verify */
		switch (host->h_addrtype) {
		case AF_INET: {
			struct sockaddr_in addr;

			if (host->h_length < sizeof(struct in_addr))  /* 32-bit */
				return -1;

			for (s = host->h_addr_list; *s; s++) {
				memset(&addr, 0, sizeof(addr));

				addr.sin_family = AF_INET;
				memcpy(&addr.sin_addr, *s, sizeof(addr.sin_addr));
				addr.sin_port = bswapBE16(port);

				if (cb((struct sockaddr *)&addr, sizeof(addr), userdata))
					return 0;
			}

			break;
		}
		case AF_INET6: {
			struct sockaddr_in6 addr;

			if (host->h_length < sizeof(struct in6_addr))  /* 32-bit */
				return -1;

			for (s = host->h_addr_list; *s; s++) {
				memset(&addr, 0, sizeof(addr));

				addr.sin6_family = AF_INET6;
				memcpy(&addr.sin6_addr, *s, sizeof(addr.sin6_addr));
				addr.sin6_port = bswapBE16(port);

				if (cb((struct sockaddr *)&addr, sizeof(addr), userdata))
					return 0;
			}

			break;
		}
		default:
			/* what */
			return -1;
		}
	}

	return -1;
}

struct network_tcp_enum_data {
	struct network_tcp *n;
	uint16_t port;
};

static int network_tcp_enum_cb(struct sockaddr *addr, size_t addrlen, void *userdata)
{
	struct network_tcp_enum_data *data = userdata;
	struct network_tcp *n = data->n;
	network_tcp_socket_t fd;

	switch (addr->sa_family) {
	case AF_INET: {
		fd = n->sfd;
		break;
	}
	case AF_INET6:
		fd = n->sfd6;
		break;
	default:
		return 0;
	}

	if (fd == SOCKET_INVALID)
		return 0; /* do nothing */

	if ((connect(fd, addr, addrlen) == SOCKET_ERROR)
		&& SOCKET_LASTERROR != SOCKET_EINPROGRESS
		&& SOCKET_LASTERROR != SOCKET_EWOULDBLOCK /* winsocks */) {
		network_tcp_perror("connect", SOCKET_LASTERROR);
		return 0;
	}

	/* got a connection */
	n->cfd = fd;
	n->connecting = 1;

	return 1;
}

int network_tcp_start_client(struct network_tcp *n, const char *node,
	uint16_t port)
{
	struct network_tcp_enum_data data;
	char buf[11];

	if (!n || !n->init)
		return -1;

	data.n = n;
	data.port = port;

	if (network_tcp_enumerate_host_ips(node, port, network_tcp_enum_cb,
		&data) < 0)
		return -1;

	if (!n->connecting)
		return -1; /* what */

	log_nl();
	log_appendf(1, "[NETWORK] Connecting to host %s on port %u",
		node, (unsigned int)port);
	status_text_flash("Connecting to host %s on port %u",
		node, (unsigned int)port);

	n->server = 0;

	return 0;
}

/* Schism prepends a 32-bit little endian unsigned integer to each transmitted
 * message, which notates the size of the next N bytes. */

int network_tcp_send(struct network_tcp *n, const void *data, uint32_t size)
{
	uint8_t x;
	size_t off;
	uint32_t dw;
#ifdef SCHISM_WIN32
	int r;
#else
	ssize_t r;
#endif

	if (!n || !n->init)
		return -1;

	/* FIXME we need a queue, if the connection is initializing ... */
	if (n->cfd == SOCKET_INVALID)
		return -1;

	/* transmit size as 32-bit little endian */
	dw = bswapLE32(size);

	r = send(n->cfd, (char *)&dw, 4, 0);
	if (r == SOCKET_ERROR) {
		network_tcp_perror("send (size)", SOCKET_LASTERROR);
		return -1; /* peer probably disconnected; abandon ship */
	}

	off = 0;

	/* now transmit the data */
	while (size > n->s.msgsize) {
		/* run the data in chunks */
		r = send(n->cfd, (char *)data + off, n->s.msgsize, 0);
		if (r == SOCKET_ERROR && SOCKET_LASTERROR == SOCKET_EMSGSIZE) {
			/* msgsize is too big. reduce it, and try again */
			if (n->s.msgsize <= 8)
				return -1; /* lost cause */

			n->s.msgsize /= 2;
			continue;
		} else if (r < 0) {
			network_tcp_perror("send (block)", SOCKET_LASTERROR);
		}

		size -= n->s.msgsize;
		off += n->s.msgsize;
	}

	r = send(n->cfd, (char *)data + off, size, 0);
	if (r == SOCKET_ERROR) {
		/* 99% of gamblers quit before they win big */
		network_tcp_perror("send (final block)", SOCKET_LASTERROR);
		return -1;
	}

	return 0;
}

static int network_tcp_server_worker(struct network_tcp *n)
{
	union {
		struct sockaddr i;
		struct sockaddr_in i4;
		struct sockaddr_in6 i6;
	} addr;
	socklen_t addrlen;
#ifdef HAVE_GETNAMEINFO
	char host[256]; /* probably big enough buffer */
#endif
	network_tcp_socket_t socks[] = { n->sfd, n->sfd6 };
	size_t i;
#ifdef HAVE_GETHOSTBYADDR
	struct hostent *hent;
#endif

	/* TODO we shouldn't just be accepting connections willy nilly.
	 * We should create a dialog asking the user if they
	 * want to accept the connection. */
	for (i = 0; i < ARRAY_SIZE(socks); i++) {
		/* ignore any invalid sockets */
		if (socks[i] == SOCKET_INVALID)
			continue;

		addrlen = sizeof(addr);
		n->cfd = accept(socks[i], &addr.i, &addrlen);
		if (n->cfd == SOCKET_INVALID) {
			/* I have no idea why this would return these errors, but whatever.. */
			if (SOCKET_LASTERROR != SOCKET_EAGAIN && SOCKET_LASTERROR != SOCKET_EWOULDBLOCK)
				network_tcp_perror("accept", SOCKET_LASTERROR);
			continue;
		}
	}

	/* one last check for if any of the accept calls failed, or
	 * we have no server sockets (which shouldn't happen) */
	if (n->cfd == SOCKET_INVALID)
		return -1; /* failed */

	if (network_tcp_socket_nonblock(n->cfd) < 0)
		return -1;

	network_tcp_socket_timeout(n->cfd, 5000000);

	/* print out the hostname */
#ifdef HAVE_GETNAMEINFO
	if (!getnameinfo((struct sockaddr *)&addr, addrlen, host, sizeof(host), NULL, 0, 0)) {
		log_appendf(4, "Accepting connection from %s", host);
		status_text_flash("Accepting connection from %s", host);
	} else
#endif
#ifdef HAVE_GETHOSTBYADDR
	if ((hent = (addr.i.sa_family == AF_INET6)
			? gethostbyaddr((char *)&addr.i6.sin6_addr, 16, AF_INET6)
			: gethostbyaddr((char *)&addr.i4.sin_addr, 4, AF_INET))) {
		/* nice spaghetti to get here */
		log_appendf(4, "Accepting connection from %s", hent->h_name);
		status_text_flash("Accepting connection from %s", hent->h_name);
	} else
#endif
	{
		switch (addr.i.sa_family) {
		case AF_INET6: {
			unsigned char *ip6 = addr.i6.sin6_addr.s6_addr;

#define I6F "%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x:%02x%02x"
#define I6X(ip6) \
	(unsigned int)ip6[0],  (unsigned int)ip6[1], \
	(unsigned int)ip6[2],  (unsigned int)ip6[3], \
	(unsigned int)ip6[4],  (unsigned int)ip6[7], \
	(unsigned int)ip6[6],  (unsigned int)ip6[8], \
	(unsigned int)ip6[8],  (unsigned int)ip6[9], \
	(unsigned int)ip6[10], (unsigned int)ip6[11], \
	(unsigned int)ip6[12], (unsigned int)ip6[13], \
	(unsigned int)ip6[14], (unsigned int)ip6[15]

			log_appendf(4, "Accepting connection from " I6F, I6X(ip6));
			status_text_flash("Accepting connection from " I6F, I6X(ip6));
			break;
		}
		case AF_INET: {
			/* ehhh  */
			unsigned char *ip = (unsigned char *)&addr.i4.sin_addr.s_addr;
			log_appendf(4, "Accepting connection from %d.%d.%d.%d",
				(int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3]);
			status_text_flash("Accepting connection from %d.%d.%d.%d",
				(int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3]);
			break;
		}
		default: {
			log_appendf(4, "Accepting connection from unknown address");
			status_text_flash("Accepting connection from unknown address");
			break;
		}
		}
	}

	Network_OnConnect();
	Network_OnServerConnect();

	return 0;
}

/* This function handles any incoming bytes, and does routine
 * maintenance with the TCP connection (such as handling nonblocking
 * connect(), handling new connections, etc) */
int network_tcp_worker(struct network_tcp *n)
{
	SCHISM_RUNTIME_ASSERT(n, "no NULL pointer");

	if (!n || !n->init)
		return -1;

	if (TCP_ISSERVER(n) && n->cfd == SOCKET_INVALID) {
		if (network_tcp_server_worker(n) < 0) {
			if (n->cfd != SOCKET_INVALID)
				closesocket(n->cfd);
			n->cfd = SOCKET_INVALID;

			return -1;
		}
	}

	if (n->connecting) {
		/* We are a client doing a non-blocking connect() */
		fd_set fds;
		struct timeval tv;
		int r, err;
		socklen_t len;

		if (n->cfd == SOCKET_INVALID) {
			/* previous connection failed; just set it here
			 * this logic is totally asinine, but just hear me out */
			n->connecting = 0;
			return -1;
		}

		FD_ZERO(&fds);
		FD_SET(n->cfd, &fds);

		/* set the timeout to zero, so we can poll effectively */
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		/* Check if we're connected */
		r = select(n->cfd + 1, NULL, &fds, NULL, &tv);
		if (r == 0) {
			/* connect() isn't done yet */
			return 0;
		} else if (r == SOCKET_ERROR) {
			network_tcp_perror("select", SOCKET_LASTERROR);
			return -1;
		}

		/* okay, we're writable. now check whether connect() succeeded */

		len = sizeof(err);
		/* stupid cast for winsocks */
		r = getsockopt(n->cfd, SOL_SOCKET, SO_ERROR, (char *)&err, &len);
		if (r < 0 || err) {
			network_tcp_perror("Connection failed", err);
			// need to port to winblows
			//status_text_flash("Connection failed: %s", strerror(err));
			closesocket(n->cfd);
			n->cfd = SOCKET_INVALID;
			return -1;
		}

		if (network_tcp_socket_nonblock(n->cfd) < 0) {
			closesocket(n->cfd);
			n->cfd = SOCKET_INVALID;
			return -1;
		}

		network_tcp_socket_timeout(n->cfd, 5000000);

		n->connecting = 0;

		/* call back into the toplevel networking layer */
		Network_OnConnect();
	}

	if (n->cfd == SOCKET_INVALID)
		return 0; /* nothing to do */

#define TCP_READ(fd, buf, size) \
do { \
	r = recv(fd, (char *)buf, size, 0); \
	if (r == SOCKET_ERROR) { \
		return (SOCKET_LASTERROR == SOCKET_EAGAIN || SOCKET_LASTERROR == SOCKET_EWOULDBLOCK) ? 0 : -1; \
	} else if (r == 0) { \
		if (TCP_ISSERVER(n)) { \
			closesocket(n->cfd); \
			n->cfd = SOCKET_INVALID; \
	\
			n->r.msgsize = 0; \
			n->r.msgrecv = 0; \
			free(n->r.membuf); \
			n->r.membuf = NULL; \
	\
			log_appendf(4, "[NETWORK] Client disconnected"); \
			status_text_flash("Client disconnected"); \
			return 0; \
		} else { \
			/* there's nothing we can do, just close the connection */ \
			network_tcp_close(n); \
			log_appendf(4, "[NETWORK] Lost connection to server"); \
			status_text_flash("Lost connection to server"); \
			return 0; \
		} \
	} \
} while (0)

	for (;;) {
#ifdef SCHISM_WIN32
		int r;
#else
		ssize_t r;
#endif
		unsigned char buf[4096]; /* 4KiB buffer */
		size_t left;

		if (!n->r.msgsize) {
			/* finished processing a message */
			uint32_t dw;

			TCP_READ(n->cfd, &dw, 4);

			if (r != 4) {
				/* something is totally wrong, or some packet somewhere
				 * was dropped */
				return -1;
			}

			dw = bswapLE32(dw);

			n->r.msgsize = dw;
			n->r.msgrecv = 0;
			n->r.membuf = mem_alloc(n->r.msgsize);
		}

		/* Only read as much data as we're expecting.
		 * This effectively handles the case where we have
		 * another message waiting. */
		left = (n->r.msgsize - n->r.msgrecv);
		left = MIN(left, sizeof(buf));

		if (!left)
			return 0; /* nothing to do */

		TCP_READ(n->cfd, buf, left);

		if (n->r.msgrecv + r > n->r.msgsize)
			return -1; /* wut? lol */

		memcpy(n->r.membuf + n->r.msgrecv, buf, r);

		n->r.msgrecv += r;

		if (n->r.msgrecv == n->r.msgsize) {
			/* if we've received the full message, pass the data off
			 * for the network subsystem to take care of */
			Network_ReceiveData(n->r.membuf, n->r.msgsize);

			/* reset the variables */
			n->r.msgsize = 0;
			n->r.msgrecv = 0;
			free(n->r.membuf);
			n->r.membuf = NULL;
		}
	}

#undef TCP_READ

	return 0;
}
