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

#ifndef SCHISM_NETWORK_TCP_H_
#define SCHISM_NETWORK_TCP_H_

#include "headers.h"

#ifdef SCHISM_WIN32
typedef uintptr_t network_tcp_socket_t;
#else
typedef int network_tcp_socket_t;
#endif

/* TCP is a client-server protocol, not a peer to peer protocol.
 * So we have to have one peer set up the server, while the other
 * peer connects to them. */

/* ethernet limit, likely a good starting point */
#define MSGSIZE_MAX (65535)

struct network_tcp {
	unsigned int init : 1;
	unsigned int server : 1;
	unsigned int connecting : 1;

	network_tcp_socket_t sfd;  /* server file descriptor */
	network_tcp_socket_t sfd6; /* server file descriptor; IPv6 */
	network_tcp_socket_t cfd;  /* client file descriptor */

	/* sending state */
	struct {
		unsigned char msg[MSGSIZE_MAX];
		/* in most cases, this will be 65535. however, if some
		 * weird network card DOESN'T support sizes that high,
		 * we will reduce it. */
		uint32_t msgsize;
	} s;

	/* receiving state */
	struct {
		unsigned char *membuf;
		uint32_t msgrecv;
		uint32_t msgsize;
	} r;
};

/* initialize the structure */
int network_tcp_init(struct network_tcp *n);
void network_tcp_close(struct network_tcp *n);
int network_tcp_start_server(struct network_tcp *n, uint16_t port);
int network_tcp_start_client(struct network_tcp *n, const char *node,
	uint16_t port);
int network_tcp_send(struct network_tcp *n, const void *data, uint32_t size);

/* worker; handles any receiving data, and for servers, incoming connections */
int network_tcp_worker(struct network_tcp *n);

#endif /* SCHISM_NETWORK_TCP_H_ */