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

#include "bswap.h"
#include "str.h"
#include "mem.h"
#include "disko.h"
#include "network.h"
#include "network-tcp.h"
#include "log.h" /* status_text_flash */

#include <sys/types.h>
#include <sys/socket.h>
#include <unistd.h>
#include <netdb.h>
#include <fcntl.h>

/* macro for checking if TCP is in server mode */
#define TCP_ISSERVER(n) ((n)->cfd != (n)->sfd && (n)->cfd != (n)->sfd6)

static inline SCHISM_ALWAYS_INLINE
int network_tcp_socket_nonblock(int fd)
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
	int flags, r;

	flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		return -1;
	r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (r == -1)
		return -1;

	return 0;
}

/* timeout is hardcoded to 5 seconds for now
 * ideally this would be user-configurable :) */
static inline SCHISM_ALWAYS_INLINE
void network_tcp_socket_timeout(int fd, uint64_t us)
{
	struct timeval tv;

	tv.tv_sec  = us / 1000000;
	tv.tv_usec = us % 1000000;

	/* hope this succeeds ... */
	setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
	setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &tv, sizeof(tv));
}

static int network_tcp_create_socket(int *pfd, int which)
{
	int fd;

	fd = socket(which, SOCK_STREAM, 0);
	if (fd < 0)
		return -1;

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
	n->sfd = n->cfd = n->sfd6 = -1;

	n->init = 1;

	n->s.msgsize = MSGSIZE_MAX;

	if ((network_tcp_create_socket(&n->sfd, AF_INET) < 0)
		|| (network_tcp_create_socket(&n->sfd6, AF_INET6) < 0))
		return -1;

#ifdef IPV6_V6ONLY
	if (n->sfd != -1 && n->sfd6 != -1) {
		/* disable IPv6 dual-stack shit */
		int optval;

		optval = 1;

		if (setsockopt(n->sfd6, IPPROTO_IPV6, IPV6_V6ONLY, &optval, sizeof(optval)) < 0) {
			log_perror("setsockopt IPV6_V6ONLY");
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

	close(n->sfd);
	close(n->sfd6);

	/* if we're running a server, then cfd is a completely
	 * different file descriptor. */
	if (TCP_ISSERVER(n))
		close(n->cfd);

	/* reset fd values */
	n->sfd  = -1;
	n->sfd6 = -1;
	n->cfd  = -1;

	n->r.msgsize = 0;
	n->r.msgrecv = 0;
	free(n->r.membuf);
	n->r.membuf = NULL;

	n->init = 0;
}

static int network_tcp_fd_bind_listen(int fd, struct sockaddr *addr, socklen_t addrlen, uint16_t port)
{
	if (bind(fd, addr, addrlen) < 0) {
		log_perror("bind");
		return -1;
	}

	/* as of now, we only allow one peer. but, we could allow multiple
	 * peers if we really wanted to. */
	if (listen(fd, 1) < 0) {
		log_perror("listen");
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
		|| (network_tcp_fd_bind_listen(n->sfd6, (struct sockaddr *)&addrip6, sizeof(addrip6), port) < 0))
		return -1;

	log_nl();
	log_appendf(1, "Starting server on port %u", (unsigned int)port);
	status_text_flash("Starting server on port %u", (unsigned int)port);

	return 0;
}

int network_tcp_start_client(struct network_tcp *n, const char *node,
	uint16_t port)
{
	int fd;
	char buf[11];
	struct addrinfo *info;
	struct addrinfo hints = {0};

	if (!n || !n->init)
		return -1;

	hints.ai_family = AF_UNSPEC; /* IPv4 or IPv6 */
	hints.ai_socktype = SOCK_STREAM;

	/* convert port number to a string, because getaddrinfo is weird */
	str_from_num(0, port, buf);

	if (getaddrinfo(node, buf, &hints, &info)) {
		log_nl();
		log_perror("[NET] getaddrinfo");
		return -1;
	}

	/* which socket we use depends on which IP version is being used */
	switch (info->ai_family) {
	case AF_INET:
		fd = n->sfd;
		break;
	case AF_INET6:
		fd = n->sfd6;
		break;
	default:
		return -1;
	}

	/* TODO the example code in the manpage actually goes over
	 * the entire linked list, trying each. maybe we should do that too? */
	if (connect(fd, info->ai_addr, info->ai_addrlen) < 0
		&& errno != EINPROGRESS) {
		log_nl();
		log_perror("[NET] connect failed");
		freeaddrinfo(info);
		return -1;
	}

	freeaddrinfo(info);

	/* fuck it, whatever */
	n->cfd = fd;

	/* tell the worker function that we're currently connecting */
	n->connecting = 1;

	log_nl();
	log_appendf(1, "[NET] Connecting to host %s on port %u",
		node, (unsigned int)port);
	status_text_flash("Connecting to host %s on port %u",
		node, (unsigned int)port);

	return 0;
}

/* Schism prepends a 32-bit little endian unsigned integer to each transmitted
 * message, which notates the size of the next N bytes. */

int network_tcp_send(struct network_tcp *n, const void *data, uint32_t size)
{
	uint8_t x;
	size_t off;
	uint32_t dw;
	int r;

	if (!n || !n->init)
		return -1;

	/* FIXME we need a queue, if the connection is initializing ... */
	if (n->cfd == -1)
		return -1;

	/* transmit size as 32-bit little endian */
	dw = bswapLE32(size);

	r = write(n->cfd, &dw, 4);
	if (r < 0) {
		log_perror("[NET] send (size)");
		return -1; /* peer probably disconnected; abandon ship */
	}

	/* now transmit the data */
	while (size > n->s.msgsize) {
		/* run the data in chunks */
		r = write(n->cfd, (const uint8_t *)data + off, n->s.msgsize);
		if (r < 0 && errno == EMSGSIZE) {
			/* msgsize is too big. reduce it, and try again */
			if (n->s.msgsize <= 8)
				return -1; /* lost cause */

			n->s.msgsize /= 2;
			continue;
		} else if (r < 0) {
			log_perror("[NET] send (block)");
		}

		size -= n->s.msgsize;
		off += n->s.msgsize;
	}

	if (write(n->cfd, data + off, size) < 0) {
		/* 99% of gamblers quit before they win big */
		log_perror("[NET] send (final block)");
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
	char host[256]; /* probably good enough */

	/* TODO we shouldn't just be accepting connections willy nilly.
	 * We should create a dialog asking the user if they
	 * want to accept the connection. */
	addrlen = sizeof(addr);
	n->cfd = accept(n->sfd, (struct sockaddr *)&addr, &addrlen);
	if (n->cfd < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) {
		/* try again with ipv6? */
		addrlen = sizeof(addr);
		n->cfd = accept(n->sfd6, (struct sockaddr *)&addr, &addrlen);
		if (n->cfd < 0)
			return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;
	}

	if (network_tcp_socket_nonblock(n->cfd) < 0)
		return -1;

	network_tcp_socket_timeout(n->sfd, 5000000);

#ifdef HAVE_GETNAMEINFO
	if (!getnameinfo((struct sockaddr *)&addr, addrlen, host, sizeof(host), NULL, 0, 0)) {
		log_appendf(4, "Accepted connection from %s", host);
		status_text_flash("Accepted connection from %s", host);
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

			log_appendf(4, "Accepted connection from " I6F, I6X(ip6));
			status_text_flash("Accepted connection from " I6F, I6X(ip6));
			break;
		}
		case AF_INET: {
			/* ehhh  */
			unsigned char *ip = (unsigned char *)&addr.i4.sin_addr.s_addr;
			log_appendf(4, "Accepted connection from %d.%d.%d.%d",
				(int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3]);
			status_text_flash("Accepted connection from %d.%d.%d.%d",
				(int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3]);
			break;
		}
		default: {
			log_appendf(4, "Accepted connection from unknown address");
			status_text_flash("Accepted connection from unknown address");
			break;
		}
		}
	}

	/* call back into the toplevel networking layer, so it can
	 * initialize the connection */
	Network_OnConnect();

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

	if (TCP_ISSERVER(n) && n->cfd == -1) {
		if (network_tcp_server_worker(n) < 0) {
			if (n->cfd != -1)
				close(n->cfd);
			n->cfd = -1;

			return -1;
		}
	}

	if (n->connecting) {
		/* We are a client doing a non-blocking connect() */
		fd_set fds;
		struct timeval tv;
		int r, err;
		socklen_t len;

		FD_ZERO(&fds);
		FD_SET(n->cfd, &fds);

		/* set the timeout to zero, so we can poll effectively */
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		printf("fd: %d\n", n->cfd);

		/* Check if we're connected */
		r = select(n->cfd + 1, NULL, &fds, NULL, &tv);
		if (r == 0) {
			/* connect() isn't done yet */
			printf("connect not finished\n");
			return 0;
		} else if (r < 0) {
			log_perror("[NET] select");
			return -1;
		}

		/* okay, we're writable. now check whether connect() succeeded */

		len = sizeof(err);
		r = getsockopt(n->cfd, SOL_SOCKET, SO_ERROR, &err, &len);
		if (r < 0 || err) {
			log_appendf(4, "[NET] Connection failed: %s", strerror(err));
			status_text_flash("Connection failed: %s", strerror(err));
			n->cfd = -1;
			return -1;
		}

		n->connecting = 0;

		/* call back into the toplevel networking layer */
		Network_OnConnect();
	}

	if (n->cfd == -1)
		return 0; /* nothing to do */

#define TCP_READ(fd, buf, size) \
do { \
	r = read(fd, buf, size); \
	if (r < 0) { \
		return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1; \
	} else if (r == 0) { \
		if (TCP_ISSERVER(n)) { \
			close(n->cfd); \
			n->cfd = -1; \
	\
			n->r.msgsize = 0; \
			n->r.msgrecv = 0; \
			free(n->r.membuf); \
			n->r.membuf = NULL; \
	\
			log_appendf(4, "[NET] Client disconnected"); \
			status_text_flash("Client disconnected"); \
			return 0; \
		} else { \
			/* there's nothing we can do, just close the connection */ \
			network_tcp_close(n); \
			log_appendf(4, "[NET] Lost connection to server"); \
			status_text_flash("Lost connection to server"); \
			return 0; \
		} \
	} \
} while (0)

	for (;;) {
		ssize_t r;
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
