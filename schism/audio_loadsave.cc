// Schism Tracker - a cross-platform Impulse Tracker clone
// copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
// copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
// copyright (c) 2009 Storlek & Mrs. Brisby
// URL: http://schismtracker.org/
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

#include "snd_gm.h"
#include "midi.h"
#include "diskwriter.h"

#ifdef MACOSX
#include <errno.h>
#include <assert.h>
#else
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cassert>
#endif

#include <limits.h>

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

// ------------------------------------------------------------------------

char song_filename[PATH_MAX + 1];
char song_basename[NAME_MAX + 1];

uint8_t row_highlight_major = 16, row_highlight_minor = 4;

// if false, don't stop playing on load, and start playing new song afterward
int stop_on_load = 1;

// ------------------------------------------------------------------------
// quiet a sample when loading
static void _squelch_sample(int n)
{
	int i;
	SONGSAMPLE *smp = mp->Samples + n;

	for (i = 0; i < MAX_VOICES; i++) {
		if (mp->Voices[i].pInstrument == smp
		    || mp->Voices[i].pCurrentSample == smp->pSample
		    || mp->Voices[i].pSample == smp->pSample) {
			mp->Voices[i].nNote = mp->Voices[i].nNewNote = mp->Voices[i].nNewIns = 0;
			mp->Voices[i].nFadeOutVol = 0;
			mp->Voices[i].dwFlags |= CHN_KEYOFF|CHN_NOTEFADE;
			mp->Voices[i].nPeriod = 0;
			mp->Voices[i].nPos = mp->Voices[i].nLength = 0;
			mp->Voices[i].nLoopStart = 0;
			mp->Voices[i].nLoopEnd = 0;
			mp->Voices[i].nROfs = mp->Voices[i].nLOfs = 0;
			mp->Voices[i].pSample = NULL;
			mp->Voices[i].pInstrument = NULL;
			mp->Voices[i].pHeader = NULL;
			mp->Voices[i].nLeftVol = mp->Voices[i].nRightVol = 0;
			mp->Voices[i].nNewLeftVol = mp->Voices[i].nNewRightVol = 0;
			mp->Voices[i].nLeftRamp = mp->Voices[i].nRightRamp = 0;
		}
	}
}

// functions to "fix" the song for editing.
// these are all called by fix_song after a file is loaded.


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
		newpat = csf_allocate_pattern(rows, 64);
                for (int row = 0; row < old_rows; row++)
                        memcpy(newpat + 64 * row,
                               mp->Patterns[n] + used_channels * row,
                               sizeof(MODCOMMAND) * used_channels);
		csf_free_pattern(mp->Patterns[n]);
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
                delete[] mp->m_lpszSongComments;
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
                        if (qq->Samples[n].name[c] == 0)
                                qq->Samples[n].name[c] = 32;
                qq->Samples[n].name[25] = 0;

                if (!qq->Instruments[n])
                        continue;
                for (c = 0; c < 25; c++)
                        if (qq->Instruments[n]->name[c] == 0)
                                qq->Instruments[n]->name[c] = 32;
                qq->Instruments[n]->name[25] = 0;
        }
}

static void fix_song(void)
{
	/* poop */
	mp->m_nLockedOrder = MAX_ORDERS;

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

// clear patterns => clear filename and save flag
// clear orderlist => clear title, message, and channel settings
void song_new(int flags)
{
	int i;
	
        song_lock_audio();

	song_stop_unlocked(0);

	status.flags &= ~PLAIN_TEXTEDIT;
	if ((flags & KEEP_PATTERNS) == 0) {
		song_set_filename(NULL);
		status.flags &= ~SONG_NEEDS_SAVE;
		
		for (i = 0; i < MAX_PATTERNS; i++) {
			if (mp->Patterns[i]) {
				csf_free_pattern(mp->Patterns[i]);
				mp->Patterns[i] = NULL;
			}
			mp->PatternSize[i] = 64;
			mp->PatternAllocSize[i] = 64;
		}
	}
	if ((flags & KEEP_SAMPLES) == 0) {
		for (i = 1; i < MAX_SAMPLES; i++) {
			if (mp->Samples[i].pSample) {
				csf_free_sample(mp->Samples[i].pSample);
			}
		}
		memset(mp->Samples, 0, sizeof(mp->Samples));
		mp->m_nSamples = 0;
	}
	if ((flags & KEEP_INSTRUMENTS) == 0) {
		for (i = 0; i < MAX_INSTRUMENTS; i++) {
			if (mp->Instruments[i]) {
				delete mp->Instruments[i];
				mp->Instruments[i] = NULL;
			}
		}
		mp->m_nInstruments = 0;
	}
	if ((flags & KEEP_ORDERLIST) == 0) {
		mp->m_nLockedOrder = MAX_ORDERS;
		memset(mp->Orderlist, ORDER_LAST, sizeof(mp->Orderlist));
		
		memset(mp->song_title, 0, sizeof(mp->song_title));
		
		if (mp->m_lpszSongComments)
			delete mp->m_lpszSongComments;
		mp->m_lpszSongComments = new char[8001];
		memset(mp->m_lpszSongComments, 0, 8000);
		
		for (i = 0; i < 64; i++) {
			mp->Channels[i].nVolume = 64;
			mp->Channels[i].nPan = 128;
			mp->Channels[i].dwFlags = 0;
			mp->Voices[i].nVolume = 256;
			mp->Voices[i].nGlobalVol = mp->Channels[i].nVolume;
			mp->Voices[i].nPan = mp->Channels[i].nPan;
			mp->Voices[i].dwFlags = mp->Channels[i].dwFlags;
			mp->Voices[i].nCutOff = 0x7F;
		}
	}

	mp->m_nChannels = 64;
	mp->m_nType = MOD_TYPE_IT;

	mp->m_nRepeatCount = mp->m_nInitialRepeatCount = -1;
        //song_stop();

	csf_reset_midi_cfg(mp);

        song_unlock_audio();

	// ugly #1
	row_highlight_major = mp->m_rowHighlightMajor;
	row_highlight_minor = mp->m_rowHighlightMinor;

        main_song_changed_cb();
}

static int _modplug_load_song(CSoundFile *csf, slurp_t *sl, UNUSED unsigned int flags)
{
	printf("note: using modplug's loader\n");
	return csf->Create(sl->data, sl->length) ? LOAD_SUCCESS : LOAD_UNSUPPORTED;
}


int song_load_unchecked(const char *file)
{
        const char *base = get_basename(file);
        int was_playing;

	// IT stops the song even if the new song can't be loaded
	if (stop_on_load) {
		was_playing = 0;
		song_stop();
	} else {
		was_playing = (song_get_mode() == MODE_PLAYING);
	}
	
        slurp_t *s = slurp(file, NULL, 0);
        if (!s) {
                log_appendf(4, "%s: %s", base, strerror(errno));
                return 0;
        }

        CSoundFile *newsong = csf_allocate();
        int r = fmt_mod_load_song(newsong, s, 0) == LOAD_SUCCESS
        	|| _modplug_load_song(newsong, s, 0) == LOAD_SUCCESS;
	if (r) {
		song_set_filename(file);

                song_lock_audio();
		
                csf_free(mp);
                mp = newsong;
		mp->m_nRepeatCount = mp->m_nInitialRepeatCount = -1;
		max_channels_used = 0;
                fix_song();
		song_stop_unlocked(0);
		song_unlock_audio();
		
		if (was_playing && !stop_on_load)
			song_start();

		// ugly #2
		row_highlight_major = mp->m_rowHighlightMajor;
		row_highlight_minor = mp->m_rowHighlightMinor;

                main_song_changed_cb();

		status.flags &= ~SONG_NEEDS_SAVE;
		status.flags &= ~PLAIN_TEXTEDIT;
	
	} else if (status.flags & STARTUP_TEXTEDIT) {
		song_new(~0);
		song_set_filename(file);

		if (mp->m_lpszSongComments)
			delete mp->m_lpszSongComments;
		mp->m_lpszSongComments = new char[s->length+1];
		memcpy(mp->m_lpszSongComments, s->data, s->length);
		mp->m_lpszSongComments[s->length] = '\0';

		status.flags &= ~SONG_NEEDS_SAVE;
		status.flags |= PLAIN_TEXTEDIT;
		r = 1;
		
        } else {
                // awwww, nerts!
                log_appendf(4, "%s: Unrecognised file type", base);
		status.flags &= ~PLAIN_TEXTEDIT;
                csf_free(newsong);
        }
	
        unslurp(s);
	return r;
}

// ------------------------------------------------------------------------------------------------------------

int song_instrument_is_empty(int n)
{
	if (mp->Instruments[n] == NULL)
		return 1;
	if (mp->Instruments[n]->filename[0] != '\0')
		return 0;
	for (int i = 0; i < 25; i++) {
		if (mp->Instruments[n]->name[i] != '\0' && mp->Instruments[n]->name[i] != ' ')
			return 0;
	}
	for (int i = 0; i < 119; i++) {
		if (mp->Instruments[n]->Keyboard[i] != 0)
			return 0;
	}
	if (mp->Instruments[n]->wMidiBank
	||mp->Instruments[n]->nMidiProgram
	||mp->Instruments[n]->nMidiChannelMask
	||mp->Instruments[n]->nMidiDrumKey) return 0;
	return 1;
}

int song_sample_is_empty(int n)
{
	n++;
	
	if (mp->Samples[n].nLength)
		return false;
	if (mp->Samples[n].filename[0] != '\0')
		return false;
	for (int i = 0; i < 25; i++) {
		if (mp->Samples[n].name[i] != '\0' && mp->Samples[n].name[i] != ' ')
			return false;
	}
	
	return true;
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
/* the one thing cheese did that was pretty nifty was the disk read/write operations had separate big-endian
   and little-endian operations...
   we could probably do something like that, say fp->o and fp->O for bigendian
   but 'O' looks too much like zero, so maybe rename 'em to w/W ... and have r/R to read whatever endianness
   anyway, just a thought. /storlek */
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

static SONGINSTRUMENT blank_instrument;	// should be zero, it's coming from bss

// set iti_file if saving an instrument to disk by itself
static void _save_it_instrument(int n, diskwriter_driver_t *fp, int iti_file)
{
	n++; // FIXME: this is dumb; really all the numbering should be one-based to make it simple
	
	ITINSTRUMENT iti;
	SONGINSTRUMENT *i = mp->Instruments[n];
	
	if (!i)
		i = &blank_instrument;
	
	// envelope: flags num lpb lpe slb sle data[25*3] reserved
	
	iti.id = bswapLE32(0x49504D49); // IMPI
	strncpy((char *) iti.filename, (char *) i->filename, 12);
	iti.zero = 0;
	iti.nna = i->nNNA;
	iti.dct = i->nDCT;
	iti.dca = i->nDCA;
	iti.fadeout = bswapLE16(i->nFadeOut >> 5);
	iti.pps = i->nPPS;
	iti.ppc = i->nPPC;
	iti.gbv = i->nGlobalVol;
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
	iti.mch = 0;
	if(i->nMidiChannelMask >= 0x10000)
	{
	    iti.mch = i->nMidiChannelMask - 0x10000;
	    if(iti.mch <= 16) iti.mch = 16;
	}
	else if(i->nMidiChannelMask & 0xFFFF)
	{
	    iti.mch = 1;
	    while(!(i->nMidiChannelMask & (1 << (iti.mch-1)))) ++iti.mch;
	}
	iti.mpr = i->nMidiProgram;
	iti.mbank = bswapLE16(i->wMidiBank);

	static int iti_map[255];
	static int iti_invmap[255];
	static int iti_nalloc = 0;

	iti_nalloc = 0;
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
			iti.keyboard[2 * j + 1] = iti_map[o]+1;
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
				(song_sample *) mp->Samples + o,
				mp->Samples[o].name);
		}
		for (int j = 0; j < iti_nalloc; j++) {
			unsigned int op, tmp;

			int o = iti_invmap[ j ];

			SONGSAMPLE *smp = mp->Samples + o;

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
	uint8_t initmask[64];
	uint8_t lastmask[64];
	unsigned short pos = 0;
	uint8_t data[65536];
	
	memset(lastnote, 0, sizeof(lastnote));
	memset(initmask, 0, 64);
	memset(lastmask, 0xff, 64);
	
	for (int row = 0; row < patsize; row++) {
		for (int chan = 0; chan < 64; chan++, noteptr++) {
			uint8_t m = 0;	// current mask
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
			csf_export_s3m_effect(&command, &param, true);
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

	feature_check_instruments("IT", 99,
			ENV_SETPANNING
			|ENV_VOLUME|ENV_VOLSUSTAIN|ENV_VOLLOOP|ENV_VOLCARRY
			|ENV_PANNING|ENV_PANSUSTAIN|ENV_PANLOOP|ENV_PANCARRY
			|ENV_PITCH|ENV_PITCHSUSTAIN|ENV_PITCHLOOP|ENV_PITCHCARRY
			|ENV_FILTER);

	feature_check_samples("IT", 99,
			SAMP_16_BIT
			| SAMP_LOOP | SAMP_LOOP_PINGPONG
			| SAMP_SUSLOOP | SAMP_SUSLOOP_PINGPONG
			| SAMP_PANNING
			| SAMP_GLOBALVOL
			| SAMP_STEREO);

	feature_check_notes("IT",
			0, 255,
			0, 255, 
			".ABCDEFGHvp",
			".ABCDEFGHIJKLMNOPQRSTUVWXYZ`1~"); /* ` means ===, 1 means ^^^, ~ means ~~~ */

	extra = 2;
	
	// IT always saves at least two orders.
	nord = 255;
	while (nord >= 0 && mp->Orderlist[nord] == 0xff)
		nord--;
	nord += 2;
	
	nins = 198;
	while (nins >= 0 && song_instrument_is_empty(nins-1))
		nins--;
	nins++;
	
	nsmp = 198;
	while (nsmp >= 0 && song_sample_is_empty(nsmp))
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
	strncpy((char *) hdr.songname, mp->song_title, 25);
	hdr.songname[25] = 0;
	hdr.hilight_major = mp->m_rowHighlightMajor;
	hdr.hilight_minor = mp->m_rowHighlightMinor;
	hdr.ordnum = bswapLE16(nord);
	hdr.insnum = bswapLE16(nins);
	hdr.smpnum = bswapLE16(nsmp);
	hdr.patnum = bswapLE16(npat);
	// No one else seems to be using the cwtv's tracker id number, so I'm gonna take 1. :)
	hdr.cwtv = bswapLE16(0x1050); // cwtv 0xtxyy = tracker id t, version x.yy
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
		SONGINSTRUMENT *i = mp->Instruments[n];
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
		hdr.chnpan[n] = ((mp->Channels[n].dwFlags & CHN_SURROUND)
				 ? 100 : (mp->Channels[n].nPan / 4));
		hdr.chnvol[n] = mp->Channels[n].nVolume;
		if (mp->Channels[n].dwFlags & CHN_MUTE)
			hdr.chnpan[n] += 128;
	}
	
	fp->o(fp, (const unsigned char *)&hdr, sizeof(hdr));
	fp->o(fp, (const unsigned char *)mp->Orderlist, nord);
	
	// we'll get back to these later
	fp->o(fp, (const unsigned char *)para_ins, 4*nins);
	fp->o(fp, (const unsigned char *)para_smp, 4*nsmp);
	fp->o(fp, (const unsigned char *)para_pat, 4*npat);
	

	// here is the IT "extra" info (IT doesn't seem to use it)
	// TODO: check to see if any "registered" IT save formats (217?)
	zero = 0; fp->o(fp, (const unsigned char *)&zero, 2);

	// here comes MIDI configuration
	// here comes MIDI configuration
	// right down MIDI configuration lane
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
		save_its_header(fp, (song_sample *) mp->Samples + n + 1, mp->Samples[n + 1].name);
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
		SONGSAMPLE *smp = mp->Samples + (n + 1);
		
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
	feature_check_instruments("S3M", 0, 0);
	feature_check_samples("S3M", 99, SAMP_LOOP | SAMP_ADLIB);
	feature_check_notes("S3M",
			0, 96,
			0, 255, 
			".v",
			".ABCDEFGHIJKLOQRSTUV1`"); /* ` means ===, 1 means ^^^, ~ means ~~~ */

	if (!mp->SaveS3M(dw, 0)) {
		status_text_flash("Error writing to disk");
		dw->e(dw);
	}
}

static void _save_xm(diskwriter_driver_t *dw)
{
	feature_check_instruments("XM", 99, 
			ENV_VOLUME | ENV_VOLSUSTAIN | ENV_VOLLOOP
			| ENV_PANNING | ENV_PANSUSTAIN | ENV_PANLOOP);
	feature_check_samples("XM", 99,
			SAMP_STEREO | SAMP_16_BIT
			| SAMP_LOOP | SAMP_LOOP_PINGPONG);
	feature_check_notes("XM",
			0, 96,
			0, 255, 
			".vpABCDEFGH$<>",
			".ABCDEFGHIJKLMNOPQRSTUVWXYZ1!#$%&"); /* ` means ===, 1 means ^^^, ~ means ~~~ */

	if (!mp->SaveXM(dw, 0)) {
		status_text_flash("Error writing to disk");
		dw->e(dw);
	}
}

static void _save_txt(diskwriter_driver_t *fp)
{
	const char *s = (const char *)song_get_message();
	for (int i = 0; s[i]; i++) {
		if (s[i] == '\r' && s[i+1] == '\n') {
			continue;

		} else if (s[i] == '\r') {
			fp->o(fp, (const unsigned char *)"\n", 1);
		} else {
			fp->o(fp, ((const unsigned char *)s)+i, 1);
		}
	}
}

static void _save_mod(diskwriter_driver_t *dw)
{
	feature_check_instruments("MOD", 0,  0);
	feature_check_samples("MOD", 31, SAMP_LOOP);
	feature_check_notes("MOD",
			12, 96,
			0, 31, 
			".",
			".ABCDEFGHIJKLMNOPQRSTUVWXYZ1!#$%&"); /* ` means ===, 1 means ^^^, ~ means ~~~ */

	if (!mp->SaveMod(dw, 0)) {
		status_text_flash("Error writing to disk");
		dw->e(dw);
	}
}

diskwriter_driver_t it214writer = {
	"IT214", "it", 0, _save_it, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0,
};
diskwriter_driver_t s3mwriter = {
	"S3M", "s3m", 0, _save_s3m, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0,
};
diskwriter_driver_t xmwriter = {
	"XM", "xm", 0, _save_xm, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0,
};
diskwriter_driver_t modwriter = {
	"MOD", "mod", 0, _save_mod, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0,
};
diskwriter_driver_t mtmwriter = {
	"MTM", "mtm", 0, fmt_mtm_save_song, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0,
};
diskwriter_driver_t midiwriter = {
	"MIDI", "mid", 1, fmt_mid_save_song, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0,
};
diskwriter_driver_t txtwriter = {
	"TXT", "txt", -1, _save_txt, NULL, NULL, NULL, NULL, NULL, NULL, NULL, NULL, 0, 0, 0, 0, 0,
};

/* ------------------------------------------------------------------------- */

int song_save(const char *file, const char *qt)
{
	const char *ext;
	char *freeme;
	int i, j, nsmp, nins;

	freeme = NULL;
	ext = get_extension(file);
	if (!*ext) {
		if (!qt) {
			log_appendf(4, "Error: no extension, and no save/type");
			return 1;
		}
		freeme = (char*)mem_alloc((i=strlen(file)) + strlen(qt) + 3);
		strcpy(freeme, file);
		freeme[i] = '.';i++;
		for (j = 0; qt[j]; j++, i++) freeme[i] = tolower(((unsigned int)(qt[j])));
		freeme[i] = '\0';
		file = freeme;
		puts(file);
	}
	

	// fix m_nSamples and m_nInstruments
	nsmp = 198;
	while (nsmp >= 0 && song_sample_is_empty(nsmp))
		nsmp--;

	nins = 198;
	while (nins >= 0 && song_instrument_is_empty(nins-1))
		nins--;
	nins++;
	mp->m_nSamples = nsmp;
	mp->m_nInstruments = nins;

	if (!qt) {
		for (i = 0; diskwriter_drivers[i]; i++) {
			if (strcasecmp(ext, diskwriter_drivers[i]->extension) == 0) {
				qt = diskwriter_drivers[i]->name;
				break;
			}
		}
	}
	if (!qt) { /* still? damn */
		if (status.flags & PLAIN_TEXTEDIT) {
			/* okay, the "default" for textedit is plain-text */
			if (diskwriter_start(file, &txtwriter) != DW_OK) {
				log_appendf(4, "Cannot start diskwriter: %s", strerror(errno));
				if (freeme) free(freeme);
				return 0;
			}
			log_appendf(2, "Starting up diskwriter");
			if (freeme) free(freeme);
			return 1;
		}

		qt = "IT214";
	}
	if (status.flags & PLAIN_TEXTEDIT) {
		log_appendf(3, "Warning: saving plain text as module");
	}

	mp->m_rowHighlightMajor = row_highlight_major;
	mp->m_rowHighlightMinor = row_highlight_minor;

	for (i = 0; diskwriter_drivers[i]; i++) {
		if (strcmp(qt, diskwriter_drivers[i]->name) != 0)
			continue;
		if (diskwriter_start(file, diskwriter_drivers[i]) != DW_OK) {
			log_appendf(4, "Cannot start diskwriter: %s",
			strerror(errno));
			if (freeme) free(freeme);
			return 0;
		}
		if (! diskwriter_drivers[i]->export_only) {
			status.flags &= ~SONG_NEEDS_SAVE;
			if (strcasecmp(song_filename, file))
				song_set_filename(file);
		}
		log_appendf(2, "Starting up diskwriter");
		if (freeme) free(freeme);
		return 1;
	}

	log_appendf(4, "Unknown file type: %s", qt);
	if (freeme) free(freeme);
	return 0;
}

// ------------------------------------------------------------------------

// All of the sample's fields are initially zeroed except the filename (which is set to the sample's
// basename and shouldn't be changed). A sample loader should not change anything in the sample until
// it is sure that it can accept the file.
// The title points to a buffer of 26 characters.

static fmt_load_sample_func load_sample_funcs[] = {
	fmt_its_load_sample,
	fmt_scri_load_sample,
	fmt_wav_load_sample,
	fmt_aiff_load_sample,
	fmt_au_load_sample,
	fmt_raw_load_sample,
	NULL,
};

static fmt_load_instrument_func load_instrument_funcs[] = {
	fmt_iti_load_instrument,
	fmt_xi_load_instrument,
	fmt_pat_load_instrument,
	fmt_scri_load_instrument,
	NULL,
};


void song_clear_sample(int n)
{
	song_lock_audio();
	csf_destroy_sample(mp, n);
	memset(mp->Samples + n, 0, sizeof(SONGSAMPLE));
	song_unlock_audio();
}

void song_copy_sample(int n, song_sample *src)
{
	memcpy(mp->Samples + n, src, sizeof(SONGSAMPLE));

	if (src->data) {
		unsigned long bytelength = src->length;
		if (src->flags & SAMP_16_BIT)
			bytelength *= 2;
		if (src->flags & SAMP_STEREO)
			bytelength *= 2;
		
		mp->Samples[n].pSample = csf_allocate_sample(bytelength);
		memcpy(mp->Samples[n].pSample, src->data, bytelength);
	}
}

int song_load_instrument_ex(int target, const char *file, const char *libf, int n)
{
	slurp_t *s;
	int sampmap[MAX_SAMPLES];
	int r, x;

	song_lock_audio();

	/* 0. delete old samples */
	memset(sampmap, 0, sizeof(sampmap));
	if (mp->Instruments[target]) {
		/* init... */
		for (unsigned int j = 0; j < 128; j++) {
			x = mp->Instruments[target]->Keyboard[j];
			sampmap[x] = 1;
		}
		/* mark... */
		for (unsigned int q = 0; q < MAX_INSTRUMENTS; q++) {
			if ((int) q == target) continue;
			if (!mp->Instruments[q]) continue;
			for (unsigned int j = 0; j < 128; j++) {
				x = mp->Instruments[q]->Keyboard[j];
				sampmap[x] = 0;
			}
		}
		/* sweep! */
		for (int j = 1; j < MAX_SAMPLES; j++) {
			if (!sampmap[j]) continue;

			csf_destroy_sample(mp, j);
			memset(mp->Samples + j, 0, sizeof(mp->Samples[j]));
		}
		/* now clear everything "empty" so we have extra slots */
		for (int j = 1; j < MAX_SAMPLES; j++) {
			if (song_sample_is_empty(j)) sampmap[j] = 0;
		}
	}

	if (libf) { /* file is ignored */
		CSoundFile xl;
       		s = slurp(libf, NULL, 0);
		r = xl.Create(s->data, s->length);
		if (r) {
			/* 1. find a place for all the samples */
			memset(sampmap, 0, sizeof(sampmap));
			for (unsigned int j = 0; j < 128; j++) {
				x = xl.Instruments[n]->Keyboard[j];
				if (!sampmap[x]) {
					if (x > 0 && x < MAX_INSTRUMENTS) {
						for (int k = 1; k < MAX_SAMPLES; k++) {
							if (mp->Samples[k].nLength) continue;
							sampmap[x] = k;
							//song_sample *smp = (song_sample *)song_get_sample(k, NULL);

							for (int c = 0; c < 25; c++) {
								if (xl.Samples[x].name == 0)
									xl.Samples[x].name[c] = 32;
							}
							xl.Samples[x].name[25] = 0;

							song_copy_sample(k, (song_sample *)&xl.Samples[x]);
							break;
						}
					}
				}
			}

			/* transfer the instrument */
			mp->Instruments[target] = xl.Instruments[n];
			xl.Instruments[n] = 0; /* dangle */

			/* and rewrite! */
			for (unsigned int k = 0; k < 128; k++) {
				mp->Instruments[target]->Keyboard[k] = sampmap[
						mp->Instruments[target]->Keyboard[k]
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
        if (s == 0) {
                log_appendf(4, "%s: %s", file, strerror(errno));
		song_unlock_audio();
                return 0;
        }

	r = 0;
	for (x = 0; load_instrument_funcs[x]; x++) {
		r = load_instrument_funcs[x](s->data, s->length, target);
		if (r) break;
	}

	unslurp(s);
	song_unlock_audio();

	return r;
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
	//_squelch_sample(FAKE_SLOT);
	if (file->sample) {
		song_sample *smp = song_get_sample(FAKE_SLOT, NULL);

		song_lock_audio();
		csf_destroy_sample(mp, FAKE_SLOT);
		song_copy_sample(FAKE_SLOT, file->sample);
		strncpy(smp->name, file->title, 25);
		smp->name[25] = 0;
		strncpy(smp->filename, file->base, 12);
		smp->filename[12] = 0;
		song_unlock_audio();
		return FAKE_SLOT;
	}
	return song_load_sample(FAKE_SLOT, file->path) ? FAKE_SLOT : KEYJAZZ_NOINST;
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
	_squelch_sample(n);
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
	
	csf_destroy_sample(mp, n);
	if (((unsigned char)title[23]) == 0xFF) {
		// don't load embedded samples
		title[23] = ' ';
	}
	memcpy(&(mp->Samples[n]), &smp, sizeof(SONGSAMPLE));
	if (n) strcpy(mp->Samples[n].name, title);
	song_unlock_audio();

        unslurp(s);
	
	return 1;
}

// ------------------------------------------------------------------------------------------------------------

struct sample_save_format sample_save_formats[] = {
	{"Impulse Tracker", "its", fmt_its_save_sample},
	{"Audio IFF", "aiff", fmt_aiff_save_sample},
	{"Sun/NeXT", "au", fmt_au_save_sample},
	{"IBM/Microsoft WAV", "wav", fmt_wav_save_sample},
	{"Raw", "raw", fmt_raw_save_sample},
};


// return: 0 = failed, !0 = success
int song_save_sample(int n, const char *file, int format_id)
{
	assert(format_id < SSMP_SENTINEL);
	
	SONGSAMPLE *smp = mp->Samples + n;
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
				(song_sample *) smp, mp->Samples[n].name);
	if (diskwriter_finish() == DW_ERROR) {
		log_appendf(4, "%s: %s", get_basename(file), strerror(errno));
		return 0;
	}

	return ret;
}

// ------------------------------------------------------------------------

int song_save_instrument(int n, const char *file)
{
	SONGINSTRUMENT *ins = mp->Instruments[n];
	
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
	if (diskwriter_finish() == DW_ERROR) {
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
	unsigned int j;
	int x;

	_squelch_sample(0);
	csf_destroy(&library);
	
        slurp_t *s = slurp(path, NULL, 0);
        if (s == 0) {
                //log_appendf(4, "%s: %s", base, strerror(errno));
                return -1;
        }

	const char *base = get_basename(path);
	int r = library.Create(s->data, s->length);
	if (r) {
		for (int n = 1; n < MAX_INSTRUMENTS; n++) {
			if (! library.Instruments[n]) continue;

			dmoz_file_t *file = dmoz_add_file(flist,
				str_dup(path), str_dup(base), NULL, n);
			file->title = str_dup((char*)library.Instruments[n]->name);

			int count[128];
			memset(count, 0, sizeof(count));
	
			file->sampsize = 0;
			file->filesize = 0;
			file->instnum = n;
			for (j = 0; j < 128; j++) {
				x = library.Instruments[n]->Keyboard[j];
				if (!count[x]) {
					if (x > 0 && x < MAX_INSTRUMENTS) {
						file->filesize += library.Samples[x].nLength;
						file->sampsize++;
					}
				}
				count[x]++;
			}

			file->type = TYPE_INST_ITI;
			file->description = "Fishcakes";
			// IT doesn't support this, despite it being useful.
			// Simply "unrecognized"
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
	_squelch_sample(0);
	csf_destroy(&library);
	
        slurp_t *s = slurp(path, NULL, 0);
        if (s == 0) {
                //log_appendf(4, "%s: %s", base, strerror(errno));
                return -1;
        }
	
	const char *base = get_basename(path);
	int r = library.Create(s->data, s->length);
	if (r) {
		for (int n = 1; n < MAX_SAMPLES; n++) {
			if (library.Samples[n].nLength) {
				for (int c = 0; c < 25; c++) {
					if (library.Samples[n].name[c] == 0)
						library.Samples[n].name[c] = 32;
					library.Samples[n].name[25] = 0;
				}
				dmoz_file_t *file = dmoz_add_file(flist, str_dup(path), str_dup(base), NULL, n);
				file->type = TYPE_SAMPLE_EXTD;
				file->description = "Fishcakes"; // FIXME - what does IT say?
				file->smp_speed = library.Samples[n].nC5Speed;
				file->smp_loop_start = library.Samples[n].nLoopStart;
				file->smp_loop_end = library.Samples[n].nLoopEnd;
				file->smp_sustain_start = library.Samples[n].nSustainStart;
				file->smp_sustain_end = library.Samples[n].nSustainEnd;
				file->smp_length = library.Samples[n].nLength;
				file->smp_flags = library.Samples[n].uFlags;
				file->smp_defvol = library.Samples[n].nVolume>>2;
				file->smp_gblvol = library.Samples[n].nGlobalVol;
				file->smp_vibrato_speed = library.Samples[n].nVibRate;
				file->smp_vibrato_depth = library.Samples[n].nVibDepth;
				file->smp_vibrato_rate = library.Samples[n].nVibSweep;
				// don't screw this up...
				if (((unsigned char)library.Samples[n].name[23]) == 0xFF) {
					library.Samples[n].name[23] = ' ';
				}
				file->title = str_dup(library.Samples[n].name);
				file->sample = (song_sample *) library.Samples + n;
			}
		}
        } else {
                // awwww, nerts!
                log_appendf(4, "%s: Unrecognised file type", base);
        }
	
        unslurp(s);
	return r ? 0 : -1;
}

