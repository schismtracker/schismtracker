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
#include "network.h"
#include "network-tcp.h"
#include "song.h"
#include "mem.h"
#include "log.h"
#include "it.h"

/* Notes:
 *  * The IPX driver seems to work on port 18757 ("IT"), so we should
 *    replicate that.
 *  * The IPX driver also seems to be the only one ever implemented.
 *  * The IPX driver (somewhat obviously) uses Novell protocols instead of the
 *    TCP/IP stack which eventually won out over IPX. Since we don't use IPX
 *    (because it is absolutely ancient and no one uses it anymore), we need
 *    not worry about 100% compatibility with IT.
 *  * This code is currently completely untested. */

static struct network_tcp tcp = {0};

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

	/* --- SCHISM EXTENSIONS START ---- */
	NETWORK_BLOCKTYPE_HANDSHAKE = 10,
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
	size_t i;

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

	network_tcp_send(&tcp, buf, size + 9);

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

	/* TODO send the pattern data */
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
	unsigned char buf[512 + 2 + 2];
	uint16_t w;

	if ((length + offset) > 512)
		return -1;

	w = bswapLE16(length);
	memcpy(buf, &w, sizeof(w));
	w = bswapLE16(offset);
	memcpy(buf + 2, &w, sizeof(w));

	if (offset == 256 && length == 256) {
		/* fast path for just the orderlist */
		song_lock_audio();
		memcpy(buf + 2 + 2, current_song->orderlist, 256);
		song_unlock_audio();
	} else {
		/* slow path for everything else */
		disko_t ds;

		if (disko_memopen_estimate(&ds, 512) < 0)
			return -1;

		song_lock_audio();
		it_save_song_header(&ds, current_song, NULL, NULL, NULL, NULL, NULL, 1);
		song_unlock_audio();

		SCHISM_RUNTIME_ASSERT(ds.length == 512, "exactly 512 bytes needed");

		/* copy the bytes we actually want to send */
		memcpy(buf + 2 + 2, (char *)ds.data + offset, length);

		disko_memclose(&ds, 0);
	}

	return Network_SendData(NETWORK_BLOCKTYPE_SONGDATA, buf, 2 + 2 + length);
}

int Network_SendOrderList(void)
{
	/* helper function */
	return Network_SendSongData(256, 256);
}

int Network_SendInstrument(uint8_t num)
{
	disko_t memfp;
	song_instrument_t *inst;
	int r;

	if (num >= MAX_INSTRUMENTS)
		return -1;

	disko_memopen_estimate(&memfp, 555);

	disko_putc(&memfp, num);

	song_lock_audio();
	inst = current_song->instruments[num + 1];
	save_iti_instrument(&memfp, current_song, inst, 0);
	song_unlock_audio();

	r = Network_SendData(NETWORK_BLOCKTYPE_INSTRUMENT, memfp.data, memfp.length);

	disko_memclose(&memfp, 0);

	return r;
}

int Network_SendSample(uint8_t num)
{
	disko_t memfp;
	song_sample_t *smp;
	int r;

	if (num >= MAX_SAMPLES)
		return -1;

	disko_memopen_estimate(&memfp, 81);

	disko_putc(&memfp, num);

	song_lock_audio();
	smp = current_song->samples + num + 1;
	save_its_header(&memfp, smp, 1);
	song_unlock_audio();

	r = Network_SendData(NETWORK_BLOCKTYPE_SAMPLE, memfp.data, memfp.length);

	disko_memclose(&memfp, 0);

	return r;
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

	smp = current_song->samples + num + 1;

	if (smp->flags & CHN_ADLIB)
		return 0; /* hmm */

	len = smp->length;
	len = bswapLE32(len);

	memcpy(data + 1, &len, 4);

	/* I don't think there's anything else here.
	 * Every time the sample data type changes (i.e. 16-bit -> 8-bit or stereo
	 * -> mono) we have to send in the sample header as well, because that info
	 * is stored in the sample header (which is not transferred here) */

	return Network_SendData(NETWORK_BLOCKTYPE_NEW_SAMPLE, data, sizeof(data));
}

#define NETWORK_SENDSAMPLE_BLOCKSIZE (65000 - 5)

int Network_SendSampleData(uint8_t num)
{
	/* hm. probably shouldn't be using a static buffer here ... whatever */
	static uint8_t membuf[65000];
	song_sample_t *smp;

	if (num >= MAX_SAMPLES)
		return -1;

	smp = current_song->samples + num + 1;

	if (!smp->data)
		return -1; /* no sample data to send */

	membuf[0] = num;

	if (smp->flags & CHN_ADLIB) {
		/* special case for adlib */
		memset(membuf + 1, 0, 4);
		memcpy(membuf + 5, smp->adlib_bytes, 12);

		if (Network_SendData(NETWORK_BLOCKTYPE_SAMPLE_DATA, membuf, 12 + 5) < 0)
			return -1;
	} else {
		uint32_t len, dw, i;

		len = smp->length
			* ((smp->flags & CHN_16BIT) ? 2 : 1)
			* ((smp->flags & CHN_STEREO) ? 2 : 1);

		/* FIXME we're actually sending this as interleaved stereo (not to
		 * spec with IT sample definition) */
		for (i = 0; i < len; i += NETWORK_SENDSAMPLE_BLOCKSIZE) {
			uint32_t blksize = MIN(NETWORK_SENDSAMPLE_BLOCKSIZE, len - i);

			dw = bswapLE32(i);
			memcpy(membuf + 1, &dw, 4);

			memcpy(membuf + 5, (char *)smp->data + i, blksize);

			if (Network_SendData(NETWORK_BLOCKTYPE_SAMPLE_DATA, membuf, blksize + 5) < 0)
				return -1;
		}
	}

	return 0;
}

int Network_SendHandshake(const char *username, const char *motd)
{
	static const char tracker[] = "Schism Tracker " VERSION;
	uint32_t usernamelen, motdlen, trackerlen;
	uint32_t len;
	uint8_t *buf;
	uint32_t dw;
	uint16_t w;

	usernamelen = strlen(username);
	motdlen = strlen(motd);
	trackerlen = (ARRAY_SIZE(tracker) - 1);

	len = 2 + 4 + trackerlen + 4 + usernamelen + 4 + motdlen;
	if (len > 65000)
		return -1; /* don't even bother */

	buf = mem_alloc(len);

	/* version */
	w = bswapLE16(1);
	memcpy(buf, &w, 2);

	/* tracker name */
	dw = bswapLE32(trackerlen);
	memcpy(buf + 2, &dw, 4);

	memcpy(buf + 2 + 4, tracker, trackerlen);

	/* user name */
	dw = bswapLE32(usernamelen);
	memcpy(buf + 2 + 4 + trackerlen, &dw, 4);

	memcpy(buf + 2 + 4 + trackerlen + 4, username, usernamelen);

	/* message of the day */
	dw = bswapLE32(motdlen);
	memcpy(buf + 2 + 4 + trackerlen + 4 + usernamelen, &dw, 4);

	memcpy(buf + 2 + 4 + trackerlen + 4 + usernamelen + 4, motd, motdlen);

	Network_SendData(NETWORK_BLOCKTYPE_HANDSHAKE, buf, len);

	free(buf);

	return 0;
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
	uint16_t offset, length;

	if (size < 4)
		return -1; /* invalid */

	memcpy(&length, (char *)data_, 2);
	length = bswapLE16(length);
	memcpy(&offset, (char *)data_ + 2, 2);
	offset = bswapLE16(offset);

	/* from IT_NET.ASM: "Sanity check" */
	if ((offset + length) > 512 || ((length + 4) > size))
		return -1;

	song_lock_audio();

	if (/*offset == 0 && */length == 512) {
		slurp_t sl;

		slurp_memstream(&sl, (uint8_t *)data_ + 4, 512);
		it_load_song_header(current_song, &sl);

		/* this is ugly */
		main_song_changed_cb();
	} else if (offset == 256 && length == 256) {
		/* Only contains order-list */
		memcpy(current_song->orderlist, (char *)data_ + 4, 256);
	} else {
		disko_t ds;
		slurp_t sl;

		if (disko_memopen_estimate(&ds, 512) < 0) {
			song_unlock_audio();
			return -1;
		}

		/* ok, we have to save our existing state, then overwrite parts with
		 * the data we received, and THEN import it back in. */
		it_save_song_header(&ds, current_song, NULL, NULL, NULL, NULL, NULL, 1);
		SCHISM_RUNTIME_ASSERT(ds.length == 512, "size must be 512 always");

		/* copy new data in */
		memcpy((char *)ds.data + offset, (char *)data_ + 4, length);

		slurp_memstream(&sl, ds.data, ds.length);

		/* load it */
		it_load_song_header(current_song, &sl);

		disko_memclose(&ds, 0);

		/* tell everything else HEY WE'VE CHANGED */
		main_song_changed_cb();
	}

	song_unlock_audio();

	return 0;
}

static int Network_ReceiveInstrument(const void *data_, size_t size)
{
	slurp_t fp;
	song_instrument_t *inst;
	int num;

	slurp_memstream(&fp, (uint8_t *)data_, size);

	num = slurp_getc(&fp);
	if (num == EOF || num >= MAX_INSTRUMENTS)
		return -1;

	inst = csf_allocate_instrument();

	if (!load_it_instrument(NULL, inst, &fp)) {
		csf_free_instrument(inst);
		return -1;
	}

	/* do as little processing as possible while we're locked */
	song_lock_audio();
	current_song->instruments[num + 1] = inst;
	song_unlock_audio();

	return 0;
}

static int Network_ReceiveSample(const void *data_, size_t size)
{
	slurp_t fp;
	song_sample_t *smp;
	int num;

	slurp_memstream(&fp, (uint8_t *)data_, size);

	num = slurp_getc(&fp);
	if (num == EOF || num >= MAX_SAMPLES)
		return -1;

	song_lock_audio();
	smp = current_song->samples + num + 1;
	load_its_header(&fp, smp);
	song_unlock_audio();

	return 0;
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
	uint8_t smpnum;

	/* data[0] == sample number */
	if (size < 1)
		return -1;

	smpnum = data[0];
	if (smpnum >= MAX_SAMPLES)
		return -1;

	song_lock_audio();
	csf_destroy_sample(current_song, smpnum + 1);
	song_unlock_audio();

	return 0;
}

static int Network_ReceiveNewSample(const void *data_, size_t size)
{
	const unsigned char *data = data_;
	uint8_t smpnum;
	song_sample_t *smp;

	if (size < 5)
		return -1;

	smpnum = data[0];

	/* sanity check */
	if (smpnum >= MAX_SAMPLES)
		return -1;

	song_lock_audio();

	/* kill any sample thats already there
	 * (dont clear the flags though!) */
	if (current_song->samples[smpnum + 1].data) {
		csf_free_sample(current_song->samples[smpnum + 1].data);
		current_song->samples[smpnum + 1].data = NULL;
	}
	//csf_destroy_sample(current_song, smpnum + 1);

	/* put in the length; we'll read in the sample later once
	 * the sample data event arrives */
	smp = current_song->samples + smpnum + 1;
	if (smp->flags & CHN_ADLIB) {
		// dumb hackaround that ought to someday be fixed:
		smp->length = 1;
		smp->data = csf_allocate_sample(1);
	} else {
		uint32_t len;

		memcpy(&len, data + 1, 4);
		len = bswapLE32(len);

		smp->length = len;

		len *= (smp->flags & CHN_16BIT)  ? 2 : 1;
		len *= (smp->flags & CHN_STEREO) ? 2 : 1;

		smp->data = csf_allocate_sample(len);
	}

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

	song_lock_audio();

	smp = current_song->samples + smpnum + 1;

	if (smp->flags & CHN_ADLIB) {
		/* I hope this is good enough to prevent against overflow */
		if ((offset > 12) || ((offset + (size - 5)) > 12)) {
			song_unlock_audio();
			return -1;
		}

		memcpy(smp->adlib_bytes + offset, data + 5, size - 5);
	} else {
		fulllength = smp->length
			* ((smp->flags & CHN_16BIT) ? 2 : 1)
			* ((smp->flags & CHN_STEREO) ? 2 : 1);

		/* I hope this is good enough to prevent against overflow */
		if (!smp->data || (offset > fulllength) || ((offset + (size - 5)) > fulllength)) {
			song_unlock_audio();
			return -1;
		}

		/* prevent races */
		memcpy((char *)smp->data + offset, data + 5, size - 5);
		csf_adjust_sample_loop(smp); /* ;) */
	}

	song_unlock_audio();

	return 0;
}

#if 1
# define NETWORK_HANDSHAKE_PRINT_INFO
#endif

#ifdef NETWORK_HANDSHAKE_PRINT_INFO
static int Network_HandshakeReceiveStringCallback(const void *data, size_t size,
	void *userdata)
{
	if (size > INT_MAX)
		return -1;

	log_appendf(1, "%s: %.*s", (char *)userdata, (int)size, (char *)data);
	return 0;
}
#endif

static int Network_ReceiveHandshake(const void *data, size_t size)
{
	slurp_t mem;
	uint16_t w;
	uint32_t dw;

	/* All integers are little endian unless otherwise specified.
	 * netstrings shall not include any NUL bytes.
	 *
	 * Additionally, since the username and motd are able to be
	 * input by the user, they may not necessarily be valid UTF-8.
	 * You'll want to make sure you handle the case where the data
	 * may be invalid.
	 *
	 * Schism Tracker always sends a tracker name of the form
	 * "Schism Tracker YYYYMMDD". Anyone else using this protocol
	 * is encouraged to also append a version number after the
	 * actual tracker name.
	 *
	 * #pragma pack(push, 1)
	 *
	 * // like a Pascal string, but can be much longer (32-bit len)
	 * typedef struct {
	 *     uint32_t len;   // string length
	 *     char data[len]; // string data, in UTF-8. must not include any
	 *                     // NUL terminating bytes (i.e. '\0')
	 * } netstring;
	 *
	 * // handshake data structure
	 * struct {
	 *     // AVAILABLE STARTING VERSION 1
	 *     uint16_t ver;       // handshake version
	 *     netstring tracker;  // tracker name
	 *     netstring username; // username
	 *     netstring motd;     // message of the day
	 * };
	 *
	 * #pragma pack(pop) */

	slurp_memstream(&mem, (uint8_t *)data, size);

	if (slurp_read(&mem, &w, 2) != 2)
		return -1;
	w = bswapLE16(w);

	if (w >= 1) {
		/* FIXME all of the data is just skipped over instead of being read */
		if (slurp_read(&mem, &dw, 4) != 4)
			return -1;
		dw = bswapLE32(dw);

#ifdef NETWORK_HANDSHAKE_PRINT_INFO
		slurp_receive(&mem, Network_HandshakeReceiveStringCallback, dw, (char *)"Tracker");
#endif
		slurp_seek(&mem, dw, SEEK_CUR);

		if (slurp_read(&mem, &dw, 4) != 4)
			return -1;
		dw = bswapLE32(dw);

#ifdef NETWORK_HANDSHAKE_PRINT_INFO
		slurp_receive(&mem, Network_HandshakeReceiveStringCallback, dw, (char *)"Username");
#endif
		slurp_seek(&mem, dw, SEEK_CUR);

		if (slurp_read(&mem, &dw, 4) != 4)
			return -1;
		dw = bswapLE32(dw);

#ifdef NETWORK_HANDSHAKE_PRINT_INFO
		slurp_receive(&mem, Network_HandshakeReceiveStringCallback, dw, (char *)"MOTD");
#endif
		slurp_seek(&mem, dw, SEEK_CUR);
	}

	/* then process version 2... then version 3... */
	return 0;
}

/* ------------------------------------------------------------------------ */
/* Data receiver type table */

typedef int (*Network_DataReceiverPtr)(const void *data, size_t size);

static const Network_DataReceiverPtr receivers[] = {
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
	[NETWORK_BLOCKTYPE_HANDSHAKE] = Network_ReceiveHandshake,
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

	/* invalid size, or unfinished block */
	if (w < 9 || w > size)
		return -1; /* invalid size */

	Network_CalculateCRC((const uint8_t *)data, size - 2, crc);

	/* Make sure the two CRCs are the same */
	if (memcmp(crc, (const uint8_t *)data + size - 2, 2))
		return -1;

	/* ok, our data's good. pass it to the interpreters,
	 * excluding the header and CRC info. */
	return receivers[type](data + 7, size - 9);
}

/* ------------------------------------------------------------------------ */

void Network_Close(void)
{
	network_tcp_close(&tcp);
}

int Network_StartServer(uint16_t port)
{
	int r;

	Network_Close();

	r = network_tcp_init(&tcp);
	if (r < 0)
		return -1;

	r = network_tcp_start_server(&tcp, port);
	if (r < 0)
		return -1;

	return 0;
}

void Network_StopServer(void)
{
	Network_Close();
}

int Network_StartClient(const char *host, uint16_t port)
{
	int r;

	Network_Close();

	r = network_tcp_init(&tcp);
	if (r < 0)
		return -1;

	r = network_tcp_start_client(&tcp, host, port);
	if (r < 0)
		return -1;

	return 0;
}

void Network_StopClient(void)
{
	Network_Close();
}

void Network_Worker(void)
{
	network_tcp_worker(&tcp);
}

/* The TCP driver calls this back once it can send messages
 * (i.e., once it's connected) */
int Network_OnConnect(void)
{
	int r;

	r = Network_SendHandshake("Unregistered", "MOTD");
	if (r < 0)
		return r;

	/* TODO once we're connected, the server should send
	 * all of the song data to the clients. */
	return 0;
}

/* Server-specific stuff */
int Network_OnServerConnect(void)
{
	/* We're controlling the song; send all of the data
	 * to the client */
	int r, n;

	r = Network_SendSongData(512, 0);
	if (r < 0) return -1;

	for (n = 0; n < MAX_SAMPLES; n++) {
		Network_SendSample(n);
		if (current_song->samples[n + 1].data || (current_song->samples[n+1].flags & CHN_ADLIB)) {
			Network_SendNewSample(n);
			Network_SendSampleData(n);
		}
	}

	for (n = 0; n < MAX_INSTRUMENTS; n++)
		Network_SendInstrument(n);

	return 0;
}
