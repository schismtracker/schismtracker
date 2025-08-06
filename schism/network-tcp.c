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
#define TCP_ISSERVER(n) ((n)->cfd != (n)->sfd)

static inline SCHISM_ALWAYS_INLINE
int network_tcp_socket_nonblock(int fd)
{
	int flags, r;

	/* Convert the socket to a non-blocking socket.
	 * We need this to do anything properly ;) */
	flags = fcntl(fd, F_GETFL);
	if (flags == -1)
		return -1;
	r = fcntl(fd, F_SETFL, flags | O_NONBLOCK);
	if (r == -1)
		return -1;

	return 0;
}

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

int network_tcp_init(struct network_tcp *n)
{
	int r;

	/* zero everything */
	memset(n, 0, sizeof(*n));

	/* 0 is stdin */
	n->sfd = n->cfd = -1;

	n->init = 1;

	n->s.msgsize = MSGSIZE_MAX;

	/* XXX it's 2025. we should support IPv6 */
	n->sfd = socket(AF_INET, SOCK_STREAM, 0);
	if (n->sfd == -1) {
		log_perror("socket");
		return -1;
	}

	if (network_tcp_socket_nonblock(n->sfd) < 0)
		return -1;

	network_tcp_socket_timeout(n->sfd, 5000000);

	return 0;
}

void network_tcp_close(struct network_tcp *n)
{
	if (!n || !n->init)
		return;

	if (n->sfd == n->cfd) {
		close(n->sfd);
	} else {
		close(n->sfd);
		close(n->cfd);
	}

	free(n->r.membuf);
	n->r.membuf = NULL;

	n->init = 0;
}

int network_tcp_start_server(struct network_tcp *n, uint16_t port)
{
	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = bswapBE16(port); /* big endian is network endian */

	/* bind to literally anything */
	if (bind(n->sfd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
		log_perror("bind");
		return -1;
	}

	if (listen(n->sfd, 1) < 0) {
		log_perror("listen");
		return -1;
	}

	return 0;
}

int network_tcp_start_client(struct network_tcp *n, const char *node,
	uint16_t port)
{
	char buf[11];
	struct addrinfo *info;
	struct addrinfo hints = {0};

	hints.ai_family = AF_INET; /* TODO AF_UNSPEC for IPv6 */
	hints.ai_socktype = SOCK_STREAM; /* TCP */

	/* convert port number to a string, because getaddrinfo is weird */
	str_from_num(0, port, buf);

	if (getaddrinfo(node, buf, &hints, &info)) {
		log_perror("getaddrinfo");
		return -1;
	}

	if (connect(n->sfd, info->ai_addr, info->ai_addrlen) < 0
		&& errno != EINPROGRESS) {
		log_perror("connect");
		freeaddrinfo(info);
		return -1;
	}

	freeaddrinfo(info);

	/* fuck it, whatever */
	n->cfd = n->sfd;

	n->connecting = 1;

	/* FIXME send some info about the client */
	status_text_flash("connecting");

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

	if (!n->init)
		return -1;

	/* we need a queue, if the connection is initializing ... */
	if (n->cfd == -1)
		return -1;

	/* transmit size as 32-bit little endian */
	dw = bswapLE32(size);

	r = send(n->cfd, &dw, 4, 0);
	if (r < 0) {
		log_perror("send 1");
		return -1; /* peer probably disconnected; abandon ship */
	}

	/* now transmit the data */
	while (size > n->s.msgsize) {
		/* run the data in chunks */
		r = send(n->cfd, (const uint8_t *)data + off, n->s.msgsize, 0);
		if (r < 0 && errno == EMSGSIZE) {
			/* msgsize is too big. reduce it, and try again */
			if (n->s.msgsize <= 8)
				return -1; /* lost cause */

			n->s.msgsize /= 2;
			continue;
		} else if (r < 0) {
			log_perror("send 2");
		}

		size -= n->s.msgsize;
		off += n->s.msgsize;
	}

	if (send(n->cfd, data + off, size, 0) < 0) {
		log_perror("send 3");
		/* 99% of gamblers quit before they win big */
		return -1;
	}

	return 0;
}

/* This function should be called every so often by main.c
 * (or some function in network.c) */
static int network_tcp_server_worker(struct network_tcp *n)
{
	struct sockaddr_in addr = {0};
	int addrlen = sizeof(addr);

	/* FIXME we shouldn't just be accepting connections willy nilly */
	n->cfd = accept(n->sfd, (struct sockaddr *)&addr, &addrlen);
	if (n->cfd < 0)
		return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1;

	if (network_tcp_socket_nonblock(n->cfd) < 0) {
		printf("failed to make nonblock\n");
		return -1;
	}

	network_tcp_socket_timeout(n->sfd, 5000000);

	if (addr.sin_family == AF_INET) {
		/* ehhh */
		unsigned char *ip = (unsigned char *)&addr.sin_addr.s_addr;
		log_appendf(4, "Accepted connection from %d.%d.%d.%d",
			(int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3]);
		status_text_flash("Accepted connection from %d.%d.%d.%d",
			(int)ip[0], (int)ip[1], (int)ip[2], (int)ip[3]);
	} else {
		/* something else ??? */
	}

	/* FIXME send some info about the server */

	return 0;
}

/* This function handles any incoming bytes, and checks
 * for incoming connections when the TCP socket is a server. */
int network_tcp_worker(struct network_tcp *n)
{
	SCHISM_RUNTIME_ASSERT(n, "no NULL pointer");

	if (TCP_ISSERVER(n) && n->cfd == -1) {
		if (network_tcp_server_worker(n) < 0) {
			if (n->cfd != -1)
				close(n->cfd);
			n->cfd = -1;

			return -1;
		}
	}

	if (n->connecting) {
		fd_set fds;
		struct timeval tv;
		int r, err;
		socklen_t len;

		FD_ZERO(&fds);
		FD_SET(n->cfd, &fds);

		/* set the timeout to zero, so we can poll effectively */
		tv.tv_sec = 0;
		tv.tv_usec = 0;

		r = select(n->cfd + 1, NULL, &fds, NULL, &tv);
		if (r == 0) {
			/* connect() isn't done yet */
			return 0;
		} else if (r < 0) {
			log_perror("select");
			return -1;
		}

		/* okay, we're writable. now check whether connect() succeeded */

		len = sizeof(err);
		r = getsockopt(n->cfd, SOL_SOCKET, SO_ERROR, &err, &len);
		if (r < 0) {
			log_appendf(4, "connect: %s", strerror(err));
			n->cfd = -1;
			return -1;
		}

		n->connecting = 0;

		Network_SendInstrument(0);
	}

	if (n->cfd == -1)
		return 0; /* nothing to do */

#define TCP_READ(fd, buf, size) \
do { \
	r = recv(fd, buf, size, 0); \
	if (r < 0) \
		return (errno == EAGAIN || errno == EWOULDBLOCK) ? 0 : -1; \
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
				/* something is totally wrong */
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
