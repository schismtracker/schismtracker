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

// Mac OS X CoreAudio driver

#include "headers.h"
#include "charset.h"
#include "threads.h"
#include "mem.h"
#include "str.h"
#include "backend/audio.h"

#include <CoreAudio/CoreAudio.h>
#include <CoreServices/CoreServices.h>
#include <AudioUnit/AudioUnit.h>
#if MAC_OS_X_VERSION_MAX_ALLOWED <= 1050
# include <AudioUnit/AUNTComponent.h>
#endif

// AudioObject APIs were added in 10.4
#if (MAC_OS_X_VERSION_MIN_REQUIRED < 1040) || \
	(!defined(AUDIO_UNIT_VERSION) || ((AUDIO_UNIT_VERSION + 0) < 1040))
# define USE_AUDIODEVICE_APIS 1
#endif

// Audio Component APIs were refactored out of Component
// Manager in 10.6; for older versions we can simply
// #define the new symbols to the old ones
#if (MAC_OS_X_VERSION_MIN_REQUIRED < 1060) || \
	(!defined(AUDIO_UNIT_VERSION) || ((AUDIO_UNIT_VERSION + 0) < 1060))
# define USE_COMPONENT_MANAGER_APIS 1
#endif

#ifdef USE_COMPONENT_MANAGER_APIS
# define AudioComponentDescription struct ComponentDescription
# define AudioComponent Component
# define AudioComponentInstance AudioUnit
# define AudioComponentInstanceNew OpenAComponent
# define AudioComponentInstanceDispose CloseComponent
# define AudioComponentFindNext FindNextComponent
#endif

struct schism_audio_device {
	// The callback and the protecting mutex
	void (*callback)(uint8_t *stream, int len);
	mt_mutex_t *mutex;

	int paused;

	// what to pass to memset() to generate silence.
	// this is 0x80 for 8-bit audio, and 0 for everything else
	uint8_t silence;

	// audio unit
	AudioComponentInstance au;

	// The buffer that the callback fills
	void *buffer;
	uint32_t buffer_offset;
	uint32_t buffer_size;
};

/* ---------------------------------------------------------- */
/* drivers */

static const char *drivers[] = {
	"coreaudio",
};

static int macosx_audio_driver_count()
{
	return ARRAY_SIZE(drivers);
}

static const char *macosx_audio_driver_name(int i)
{
	if (i >= ARRAY_SIZE(drivers) || i < 0)
		return NULL;

	return drivers[i];
}

/* ------------------------------------------------------------------------ */
// Our local "cache" for audio devices; stores the ID as well as a UTF-8 name.

static struct {
	AudioDeviceID id;
	char *name;
} *devices = NULL;
static uint32_t devices_size = 0;

static void _macosx_audio_free_devices(void)
{
	if (devices) {
		for (uint32_t i = 0; i < devices_size; i++)
			free(devices[i].name);
		free(devices);

		devices = NULL;
		devices_size = 0;
	}
}

static char *_macosx_cfstring_to_utf8(CFStringRef cfstr)
{
	size_t len = CFStringGetMaximumSizeForEncoding(CFStringGetLength(cfstr), kCFStringEncodingUTF8);
	char *buf = mem_alloc(len + 1);
	if (!CFStringGetCString(cfstr, buf, len + 1, kCFStringEncodingUTF8)) {
		free(buf);
		return NULL;
	}
	// nul terminate
	buf[len] = '\0';
	return buf;
}

static uint32_t macosx_audio_device_count(void)
{
	OSStatus result;
	UInt32 size;

#ifndef USE_AUDIODEVICE_APIS
	AudioObjectPropertyAddress addr = {
		kAudioHardwarePropertyDevices,
		kAudioObjectPropertyScopeGlobal,
		// FIXME this was renamed to kAudioObjectPropertyElementMain in 12.0
		kAudioObjectPropertyElementMaster
	};
#endif

	_macosx_audio_free_devices();

#ifdef USE_AUDIODEVICE_APIS
	result = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices, &size, NULL);
#else
	result = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, NULL, &size);
#endif
	if (result != kAudioHardwareNoError)
		return 0;

	devices_size = size / sizeof(AudioDeviceID);
	if (devices_size <= 0)
		return 0;

	AudioDeviceID device_ids[devices_size];
#ifdef USE_AUDIODEVICE_APIS
	// I bought a property in Egypt and what they do for you is they give you the property
	result = AudioHardwareGetProperty(kAudioHardwarePropertyDevices, &size, device_ids);
#else
	result = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &size, device_ids);
#endif
	if (result != kAudioHardwareNoError)
		return 0;

	devices = mem_calloc(devices_size, sizeof(*devices));

	// final count of all of the devices
	uint32_t c = 0;

	for (uint32_t i = 0; i < devices_size; i++) {
		// XXX why is this so damn paranoid?
		char *ptr = NULL;
		int usable = 0;
		CFIndex len = 0;

		{
#ifdef USE_AUDIODEVICE_APIS
			result = AudioDeviceGetPropertyInfo(device_ids[i], 0, 0, kAudioDevicePropertyStreamConfiguration, &size, NULL);
#else
			addr.mScope = kAudioDevicePropertyScopeOutput;
			addr.mSelector = kAudioDevicePropertyStreamConfiguration;
			result = AudioObjectGetPropertyDataSize(device_ids[i], &addr, 0, NULL, &size);
#endif
			if (result != noErr)
				continue;

			AudioBufferList *buflist = (AudioBufferList *)mem_alloc(size);

#ifdef USE_AUDIODEVICE_APIS
			result = AudioDeviceGetProperty(device_ids[i], 0, 0, kAudioDevicePropertyStreamConfiguration, &size, buflist);
#else
			result = AudioObjectGetPropertyData(device_ids[i], &addr, 0, NULL, &size, buflist);
#endif
			if (result == noErr) {
				UInt32 j;
				for (j = 0; j < buflist->mNumberBuffers; j++) {
					if (buflist->mBuffers[j].mNumberChannels > 0) {
						usable = 1;
						break;
					}
				}
			}

			free(buflist);
		}

		if (!usable)
			continue;

		{
			// Prioritize CFString so we know we're getting UTF-8
			CFStringRef cfstr;
			size = sizeof(cfstr);

#ifdef USE_AUDIODEVICE_APIS
			result = AudioDeviceGetProperty(device_ids[i], 0, 0, kAudioDevicePropertyDeviceNameCFString, &size, &cfstr);
#else
			addr.mSelector = kAudioObjectPropertyName;
			result = AudioObjectGetPropertyData(device_ids[i], &addr, 0, NULL, &size, &cfstr);
#endif
			if (result == kAudioHardwareNoError) {
				ptr = _macosx_cfstring_to_utf8(cfstr);
				CFRelease(cfstr);
			}
#ifdef USE_AUDIODEVICE_APIS
			else {
				// Fallback to just receiving it as a C string
				// XXX: what encoding is this in?
				result = AudioDeviceGetPropertyInfo(device_ids[i], 0, 0, kAudioDevicePropertyDeviceName, &size, NULL);
				if (result != kAudioHardwareNoError)
					continue;

				ptr = mem_alloc(size + 1);

				result = AudioDeviceGetProperty(device_ids[i], 0, 0, kAudioDevicePropertyDeviceName, &size, ptr);
				if (result != kAudioHardwareNoError) {
					free(ptr);
					continue;
				}

				ptr[size] = '\0';
			}
#endif
		}

		if (usable) {
			// Trim any whitespace off the end of the name
			str_rtrim(ptr);
			usable = (strlen(ptr) > 0);
		}

		if (usable) {
			devices[c].id = device_ids[i];
			devices[c].name = ptr;

			c++;
		} else {
			free(ptr);
		}
	}

	// keep the real size, don't care if we allocated more.
	devices_size = c;

	return devices_size;
}

static const char *macosx_audio_device_name(uint32_t i)
{
	if (i < 0 || i >= devices_size)
		return NULL;

	return devices[i].name;
}

/* ---------------------------------------------------------- */

static int macosx_audio_init_driver(const char *driver)
{
	int fnd = 0;
	for (int i = 0; i < ARRAY_SIZE(drivers); i++) {
		if (!strcmp(drivers[i], driver)) {
			fnd = 1;
			break;
		}
	}
	if (!fnd)
		return -1;

	(void)macosx_audio_device_count();
	return 0;
}

static void macosx_audio_quit_driver(void)
{
	_macosx_audio_free_devices();
}

/* -------------------------------------------------------- */

static OSStatus macosx_audio_callback(void *inRefCon, AudioUnitRenderActionFlags *ioActionFlags, const AudioTimeStamp *inTimeStamp, UInt32 inBusNumber, UInt32 inNumberFrames, AudioBufferList *ioData)
{
	schism_audio_device_t *dev = (schism_audio_device_t *)inRefCon;
	void *ptr;
	uint32_t i;

	if (dev->paused) {
		for (i = 0; i < ioData->mNumberBuffers; i++)
			memset(ioData->mBuffers[i].mData, dev->silence, ioData->mBuffers[i].mDataByteSize);

		return 0;
	}

	for (i = 0; i < ioData->mNumberBuffers; i++) {
		uint32_t remaining = ioData->mBuffers[i].mDataByteSize;
		void *ptr = ioData->mBuffers[i].mData;
		while (remaining > 0) {
			if (dev->buffer_offset >= dev->buffer_size) {
				memset(dev->buffer, dev->silence, dev->buffer_size);
				mt_mutex_lock(dev->mutex);
				dev->callback(dev->buffer, dev->buffer_size);
				mt_mutex_unlock(dev->mutex);
				dev->buffer_offset = 0;
			}

			uint32_t len = dev->buffer_size - dev->buffer_offset;
			if (len > remaining)
				len = remaining;
			memcpy(ptr, (char *)dev->buffer + dev->buffer_offset, len);
			ptr = (char *)ptr + len;
			remaining -= len;
			dev->buffer_offset += len;
		}
	}

	return 0;
}

// nonzero on success
static schism_audio_device_t *macosx_audio_open_device(uint32_t id, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained)
{
	schism_audio_device_t *dev = mem_calloc(1, sizeof(schism_audio_device_t));

	dev->mutex = mt_mutex_create();
	if (!dev->mutex) {
		free(dev);
		return NULL;
	}

	OSStatus result = noErr;

	// build our audio stream
	AudioStreamBasicDescription desired_ca = {
		.mFormatID = kAudioFormatLinearPCM,
		.mFormatFlags =
#ifdef WORDS_BIGENDIAN // data is native endian
			kLinearPCMFormatFlagIsBigEndian |
#endif
			((desired->bits != 8) ? kLinearPCMFormatFlagIsSignedInteger : 0) |
			kLinearPCMFormatFlagIsPacked,
		.mChannelsPerFrame = desired->channels,
		.mSampleRate = desired->freq,
		.mBitsPerChannel = desired->bits,
		.mFramesPerPacket = 1,
	};
	desired_ca.mBytesPerFrame = desired_ca.mBitsPerChannel * desired_ca.mChannelsPerFrame / 8;
	desired_ca.mBytesPerPacket = desired_ca.mBytesPerFrame * desired_ca.mFramesPerPacket;

	AudioComponentDescription desc = {
		.componentType = kAudioUnitType_Output,
		.componentSubType = kAudioUnitSubType_DefaultOutput,
		.componentManufacturer = kAudioUnitManufacturer_Apple,
	};

	AudioComponent comp = AudioComponentFindNext(NULL, &desc);
	if (!comp) {
		mt_mutex_delete(dev->mutex);
		free(dev);
		return NULL;
	}

	result = AudioComponentInstanceNew(comp, &dev->au);
	if (result != noErr) {
		mt_mutex_delete(dev->mutex);
		free(dev);
		return NULL;
	}

	result = AudioUnitInitialize(dev->au);
	if (result != noErr) {
		mt_mutex_delete(dev->mutex);
		free(dev);
		return NULL;
	}

	// If a device is provided, try to find it in the list and set the current device
	// If we can't find it, punt
	if (id != AUDIO_BACKEND_DEFAULT) {
		result = AudioUnitSetProperty(dev->au, kAudioOutputUnitProperty_CurrentDevice, kAudioUnitScope_Global, 0, &devices[id].id, sizeof(devices[id].id));
		if (result != noErr) {
			mt_mutex_delete(dev->mutex);
			free(dev);
			return NULL;
		}
	}

	// Set the input format of the audio unit.
	result = AudioUnitSetProperty(dev->au, kAudioUnitProperty_StreamFormat, kAudioUnitScope_Input, 0, &desired_ca, sizeof(desired_ca));
	if (result != noErr) {
		mt_mutex_delete(dev->mutex);
		free(dev);
		return NULL;
	}

	struct AURenderCallbackStruct callback = {
		.inputProc = macosx_audio_callback,
		.inputProcRefCon = dev,
	};
	result = AudioUnitSetProperty(dev->au, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback));
	if (result != noErr) {
		mt_mutex_delete(dev->mutex);
		free(dev);
		return NULL;
	}

	dev->buffer_offset = dev->buffer_size = desired->samples * desired->channels * (desired->bits / 8);
	dev->buffer = mem_alloc(dev->buffer_size);
	dev->callback = desired->callback;
	dev->silence = (desired->bits == 8) ? 0x80 : 0;

	result = AudioOutputUnitStart(dev->au);
	if (result != noErr) {
		mt_mutex_delete(dev->mutex);
		free(dev->buffer);
		free(dev);
		return NULL;
	}

	memcpy(obtained, desired, sizeof(schism_audio_spec_t));

	return dev;
}

static void macosx_audio_close_device(schism_audio_device_t *dev)
{
	OSStatus result = noErr;

	if (!dev)
		return;

	result = AudioOutputUnitStop(dev->au);
	if (result != noErr)
		return;

	// Remove the callback
	struct AURenderCallbackStruct callback = {0};
	result = AudioUnitSetProperty(dev->au, kAudioUnitProperty_SetRenderCallback, kAudioUnitScope_Input, 0, &callback, sizeof(callback));
	if (result != noErr)
		return;

	result = AudioComponentInstanceDispose(dev->au);
	if (result != noErr)
		return;

	free(dev->buffer);
	mt_mutex_delete(dev->mutex);
	free(dev);
}

static void macosx_audio_lock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
}

static void macosx_audio_unlock_device(schism_audio_device_t *dev)
{
	if (!dev)
		return;

	mt_mutex_unlock(dev->mutex);
}

static void macosx_audio_pause_device(schism_audio_device_t *dev, int paused)
{
	if (!dev)
		return;

	mt_mutex_lock(dev->mutex);
	dev->paused = !!paused;
	mt_mutex_unlock(dev->mutex);
}

//////////////////////////////////////////////////////////////////////////////
// init functions (stubs)

static int macosx_audio_init(void)
{
	return 1;
}

static void macosx_audio_quit(void)
{
	// dont do anything
}

//////////////////////////////////////////////////////////////////////////////

const schism_audio_backend_t schism_audio_backend_macosx = {
	.init = macosx_audio_init,
	.quit = macosx_audio_quit,

	.driver_count = macosx_audio_driver_count,
	.driver_name = macosx_audio_driver_name,

	.device_count = macosx_audio_device_count,
	.device_name = macosx_audio_device_name,

	.init_driver = macosx_audio_init_driver,
	.quit_driver = macosx_audio_quit_driver,

	.open_device = macosx_audio_open_device,
	.close_device = macosx_audio_close_device,
	.lock_device = macosx_audio_lock_device,
	.unlock_device = macosx_audio_unlock_device,
	.pause_device = macosx_audio_pause_device,
};
