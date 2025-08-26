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

#ifndef SCHISM_NETWORK_H_
#define SCHISM_NETWORK_H_

#include "headers.h"

/* TCP port 18757 */
#define NETWORK_DEFAULT_PORT (18757) /* "IT" */

/* Send out info */
int Network_SendPartialPattern(uint8_t num, uint8_t channel, uint8_t row,
	uint8_t width, uint8_t height);
int Network_SendPattern(uint8_t num);
int Network_SendRequestPattern(uint8_t num);
int Network_SendSongData(uint16_t length, uint16_t offset);
int Network_SendInstrument(uint8_t num);
int Network_SendSample(uint8_t num);
int Network_SendPatternLength(uint8_t num, uint8_t new_maxrow);
int Network_SendDeleteSample(uint8_t num);
int Network_SendNewSample(uint8_t num);
int Network_SendSampleData(uint8_t num);
int Network_SendHandshake(const char *username, const char *motd);

/* "helper" sending functions: */
int Network_SendOrderList(void);

/* this likely shouldn't be extern (but whatever) */
int Network_ReceiveData(const void *data, size_t size);

/* server/client stuff */
void Network_Close(void);
int Network_StartServer(uint16_t port);
void Network_StopServer(void);
int Network_StartClient(const char *host, uint16_t port);
void Network_StopClient(void);

/* asynchronous worker */
void Network_Worker(void);

/* called by the driver once it's connected (i.e., we can
 * actually write data) */
int Network_OnConnect(void);

/* server stuff (also call Network_OnConnect) */
int Network_OnServerConnect(void);

#endif /* SCHISM_NETWORK_H_ */
