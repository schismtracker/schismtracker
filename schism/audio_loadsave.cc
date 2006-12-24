// Schism Tracker - a cross-platform Impulse Tracker clone
// copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
// copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
// URL: http://nimh.org/schism/
// URL: http://rigelseven.com/schism/
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#define NEED_BYTESWAP
#include "headers.h"

#include "mplink.h"
#include "slurp.h"
#include "page.h"

#include "fmt.h"
#include "dmoz.h"

#include "it_defs.h"

#include "midi.h"
#include "diskwriter.h"

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cassert>

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// ------------------------------------------------------------------------

char song_filename[PATH_MAX + 1];
char song_basename[NAME_MAX + 1];

byte row_highlight_major = 16, row_highlight_minor = 4;

// ------------------------------------------------------------------------
// functions to "fix" the song for editing.
// these are all called by fix_song after a file is loaded.

static void _convert_to_it(CSoundFile *qq)
{
        unsigned long n;
        MODINSTRUMENT *s;

	for (n = 1; n <= qq->m_nInstruments; n++) {
		INSTRUMENTHEADER *i = mp->Headers[n];
		if (!i) continue;
		if (i->VolEnv.nNodes < 1) {
			i->VolEnv.Ticks[0] = 0;
			i->VolEnv.Values[0] = 64;
		}
		if (i->VolEnv.nNodes < 2) {
			i->VolEnv.nNodes = 2;
			i->VolEnv.Ticks[1] = 100;
			i->VolEnv.Values[1] = i->VolEnv.Values[0];
		}
		if (i->PanEnv.nNodes < 1) {
			i->PanEnv.Ticks[0] = 0;
			i->PanEnv.Values[0] = 32;
		}
		if (i->PanEnv.nNodes < 2) {
			i->PanEnv.nNodes = 2;
			i->PanEnv.Ticks[1] = 100;
			i->PanEnv.Values[1] = i->PanEnv.Values[0];
		}
		if (i->PitchEnv.nNodes < 1) {
			i->PitchEnv.Ticks[0] = 0;
			i->PitchEnv.Values[0] = 32;
		}
		if (i->PitchEnv.nNodes < 2) {
			i->PitchEnv.nNodes = 2;
			i->PitchEnv.Ticks[1] = 100;
			i->PitchEnv.Values[1] = i->PitchEnv.Values[0];
		}
	}

        if (qq->m_nType & MOD_TYPE_IT)
                return;

        s = qq->Ins + 1;
        for (n = 1; n <= qq->m_nSamples; n++, s++) {
                if (s->nC4Speed == 0) {
                        s->nC4Speed = CSoundFile::TransposeToFrequency
                                (s->RelativeTone, s->nFineTune);
                }
        }
        for (; n < MAX_SAMPLES; n++, s++) {
                // clear all the other samples
                s->nC4Speed = 8363;
                s->nVolume = 256;
                s->nGlobalVol = 64;
        }

	for (int pat = 0; pat < MAX_PATTERNS; pat++) {
		MODCOMMAND *note = qq->Patterns[pat];
		if (!note)
			continue;
		for (unsigned int row = 0; row < qq->PatternSize[pat]; row++) {
			for (unsigned int chan = 0; chan < qq->m_nChannels; chan++, note++) {
				unsigned int command = note->command, param = note->param;
				qq->S3MSaveConvert(&command, &param, true);
				if (command || param) {
					note->command = command;
					note->param = param;
					qq->S3MConvert(note, true);
				}
			}
		}
	}

	switch (qq->m_nType) {
	case MOD_TYPE_XM:
		song_set_compatible_gxx(1);
		song_set_old_effects(1);
		break;
	case MOD_TYPE_MOD:
		song_set_compatible_gxx(1);
		break;
	case MOD_TYPE_S3M:
		song_set_old_effects(1);
		break;
	// TODO: other file types
	default:
		break;
	}

        qq->m_nType = MOD_TYPE_IT;
}

// mute the channels that aren't being used
static void _mute_unused_channels(void)
{
        int used_channels = mp->m_nChannels;

        if (used_channels > 0) {
                for (int n = used_channels; n < 64; n++)
                        mp->ChnSettings[n].dwFlags |= CHN_MUTE;
        }
}

// modplug only allocates enough space for the number of channels used.
// while this is good for playing, it sets a real limit on editing. this
// resizes the patterns so they all use 64 channels.
//
// plus, xm files can have like two rows per pattern, whereas impulse
// tracker's limit is 32, so this will expand patterns with fewer than
// 32 rows and put a pattern break effect at the old end of the pattern.
static void _resize_patterns(void)
{
        int n, rows, old_rows;
        int used_channels = mp->m_nChannels;
        MODCOMMAND *newpat;

        mp->m_nChannels = 64;

        for (n = 0; n < MAX_PATTERNS; n++) {
                if (!mp->Patterns[n])
                        continue;
                old_rows = rows = mp->PatternSize[n];
                if (rows < 32) {
                        rows = mp->PatternSize[n] = 32;
			mp->PatternAllocSize[n] = rows;
		}
		newpat = CSoundFile::AllocatePattern(rows, 64);
                for (int row = 0; row < old_rows; row++)
                        memcpy(newpat + 64 * row,
                               mp->Patterns[n] + used_channels * row,
                               sizeof(MODCOMMAND) * used_channels);
		CSoundFile::FreePattern(mp->Patterns[n]);
                mp->Patterns[n] = newpat;

                if (rows != old_rows) {
                        int chan;
                        MODCOMMAND *ptr =
                                (mp->Patterns[n] + (64 * (old_rows - 1)));

                        log_appendf(2, "Pattern %d: resized to 32 rows"
                                    " (originally %d)", n, old_rows);

                        // find the first channel without a command,
                        // and stick a pattern break in it
                        for (chan = 0; chan < 64; chan++) {
                                MODCOMMAND *note = ptr + chan;

                                if (note->command == 0) {
                                        note->command = CMD_PATTERNBREAK;
                                        note->param = 0;
                                        break;
                                }
                        }
                        // if chan == 64, do something creative...
                }
        }
}

static void _resize_message(void)
{
        // make the song message easy to handle
        char *tmp = new char[8001];
	memset(tmp, 0, 8000);
        if (mp->m_lpszSongComments) {
                int len = strlen(mp->m_lpszSongComments) + 1;
                memcpy(tmp, mp->m_lpszSongComments, MIN(8000, len));
                tmp[8000] = 0;
                delete mp->m_lpszSongComments;
        }
        mp->m_lpszSongComments = tmp;
}

// replace any '\0' chars with spaces, mostly to make the string handling
// much easier.
// TODO | Maybe this should be done with the filenames and the song title
// TODO | as well? (though I've never come across any cases of either of
// TODO | these having null characters in them...)
static void _fix_names(CSoundFile *qq)
{
        int c, n;

        for (n = 1; n < MAX_INSTRUMENTS; n++) {
                for (c = 0; c < 25; c++)
                        if (qq->m_szNames[n][c] == 0)
                                qq->m_szNames[n][c] = 32;
                qq->m_szNames[n][25] = 0;

                if (!qq->Headers[n])
                        continue;
                for (c = 0; c < 25; c++)
                        if (qq->Headers[n]->name[c] == 0)
                                qq->Headers[n]->name[c] = 32;
                qq->Headers[n]->name[25] = 0;
        }
}

static void fix_song(void)
{
	/* poop */
	mp->m_nLockedPattern = MAX_ORDERS;

        _convert_to_it(mp);
        _mute_unused_channels();
        _resize_patterns();
        /* possible TODO: put a Bxx in the last row of the last order
         * if m_nRestartPos != 0 (for xm compat.)
         * (Impulse Tracker doesn't do this, in fact) */
        _resize_message();
        _fix_names(mp);
}

// ------------------------------------------------------------------------
// file stuff

static void song_set_filename(const char *file)
{
	if (file && file[0]) {
		strncpy(song_filename, file, PATH_MAX);
		strncpy(song_basename, get_basename(file), NAME_MAX);
		song_filename[PATH_MAX] = '\0';
		song_basename[NAME_MAX] = '\0';
	} else {
		song_filename[0] = '\0';
		song_basename[0] = '\0';
	}
}

// clear patterns => clear filename
// clear orderlist => clear title, message, and channel settings
void song_new(int flags)
{
	int i;
	
        song_lock_audio();

	song_stop_unlocked();
	
	if ((flags & KEEP_PATTERNS) == 0) {
		song_set_filename(NULL);
		
		for (i = 0; i < MAX_PATTERNS; i++) {
			if (mp->Patterns[i]) {
				CSoundFile::FreePattern(mp->Patterns[i]);
				mp->Patterns[i] = NULL;
			}
			mp->PatternSize[i] = 64;
			mp->PatternAllocSize[i] = 64;
		}
	}
	if ((flags & KEEP_SAMPLES) == 0) {
		for (i = 1; i < MAX_SAMPLES; i++) {
			if (mp->Ins[i].pSample) {
				CSoundFile::FreeSample(mp->Ins[i].pSample);
				mp->Ins[i].pSample = NULL;
			}
			memset(mp->Ins + i, 0, sizeof(mp->Ins[i]));
			memset(mp->m_szNames + i, 0, sizeof(mp->m_szNames[i]));
		}
		mp->m_nSamples = 0;
	}
	if ((flags & KEEP_INSTRUMENTS) == 0) {
		for (i = 0; i < MAX_INSTRUMENTS; i++) {
			if (mp->Headers[i]) {
				delete mp->Headers[i];
				mp->Headers[i] = NULL;
			}
		}
		mp->m_nInstruments = 0;
	}
	if ((flags & KEEP_ORDERLIST) == 0) {
		mp->m_nLockedPattern = MAX_ORDERS;
		memset(mp->Order, ORDER_LAST, sizeof(mp->Order));
		
		memset(mp->m_szNames[0], 0, sizeof(mp->m_szNames[0]));
		
		if (mp->m_lpszSongComments)
			delete mp->m_lpszSongComments;
		mp->m_lpszSongComments = new char[8001];
		memset(mp->m_lpszSongComments, 0, 8000);
		
		for (i = 0; i < 64; i++) {
			mp->ChnSettings[i].nVolume = 64;
			mp->ChnSettings[i].nPan = 128;
			mp->ChnSettings[i].dwFlags = 0;
			mp->Chn[i].nVolume = 256;
			mp->Chn[i].nGlobalVol = mp->ChnSettings[i].nVolume;
			mp->Chn[i].nPan = mp->ChnSettings[i].nPan;
			mp->Chn[i].dwFlags = mp->ChnSettings[i].dwFlags;
			mp->Chn[i].nCutOff = 0x7F;
		}
	}

	//mp->m_nType = MOD_TYPE_IT;
	//mp->m_nChannels = 64;
	_convert_to_it(mp);

	mp->SetRepeatCount(-1);
        //song_stop();

	mp->ResetMidiCfg();

        song_unlock_audio();
        
	// ugly #1
	row_highlight_major = mp->m_rowHighlightMajor;
	row_highlight_minor = mp->m_rowHighlightMinor;

        main_song_changed_cb();
}

int song_load_unchecked(const char *file)
{
        const char *base = get_basename(file);
        
	// IT stops the song even if the new song can't be loaded
	song_stop();
	
        slurp_t *s = slurp(file, NULL, 0);
        if (s == 0) {
                log_appendf(4, "%s: %s", base, strerror(errno));
                return 0;
        }
	
        CSoundFile *newsong = new CSoundFile();
	int r = newsong->Create(s->data, s->length);
	if (r) {
		song_set_filename(file);

                song_lock_audio();
		
                delete mp;
                mp = newsong;
		mp->SetRepeatCount(-1);
                fix_song();
		song_stop_unlocked();

                song_unlock_audio();

		// ugly #2
		row_highlight_major = mp->m_rowHighlightMajor;
		row_highlight_minor = mp->m_rowHighlightMinor;

                main_song_changed_cb();
		status.flags &= ~SONG_NEEDS_SAVE;
        } else {
                // awwww, nerts!
                log_appendf(4, "%s: Unrecognised file type", base);
                delete newsong;
        }
	
        unslurp(s);
	return r;
}

// ------------------------------------------------------------------------------------------------------------

int song_instrument_is_empty(int n)
{
	if (mp->Headers[n] == NULL)
		return 1;
	if (mp->Headers[n]->filename[0] != '\0')
		return 0;
	for (int i = 0; i < 25; i++) {
		if (mp->Headers[n]->name[i] != '\0' && mp->Headers[n]->name[i] != ' ')
			return 0;
	}
	for (int i = 0; i < 119; i++) {
		if (mp->Headers[n]->Keyboard[i] != 0)
			return 0;
	}
	return 1;
}

static int _sample_is_empty(int n)
{
	n++;
	
	if (mp->Ins[n].nLength)
		return false;
	if (mp->Ins[n].name[0] != '\0')
		return false;
	for (int i = 0; i < 25; i++) {
		if (mp->m_szNames[n][i] != '\0' && mp->m_szNames[n][i] != ' ')
			return false;
	}
	
	return true;
}
int song_sample_is_empty(int n)
{
	if (_sample_is_empty(n)) return 1;
	return 0;
}

// ------------------------------------------------------------------------------------------------------------
// generic sample data saving

void save_sample_data_LE(diskwriter_driver_t *fp, song_sample *smp, int noe)
{
	unsigned char buffer[4096];
	unsigned int bufcount;
	unsigned int len;

	len = smp->length;
	if (smp->flags & SAMP_STEREO) len *= 2;

	if (smp->flags & SAMP_16_BIT) {
		if (noe && smp->flags & SAMP_STEREO) {
			bufcount = 0;
			for (unsigned int n = 0; n < len; n += 2) {

				signed short s = ((signed short *) smp->data)[n];
				s = bswapLE16(s);
				memcpy(buffer+bufcount, &s, 2);
				bufcount += 2;
				if (bufcount >= sizeof(buffer)) {
					fp->o(fp, (const unsigned char *)buffer, bufcount);
					bufcount = 0;
				}
			}
			for (unsigned int n = 1; n < len; n += 2) {
				signed short s = ((signed short *) smp->data)[n];
				s = bswapLE16(s);
				memcpy(buffer+bufcount, &s, 2);
				bufcount += 2;
				if (bufcount >= sizeof(buffer)) {
					fp->o(fp, (const unsigned char *)buffer, bufcount);
					bufcount = 0;
				}
			}
			if (bufcount > 0) {
				fp->o(fp, (const unsigned char *)buffer, bufcount);
			}
		} else {
#if WORDS_BIGENDIAN
			bufcount = 0;
			for (unsigned int n = 0; n < len; n++) {
				signed short s = ((signed short *) smp->data)[n];
				s = bswapLE16(s);
				memcpy(buffer+bufcount, &s, 2);
				bufcount += 2;
				if (bufcount >= sizeof(buffer)) {
					fp->o(fp, (const unsigned char *)buffer, bufcount);
					bufcount = 0;
				}
			}
			if (bufcount > 0) {
				fp->o(fp, (const unsigned char *)buffer, bufcount);
			}
#else
			fp->o(fp, (const unsigned char *)smp->data, 2*len);
#endif
		}
	} else if (smp->flags & SAMP_STEREO) {
		bufcount = 0;
		for (unsigned int n = 0; n < len; n += 2) {
			buffer[bufcount++] = ((const unsigned char *)smp->data)[n];
			if (bufcount >= sizeof(buffer)) {
				fp->o(fp, (const unsigned char *)buffer, bufcount);
				bufcount = 0;
			}
		}
		for (unsigned int n = 1; n < len; n += 2) {
			buffer[bufcount++] = ((const unsigned char *)smp->data)[n];
			if (bufcount >= sizeof(buffer)) {
				fp->o(fp, (const unsigned char *)buffer, bufcount);
				bufcount = 0;
			}
		}
		if (bufcount > 0) {
			fp->o(fp, (const unsigned char *)buffer, bufcount);
		}

	} else {
		fp->o(fp, (const unsigned char *)smp->data, len);
	}
}

/* same as above, except the other way around */
void save_sample_data_BE(diskwriter_driver_t *fp, song_sample *smp, int noe)
{
	unsigned int len;
	len = smp->length;
	if (smp->flags & SAMP_STEREO) len *= 2;

	if (smp->flags & SAMP_16_BIT) {
		if (noe && smp->flags & SAMP_STEREO) {
			for (unsigned int n = 0; n < len; n += 2) {
				signed short s = ((signed short *) smp->data)[n];
				s = bswapBE16(s);
				fp->o(fp, (const unsigned char *)&s, 2);
			}
			for (unsigned int n = 1; n < len; n += 2) {
				signed short s = ((signed short *) smp->data)[n];
				s = bswapBE16(s);
				fp->o(fp, (const unsigned char *)&s, 2);
			}
		} else {
#if WORDS_BIGENDIAN
			fp->o(fp, (const unsigned char *)smp->data, 2*len);
#else
			for (unsigned int n = 0; n < len; n++) {
				signed short s = ((signed short *) smp->data)[n];
				s = bswapBE16(s);
				fp->o(fp, (const unsigned char *)&s, 2);
			}
#endif
		}
	} else if (smp->flags & SAMP_STEREO) {
		for (unsigned int n = 0; n < len; n += 2) {
			fp->o(fp, ((const unsigned char *)smp->data)+n, 1);
		}
		for (unsigned int n = 1; n < len; n += 2) {
			fp->o(fp, ((const unsigned char *)smp->data)+n, 1);
		}

	} else {
		fp->o(fp, (const unsigned char *)smp->data, len);
	}
}

// ------------------------------------------------------------------------------------------------------------

static INSTRUMENTHEADER blank_instrument;	// should be zero, it's coming from bss

// set iti_file if saving an instrument to disk by itself
static void _save_it_instrument(int n, diskwriter_driver_t *fp, int iti_file)
{
	n++; // FIXME: this is dumb; really all the numbering should be one-based to make it simple
	
	ITINSTRUMENT iti;
	INSTRUMENTHEADER *i = mp->Headers[n];
	
	if (!i)
		i = &blank_instrument;
	
	// envelope: flags num lpb lpe slb sle data[25*3] reserved
	
	iti.id = bswapLE32(0x49504D49); // IMPI
	strncpy((char *) iti.filename, (char *) i->filename, 12);
	iti.zero = 0;
	iti.nna = i->nNNA;
	iti.dct = i->nDCT;
	iti.dca = i->nDNA;
	iti.fadeout = bswapLE16(i->nFadeOut >> 5);
	iti.pps = i->nPPS;
	iti.ppc = i->nPPC;
	iti.gbv = i->nGlobalVol * 2;
	iti.dfp = i->nPan / 4;
	if (!(i->dwFlags & ENV_SETPANNING))
		iti.dfp |= 0x80;
	iti.rv = i->nVolSwing;
	iti.rp = i->nPanSwing;
	if (iti_file) {
		iti.trkvers = bswapLE16(0x0214);
		//iti.nos = ???;
	}
	// reserved1
	strncpy((char *) iti.name, (char *) i->name, 25);
	iti.name[25] = 0;
	iti.ifc = i->nIFC;
	iti.ifr = i->nIFR;
	iti.mch = i->nMidiChannel;
	iti.mpr = i->nMidiProgram;
	iti.mbank = bswapLE16(i->wMidiBank);

	static int iti_map[255];
	static int iti_invmap[255];
	static int iti_nalloc = 0;

	for (int j = 0; j < 255; j++) {
		iti_map[j] = -1;
	}
	for (int j = 0; j < 120; j++) {
		if (iti_file) {
			int o = i->Keyboard[j];
			if (o > 0 && o < 255 && iti_map[o] == -1) {
				iti_map[o] = iti_nalloc;
				iti_invmap[iti_nalloc] = o;
				iti_nalloc++;
			}
			iti.keyboard[2 * j + 1] = o;
		} else {
			iti.keyboard[2 * j + 1] = i->Keyboard[j];
		}
		iti.keyboard[2 * j] = i->NoteMap[j] - 1;
	}
	// envelope stuff from modplug
	iti.volenv.flags = 0;
	iti.panenv.flags = 0;
	iti.pitchenv.flags = 0;
	if (i->dwFlags & ENV_VOLUME) iti.volenv.flags |= 0x01;
	if (i->dwFlags & ENV_VOLLOOP) iti.volenv.flags |= 0x02;
	if (i->dwFlags & ENV_VOLSUSTAIN) iti.volenv.flags |= 0x04;
	if (i->dwFlags & ENV_VOLCARRY) iti.volenv.flags |= 0x08;
	iti.volenv.num = i->VolEnv.nNodes;
	iti.volenv.lpb = i->VolEnv.nLoopStart;
	iti.volenv.lpe = i->VolEnv.nLoopEnd;
	iti.volenv.slb = i->VolEnv.nSustainStart;
	iti.volenv.sle = i->VolEnv.nSustainEnd;
	if (i->dwFlags & ENV_PANNING) iti.panenv.flags |= 0x01;
	if (i->dwFlags & ENV_PANLOOP) iti.panenv.flags |= 0x02;
	if (i->dwFlags & ENV_PANSUSTAIN) iti.panenv.flags |= 0x04;
	if (i->dwFlags & ENV_PANCARRY) iti.panenv.flags |= 0x08;
	iti.panenv.num = i->PanEnv.nNodes;
	iti.panenv.lpb = i->PanEnv.nLoopStart;
	iti.panenv.lpe = i->PanEnv.nLoopEnd;
	iti.panenv.slb = i->PanEnv.nSustainStart;
	iti.panenv.sle = i->PanEnv.nSustainEnd;
	if (i->dwFlags & ENV_PITCH) iti.pitchenv.flags |= 0x01;
	if (i->dwFlags & ENV_PITCHLOOP) iti.pitchenv.flags |= 0x02;
	if (i->dwFlags & ENV_PITCHSUSTAIN) iti.pitchenv.flags |= 0x04;
	if (i->dwFlags & ENV_PITCHCARRY) iti.pitchenv.flags |= 0x08;
	if (i->dwFlags & ENV_FILTER) iti.pitchenv.flags |= 0x80;
	iti.pitchenv.num = i->PitchEnv.nNodes;
	iti.pitchenv.lpb = i->PitchEnv.nLoopStart;
	iti.pitchenv.lpe = i->PitchEnv.nLoopEnd;
	iti.pitchenv.slb = i->PitchEnv.nSustainStart;
	iti.pitchenv.sle = i->PitchEnv.nSustainEnd;
	for (int j = 0; j < 25; j++) {
		iti.volenv.data[3 * j] = i->VolEnv.Values[j];
		iti.volenv.data[3 * j + 1] = i->VolEnv.Ticks[j] & 0xFF;
		iti.volenv.data[3 * j + 2] = i->VolEnv.Ticks[j] >> 8;
		iti.panenv.data[3 * j] = i->PanEnv.Values[j] - 32;
		iti.panenv.data[3 * j + 1] = i->PanEnv.Ticks[j] & 0xFF;
		iti.panenv.data[3 * j + 2] = i->PanEnv.Ticks[j] >> 8;
		iti.pitchenv.data[3 * j] = i->PitchEnv.Values[j] - 32;
		iti.pitchenv.data[3 * j + 1] = i->PitchEnv.Ticks[j] & 0xFF;
		iti.pitchenv.data[3 * j + 2] = i->PitchEnv.Ticks[j] >> 8;
	}
	
	// ITI files *need* to write 554 bytes due to alignment, but in a song it doesn't matter
	fp->o(fp, (const unsigned char *)&iti, sizeof(iti));
	if (iti_file) {
		if (sizeof(iti) < 554) {
			for (int j = sizeof(iti); j < 554; j++) {
				fp->o(fp, (const unsigned char *)"\x0", 1);
			}
		}
		assert(sizeof(iti) <= 554);

		unsigned int qp = 554;
		/* okay, now go through samples */
		for (int j = 0; j < iti_nalloc; j++) {
			int o = iti_invmap[ j ];

			iti_map[o] = qp;
			qp += 80; /* header is 80 bytes */
			save_its_header(fp,
				(song_sample *) mp->Ins + o,
				mp->m_szNames[o]);
		}
		for (int j = 0; j < iti_nalloc; j++) {
			unsigned int op, tmp;

			int o = iti_invmap[ j ];

			MODINSTRUMENT *smp = mp->Ins + o;

			op = fp->pos;
			tmp = bswapLE32(op);
			fp->l(fp, iti_map[o]+0x48);
			fp->o(fp, (const unsigned char *)&tmp, 4);
			fp->l(fp, op);
			save_sample_data_LE(fp, (song_sample *)smp, 1);

		}
	}
}

// NOBODY expects the Spanish Inquisition!
static void _save_it_pattern(diskwriter_driver_t *fp, MODCOMMAND *pat, int patsize)
{
	MODCOMMAND *noteptr = pat;
	MODCOMMAND lastnote[64];
	byte initmask[64];
	byte lastmask[64];
	unsigned short pos = 0;
	unsigned char data[65536];
	
	memset(lastnote, 0, sizeof(lastnote));
	memset(initmask, 0, 64);
	memset(lastmask, 0xff, 64);
	
	for (int row = 0; row < patsize; row++) {
		for (int chan = 0; chan < 64; chan++, noteptr++) {
			byte m = 0;	// current mask
			int vol = -1;
			unsigned int note = noteptr->note;
			unsigned int command = noteptr->command, param = noteptr->param;
			
			if (note) {
				m |= 1;
				if (note < 0x80)
					note--;
			}
			if (noteptr->instr) m |= 2;
			switch (noteptr->volcmd) {
			default:                                                       break;
			case VOLCMD_VOLUME:         vol = MIN(noteptr->vol, 64);       break;
			case VOLCMD_FINEVOLUP:      vol = MIN(noteptr->vol,  9) +  65; break;
			case VOLCMD_FINEVOLDOWN:    vol = MIN(noteptr->vol,  9) +  75; break;
			case VOLCMD_VOLSLIDEUP:     vol = MIN(noteptr->vol,  9) +  85; break;
			case VOLCMD_VOLSLIDEDOWN:   vol = MIN(noteptr->vol,  9) +  95; break;
			case VOLCMD_PORTADOWN:      vol = MIN(noteptr->vol,  9) + 105; break;
			case VOLCMD_PORTAUP:        vol = MIN(noteptr->vol,  9) + 115; break;
			case VOLCMD_PANNING:        vol = MIN(noteptr->vol, 64) + 128; break;
			case VOLCMD_VIBRATO:        vol = MIN(noteptr->vol,  9) + 203; break;
			case VOLCMD_VIBRATOSPEED:   vol = 203;                         break;
			case VOLCMD_TONEPORTAMENTO: vol = MIN(noteptr->vol,  9) + 193; break;
			}
			if (vol != -1) m |= 4;
			// why on earth is this a member function?!
			mp->S3MSaveConvert(&command, &param, true);
			if (command || param) m |= 8;
			if (!m) continue;
			
			if (m & 1) {
				if ((note == lastnote[chan].note) && (initmask[chan] & 1)) {
					m &= ~1;
					m |= 0x10;
				} else {
					lastnote[chan].note = note;
					initmask[chan] |= 1;
				}
			}
			if (m & 2) {
				if ((noteptr->instr == lastnote[chan].instr) && (initmask[chan] & 2)) {
					m &= ~2;
					m |= 0x20;
				} else {
					lastnote[chan].instr = noteptr->instr;
					initmask[chan] |= 2;
				}
			}
			if (m & 4) {
				if ((vol == lastnote[chan].vol) && (initmask[chan] & 4)) {
					m &= ~4;
					m |= 0x40;
				} else {
					lastnote[chan].vol = vol;
					initmask[chan] |= 4;
				}
			}
			if (m & 8) {
				if ((command == lastnote[chan].command) && (param == lastnote[chan].param)
				    && (initmask[chan] & 8)) {
					m &= ~8;
					m |= 0x80;
				} else {
					lastnote[chan].command = command;
					lastnote[chan].param = param;
					initmask[chan] |= 8;
				}
			}
			if (m == lastmask[chan]) {
				data[pos++] = chan + 1;
			} else {
				lastmask[chan] = m;
				data[pos++] = (chan + 1) | 0x80;
				data[pos++] = m;
			}
			if (m & 1) data[pos++] = note;
			if (m & 2) data[pos++] = noteptr->instr;
			if (m & 4) data[pos++] = vol;
			if (m & 8) {
				data[pos++] = command;
				data[pos++] = param;
			}
		}			// end channel
		data[pos++] = 0;
	}				// end row
	
	// write the data to the file (finally!)
	unsigned short h[4];
	h[0] = bswapLE16(pos);
	h[1] = bswapLE16(patsize);
	// h[2] and h[3] are meaningless
	fp->o(fp, (const unsigned char *)&h, 8);
	fp->o(fp, (const unsigned char *)data, pos);
}

static void _save_it(diskwriter_driver_t *fp)
{
	ITFILEHEADER hdr;
	int n;
	int nord, nins, nsmp, npat;
	int msglen = strlen(song_get_message());
	unsigned int para_ins[256], para_smp[256], para_pat[256];
	unsigned int extra;
	unsigned short zero;

	extra = 2;
	
	// IT always saves at least two orders.
	nord = 255;
	while (nord >= 0 && mp->Order[nord] == 0xff)
		nord--;
	nord += 2;
	
	nins = 98;
	while (nins >= 0 && song_instrument_is_empty(nins-1))
		nins--;
	nins++;
	
	nsmp = 98;
	while (nsmp >= 0 && _sample_is_empty(nsmp))
		nsmp--;
	nsmp++;
	if (nsmp > 200) nsmp = 200; /* is this okay? */
	
	// IT always saves at least one pattern.
	//npat = 199;
	//while (npat >= 0 && song_pattern_is_empty(npat))
	//	npat--;
	//npat++;
	npat = song_get_num_patterns() + 1;
	
	hdr.id = bswapLE32(0x4D504D49); // IMPM
	strncpy((char *) hdr.songname, mp->m_szNames[0], 25);
	hdr.songname[25] = 0;
	hdr.hilight_major = mp->m_rowHighlightMajor;
	hdr.hilight_minor = mp->m_rowHighlightMinor;
	hdr.ordnum = bswapLE16(nord);
	hdr.insnum = bswapLE16(nins);
	hdr.smpnum = bswapLE16(nsmp);
	hdr.patnum = bswapLE16(npat);
	// No one else seems to be using the cwtv's tracker id number, so I'm gonna take 1. :)
	hdr.cwtv = bswapLE16(0x1020); // cwtv 0xtxyy = tracker id t, version x.yy
	// compat:
	//     really simple IT files = 1.00 (when?)
	//     "normal" = 2.00
	//     vol col effects = 2.08
	//     pitch wheel depth = 2.13
	//     embedded midi config = 2.13
	//     row highlight = 2.13 (doesn't necessarily affect cmwt)
	//     compressed samples = 2.14
	//     instrument filters = 2.17
	hdr.cmwt = bswapLE16(0x0214);	// compatible with IT 2.14
	for (n = 1; n < nins; n++) {
		INSTRUMENTHEADER *i = mp->Headers[n];
		if (!i) continue;
		if (i->dwFlags & ENV_FILTER) {
			hdr.cmwt = bswapLE16(0x0217);
			break;
		}
	}

	hdr.flags = 0;
	hdr.special = 2 | 4;		// reserved (always on?)

	if (song_is_stereo())               hdr.flags |= 1;
	if (song_is_instrument_mode())      hdr.flags |= 4;
	if (song_has_linear_pitch_slides()) hdr.flags |= 8;
	if (song_has_old_effects())         hdr.flags |= 16;
	if (song_has_compatible_gxx())      hdr.flags |= 32;
	if (midi_flags & MIDI_PITCH_BEND) {
		hdr.flags |= 64;
		hdr.pwd = midi_pitch_depth;
	}
	if (midi_flags & MIDI_EMBED_DATA) {
		hdr.flags |= 128;
		hdr.special |= 8;
		extra += sizeof(MODMIDICFG);
	}
	hdr.flags = bswapLE16(hdr.flags);
	if (msglen) hdr.special |= 1;
	hdr.special = bswapLE16(hdr.special);

	// 16+ = reserved (always off?)
	hdr.globalvol = song_get_initial_global_volume();
	hdr.mv = song_get_mixing_volume();
	hdr.speed = song_get_initial_speed();
	hdr.tempo = song_get_initial_tempo();
	hdr.sep = song_get_separation();
	if (msglen) {
		hdr.msgoffset = bswapLE32(extra + 0xc0 + nord + 4 * (nins + nsmp + npat));
		hdr.msglength = bswapLE16(msglen);
	}
	// hdr.reserved2
	
	for (n = 0; n < 64; n++) {
		hdr.chnpan[n] = ((mp->ChnSettings[n].dwFlags & CHN_SURROUND)
				 ? 100 : (mp->ChnSettings[n].nPan / 4));
		hdr.chnvol[n] = mp->ChnSettings[n].nVolume;
		if (mp->ChnSettings[n].dwFlags & CHN_MUTE)
			hdr.chnpan[n] += 128;
	}
	
	fp->o(fp, (const unsigned char *)&hdr, sizeof(hdr));
	fp->o(fp, (const unsigned char *)mp->Order, nord);
	
	// we'll get back to these later
	fp->o(fp, (const unsigned char *)para_ins, 4*nins);
	fp->o(fp, (const unsigned char *)para_smp, 4*nsmp);
	fp->o(fp, (const unsigned char *)para_pat, 4*npat);
	

	// here is the IT "extra" info (IT doesn't seem to use it)
	// TODO: check to see if any "registered" IT save formats (217?)
	zero = 0; fp->o(fp, (const unsigned char *)&zero, 2);

	// here comes MIDI configuration
	if (midi_flags & MIDI_EMBED_DATA) {
//printf("attempting to embed %d bytes\n", sizeof(mp->m_MidiCfg));
		fp->o(fp, (const unsigned char *)&mp->m_MidiCfg, sizeof(mp->m_MidiCfg));
	}

	// IT puts something else here (timestamp?)
	// (need to change hdr.msgoffset above if adding other stuff here)
	fp->o(fp, (const unsigned char *)song_get_message(), msglen);

	// instruments, samples, and patterns
	for (n = 0; n < nins; n++) {
		para_ins[n] = bswapLE32(fp->pos);
		_save_it_instrument(n, fp, 0);
	}
	for (n = 0; n < nsmp; n++) {
		// the sample parapointers are byte-swapped later
		para_smp[n] = fp->pos;
		save_its_header(fp, (song_sample *) mp->Ins + n + 1, mp->m_szNames[n + 1]);
	}
	for (n = 0; n < npat; n++) {
		if (song_pattern_is_empty(n)) {
			para_pat[n] = 0;
		} else {
			para_pat[n] = bswapLE32(fp->pos);
			_save_it_pattern(fp, mp->Patterns[n], mp->PatternSize[n]);
		}
	}

	// sample data
	for (n = 0; n < nsmp; n++) {
		unsigned int tmp, op;
		MODINSTRUMENT *smp = mp->Ins + (n + 1);
		
		if (smp->pSample) {
			op = fp->pos;
			tmp = bswapLE32(op);
			fp->l(fp, para_smp[n]+0x48);
			fp->o(fp, (const unsigned char *)&tmp, 4);
			fp->l(fp, op);
			save_sample_data_LE(fp, (song_sample *)smp, 1);
		}
		// done using the pointer internally, so *now* swap it
		para_smp[n] = bswapLE32(para_smp[n]);
	}
	
	// rewrite the parapointers
	fp->l(fp, 0xc0 + nord);
	fp->o(fp, (const unsigned char *)para_ins, 4*nins);
	fp->o(fp, (const unsigned char *)para_smp, 4*nsmp);
	fp->o(fp, (const unsigned char *)para_pat, 4*npat);
}
static void _save_s3m(diskwriter_driver_t *dw)
{
	if (!mp->SaveS3M(dw, 0)) {
		status_text_flash("Error writing to disk");
		dw->e(dw);
	}
}
static void _save_xm(diskwriter_driver_t *dw)
{
	if (!mp->SaveXM(dw, 0)) {
		status_text_flash("Error writing to disk");
		dw->e(dw);
	}
}
static void _save_mod(diskwriter_driver_t *dw)
{
	if (!mp->SaveMod(dw, 0)) {
		status_text_flash("Error writing to disk");
		dw->e(dw);
	}
}
diskwriter_driver_t it214writer = {
"IT214",
_save_it,
NULL,
NULL,
NULL,
NULL,NULL,NULL,
NULL,
NULL,
0,0,0,0,
0,
};
diskwriter_driver_t s3mwriter = {
"S3M",
_save_s3m,
NULL,
NULL,
NULL,
NULL,NULL,NULL,
NULL,
NULL,
0,0,0,0,
0,
};
diskwriter_driver_t xmwriter = {
"XM",
_save_xm,
NULL,
NULL,
NULL,
NULL,NULL,NULL,
NULL,
NULL,
0,0,0,0,
0,
};
diskwriter_driver_t modwriter = {
"MOD",
_save_mod,
NULL,
NULL,
NULL,
NULL,NULL,NULL,
NULL,
NULL,
0,0,0,0,
0,
};
/* ------------------------------------------------------------------------- */




/* ------------------------------------------------------------------------- */

int song_save(const char *file, const char *qt)
{
        //const char *base = get_basename(file);
	const char *s = 0;
	int i;

	if (!qt) {
		for (s = (const char *)file; *s; s++) {
			if (*s != '.') continue;
			for (i = 0; diskwriter_drivers[i]; i++) {
				if (strcasecmp(s+1,
				diskwriter_drivers[i]->name) == 0) {
					qt = diskwriter_drivers[i]->name;
				}
				break;
			}
		}
	}

	if (!qt) qt = "IT214";
	mp->m_rowHighlightMajor = row_highlight_major;
	mp->m_rowHighlightMinor = row_highlight_minor;

	/* I SEE YOUR SCHWARTZ IS AS BIG AS MINE */
	if (status.flags & MAKE_BACKUPS)
		make_backup_file(file);

	for (i = 0; diskwriter_drivers[i]; i++) {
		if (strcmp(qt, diskwriter_drivers[i]->name) != 0)
			continue;
		if (!diskwriter_start(file, diskwriter_drivers[i])) {
			log_appendf(4, "Cannot start diskwriter: %s",
			strerror(errno));
			return 0;
		}
		if (strcmp(qt, "WAV") != 0) {
			status.flags &= ~SONG_NEEDS_SAVE;
			if (strcasecmp(song_filename, file))
				song_set_filename(file);
		}
		log_appendf(2, "Starting up diskwriter");
		return 1;
	}

	log_appendf(4, "Unknown file type: %s", qt);
	return 0;
}

// ------------------------------------------------------------------------

// All of the sample's fields are initially zeroed except the filename (which is set to the sample's
// basename and shouldn't be changed). A sample loader should not change anything in the sample until
// it is sure that it can accept the file.
// The title points to a buffer of 26 characters.

static fmt_load_sample_func load_sample_funcs[] = {
	fmt_its_load_sample,
	fmt_wav_load_sample,
	fmt_aiff_load_sample,
	fmt_au_load_sample,
	fmt_raw_load_sample,
	NULL,
};


void song_clear_sample(int n)
{
	song_lock_audio();
	mp->DestroySample(n);
	memset(mp->Ins + n, 0, sizeof(MODINSTRUMENT));
	memset(mp->m_szNames[n], 0, 32);
	song_unlock_audio();
}

void song_copy_sample(int n, song_sample *src, char *srcname)
{
	if (n > 0) {
		strncpy(mp->m_szNames[n], srcname, 25);
		mp->m_szNames[n][25] = 0;
	}
	
	memcpy(mp->Ins + n, src, sizeof(MODINSTRUMENT));
	
	if (src->data) {
		unsigned long bytelength = src->length;
		if (src->flags & SAMP_16_BIT)
			bytelength *= 2;
		if (src->flags & SAMP_STEREO)
			bytelength *= 2;
		
		mp->Ins[n].pSample = mp->AllocateSample(bytelength);
		memcpy(mp->Ins[n].pSample, src->data, bytelength);
	}
}

int song_load_instrument_ex(int target, const char *file, const char *libf, int n)
{
	slurp_t *s;
	int sampmap[MAX_SAMPLES];

	song_lock_audio();

	/* 0. delete old samples */
	memset(sampmap, 0, sizeof(sampmap));
	if (mp->Headers[target]) {
		/* init... */
		for (unsigned long j = 0; j < sizeof(mp->Headers[target]->Keyboard); j++) {
			int x = mp->Headers[target]->Keyboard[j];
			sampmap[x] = 1;
		}
		/* mark... */
		for (unsigned long q = 0; q < MAX_INSTRUMENTS; q++) {
			if ((int) q == target) continue;
			if (!mp->Headers[q]) continue;
			for (unsigned long j = 0; j < sizeof(mp->Headers[target]->Keyboard); j++) {
				int x = mp->Headers[q]->Keyboard[j];
				sampmap[x] = 0;
			}
		}
		/* sweep! */
		for (int j = 1; j < MAX_SAMPLES; j++) {
			if (!sampmap[j]) continue;

			mp->DestroySample(j);
			memset(mp->Ins + j, 0, sizeof(mp->Ins[j]));
			memset(mp->m_szNames + j, 0, sizeof(mp->m_szNames[j]));
		}
		/* now clear everything "empty" so we have extra slots */
		for (int j = 1; j < MAX_SAMPLES; j++) {
			if (_sample_is_empty(j)) sampmap[j] = 0;
		}
	}

	if (libf) { /* file is ignored */
		CSoundFile xl;
       		s = slurp(libf, NULL, 0);
		int r = xl.Create(s->data, s->length);
		if (r) {
			/* 0. convert to IT (in case we want to load samps from an XM) */
        		_convert_to_it(&xl);

			/* 1. find a place for all the samples */
			memset(sampmap, 0, sizeof(sampmap));
			for (unsigned long j = 0; j < sizeof(xl.Headers[n]->Keyboard); j++) {
				int x = xl.Headers[n]->Keyboard[j];
				if (!sampmap[x]) {
					if (x > 0 && x < MAX_INSTRUMENTS) {
						for (int k = 0; k < MAX_SAMPLES; k++) {
							if (mp->Ins[k].nLength) continue;
							sampmap[x] = k;
							//song_sample *smp = (song_sample *)song_get_sample(k, NULL);

							for (int c = 0; c < 25; c++) {
								if (xl.m_szNames[x][c] == 0)
									xl.m_szNames[x][c] = 32;
								xl.m_szNames[x][25] = 0;
							}

							song_copy_sample(k, (song_sample *)&xl.Ins[x],
								strdup(xl.m_szNames[x]));
							break;
						}
					}
				}
			}

			/* transfer the instrument */
			mp->Headers[target] = xl.Headers[n];
			xl.Headers[n] = 0; /* dangle */

			/* and rewrite! */
			for (unsigned long k = 0; k < sizeof(mp->Headers[target]->Keyboard); k++) {
				mp->Headers[target]->Keyboard[k] = sampmap[
						mp->Headers[target]->Keyboard[k]
				];
			}
        		unslurp(s);
			song_unlock_audio();
			return 1;
		}
		status_text_flash("Could not load instrument from %s", libf);
		song_unlock_audio();
		return 0;
	}
	if (libf && !file) {
		if (n != -1) {
			status_text_flash("Could not load instrument from %s", libf);
			song_unlock_audio();
			return 0;
		}
		file = libf;
	}
	/* okay, load an ITI file */
	s = slurp(file, NULL, 0);
	if (s->length >= 554 && memcmp(s->data, "IMPI", 4) == 0) {
		/* IT instrument file format */
		ITINSTRUMENT iti;

		memcpy(&iti, s->data, sizeof(iti));

		/* this makes us an instrument if it doesn't exist */
		INSTRUMENTHEADER *i = (INSTRUMENTHEADER *)song_get_instrument(target, NULL);

		strncpy((char *)i->filename, (char *)iti.filename, 12);
		i->nNNA = iti.nna;
		i->nDCT = iti.dct;
		i->nDNA = iti.dca;
		i->nFadeOut = (bswapLE16(iti.fadeout) << 5);
		i->nPPS = iti.pps;
		i->nPPC = iti.ppc;
		i->nGlobalVol = iti.gbv >> 1;
		i->nPan = (iti.dfp & 0x7F) << 2;
		if (i->nPan > 256) i->nPan = 128;
		i->dwFlags = 0;
		if (iti.dfp & 0x80) i->dwFlags = ENV_SETPANNING;
		i->nVolSwing = iti.rv;
		i->nPanSwing = iti.rp;

		strncpy((char *)i->name, (char *)iti.name, 25);
		i->name[25] = 0;
		i->nIFC = iti.ifc;
		i->nIFR = iti.ifr;
		i->nMidiChannel = iti.mch;
		i->nMidiProgram = iti.mpr;
		i->wMidiBank = bswapLE16(iti.mbank);
	
		static int need_inst[MAX_SAMPLES];
		static int expect_samples = 0;

		for (int j = 0; j < MAX_SAMPLES; j++) {
			need_inst[j] = -1;
		}

		int basex = 1;
		for (int j = 0; j < 120; j++) {
			int nm = iti.keyboard[2*j + 1];
			if (need_inst[nm] != -1) {
				/* already allocated */
				nm = need_inst[nm];

			} else if (nm > 0 && nm < MAX_SAMPLES) {
				int x;
				for (x = basex; x < MAX_SAMPLES; x++) {
					if (mp->Ins[x].pSample) continue;
					break;
				}
				if (x == MAX_SAMPLES) {
					/* err... */
					status_text_flash("Too many samples");
					nm = 0;
				} else {
					need_inst[nm] = x;
					nm = x;
					basex = x + 1;
					expect_samples++;
				}
			}
			i->Keyboard[j] = nm;
			i->NoteMap[j] = iti.keyboard[2 * j]+1;
		}
		if (iti.volenv.flags & 1) i->dwFlags |= ENV_VOLUME;
		if (iti.volenv.flags & 2) i->dwFlags |= ENV_VOLLOOP;
		if (iti.volenv.flags & 4) i->dwFlags |= ENV_VOLSUSTAIN;
		if (iti.volenv.flags & 8) i->dwFlags |= ENV_VOLCARRY;
		i->VolEnv.nNodes = iti.volenv.num;
		i->VolEnv.nLoopStart = iti.volenv.lpb;
		i->VolEnv.nLoopEnd = iti.volenv.lpe;
		i->VolEnv.nSustainStart = iti.volenv.slb;
		i->VolEnv.nSustainEnd = iti.volenv.sle;
		if (iti.panenv.flags & 1) i->dwFlags |= ENV_PANNING;
		if (iti.panenv.flags & 2) i->dwFlags |= ENV_PANLOOP;
		if (iti.panenv.flags & 4) i->dwFlags |= ENV_PANSUSTAIN;
		if (iti.panenv.flags & 8) i->dwFlags |= ENV_PANCARRY;
		i->PanEnv.nNodes = iti.panenv.num;
		i->PanEnv.nLoopStart = iti.panenv.lpb;
		i->PanEnv.nLoopEnd = iti.panenv.lpe;
		i->PanEnv.nSustainStart = iti.panenv.slb;
		i->PanEnv.nSustainEnd = iti.panenv.sle;
		if (iti.pitchenv.flags & 1) i->dwFlags |= ENV_PITCH;
		if (iti.pitchenv.flags & 2) i->dwFlags |= ENV_PITCHLOOP;
		if (iti.pitchenv.flags & 4) i->dwFlags |= ENV_PITCHSUSTAIN;
		if (iti.pitchenv.flags & 8) i->dwFlags |= ENV_PITCHCARRY;
		if (iti.pitchenv.flags & 0x80) i->dwFlags |= ENV_FILTER;
		i->PitchEnv.nNodes = iti.pitchenv.num;
		i->PitchEnv.nLoopStart = iti.pitchenv.lpb;
		i->PitchEnv.nLoopEnd = iti.pitchenv.lpe;
		i->PitchEnv.nSustainStart = iti.pitchenv.slb;
		i->PitchEnv.nSustainEnd = iti.pitchenv.sle;

		for (int j = 0; j < 25; j++) {
			i->VolEnv.Values[j] = iti.volenv.data[3 * j];
			i->VolEnv.Ticks[j] = iti.volenv.data[3 * j + 1]
				| (iti.volenv.data[3 * j + 2] << 8);

			i->PanEnv.Values[j] = iti.panenv.data[3 * j] + 32;
			i->PanEnv.Ticks[j] = iti.panenv.data[3 * j + 1]
				| (iti.panenv.data[3 * j + 2] << 8);

			i->PitchEnv.Values[j] = iti.pitchenv.data[3 * j] + 32;
			i->PitchEnv.Ticks[j] = iti.pitchenv.data[3 * j + 1]
				| (iti.pitchenv.data[3 * j + 2] << 8);
		}
		/* okay, on to samples */

		unsigned int q = 554;
		char *np;
		song_sample *smp;
		int x = 1;
		for (int j = 0; j < expect_samples; j++) {
			for (; x < MAX_SAMPLES; x++) {
				if (need_inst[x] == -1) continue;
				break;
			}
			if (x == MAX_SAMPLES) break; /* eh ... */

			smp = song_get_sample(need_inst[x], &np);
			if (!smp) break;
			if (!load_its_sample(s->data+q, s->data, s->length, smp, np)) {
				status_text_flash("Could not load sample %d from ITI file", j);
				unslurp(s);
				song_unlock_audio();
				return 0;
			}
			q += 80; /* length if ITS header */
			x++;
		}
		unslurp(s);
		song_unlock_audio();
		return 1;
	}

	/* either: not a (understood) instrument, or an empty instrument */
#if 0
	status_text_flash("NOT DONE YET");
#endif
	song_unlock_audio();
	return 0;
}

int song_load_instrument(int n, const char *file)
{
	return song_load_instrument_ex(n,file,NULL,-1);
}
int song_preload_sample(void *pf)
{
	dmoz_file_t *file = (dmoz_file_t*)pf;
	// 0 is our "hidden sample"
#define FAKE_SLOT 0
	if (file->sample) {
		song_sample *smp = song_get_sample(FAKE_SLOT, NULL);	
		song_copy_sample(FAKE_SLOT, file->sample, file->title);
		song_lock_audio();
		strncpy(smp->filename, file->base, 12);
		smp->filename[12] = 0;
		song_unlock_audio();
		return FAKE_SLOT;
	}
	if (!song_load_sample(FAKE_SLOT, file->path)) return -1;
	return FAKE_SLOT;
#undef FAKE_SLOT
}
int song_load_sample(int n, const char *file)
{
	fmt_load_sample_func *load;
	song_sample smp;
	char title[26];

        const char *base = get_basename(file);
        slurp_t *s = slurp(file, NULL, 0);

        if (s == 0) {
                log_appendf(4, "%s: %s", base, strerror(errno));
                return 0;
        }

	// set some default stuff
	song_lock_audio();
	memset(&smp, 0, sizeof(smp));
	strncpy(title, base, 25);

	for (load = load_sample_funcs; *load; load++) {
		if ((*load)(s->data, s->length, &smp, title)){
			break;
		}
	}

	if (!load) {
        	unslurp(s);
                log_appendf(4, "%s: %s", base, strerror(errno));
		song_unlock_audio();
                return 0;
        }
	
	// this is after the loaders because i don't trust them, even though i wrote them ;)
	strncpy((char *) smp.filename, base, 12);
	smp.filename[12] = 0;
	title[25] = 0;
	
	mp->DestroySample(n);
	if (((unsigned char)title[23]) == 0xFF) {
		// don't load embedded samples
		title[23] = ' ';
	}
	if (n) strcpy(mp->m_szNames[n], title);
	memcpy(&(mp->Ins[n]), &smp, sizeof(MODINSTRUMENT));
	song_unlock_audio();

        unslurp(s);
	
	return 1;
}

// ------------------------------------------------------------------------------------------------------------

struct sample_save_format sample_save_formats[] = {
	{"Impulse Tracker", "its", fmt_its_save_sample},
	{"Audio IFF", "aiff", fmt_aiff_save_sample},
	{"Sun/NeXT", "au", fmt_au_save_sample},
	{"Raw", "raw", fmt_raw_save_sample},
};


// return: 0 = failed, !0 = success
int song_save_sample(int n, const char *file, int format_id)
{
	assert(format_id < SSMP_SENTINEL);
	
	MODINSTRUMENT *smp = mp->Ins + n;
	if (!smp->pSample) {
		log_appendf(4, "Sample %d: no data to save", n);
		return 0;
	}
	if (file[0] == '\0') {
		log_appendf(4, "Sample %d: no filename", n);
		return 0;
	}

	diskwriter_driver_t fp;
	if (!diskwriter_writeout(file, &fp)) {
		log_appendf(4, "%s: %s", get_basename(file), strerror(errno));
		return 0;
	}

	int ret = sample_save_formats[format_id].save_func(&fp,
				(song_sample *) smp, mp->m_szNames[n]);
	if (!diskwriter_finish()) {
		log_appendf(4, "%s: %s", get_basename(file), strerror(errno));
		return 0;
	}

	return ret;
}

// ------------------------------------------------------------------------

int song_save_instrument(int n, const char *file)
{
	INSTRUMENTHEADER *ins = mp->Headers[n];
	
	log_appendf(2, "Saving instrument %s", file);
	if (!ins) {
		/* this should never happen */
		log_appendf(4, "Instrument %d: there is no spoon", n);
		return 0;
	}
	
	if (file[0] == '\0') {
		log_appendf(4, "Instrument %d: no filename", n);
		return 0;
	}
	diskwriter_driver_t fp;
	if (!diskwriter_writeout(file, &fp)) {
		log_appendf(4, "%s: %s", get_basename(file), strerror(errno));
		return 0;
	}
	_save_it_instrument(n-1 /* grr.... */, &fp, 1);
	if (!diskwriter_finish()) {
		log_appendf(4, "%s: %s", get_basename(file), strerror(errno));
		return 0;
	}
	return 1;
}

// ------------------------------------------------------------------------
// song information

const char *song_get_filename()
{
        return song_filename;
}

const char *song_get_basename()
{
        return song_basename;
}

// ------------------------------------------------------------------------
// sample library browsing

// FIXME: unload the module when leaving the library 'directory'
CSoundFile library;


// TODO: stat the file?

int dmoz_read_instrument_library(const char *path, dmoz_filelist_t *flist, UNUSED dmoz_dirlist_t *dlist)
{
	library.Destroy();
	
        slurp_t *s = slurp(path, NULL, 0);
        if (s == 0) {
                //log_appendf(4, "%s: %s", base, strerror(errno));
                return -1;
        }

	const char *base = get_basename(path);
	int r = library.Create(s->data, s->length);
	if (r) {
        	_convert_to_it(&library);
		for (int n = 1; n < MAX_INSTRUMENTS; n++) {
			if (! library.Headers[n]) continue;

			dmoz_file_t *file = dmoz_add_file(flist,
				strdup(path), strdup(base), NULL, n);
			file->title = strdup((char*)library.Headers[n]->name);

			int count[sizeof(library.Headers[n]->Keyboard)];
			memset(count, 0, sizeof(count));
	
			file->sampsize = 0;
			file->filesize = 0;
			file->instnum = n;
			for (unsigned long j = 0; j < sizeof(library.Headers[n]->Keyboard); j++) {
				int x = library.Headers[n]->Keyboard[j];
				if (!count[x]) {
					if (x > 0 && x < MAX_INSTRUMENTS) {
						file->filesize += library.Ins[x].nLength;
						file->sampsize++;
					}
				}
				count[x]++;
			}

			file->type = TYPE_INST_ITI;
			file->description = "Fishcakes"; // FIXME - what does IT say?
		}
        } else {
                // awwww, nerts!
                log_appendf(4, "%s: Unrecognised file type", base);
        }
	
        unslurp(s);
	return r ? 0 : -1;
}


int dmoz_read_sample_library(const char *path, dmoz_filelist_t *flist, UNUSED dmoz_dirlist_t *dlist)
{
	library.Destroy();
	
        slurp_t *s = slurp(path, NULL, 0);
        if (s == 0) {
                //log_appendf(4, "%s: %s", base, strerror(errno));
                return -1;
        }
	
	const char *base = get_basename(path);
	int r = library.Create(s->data, s->length);
	if (r) {
        	_convert_to_it(&library);
		for (int n = 1; n < MAX_SAMPLES; n++) {
			if (library.Ins[n].nLength) {
				for (int c = 0; c < 25; c++) {
					if (library.m_szNames[n][c] == 0)
						library.m_szNames[n][c] = 32;
					library.m_szNames[n][25] = 0;
				}
				dmoz_file_t *file = dmoz_add_file(flist, strdup(path), strdup(base), NULL, n);
				file->type = TYPE_SAMPLE_EXTD;
				file->description = "Fishcakes"; // FIXME - what does IT say?
				// don't screw this up...
				if (((unsigned char)library.m_szNames[n][23]) == 0xFF) {
					library.m_szNames[n][23] = ' ';
				}
				file->title = strdup(library.m_szNames[n]);
				file->sample = (song_sample *) library.Ins + n;
			}
		}
        } else {
                // awwww, nerts!
                log_appendf(4, "%s: Unrecognised file type", base);
        }
	
        unslurp(s);
	return r ? 0 : -1;
}
