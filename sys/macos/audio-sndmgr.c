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
#include "mem.h"
#include "util.h"
#include "backend/audio.h"
#include "loadso.h"
#include "charset.h"
#include "str.h"
#include "log.h"

#include <Sound.h>
#include <Gestalt.h>
#include <Components.h>
#include <DriverServices.h>

/* SDL 1.2 doesn't have audio device selection.
 * For a while I thought the Sound Manager never supported it at all.
 *
 * However, PortAudio has a comment in the now-removed Macintosh source,
 * in the channel opening code, pointing to a "kUseOptionalOutputDevice".
 * According to old Apple docs, this was added in System 7, which is old
 * enough to where we really have no reason to worry about it.
 * The same Apple doc provides an example for grabbing all the available
 * sound output components (thanks!) */

/* Portions of this code were written by Ryan C. Gordon (icculus)
 * for SDL 1.2, under the LGPL-2.1. */

#pragma options align=power

#define NUM_BUFFERS 2
#if NUM_BUFFERS < 2
# error invalid number of buffers
#endif

#define USE_ONE_BUF_ALLOCATION

struct schism_audio_device {
	SndChannelPtr chn;

	/* this is a stupid API */
	CmpSoundHeader hdr;

	volatile SInt32 locked;
	volatile SInt32 to_mix;
	volatile UInt32 running;
	volatile UInt32 fill_me;

	void (*callback)(uint8_t *stream, int len);

	/* buf[0] is the actual allocation */
	UInt8 *buf[NUM_BUFFERS];

	uint32_t samples;
	uint32_t size;

	uint8_t silence;

	int paused;
};

static void sndmgr_free_devices(void);
static uint32_t sndmgr_audio_device_count(uint32_t);

/* ---------------------------------------------------------- */
/* drivers */

static int sndmgr_audio_init_driver(const char *name)
{
	/* TODO call Gestalt to get some info about the actual state
	 * of the Sound Manager. Inside Macintosh provides the following
	 * attributes, under the selector 'gestaltSoundAttr':
	 *
	 * gestaltStereoCapability    = 0; {built-in hw can play stereo sounds}
	 * gestaltStereoMixing        = 1; {built-in hw mixes stereo to mono}
	 * gestaltSoundIOMgrPresent   = 3; {sound input routines available}
	 * gestaltBuiltInSoundInput   = 4; {built-in input hw available}
	 * gestaltHasSoundInputDevice = 5; {sound input device available}
	 * gestaltPlayAndRecord       = 6; {built-in hw can play while recording}
	 * gestalt16BitSoundIO        = 7; {built-in hw can handle 16-bit data}
	 * gestaltStereoInput         = 8; {built-in hw can record stereo sounds}
	 * gestaltLineLevelInput      = 9;  {built-in input hw needs line level}
	 * gestaltSndPlayDoubleBuffer = 10; {play from disk routines available}
	 * gestaltMultiChannels       = 11; {multiple channels of sound supported}
	 * gestalt16BitAudioSupport   = 12; {16-bit audio data supported}
	 *
	 * The most important of which would be gestaltSndPlayDoubleBuffer.
	 * However, if we don't have that, then we probably won't be able to
	 * do anything anyway.. */

	/* refresh the audio devices */
	sndmgr_audio_device_count(0);
	return 0;
}

static void sndmgr_audio_quit_driver(void)
{
	sndmgr_free_devices();
}

/* ---------------------------------------------------------- */

static int sndmgr_audio_driver_count(void)
{
	return 1;
}

static const char *sndmgr_audio_driver_name(int x)
{
	switch (x) {
	case 0: return "sndmgr";
	default: break;
	}

	/* if we're here we're already screwed, and there's a bug.
	 * to make it obvious, crash the program :) */
	return NULL;
}

/* --------------------------------------------------------------- */

static struct {
	Component cpnt;
	char *desc; /* UTF-8 */
} *devices = NULL;
static size_t devices_size = 0;

static void sndmgr_free_devices(void)
{
	size_t i;

	for (i = 0; i < devices_size; i++)
		free(devices[i].desc);
	free(devices);
}

static uint32_t sndmgr_audio_device_count(uint32_t flags)
{
	Component lcpnt;
	ComponentDescription cdesc = {0};
	long ncomponents;
	long i;
	OSErr err;

	if (flags & AUDIO_BACKEND_CAPTURE)
		return 0;

	cdesc.componentType = kSoundOutputDeviceType;

	ncomponents = CountComponents(&cdesc);
	SCHISM_RUNTIME_ASSERT(ncomponents >= 0, "no negative number");

	devices = mem_alloc(ncomponents * sizeof(*devices));
	devices_size = ncomponents;

	lcpnt = NULL;
	for (i = 0; i < ncomponents; i++) {
		Component cpnt;

		cpnt = FindNextComponent(lcpnt, &cdesc);
		devices[i].cpnt = cpnt;
		{
			/* grab the component info, which will give us the name */
			Handle hname;
			ComponentDescription desc; /* ... */

			hname = NewHandle(0);
			SCHISM_RUNTIME_ASSERT(hname, "need to be able to get the device name");

			err = GetComponentInfo(cpnt, &desc, hname, NULL, NULL);
			if (err == noErr) {
				char cstr[256];

				/* Mac OS is Pascal-coded */
				str_from_pascal((unsigned char *)*hname, cstr);

				/* Audio device names are UTF-8 internally */
				devices[i].desc = charset_iconv_easy(cstr,
					CHARSET_SYSTEMSCRIPT, CHARSET_UTF8);
			}

			DisposeHandle(hname);
		}
		lcpnt = cpnt;
	}

	return devices_size;
}

static const char *sndmgr_audio_device_name(SCHISM_UNUSED uint32_t i)
{
	SCHISM_RUNTIME_ASSERT(i < devices_size, "overflow");

	return devices[i].desc;
}

/* ------------------------------------------------------------------------ */

static void mix(schism_audio_device_t *dev, UInt8 *buffer)
{
	if (dev->paused) {
		memset(buffer, dev->silence, dev->size);
	} else {
		dev->callback(buffer, dev->size);
	}

	DecrementAtomic((SInt32 *)&dev->to_mix);
}

static pascal void cbproc(SndChannel *chan, SndCommand *cmd_passed)
{
	UInt32 play_me;
	SndCommand cmd;
	schism_audio_device_t *dev = (schism_audio_device_t *)chan->userInfo;

	IncrementAtomic((SInt32 *)&dev->to_mix);

	/* buffer that has just finished playing, so fill it */
	dev->fill_me = cmd_passed->param2;
	/* filled buffer to play _now_ */
	play_me      = (dev->fill_me + 1) % NUM_BUFFERS;

	/* queue previously mixed buffer for playback. */
	dev->hdr.samplePtr = (Ptr)dev->buf[play_me];
	cmd.cmd = bufferCmd;
	cmd.param1 = 0; 
	cmd.param2 = (long)&dev->hdr;
	SndDoCommand(chan, &cmd, 0);

	/*
	 * if audio device isn't locked, mix the next buffer to be queued in
	 *  the memory block that just finished playing.
	 */
	if (!BitAndAtomic(0xFFFFFFFF, (UInt32 *)&dev->locked))
		mix(dev, dev->buf[dev->fill_me]);

	/* set this callback to run again when current buffer drains. */
	if (dev->running) {
		cmd.cmd = callBackCmd;
		cmd.param1 = 0;
		cmd.param2 = play_me;

		SndDoCommand(chan, &cmd, 0);
	}
}

static void sndmgr_audio_close_device(schism_audio_device_t *dev);

static schism_audio_device_t *sndmgr_audio_open_device(uint32_t id, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	SndCallBackUPP callback;
	int i;
	schism_audio_device_t *dev;
	OSErr err;

	dev = mem_calloc(1, sizeof(*dev));
	dev->callback = desired->callback;

	dev->samples = desired->samples;
	dev->size = desired->samples * desired->channels * (desired->bits / 8);
	dev->silence = AUDIO_SPEC_SILENCE(*desired);

	log_appendf(4, "[sndmgr] size: %" PRIu32 ", samples: %" PRIu32, dev->size, dev->samples);

	callback = (SndCallBackUPP)NewSndCallBackUPP(cbproc);

	/* prepare bufferCmd header */
	dev->hdr.numChannels = desired->channels;
	dev->hdr.sampleSize  = (desired->bits > 8) ? 16 : 8;
	dev->hdr.sampleRate  = desired->freq << 16;
	dev->hdr.numFrames   = desired->samples;
	dev->hdr.encode      = cmpSH;

#ifdef USE_ONE_BUF_ALLOCATION
	/* Buffer overflow somewhere... */
	dev->buf[0] = mem_alloc(NUM_BUFFERS * dev->size);

	for (i = 1; i < NUM_BUFFERS; i++) {
		/* find this pointer from the last */
		dev->buf[i] = dev->buf[i-1] + dev->size;
	}
#else
	for (i = 0; i < NUM_BUFFERS; i++)
		dev->buf[i] = mem_alloc(dev->size);
#endif

	dev->chn = mem_alloc(sizeof(*dev->chn));

	dev->chn->userInfo = (long)dev;
	dev->chn->qLength = 128;

	if (id == AUDIO_BACKEND_DEFAULT) {
		err = SndNewChannel(&dev->chn, sampledSynth,
			(desired->channels >= 2) ? initStereo : initMono, callback);
		if (err != noErr) {
			log_appendf(4, "SndNewChannel: %" PRId32, (int32_t)err);
			goto err;
		}
	} else {
		err = SndNewChannel(&dev->chn, kUseOptionalOutputDevice,
			(long)devices[id].cpnt, callback);
		if (err != noErr) {
			log_appendf(4, "SndNewChannel: %" PRId32 ", Component: %p", (int32_t)err, devices[id].cpnt);
			goto err;
		}

		/* Sound channel SEEMS to default to stereo, even though it isn't
		 * actually documented anywhere (but it is implied; the Apple docs
		 * that I referenced say that a channel should be created for each
		 * "pair" of channels). Thus we must reinit the channel with mono
		 * sound if it is desired. */
		if (desired->channels < 2) {
			SndCommand cmd;

			/* ugly */
			if (err != noErr)
				goto err;

			/* Since we couldn't pass any init flags to SndNewChannel,
			 * we should send a reInitCmd command to the channel to
			 * tell it to do mono sound. */
			cmd.cmd = reInitCmd;
			cmd.param1 = 0; /* unused */
			cmd.param2 = initMono;

			err = SndDoCommand(dev->chn, &cmd, 0 /* = wait for space in the queue */);
			if (err != noErr) {
				log_appendf(4, "SndDoCommand: %" PRId32, (int32_t)err);
				goto err;
			}
		}
	}

	{
		SndCommand cmd;
		cmd.cmd = callBackCmd;
		cmd.param2 = 0;
		dev->running = 1;
		SndDoCommand(dev->chn, &cmd, 0);
	}

	obtained->bits = dev->hdr.sampleSize;
	obtained->samples = desired->samples;
	obtained->freq = desired->freq;
	obtained->channels = desired->channels;

	return dev;

err:
	sndmgr_audio_close_device(dev);
	return NULL;
}

static void sndmgr_audio_close_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	dev->running = 0;

	if (dev->chn) {
		SndDisposeChannel(dev->chn, 1);
		free(dev->chn);
	}

#ifdef USE_ONE_BUF_ALLOCATION
	/* allocation lives here */
	free(dev->buf[0]);
#else
	{
		int i;

		for (i = 0; i < NUM_BUFFERS; i++)
			free(dev->buf[i]);
	}
#endif

	free(dev);
}

/* lock/unlock/pause */

static void sndmgr_audio_lock_device(schism_audio_device_t *dev)
{
	IncrementAtomic((SInt32 *)&dev->locked);
}

static void sndmgr_audio_unlock_device(schism_audio_device_t *dev)
{
	SInt32 oldval;

	oldval = DecrementAtomic((SInt32 *)&dev->locked);
	if (oldval != 1) /* != 1 means audio is still locked. */
		return;

	/* Did we miss the chance to mix in an interrupt? Do it now. */
	if (BitAndAtomic(0xFFFFFFFF, (UInt32 *)&dev->to_mix)) {
		/*
		 * Note that this could be a problem if you missed an interrupt
		 *  while the audio was locked, and get preempted by a second
		 *  interrupt here, but that means you locked for way too long anyhow.
		 */
		mix(dev, dev->buf[dev->fill_me]);
	}
}

static void sndmgr_audio_pause_device(schism_audio_device_t *dev, int paused)
{
	dev->paused = !!paused;
}

/* ------------------------------------------------------------------------ */
/* dynamic loading */

static int sndmgr_audio_init(void)
{
	return 1;
}

static void sndmgr_audio_quit(void)
{
	sndmgr_free_devices();
}

/* ------------------------------------------------------------------------ */

const schism_audio_backend_t schism_audio_backend_sndmgr = {
	.init = sndmgr_audio_init,
	.quit = sndmgr_audio_quit,

	.driver_count = sndmgr_audio_driver_count,
	.driver_name = sndmgr_audio_driver_name,

	.device_count = sndmgr_audio_device_count,
	.device_name = sndmgr_audio_device_name,

	.init_driver = sndmgr_audio_init_driver,
	.quit_driver = sndmgr_audio_quit_driver,

	.open_device = sndmgr_audio_open_device,
	.close_device = sndmgr_audio_close_device,
	.lock_device = sndmgr_audio_lock_device,
	.unlock_device = sndmgr_audio_unlock_device,
	.pause_device = sndmgr_audio_pause_device,
};
