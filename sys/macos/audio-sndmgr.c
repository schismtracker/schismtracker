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
#include "mt.h"
#include "charset.h"
#include "str.h"
#include "log.h"

#include <Sound.h>
#include <Gestalt.h>
#include <Components.h>

/* SDL 1.2 doesn't have audio device selection.
 * For a while I thought the Sound Manager never supported it at all.
 *
 * However, PortAudio has a comment in the now-removed Macintosh source,
 * in the channel opening code, pointing to a "kUseOptionalOutputDevice".
 * According to old Apple docs, this was added in System 7, which is old
 * enough to where we really have no reason to worry about it.
 * The same Apple doc provides an example for grabbing all the available
 * sound output components (thanks!) */

struct schism_audio_device {
	SndChannelPtr chn;
	int cancelled;

	/* this is a stupid API */
	SndDoubleBufferPtr buf[2];

	mt_mutex_t *mutex;

	void (*callback)(uint8_t *stream, int len);

	int paused;

	uint32_t samples;
	uint32_t size;

	uint8_t silence;
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

static pascal void sndmgr_double_cb(SndChannelPtr chan,
	SndDoubleBufferPtr newbuf)
{
	schism_audio_device_t *dev;

	dev = (schism_audio_device_t *)newbuf->dbUserInfo[0];
	if (dev->cancelled)
		return;
	
	newbuf->dbNumFrames = dev->samples;
	if (dev->paused) {
		memset(newbuf->dbSoundData, dev->silence, dev->size);
	} else {
		mt_mutex_lock(dev->mutex);
		dev->callback((uint8_t *)newbuf->dbSoundData, dev->size);
		mt_mutex_unlock(dev->mutex);
	}

	newbuf->dbFlags |= dbBufferReady;
}

static schism_audio_device_t *sndmgr_audio_open_device(SCHISM_UNUSED uint32_t id, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	SndDoubleBufferHeader2 dbh = {0};
	SndDoubleBackUPP dbupp;
	int i;
	schism_audio_device_t *dev = NULL;
	OSErr err;
	long init_flags;

	dev = mem_calloc(1, sizeof(*dev));
	dev->callback = desired->callback;

	dev->samples = desired->samples;
	dev->size = desired->samples * desired->channels * (desired->bits / 8);
	dev->silence = AUDIO_SPEC_SILENCE(*desired);

	dbupp = NewSndDoubleBackProc(sndmgr_double_cb);

	dbh.dbhNumChannels   = desired->channels;
	dbh.dbhSampleSize    = (desired->bits > 8) ? 16 : 8;
	dbh.dbhCompressionID = 0;
	dbh.dbhPacketSize    = 0;
	dbh.dbhSampleRate    = desired->freq << 16;
	dbh.dbhDoubleBack    = dbupp;
	dbh.dbhFormat        = 0;

	for (i = 0; i < 2; i++) {
		dev->buf[i] = mem_calloc(1, sizeof(SndDoubleBuffer) + dev->size);
		dev->buf[i]->dbNumFrames   = desired->samples;
		dev->buf[i]->dbFlags       = dbBufferReady;
		dev->buf[i]->dbUserInfo[0] = (long)dev;

		dbh.dbhBufferPtr[i] = dev->buf[i];
	}

	dev->chn = mem_calloc(1, sizeof(*dev->chn));

	/* SDL doesn't do this, but it's required according to the docs. */
	dev->chn->qLength = stdQLength;

	if (id == AUDIO_BACKEND_DEFAULT) {
		err = SndNewChannel(&dev->chn, sampledSynth,
			(desired->channels >= 2) ? initStereo : initMono, 0);
		if (err != noErr) {
			log_appendf(4, "SndNewChannel: %" PRId32, (int32_t)err);
			goto err;
		}
	} else {
		err = SndNewChannel(&dev->chn, kUseOptionalOutputDevice,
			(long)devices[id].cpnt, 0);
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

	err = SndPlayDoubleBuffer(dev->chn, (SndDoubleBufferHeaderPtr)&dbh);
	if (err != noErr) {
		log_appendf(4, "SndPlayDoubleBuffer: %" PRId32, (int32_t)err);
		goto err;
	}

	obtained->bits = dbh.dbhSampleSize;
	obtained->samples = desired->samples;
	obtained->freq = desired->freq;
	obtained->channels = desired->channels;

	return dev;

err:
	for (i = 0; i < 2; i++)
		free(dev->buf[i]);
	free(dev->chn);
	free(dev);
	return NULL;
}

static void sndmgr_audio_close_device(schism_audio_device_t *dev)
{
	int i;

	if (!dev)
		return;

	dev->cancelled = 1;

	SndDisposeChannel(dev->chn, 1);
	free(dev->chn);

	for (i = 0; i < 2; i++)
		free(dev->buf[i]);

	free(dev);
}

/* lock/unlock/pause */

static void sndmgr_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
}

static void sndmgr_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_unlock(dev->mutex);
}

static void sndmgr_audio_pause_device(schism_audio_device_t *dev, int paused)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
	dev->paused = !!paused;
	mt_mutex_unlock(dev->mutex);
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
