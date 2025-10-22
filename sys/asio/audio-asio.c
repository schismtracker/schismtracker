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

#include "backend/audio.h"
#include "mem.h"
#include "osdefs.h"
#include "loadso.h"
#include "log.h"
#include "mt.h"
#include "video.h"
#include "str.h"

#include "audio-asio.h"

/* ------------------------------------------------------------------------ */

/* "drivers" are refreshed every time this is called */
static uint32_t asio_device_count(uint32_t flags)
{
	/* Ignore this for now */
	if (flags & AUDIO_BACKEND_CAPTURE)
		return 0;

	Asio_DriverPoll();

	return Asio_DriverCount();
}

static const char *asio_device_name(uint32_t i)
{
	return Asio_DriverDescription(i);
}

/* ---------------------------------------------------------------------------- */

static int asio_driver_count(void)
{
	return 1;
}

static const char *asio_driver_name(int i)
{
	switch (i) {
	case 0: return "asio";
	default: return NULL;
	}
}

static int asio_init_driver(const char *driver)
{
	if (strcmp("asio", driver))
		return -1;

	/* Do the initial poll for drivers here */
	Asio_DriverPoll();
	return 0;
}

static void asio_quit_driver(void) { /* eh, okay */ }

/* ---------------------------------------------------------------------------- */

struct schism_audio_device {
	IAsio *asio;

	/* callback; fills the audio buffer */
	void (*callback)(uint8_t *stream, int len);
	mt_mutex_t *mutex;

	/* the ASIO buffers
	 * these are each responsible for one channel. */
	struct AsioBuffers *buffers;
	uint32_t numbufs;

	void *membuf;

	uint32_t bufsmps; /* buffer length in samples */
	uint32_t bps;     /* bytes per sample */
	uint32_t buflen;  /* bufsmps * bytes per sample * numbufs (only used for mono) */
	unsigned int swap : 1; /* need to byteswap? */

	/* Some ASIO drivers are BUGGY AS FUCK; they copy the callbacks POINTER,
	 * and not the actual data contained within it.
	 *
	 * So, since there is nothing preventing an ASIO driver from overwriting
	 * our precious callbacks, allocate them with the rest of the device,
	 * just to be safe. */
	struct AsioCreateBufferCallbacks callbacks;
};

/* butt-ugly global because ASIO has an API from the stone age */
static schism_audio_device_t *current_device = NULL;

static uint32_t ASIO_CDECL asio_msg(uint32_t class, uint32_t msg,
	SCHISM_UNUSED void *unk3, SCHISM_UNUSED void *unk4)
{
	schism_audio_device_t *dev = current_device;

	switch (class) {
	case ASIO_CLASS_SUPPORTS_CLASS:
		switch (msg) {
		case ASIO_CLASS_ASIO_VERSION:
		case ASIO_CLASS_SUPPORTS_BUFFER_FLIP_EX:
			return 1;
		default:
			break;
		}
		return 0;
	case ASIO_CLASS_ASIO_VERSION:
		return 2;
	case ASIO_CLASS_SUPPORTS_BUFFER_FLIP_EX:
		return 0;
	default:
		break;
	}

	return 0;
}

static void ASIO_CDECL asio_dummy2(void)
{
	/* dunno what this is */
	log_appendf(1, "[ASIO] asio_dummy2 was called by the driver");
}

static void ASIO_CDECL asio_buffer_flip(uint32_t buf,
	SCHISM_UNUSED uint32_t unk1)
{
	schism_audio_device_t *dev = current_device;

	if (dev->numbufs == 1) {
		/* we can fill the buffer directly */
		mt_mutex_lock(dev->mutex);
		dev->callback(dev->buffers[0].ptrs[buf], dev->buflen);
		mt_mutex_unlock(dev->mutex);
	} else {
		/* we need a temporary buffer to deinterleave stereo */
		uint32_t i;

		mt_mutex_lock(dev->mutex);
		dev->callback(dev->membuf, dev->buflen);
		mt_mutex_unlock(dev->mutex);

		switch (dev->bps) {
		/* I have a love-hate relationship with the preprocessor */

#define DEINTERLEAVE_INT(BITS) \
	do { \
		for (i = 0; i < dev->bufsmps; i++) { \
			((uint##BITS##_t *)(dev->buffers[0].ptrs[buf]))[i] \
				= ((uint##BITS##_t *)dev->membuf)[i*2+0]; \
			((uint##BITS##_t *)(dev->buffers[1].ptrs[buf]))[i] \
				= ((uint##BITS##_t *)dev->membuf)[i*2+1]; \
		} \
	} while (0)

		case 1: DEINTERLEAVE_INT(8);  break;
		case 2: DEINTERLEAVE_INT(16); break;
		case 4: DEINTERLEAVE_INT(32); break;
		case 8: DEINTERLEAVE_INT(64); break;

#undef DEINTERLEAVE_INT

#define DEINTERLEAVE_MEMCPY(BPS) \
	do { \
		for (i = 0; i < dev->bufsmps; i++) { \
			memcpy((char *)(dev->buffers[0].ptrs[buf]) + (i * BPS), \
				(char *)dev->membuf + (BPS * (i * 2 + 0)), BPS); \
			memcpy((char *)(dev->buffers[1].ptrs[buf]) + (BPS * i), \
				(char *)dev->membuf + (BPS * (i * 2 + 1)), BPS); \
		} \
	} while (0)

		/* gcc is usually smart enough to inline calls to memcpy with
		 * an integer literal */
		case 3:  DEINTERLEAVE_MEMCPY(3); break;
		default: DEINTERLEAVE_MEMCPY(dev->bps); break;

#undef DEINTERLEAVE_MEMCPY
		}

		/* swap the bytes if necessary */
		if (dev->swap) {
			switch (dev->bps) {
#define BSWAP_EX(BITS, INLOOP) \
	do { \
		uint32_t j; \
	\
		for (j = 0; j < dev->numbufs; j++) { \
			uint##BITS##_t *xx = dev->buffers[j].ptrs[buf]; \
			for (i = 0; i < dev->bufsmps; i++) { \
				INLOOP \
			} \
		} \
	} while (0)

#define BSWAP(BITS) BSWAP_EX(BITS, { xx[i] = bswap_##BITS(xx[i]); } )

			case 2: BSWAP(16); break;
			case 4: BSWAP(32); break;
			case 8: BSWAP(64); break;

#undef BSWAP

			case 3: BSWAP_EX(8, { uint8_t tmp = xx[i*3+0]; xx[i*3+0] = xx[i*3+2]; xx[i*3+2] = tmp; } ); break;

#undef BSWAP_EX

			}
		}
	}

	IAsio_OutputReady(dev->asio);
}

static void *ASIO_CDECL asio_buffer_flip_ex(SCHISM_UNUSED void *unk1,
	uint32_t buf, SCHISM_UNUSED uint32_t unk2)
{
	/* BUG: Steinberg's "built-in" ASIO driver completely ignores the
	 * return value for ASIO_CLASS_SUPPORTS_BUFFER_FLIP_EX, instead
	 * opting to use it even when we say we don't want it. Just forward
	 * the values, I guess... */

	asio_buffer_flip(buf, unk2);

	/* what is this SUPPOSED to return? */
	return NULL;
}

/* ----------------------------------------------------------------------- */
/* stupid ASIO-specific crap */

static void asio_control_panel(schism_audio_device_t *dev)
{
	if (!dev->asio)
		return;

	IAsio_ControlPanel(dev->asio);
}

/* ----------------------------------------------------------------------- */

static void asio_close_device(schism_audio_device_t *dev);

static schism_audio_device_t *asio_open_device(uint32_t id,
	const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	schism_audio_device_t *dev;
	AsioError err;
	uint32_t i;

	if (current_device)
		return NULL; /* ASIO only supports one device at a time */

	/* Special handling for "default" ASIO device:
	 *
	 * Try to open each driver, and return the first one that works.
	 * It may be better to initialize a dummy device that then allows
	 * the user to select which ASIO driver they actually want to use. */
	if (id == AUDIO_BACKEND_DEFAULT) {
		uint32_t count = Asio_DriverCount();

		for (i = 0; i < count; i++) {
			/* zero out the obtained structure, as any previous call
			 * might have messed up the values */
			memset(obtained, 0, sizeof(*obtained));

			dev = asio_open_device(i, desired, obtained);
			if (dev)
				return dev;
		}

		/* no working device found */
		return NULL;
	}

	dev = mem_calloc(1, sizeof(struct schism_audio_device));

	/* "ASIO under Windows is accessed as a COM in-process server object"
	 *  - ASIO4ALL Private API specification.
	 *
	 * https://asio4all.org/about/developer-resources/private-api-v2-0/
	 *
	 * Yet despite this fact, it is using the same value for CLSID as IID.
	 * If it were truly COM, the IAsio interface would have its own IID.
	 * But I digress. */
	dev->asio = Asio_DriverGet(id);
	if (!dev->asio)
		goto ASIO_fail;

	err = IAsio_Init(dev->asio);
	if (err < 0) {
		log_appendf(4, "[ASIO] IAsio_Init error %d", err);
		goto ASIO_fail;
	}

	{
		uint32_t bufmin, bufmax, bufpref, bufunk;

		err = IAsio_GetBufferSize(dev->asio, &bufmin, &bufmax, &bufpref,
			&bufunk);
		if (err < 0) {
			log_appendf(4, "[ASIO] IAsio_GetBufferSize error %d", err);
			goto ASIO_fail;
		}

		dev->bufsmps = CLAMP(desired->samples, bufmin, bufmax);
		obtained->samples = dev->bufsmps;
	}

	{
		double rate = desired->freq;

		err = IAsio_SupportsSampleRate(dev->asio, rate);
		if (err >= 0)
			IAsio_SetSampleRate(dev->asio, rate);

		/* grab the actual rate and put it in the audio spec */
		err = IAsio_GetSampleRate(dev->asio, &rate);
		if (err < 0) {
			log_appendf(4, "[ASIO] IAsio_GetSampleRate error %d", err);
			goto ASIO_fail;
		}

		obtained->freq = rate;
	}


	{
		/* don't care about input channels, throw out the value */
		uint32_t xyzzy, maxchn;

		err = IAsio_GetChannels(dev->asio, &xyzzy, &maxchn);
		if (err < 0) {
			log_appendf(4, "[ASIO] IAsio_GetChannels error %d", err);
			goto ASIO_fail;
		}

		dev->numbufs = MIN(maxchn, desired->channels);
	}

	if (!dev->numbufs)
		goto ASIO_fail;

	dev->buffers = mem_calloc(dev->numbufs, sizeof(*dev->buffers));
	for (i = 0; i < dev->numbufs; i++) {
		struct AsioChannelInfo chninfo;
		int be;

		chninfo.index = i;
		chninfo.input = 0;

		err = IAsio_GetChannelInfo(dev->asio, &chninfo);
		if (err < 0) {
			log_appendf(4, "[ASIO] GetChannelInfo ERROR: %" PRId32, err);
			goto ASIO_fail;
		}

		switch (chninfo.sample_type) {
		case ASIO_SAMPLE_TYPE_INT16BE:
			obtained->fp = 0;
			dev->bps = 2;
			be = 1;
			break;
		case ASIO_SAMPLE_TYPE_INT24BE:
			obtained->fp = 0;
			dev->bps = 3;
			be = 1;
			break;
		case ASIO_SAMPLE_TYPE_INT32BE:
			obtained->fp = 0;
			dev->bps = 4;
			be = 1;
			break;
		case ASIO_SAMPLE_TYPE_INT16LE:
			obtained->fp = 0;
			dev->bps = 2;
			be = 0;
			break;
		case ASIO_SAMPLE_TYPE_INT24LE:
			obtained->fp = 0;
			dev->bps = 3;
			be = 0;
			break;
		case ASIO_SAMPLE_TYPE_INT32LE:
			obtained->fp = 0;
			dev->bps = 4;
			be = 0;
			break;
		case ASIO_SAMPLE_TYPE_FLOAT32LE:
			obtained->fp = 1;
			dev->bps = 4;
			be = 0;
			break;
		default:
			log_appendf(4, "[ASIO] unknown sample type %" PRIx32,
				chninfo.sample_type);
			goto ASIO_fail;
		}

		obtained->bits = (dev->bps << 3);

#ifdef WORDS_BIGENDIAN
		/* toggle byteswapping */
		if (!be) dev->swap = 1;
#else
		if (be) dev->swap = 1;
#endif

		dev->buffers[i].input = 0;
		dev->buffers[i].channel = i;
	}

	{
		dev->callbacks.buffer_flip = asio_buffer_flip;
		dev->callbacks.unk2 = asio_dummy2;
		dev->callbacks.msg = asio_msg;
		dev->callbacks.buffer_flip_ex = asio_buffer_flip_ex;

		err = IAsio_CreateBuffers(dev->asio, dev->buffers, dev->numbufs,
			dev->bufsmps, &dev->callbacks);
		if (err < 0) {
			log_appendf(4, "[ASIO] IAsio_CreateBuffers error %" PRId32, err);
			goto ASIO_fail;
		}
	}

	dev->mutex = mt_mutex_create();
	if (!dev->mutex)
		goto ASIO_fail;

	obtained->channels = dev->numbufs;

	dev->buflen = dev->bufsmps;
	dev->buflen *= (obtained->bits / 8);
	dev->buflen *= dev->numbufs;

	dev->membuf = mem_alloc(dev->buflen);
	dev->callback = desired->callback;

	/* set this for the ASIO callbacks */
	current_device = dev;

	return dev;

ASIO_fail:
	asio_close_device(dev);
	return NULL;
}

static void asio_close_device(schism_audio_device_t *dev)
{
	if (dev) {
		/* stop playing */
		if (dev->asio) {
			IAsio_Stop(dev->asio);

			if (dev->buffers)
				IAsio_DestroyBuffers(dev->asio);

			IAsio_Quit(dev->asio);
		}

		if (dev->mutex)
			mt_mutex_delete(dev->mutex);

		free(dev->membuf);
		free(dev->buffers);
		free(dev);
	}

	current_device = NULL;
}

static void asio_lock_device(schism_audio_device_t *dev)
{
	mt_mutex_lock(dev->mutex);
}

static void asio_unlock_device(schism_audio_device_t *dev)
{
	mt_mutex_unlock(dev->mutex);
}

static void asio_pause_device(schism_audio_device_t *dev, int paused)
{
	mt_mutex_lock(dev->mutex);
	(paused ? IAsio_Stop : IAsio_Start)(dev->asio);
	mt_mutex_unlock(dev->mutex);
}

/* ------------------------------------------------------------------------ */

static int asio_init(void)
{
	return Asio_Init() >= 0;
}

static void asio_quit(void)
{
	return Asio_Quit();
}

/* ---------------------------------------------------------------------------- */

const schism_audio_backend_t schism_audio_backend_asio = {
	.init = asio_init,
	.quit = asio_quit,

	.driver_count = asio_driver_count,
	.driver_name = asio_driver_name,

	.device_count = asio_device_count,
	.device_name = asio_device_name,

	.init_driver = asio_init_driver,
	.quit_driver = asio_quit_driver,

	.open_device = asio_open_device,
	.close_device = asio_close_device,
	.lock_device = asio_lock_device,
	.unlock_device = asio_unlock_device,
	.pause_device = asio_pause_device,

	.control_panel = asio_control_panel,
};
