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

#include "fmt.h"
#include "mem.h"
#include "bits.h"
#include "osdefs.h"

#include <AudioToolbox/AudioToolbox.h>

#define MACOSXCA_BUFFER_SIZE (4096)
#define MACOSXCA_TITLE_SIZE (128) /* this is more than we'll ever need */

/* many of these aren't defined in old SDKs, and this fixes the build
 * apple defines these in an enum so we're good to #define over them */
#define kAudioFormat60958AC3 'cac3'
#define kAudioFormatAC3 'ac-3'
#define kAudioFormatAES3 'aes3'
#define kAudioFormatALaw 'alaw'
#define kAudioFormatAMR 'samr'
#define kAudioFormatAMR_WB 'sawb'
#define kAudioFormatAppleIMA4 'ima4'
#define kAudioFormatAppleLossless 'alac'
#define kAudioFormatAudible 'AUDB'
#define kAudioFormatDVIIntelIMA 'ms'
#define kAudioFormatEnhancedAC3 'ec-3'
#define kAudioFormatFLAC 'flac'
#define kAudioFormatLinearPCM 'lpcm'
#define kAudioFormatMACE3 'MAC3'
#define kAudioFormatMACE6 'MAC6'
#define kAudioFormatMIDIStream 'midi'
#define kAudioFormatMPEG4AAC 'aac '
#define kAudioFormatMPEG4AAC_ELD_SBR 'aacf'
#define kAudioFormatMPEG4AAC_ELD_V2 'aacg'
#define kAudioFormatMPEG4AAC_ELD 'aace'
#define kAudioFormatMPEG4AAC_HE 'aach'
#define kAudioFormatMPEG4AAC_HE_V2 'aacp'
#define kAudioFormatMPEG4AAC_LD 'aacl'
#define kAudioFormatMPEG4AAC_Spatial 'aacs'
#define kAudioFormatMPEG4CELP 'celp'
#define kAudioFormatMPEG4HVXC 'hvxc'
#define kAudioFormatMPEG4TwinVQ 'twvq'
#define kAudioFormatMPEGD_USAC 'usac'
#define kAudioFormatMPEGLayer1 '.mp1'
#define kAudioFormatMPEGLayer2 '.mp2'
#define kAudioFormatMPEGLayer3 '.mp3'
#define kAudioFormatMicrosoftGSM 'ms1'
#define kAudioFormatOpus 'opus'
#define kAudioFormatParameterValueStream 'apvs'
#define kAudioFormatQDesign 'QDMC'
#define kAudioFormatQDesign2 'QDM2'
#define kAudioFormatQUALCOMM 'Qclp'
#define kAudioFormatTimeCode 'time'
#define kAudioFormatULaw 'ulaw'
#define kAudioFormatiLBC 'ilbc'

#define kAudioFilePropertyEstimatedDuration 'edur'

static const char *ca_type_id_description(UInt32 format)
{
#if 0 /* uncomment this code to print out all the values for redefinition */
#define PRINT_VALUE(x) \
	do { \
		uint32_t i = bswapBE32(x); \
		char *s = (char *)&i; \
	\
		fprintf(stderr, "#define %s '%c%c%c%c'\n", #x, s[0], s[1], s[2], s[3]); \
	} while (0)
	PRINT_VALUE(kAudioFormatiLBC);
#undef PRINT_VALUE
#endif

	switch (format) {
	case kAudioFormat60958AC3:
	case kAudioFormatAC3: return "AC-3";
	case kAudioFormatAES3: return "AES3-2003";
	case kAudioFormatALaw: return "A-law 2:1";
	case kAudioFormatAMR: return "Adaptive Multi-Rate";
	case kAudioFormatAMR_WB: return "AMR Wideband";
	case kAudioFormatAppleIMA4: return "Apple IMA 4:1 ADPCM";
	case kAudioFormatAppleLossless: return "Apple Lossless Audio Codec";
	case kAudioFormatAudible: return "Audible";
	case kAudioFormatDVIIntelIMA: return "DVI/Intel IMA ADPCM";
	case kAudioFormatEnhancedAC3: return "Enhanced AC-3";
	case kAudioFormatFLAC: return "Free Lossless Audio Codec";
	case kAudioFormatLinearPCM: return "Linear PCM";
	case kAudioFormatMACE3: return "MACE 3:1";
	case kAudioFormatMACE6: return "MACE C:1";
	case kAudioFormatMIDIStream: return "MIDI Stream";
	case kAudioFormatMPEG4AAC: return "MPEG-4 AAC Low Complexity";
	case kAudioFormatMPEG4AAC_ELD_SBR:
	case kAudioFormatMPEG4AAC_ELD_V2:
	case kAudioFormatMPEG4AAC_ELD: return "MPEG-4 Enhanced Low Delay AAC";
	case kAudioFormatMPEG4AAC_HE:
	case kAudioFormatMPEG4AAC_HE_V2: return "MPEG-4 High-Efficiency AAC";
	case kAudioFormatMPEG4AAC_LD: return "MPEG-4 Low Delay AAC";
	case kAudioFormatMPEG4AAC_Spatial: return "MPEG-4 Spatial Audio Coding";
	case kAudioFormatMPEG4CELP: return "MPEG-4 CELP";
	case kAudioFormatMPEG4HVXC: return "MPEG-4 HVXC";
	case kAudioFormatMPEG4TwinVQ: return "MPEG-4 TwinVQ";
	case kAudioFormatMPEGD_USAC: return "MPEG-D Unified Speech and Audio Coding";
	case kAudioFormatMPEGLayer1: return "MPEG-1/2, Layer I";
	case kAudioFormatMPEGLayer2: return "MPEG-1/2, Layer II";
	case kAudioFormatMPEGLayer3: return "MPEG-1/2, Layer III";
	case kAudioFormatMicrosoftGSM: return "Microsoft GSM 6.10 - ACM code 49";
	case kAudioFormatOpus: return "Xiph Opus";
	/* case kAudioFormatParameterValueStream: return "???"; */
	case kAudioFormatQDesign: return "QDesign";
	case kAudioFormatQDesign2: return "QDesign 2";
	case kAudioFormatQUALCOMM: return "Qualcomm PureVoice";
	/* case kAudioFormatTimeCode: return "???"; */
	case kAudioFormatULaw: return "\xE6-Law 2:1"; /* mu-law */
	case kAudioFormatiLBC: return "Internet Low Bitrate Codec";
	default: return "CoreAudio";
	}
}

/* CoreAudio is realistic */
static SInt64 macosxca_size_cb(void *userdata)
{
	slurp_t *fp = userdata;

	/* I don't really know what we could do in the case of a nonseekable stream
	 * here  --paper */
	return slurp_length(fp);
}

static OSStatus macosxca_read_cb(void *userdata, SInt64 pos,
	UInt32 request_count, void *buffer, UInt32 *actual_count)
{
	slurp_t *fp = userdata;
	slurp_seek(fp, pos, SEEK_SET);
	*actual_count = slurp_read(fp, buffer, request_count);
	return noErr;
}

static int macosxca_read(slurp_t *fp, dmoz_file_t *file, song_sample_t *smp)
{
	OSStatus err;
	AudioFileID fid;
	UInt32 prop_size;
	AudioStreamBasicDescription format;
	char title[MACOSXCA_TITLE_SIZE];
	CFDictionaryRef info;

	title[0] = 0;

	err = AudioFileOpenWithCallbacks(fp, macosxca_read_cb, NULL,
		macosxca_size_cb, NULL, 0, &fid);
	if (err != noErr)
		return 0;

	/* XXX we also have kAudioFilePropertyFileFormat, maybe that would
	 * be more useful ? */
	prop_size = sizeof(format);
	err = AudioFileGetProperty(fid, kAudioFilePropertyDataFormat, &prop_size,
		&format);
	if (err != noErr) {
		AudioFileClose(fid);
		return 0;
	}

	/* grab the title, we use this for both file and smp */
	prop_size = sizeof(info);
	err = AudioFileGetProperty(fid, kAudioFilePropertyInfoDictionary, &prop_size,
		&info);
	if (err == noErr) {
		CFStringRef titleref;

		titleref = CFDictionaryGetValue(info, CFSTR("title"));
		if (titleref) {
			/* FIXME this should be UTF-8, but oh well */
			CFStringGetCString(titleref, title, sizeof(title), kCFStringEncodingDOSLatinUS);
			/* for completeness's sake */
			title[sizeof(title)-1] = 0;
		}
		CFRelease(info);
	}

	if (file) {
		Float64 estimated_duration;

		/* FIXME need to fill in sample stuff */
		file->description = ca_type_id_description(format.mFormatID);
		file->title = strn_dup(title, sizeof(title));
		file->type = TYPE_SAMPLE_COMPR;

		file->smp_speed = format.mSampleRate;

		prop_size = sizeof(estimated_duration);
		err = AudioFileGetProperty(fid, kAudioFilePropertyEstimatedDuration,
			&prop_size, &estimated_duration);
		if (err == noErr)
			file->smp_length = estimated_duration * file->smp_speed;
	}

	if (smp) {
		AudioStreamBasicDescription output_format;
		ExtAudioFileRef efid;
		AudioBufferList buflist;
		disko_t ds;
		unsigned char buf[MACOSXCA_BUFFER_SIZE];
		UInt32 bufsmps;
		slurp_t fakefp;
		uint32_t total_samples;

		err = ExtAudioFileWrapAudioFileID(fid, 0, &efid);
		if (err != noErr) {
			AudioFileClose(fid);
			return 0;
		}

		output_format.mSampleRate = format.mSampleRate;
		output_format.mChannelsPerFrame = MIN(format.mChannelsPerFrame, 2);
		output_format.mFormatID = kAudioFormatLinearPCM;
		output_format.mFormatFlags = kAudioFormatFlagIsSignedInteger;
		output_format.mFramesPerPacket = 1;
		/* don't bother with extra precision if it's not necessary */
		output_format.mBitsPerChannel = (!format.mBitsPerChannel || format.mBitsPerChannel > 8) ? 16 : 8;

#ifdef WORDS_BIGENDIAN
		output_format.mFormatFlags |= kAudioFormatFlagIsBigEndian;
#endif

		/* SDL_sound: these are the same for PCM */
		output_format.mBytesPerPacket = (output_format.mBitsPerChannel / 8) * output_format.mChannelsPerFrame;
		output_format.mBytesPerFrame = (output_format.mBitsPerChannel / 8) * output_format.mChannelsPerFrame;

		err = ExtAudioFileSetProperty(efid, kExtAudioFileProperty_ClientDataFormat, sizeof(output_format), &output_format);
		if (err != noErr) {
			ExtAudioFileDispose(efid);
			AudioFileClose(fid);
			return 0;
		}

		if (disko_memopen(&ds) < 0) {
			ExtAudioFileDispose(efid);
			AudioFileClose(fid);
			return 0;
		}

		buflist.mNumberBuffers = 1;
		buflist.mBuffers[0].mDataByteSize = MACOSXCA_BUFFER_SIZE;
		buflist.mBuffers[0].mNumberChannels = output_format.mChannelsPerFrame;
		buflist.mBuffers[0].mData = buf;

		/* keep going until we run out of audio to process
		 * (note: this is a waste of memory; we could make a slurp wrapper but im too lazy) */
		while (((err = ExtAudioFileRead(efid, &bufsmps, &buflist)) == noErr) && bufsmps > 0) {
			disko_write(&ds, buf, bufsmps * (output_format.mBitsPerChannel / 8) * output_format.mChannelsPerFrame);
			total_samples += bufsmps;
		}

		if (err != noErr) {
			ExtAudioFileDispose(efid);
			AudioFileClose(fid);
			return 0;
		}

		smp->length = total_samples;
		smp->c5speed = format.mSampleRate;
		/* yay */
		memcpy(smp->name, title, sizeof(smp->name)-1);
		smp->name[sizeof(smp->name)-1] = 0;

		slurp_memstream(&fakefp, ds.data, ds.length);

		csf_read_sample(smp, SF_PCMS
#ifdef WORDS_BIGENDIAN
			| SF_BE
#else
			| SF_LE
#endif
			| ((output_format.mChannelsPerFrame >= 2) ? SF_SI : SF_M)
			| ((output_format.mBitsPerChannel == 16) ? SF_16 : SF_8),
			&fakefp);

		ExtAudioFileDispose(efid);
	}

	AudioFileClose(fid);
	return 1;
}

int fmt_macosxca_read_info(dmoz_file_t *file, slurp_t *fp)
{
	return macosxca_read(fp, file, NULL);
}

int fmt_macosxca_load_sample(slurp_t *fp, song_sample_t *smp)
{
	return macosxca_read(fp, NULL, smp);
}
