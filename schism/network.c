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

#include "song.h"
#include "mem.h"

/* Notes:
 *  * The IPX driver seems to work on port 18757 ("IT"), so we should
 *    replicate that.
 *  * The IPX driver also seems to be the only one ever implemented.
 *  * The IPX driver seems to use MAC addresses rather than IPv4 addresses.
 *    This makes sense for local addresses, but for internet networking
 *    it will not work at all. Hence, we should be able to take in a hostname.
 *    This could be literally anything (i.e. 192.168.1.67, fe80::250:56ff:fec0:1,
 *    or something like schismtracker.org).
 *  * This code is currently completely untested. */

enum {
	/* extra data:
	 *  uint8_t -- pattern number
	 *  uint32_t -- bounding box (?)
	 *  uint8_t[] -- pattern data */
	NETWORK_BLOCKTYPE_PARTIAL_PATTERN = 0,
	/* extra data:
	 *  uint8_t -- pattern number
	 *  uint8_t[] -- pattern data */
	NETWORK_BLOCKTYPE_PATTERN = 1,
	/* extra data:
	 *  uint8_t -- pattern number */
	NETWORK_BLOCKTYPE_REQUEST_PATTERN = 2,
	/* extra data:
	 *  uint16_t -- length
	 *  uint16_t -- offset */
	NETWORK_BLOCKTYPE_SONGDATA = 3,
	/* extra data:
	 *  uint8_t -- instrument number */
	NETWORK_BLOCKTYPE_INSTRUMENT = 4,
	/* extra data:
	 *  uint8_t -- sample number */
	NETWORK_BLOCKTYPE_SAMPLE = 5,
	/* extra data:
	 *  uint8_t -- pattern number
	 *  uint8_t -- New MaxRow (?) */
	NETWORK_BLOCKTYPE_PATTERN_LENGTH = 6,
	/* extra data:
	 *  uint8_t -- sample number */
	NETWORK_BLOCKTYPE_DELETE_SAMPLE = 7,
	/* extra data:
	 *  uint8_t -- sample number
	 *  uint32_t -- sample length (documented in IT_NET.ASM as "sends length also") */
	NETWORK_BLOCKTYPE_NEW_SAMPLE = 8,
	/* extra data:
	 *  uint8_t -- sample number
	 *  uint32_t -- offset */
	NETWORK_BLOCKTYPE_SAMPLE_DATA = 9,
};

/* Network data layout:
 *
 * #pragma pack(push, 1)
 * struct {
 *   uint32_t id; // "JLNP"
 *   uint16_t size; // Byte count including headers and CRC check
 *   uint8_t type; // Block type
 *   uint8_t data[]; // Extra data for this buffer
 *   uint16_t crc;
 * }
 * #pragma pack(pop)
*/

static void Network_CalculateCRC(const uint8_t *data, size_t size, uint8_t crc[2])
{
	uint8_t dh, dl;

	dh = 0;
	dl = 0;

	for (i = 0; i < size; i++) {
		dh += data[i];
		dl ^= dh;
	}

	crc[0] = dl - dh;
	crc[1] = -dl;
}

/* This function does the bulk of the work sending data around. */
static int Network_SendData(uint8_t type, const void *data, uint16_t size)
{
	uint16_t w;
	uint8_t *buf;
	uint16_t i;
	uint8_t dh, dl;

	/* IT_NET.ASM says this has a max of 65000 bytes
	 * NOTE: we should definitely be able to handle samples that
	 * are bigger than 65000 bytes. */
	if (size > 65000)
		return -1;

	buf = mem_alloc(size + 9);

	/* magic bytes */
	memcpy(buf, "JLNP", 4);
	/* size */
	w = size + 9;
	w = bswapLE16(w);
	memcpy(buf + 4, &w, 2);
	/* type */
	buf[6] = type;
	/* data */
	memcpy(buf + 7, data, size);

	/* calculate the CRC */
	Network_CalculateCRC(buf, size + 7, buf + size + 7);

	/* XXX send the finished buffer to a "driver" */

	free(buf);

	return 0;
}

/* ------------------------------------------------------------------------ */
/* Data sending abstractions */

int Network_SendPartialPattern(uint8_t num, uint8_t channel, uint8_t row,
	uint8_t width, uint8_t height)
{
	uint8_t x[5];

	x[0] = num;
	x[1] = channel;
	x[2] = row;
	x[3] = width;
	x[4] = height;

	return Network_SendData(NETWORK_BLOCKTYPE_PARTIAL_PATTERN, x, sizeof(x));
}

int Network_SendPattern(uint8_t num)
{
	/* TODO, need to hook this into the .IT exporting routines.
	 * (likely for the above as well) */
	return -1;
}

int Network_SendRequestPattern(uint8_t num)
{
	return Network_SendData(NETWORK_BLOCKTYPE_REQUEST_PATTERN, &num, sizeof(num));
}

int Network_SendSongData(uint16_t length, uint16_t offset)
{
	uint16_t x[2];

	x[0] = length;
	x[1] = offset;

	/* This definitely isn't correct ... */

	return Network_SendData(NETWORK_BLOCKTYPE_SONGDATA, x, sizeof(x));
}

int Network_SendInstrument(uint8_t num)
{
	/* TODO actually send the instrument header */
	return Network_SendData(NETWORK_BLOCKTYPE_INSTRUMENT, &num, sizeof(num));
}

int Network_SendSample(uint8_t num)
{
	/* TODO actually send the sample header */
	return Network_SendData(NETWORK_BLOCKTYPE_SAMPLE, &num, sizeof(num));
}

int Network_SendPatternLength(uint8_t num, uint8_t new_maxrow)
{
	uint8_t d[2];

	d[0] = num;
	d[1] = new_maxrow;

	return Network_SendData(NETWORK_BLOCKTYPE_PATTERN_LENGTH, d, sizeof(d));
}

int Network_SendDeleteSample(uint8_t num)
{
	return Network_SendData(NETWORK_BLOCKTYPE_DELETE_SAMPLE, &num, sizeof(num));
}

int Network_SendNewSample(uint8_t num)
{
	unsigned char data[5];
	song_sample_t *smp;
	uint32_t len;

	if (num >= MAX_SAMPLES)
		return -1;

	data[0] = num;

	smp = current_song->samples[num];

	len = smp->length;
	len = bswapLE32(len);

	memcpy(data + 1, &len, 4);

	/* I don't think there's anything else here.
	 * Every time the sample data type changes (i.e. 16-bit -> 8-bit or stereo
	 * -> mono) we have to send in the sample header as well, because that info
	 * is stored in the sample header (which is not transferred here) */

	return Network_SendData(NETWORK_BLOCKTYPE_NEW_SAMPLE, data, sizeof(data));
}

int Network_SendSampleData(uint8_t num)
{
	/* TODO actually send the data */
	return Network_SendData(NETWORK_BLOCKTYPE_SAMPLE, &num, sizeof(num));
}

/* ------------------------------------------------------------------------ */
/* Data receivers */

static int Network_ReceivePartialPattern(const void *data_, size_t size)
{
	const unsigned char *data = data_;

	if (size < 5)
		return -1;

	/* data[0] == pattern number
	 * data[1] == channel
	 * data[2] == row
	 * data[3] == width
	 * data[4] == height */

	if (!(data[3] | data[4]))
		return -1; /* invalid */

	/* XXX: hook into the IT pattern loading code. */
	return -1;
}

static int Network_ReceivePattern(const void *data_, size_t size)
{
	if (size < 1)
		return -1;

	/* XXX: hook into the IT pattern loading code. */
	return -1;
}

static int Network_ReceiveRequestPattern(const void *data_, size_t size)
{
	const unsigned char *data = data_;

	if (size < 1)
		return -1;

	return Network_SendPattern(data[0]);
}

static int Network_ReceiveSongData(const void *data_, size_t size)
{
	uint16_t x[2];

	if (size < 4)
		return -1; /* invalid */

	memcpy(x, data_, 4);

	/* x[0] == length
	 * x[1] == offset */

	/* from IT_NET.ASM: "Sanity check" */
	if (x[0] + x[1] > 512)
		return -1;

	/* TODO no idea how to handle this  --paper */
	return -1;
}

static int Network_ReceiveInstrument(const void *data_, size_t size)
{
	/* TODO 99% sure this just receives an IT instrument header.
	 * Hence we have to handle it properly :) */
	return -1;
}

static int Network_ReceiveSample(const void *data_, size_t size)
{
	/* TODO ditto with the above, probably IT sample header. */
	return -1;
}

static int Network_ReceivePatternLength(const void *data_, size_t size)
{
	const unsigned char *data = data_;

	/* data[0] == pattern number
	 * data[1] == "New MaxRow" */

	if (size < 2)
		return -1;

	/* TODO */
	return -1;
}

static int Network_ReceiveDeleteSample(const void *data_, size_t size)
{
	const unsigned char *data = data_;

	/* data[0] == sample number */

	/* TODO kill the sample */
	return -1;
}

static int Network_NewSample(const void *data_, size_t size)
{
	const unsigned char *data = data_;
	uint8_t smpnum;
	uint32_t len;

	if (size < 5)
		return -1;

	/* length is already in samples (i.e. no x2 for 16-bit)
	 * X to doubt we could send stereo samples sanely. */

	smpnum = data[0];

	/* sanity check */
	if (smpnum >= MAX_SAMPLES)
		return -1;

	memcpy(&len, data + 1, 4);
	len = bswapLE32(len);

	song_lock_audio();

	/* kill any sample thats already there */
	csf_destroy_sample(current_song->samples[smpnum + 1]);

	/* put in the length; we'll read in the sample later once
	 * the sample data event arrives */
	current_song->samples[smpnum + 1].length = len;

	song_unlock_audio();

	return 0;
}

static int Network_ReceiveSampleData(const void *data_, size_t size)
{
	/* Impulse Tracker sends sample data in increments
	 * (hence the offset parameter) */
	const unsigned char *data = data_;
	song_sample_t *smp;
	uint8_t smpnum;
	uint32_t fulllength;
	uint32_t offset;

	if (size < 5)
		return -1;

	smpnum = data[0];
	if (smpnum >= MAX_SAMPLES)
		return -1;

	memcpy(&offset, data + 1, 4);
	offset = bswapLE32(offset);

	smp = current_song->samples[smpnum + 1];

	fulllength = smp->length
		* ((smp->flags & CHN_16BIT) ? 2 : 1)
		* ((smp->flags & CHN_STEREO) : 2 : 1);

	/* I hope this is good enough to prevent again overflow */
	if ((offset >= fulllength) || ((offset + size) >= fulllength))
		return -1;

	/* prevent races */
	song_lock_audio();
	memcpy((char *)smp->data + offset, data + 5, size - 5);
	song_unlock_audio();

	return 0;
}

/* ------------------------------------------------------------------------ */
/* Data receiver type table */

typedef int (*Network_DataReceiverPtr)(const void *data, size_t size);

static const Network_DataReceiverPtr receivers = {
	[NETWORK_BLOCKTYPE_PARTIAL_PATTERN] = Network_ReceivePartialPattern,
	[NETWORK_BLOCKTYPE_PATTERN] = Network_ReceivePattern,
	[NETWORK_BLOCKTYPE_REQUEST_PATTERN] = Network_ReceiveRequestPattern,
	[NETWORK_BLOCKTYPE_SONGDATA] = Network_ReceiveSongData,
	[NETWORK_BLOCKTYPE_INSTRUMENT] = Network_ReceiveInstrument,
	[NETWORK_BLOCKTYPE_SAMPLE] = Network_ReceiveSample,
	[NETWORK_BLOCKTYPE_PATTERN_LENGTH] = Network_ReceivePatternLength,
	[NETWORK_BLOCKTYPE_DELETE_SAMPLE] = Network_ReceiveDeleteSample,
	[NETWORK_BLOCKTYPE_NEW_SAMPLE] = Network_ReceiveNewSample,
	[NETWORK_BLOCKTYPE_SAMPLE_DATA] = Network_ReceiveSampleData,
};

/* ------------------------------------------------------------------------ */

/* network "driver" should call this when a block of data is received
 * this function does a good bit of checking to make sure the data is sane. */
int Network_ReceiveData(const void *data, size_t size)
{
	uint16_t w;
	uint8_t type;
	uint8_t crc[2];

	if (size < 9)
		return -1; /* missing header info and CRC */

	if (memcmp(data, "JLNP", 4))
		return -1;

	memcpy(&type, data + 6, 1);

	/* is this a type we can handle? */
	if (type >= ARRAY_SIZE(receivers) || !receivers[type])
		return -1;

	memcpy(&w, data + 4, 2);
	w = bswapLE16(w);

	if (w < 9)
		return -1; /* invalid size */

	/* unfinished block ?? */
	if (w > size)
		return -1;

	Network_CalculateCRC((const uint8_t *)data, size - 2, crc);

	/* Make sure the two CRCs are the same */
	if (memcmp(crc, (const uint8_t *)data + size - 2, 2))
		return -1;

	/* ok, our data's good. pass it to the interpreters,
	 * excluding the header and CRC info. */
	return receivers[type](data + 7, size - 9);
}
