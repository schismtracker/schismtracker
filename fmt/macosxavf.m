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

#include "charset.h"
#include "fmt.h"
#include "util.h"
#include "loadso.h"
#include "mem.h"
#include "log.h"
#include "osdefs.h"

#import <Cocoa/Cocoa.h>
#import <AVFoundation/AVFoundation.h>

/* these aren't defined in older SDKs; define them here. */
#define kAudioFormatFLAC 'flac'
#define kAudioFormatMPEGD_USAC 'usac'
#define kAudioFormatOpus 'opus'

/* AVFoundation, like MediaFoundation is ridiculous when it comes to
 * implementing custom read/tell/seek implementations. Thus, here
 * I have decided to just do stuff the hacky way and hijack the
 * slurp structure - which I'm not exactly too keen on doing - but
 * it's really the only option I have. */

static const char *avf_type_id_description(UInt32 format)
{
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
	default: return "AVFoundation";
	}
}

static inline NSURL *url_from_path(const char *path)
{
	return [NSURL fileURLWithPath: [NSString stringWithUTF8String: path]];
}

/* caller must release */
static AVAudioFile *macosxavf_open(const char *path)
{
	NSError *err;
	AVAudioFile *af;

	af = [[AVAudioFile alloc] initForReading: url_from_path(path) error: &err];
	if (err)
		return NULL;

	return af;
}

static AVAudioPCMBuffer *avf_read_af_into_avbuf(AVAudioFile *af)
{
	AVAudioPCMBuffer *buf;
	NSError *err;

	buf = [[AVAudioPCMBuffer alloc] initWithPCMFormat: af.processingFormat frameCapacity: af.length];
	if (!buf)
		return NULL;

	if (![af readIntoBuffer: buf error: &err]) {
		[buf release];
		return NULL;
	}

	return buf;
}

static int avf_strdesc_to_sf_flags(const AudioStreamBasicDescription *apo, uint32_t *flags)
{
	if (apo->mFormatID != kAudioFormatLinearPCM)
		return 0;

	if (flags) {
		*flags = 0;

		/* channels */
		switch (apo->mChannelsPerFrame) {
		case 1:
			*flags |= SF_M;
			break;
		case 2:
			*flags |= (apo->mFormatFlags & kAudioFormatFlagIsNonInterleaved)
				? SF_SS
				: SF_SI;
			break;
		default:
			return 0;
		}

		/* bits */
		switch (apo->mBitsPerChannel) {
		case 8:  *flags |= SF_8;  break;
		case 16: *flags |= SF_16; break;
		case 24: *flags |= SF_24; break;
		case 32: *flags |= SF_32; break;
		case 64: *flags |= SF_64; break;
		default: return 0;
		}

		/* endian */
		*flags |= (apo->mFormatFlags & kAudioFormatFlagIsBigEndian)
			? SF_BE
			: SF_LE;

		/* encoding */
		*flags |= (apo->mFormatFlags & kAudioFormatFlagIsFloat)
			? SF_IEEE
			: (apo->mFormatFlags & kAudioFormatFlagIsSignedInteger)
				? SF_PCMS
				: SF_PCMU;
	}

	return 1;
}

static int avf_load_file(const char *path, AVAudioPCMBuffer **buf, uint32_t *flags, uint32_t *c5speed, uint32_t *bits, AudioFormatID *fmtid, uint32_t *length)
{
	AVAudioFile *af;
	uint32_t len;

	af = macosxavf_open(path);
	if (!af)
		return 0;

	len = af.length;

	if (length) *length = len;

	{
		const AudioStreamBasicDescription *apo, *afo;

		apo = af.processingFormat.streamDescription;
		afo = af.fileFormat.streamDescription;

		if (!avf_strdesc_to_sf_flags(apo, flags)) {
			[af release];
			return 0;
		}

		if (c5speed) *c5speed = (apo->mSampleRate);
		if (bits) *bits = (afo->mBitsPerChannel);
		if (fmtid) *fmtid = (afo->mFormatID);
	}

	if (buf) {
		*buf = avf_read_af_into_avbuf(af);
		if (!*buf || len != af.length) {
			[af release];
			return 0;
		}
	}

	/* no longer need the file. */
	[af release];

	return 1;
}

int fmt_macosxavf_read_info(dmoz_file_t *file, slurp_t *fp)
{
	if (!macosx_ver_atleast(10, 11, 0))
		return 0;

	{
		uint32_t flags, c5speed, bits, length;
		AudioFormatID fmtid;

		if (!avf_load_file(file->path, NULL, &flags, &c5speed, &bits, &fmtid, &length))
			return 0;

		file->description = avf_type_id_description(fmtid);
		file->type = (bits) ? TYPE_SAMPLE_PLAIN : TYPE_SAMPLE_COMPR;

		file->smp_speed = c5speed;

		switch (flags & SF_CHN_MASK) {
		case SF_SI:
		case SF_SS:
			file->smp_flags |= CHN_STEREO;
			break;
		case SF_M:
		default:
			break;
		}

		if ((flags & SF_BIT_MASK) > 8)
			file->smp_flags |= CHN_16BIT;

		file->smp_length = length;
	}

	{
		AVURLAsset *asset;
		NSArray *metas;
		NSUInteger i;

		asset = [AVURLAsset URLAssetWithURL: url_from_path(file->path) options: nil];
		if (!asset)
			return 0;

		metas = asset.commonMetadata;
		for (i = 0; i < [metas count]; i++) {
			AVMetadataItem *meta = [metas objectAtIndex: i];

			if ([meta.commonKey compare: @"title"] == NSOrderedSame) {
				file->title = str_dup([meta.stringValue UTF8String]);
			}
		}
	}

	return 1;
}

int fmt_macosxavf_load_sample(slurp_t *fp, song_sample_t *smp)
{
	uint32_t flags;
	AVAudioPCMBuffer *buf;

	if (!macosx_ver_atleast(10, 11, 0))
		return 0;

	{
		/* This is a horrible hack */
		int fd;
		char path[MAXPATHLEN];

		if (fp->internal.memory.interfaces.mmap.fd) {
			fd = fp->internal.memory.interfaces.mmap.fd;
		} else if (!fp->internal.memory.pos) {
			fd = fileno(fp->internal.stdio.fp);
		}

		if (fcntl(fd, F_GETPATH, path) == -1) {
			log_perror("fcntl");
			return 0;
		}

		if (!avf_load_file(path, &buf, &flags, &smp->c5speed, NULL, NULL, NULL))
			return 0;
	}

	smp->flags = 0;
	smp->volume        = 64 * 4;
	smp->global_volume = 64;
	smp->length = buf.frameCapacity;

	switch (flags & SF_CHN_MASK) {
	case SF_SI:
	case SF_M: {
		slurp_t mem;
		uint32_t chns = ((flags & SF_CHN_MASK) == SF_SI) ? 2 : 1;

		if (buf.floatChannelData) {
			slurp_memstream(&mem, (uint8_t *)*buf.floatChannelData, buf.frameCapacity * 4 * chns);
		} else if (buf.int32ChannelData) {
			slurp_memstream(&mem, (uint8_t *)*buf.int32ChannelData, buf.frameCapacity * 4 * chns);
		} else if (buf.int16ChannelData) {
			slurp_memstream(&mem, (uint8_t *)*buf.int16ChannelData, buf.frameCapacity * 2 * chns);
		} else {
			[buf release];
			return LOAD_UNSUPPORTED; /* what? */
		}

		csf_read_sample(smp, flags, &mem);

		break;
	}
	case SF_SS: {
		/* FIXME: This is a big waste of memory... */
		uint32_t stride = ((buf.floatChannelData || buf.int32ChannelData) ? 4 : 2);
		uint32_t mem_size = (buf.frameCapacity * stride);
		slurp_t s;

		if (buf.floatChannelData) {
			slurp_2memstream(&s, (uint8_t *)buf.floatChannelData[0], (uint8_t *)buf.floatChannelData[1], buf.frameCapacity * 4);
		} else if (buf.int32ChannelData) {
			slurp_2memstream(&s, (uint8_t *)buf.int32ChannelData[0], (uint8_t *)buf.int32ChannelData[1], buf.frameCapacity * 4);
		} else if (buf.int16ChannelData) {
			slurp_2memstream(&s, (uint8_t *)buf.int16ChannelData[0], (uint8_t *)buf.int16ChannelData[1], buf.frameCapacity * 2);
		} else {
			[buf release];
			return LOAD_UNSUPPORTED;
		}

		csf_read_sample(smp, flags, &s);

		break;
	}
	default:
		break;
	}

	[buf release];

	return 1;
}
