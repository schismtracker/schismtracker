// Schism Tracker - a cross-platform Impulse Tracker clone
// copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cassert>

#include <limits.h>

// ------------------------------------------------------------------------

char song_filename[PATH_MAX + 1];
char song_basename[NAME_MAX + 1];

byte row_highlight_major = 16, row_highlight_minor = 4;

// ------------------------------------------------------------------------
// functions to "fix" the song for editing.
// these are all called by fix_song after a file is loaded.

#if 0
static void _convert_to_it(void)
{
        unsigned long n;
        MODINSTRUMENT *s;

        if (mp->m_nType & MOD_TYPE_IT)
                return;

        s = mp->Ins + 1;
        for (n = 1; n <= mp->m_nSamples; n++, s++) {
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
		MODCOMMAND *note = mp->Patterns[pat];
		if (!note)
			continue;
		for (unsigned int row = 0; row < mp->PatternSize[pat]; row++) {
			for (unsigned int chan = 0; chan < mp->m_nChannels; chan++, note++) {
				unsigned long command = note->command, param = note->param;
				mp->S3MSaveConvert(&command, &param, true);
				if (command || param) {
					note->command = command;
					note->param = param;
					mp->S3MConvert(note, true);
				}
			}
		}
	}

        mp->m_nType = MOD_TYPE_IT;
}
#endif

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
                if (rows < 32)
                        rows = mp->PatternSize[n] = 32;
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
static void _fix_names(void)
{
        int c, n;

        for (n = 1; n < 100; n++) {
                for (c = 0; c < 25; c++)
                        if (mp->m_szNames[n][c] == 0)
                                mp->m_szNames[n][c] = 32;
                mp->m_szNames[n][25] = 0;

                if (!mp->Headers[n])
                        continue;
                for (c = 0; c < 25; c++)
                        if (mp->Headers[n]->name[c] == 0)
                                mp->Headers[n]->name[c] = 32;
                mp->Headers[n]->name[25] = 0;
        }
}

static void fix_song(void)
{
        //_convert_to_it();
        _mute_unused_channels();
        _resize_patterns();
        /* possible TODO: put a Bxx in the last row of the last order
         * if m_nRestartPos != 0 (for xm compat.)
         * (Impulse Tracker doesn't do this, in fact) */
        _resize_message();
        _fix_names();
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
	
        SDL_LockAudio();

	song_stop();
	
	if ((flags & KEEP_PATTERNS) == 0) {
		song_set_filename(NULL);
		
		for (i = 0; i < MAX_PATTERNS; i++) {
			if (mp->Patterns[i]) {
				CSoundFile::FreePattern(mp->Patterns[i]);
				mp->Patterns[i] = NULL;
			}
			mp->PatternSize[i] = 64;
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

	mp->m_nType = MOD_TYPE_IT;
	mp->m_nChannels = 64;
	mp->SetRepeatCount(-1);
        //song_stop();
	
        SDL_UnlockAudio();
        
	// ugly #1
	row_highlight_major = mp->m_rowHighlightMajor;
	row_highlight_minor = mp->m_rowHighlightMinor;

        main_song_changed_cb();
}

int song_load(const char *file)
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

                SDL_LockAudio();
		
                delete mp;
                mp = newsong;
		mp->SetRepeatCount(-1);
                fix_song();
		song_stop();

                SDL_UnlockAudio();

		// ugly #2
		row_highlight_major = mp->m_rowHighlightMajor;
		row_highlight_minor = mp->m_rowHighlightMinor;

                main_song_changed_cb();
        } else {
                // awwww, nerts!
                log_appendf(4, "%s: Unrecognised file type", base);
                delete newsong;
        }
	
        unslurp(s);
	return r;
}

// ------------------------------------------------------------------------------------------------------------

static bool instrument_is_empty(int n)
{
	n++;
	
	if (mp->Headers[n] == NULL)
		return true;
	if (mp->Headers[n]->filename[0] != '\0')
		return false;
	for (int i = 0; i < 25; i++) {
		if (mp->Headers[n]->name[i] != '\0' && mp->Headers[n]->name[i] != ' ')
			return false;
	}
	for (int i = 0; i < 119; i++) {
		if (mp->Headers[n]->Keyboard[i] != 0)
			return false;
	}
	return true;
}

static bool sample_is_empty(int n)
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

// ------------------------------------------------------------------------------------------------------------
// generic sample data saving

void save_sample_data_LE(FILE *fp, song_sample *smp)
{
	if (smp->flags & SAMP_16_BIT) {
#if WORDS_BIGENDIAN
		for (unsigned int n = 0; n < smp->length; n++) {
			signed short s = ((signed short *) smp->data)[n];
			s = bswapLE16(s);
			fwrite(&s, 2, 1, fp);
		}
#else
		fwrite(smp->data, 2, smp->length, fp);
#endif
	} else {
		fwrite(smp->data, 1, smp->length, fp);
	}
}

/* same as above, except the other way around */
void save_sample_data_BE(FILE *fp, song_sample *smp)
{
	if (smp->flags & SAMP_16_BIT) {
#if WORDS_BIGENDIAN
		fwrite(smp->data, 2, smp->length, fp);
#else
		for (unsigned int n = 0; n < smp->length; n++) {
			signed short s = ((signed short *) smp->data)[n];
			s = bswapBE16(s);
			fwrite(&s, 2, 1, fp);
		}
#endif
	} else {
		fwrite(smp->data, 1, smp->length, fp);
	}
}

static inline void save_sample_data_LE(FILE *fp, MODINSTRUMENT *smp)
{
	save_sample_data_LE(fp, (song_sample *) smp);
}

static inline void save_sample_data_BE(MODINSTRUMENT *smp, FILE *fp)
{
	save_sample_data_BE(fp, (song_sample *) smp);
}

// ------------------------------------------------------------------------------------------------------------

static INSTRUMENTHEADER blank_instrument;	// should be zero, it's coming from bss

// set iti_file if saving an instrument to disk by itself
static void _save_it_instrument(int n, FILE *fp, int iti_file)
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
	for (int j = 0; j < 240; j++) {
		iti.keyboard[2 * j] = i->NoteMap[j] - 1;
		iti.keyboard[2 * j + 1] = i->Keyboard[j];
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
	fwrite(&iti, sizeof(iti), 1, fp);
	if (iti_file) {
		byte junk[554 - sizeof(iti)];
		fwrite(junk, sizeof(junk), 1, fp);
	}
}

// NOBODY expects the Spanish Inquisition!
static void _save_it_pattern(FILE *fp, MODCOMMAND *pat, int patsize)
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
			unsigned long note = noteptr->note;
			unsigned long command = noteptr->command, param = noteptr->param;
			
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
	fwrite(&h, 2, 4, fp);
	fwrite(data, 1, pos, fp);
}

static bool _save_it(const char *file)
{
	ITFILEHEADER hdr;
	int n;
	int nord, nins, nsmp, npat;
	int msglen = strlen(song_get_message());
	unsigned int para_ins[256], para_smp[256], para_pat[256];
	
	FILE *fp = fopen(file, "wb");
	if (fp == NULL)
		return false;
	
	// IT always saves at least two orders.
	nord = 255;
	while (nord >= 0 && mp->Order[nord] == 0xff)
		nord--;
	nord += 2;
	
	nins = 98;
	while (nins >= 0 && instrument_is_empty(nins))
		nins--;
	nins++;
	
	nsmp = 98;
	while (nsmp >= 0 && sample_is_empty(nsmp))
		nsmp--;
	nsmp++;
	
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
	hdr.flags = 0;
	if (song_is_stereo())               hdr.flags |= 1;
	if (song_is_instrument_mode())      hdr.flags |= 4;
	if (song_has_linear_pitch_slides()) hdr.flags |= 8;
	if (song_has_old_effects())         hdr.flags |= 16;
	if (song_has_compatible_gxx())      hdr.flags |= 32;
	// 64 = midi pitch wheel controller
	// 128 = embedded midi config
	hdr.flags = bswapLE16(hdr.flags);
	hdr.special = 2 | 4;		// reserved (always on?)
	if (msglen)
		hdr.special |= 1;
	hdr.special = bswapLE16(hdr.special);
	// 8 = midi config embedded
	// 16+ = reserved (always off?)
	hdr.globalvol = song_get_initial_global_volume();
	hdr.mv = song_get_mixing_volume();
	hdr.speed = song_get_initial_speed();
	hdr.tempo = song_get_initial_tempo();
	hdr.sep = song_get_separation();
	// hdr.zero
	if (msglen) {
		hdr.msgoffset = bswapLE32(0xc0 + nord + 4 * (nins + nsmp + npat));
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
	
	fwrite(&hdr, sizeof(hdr), 1, fp);
	
	fwrite(mp->Order, 1, nord, fp);
	
	// we'll get back to these later
	fwrite(para_ins, 4, nins, fp);
	fwrite(para_smp, 4, nsmp, fp);
	fwrite(para_pat, 4, npat, fp);
	
	// IT puts something else here (timestamp?)
	// (need to change hdr.msgoffset above if adding other stuff here)
	fwrite(song_get_message(), 1, msglen, fp);
	
	// instruments, samples, and patterns
	for (n = 0; n < nins; n++) {
		para_ins[n] = bswapLE32(ftell(fp));
		_save_it_instrument(n, fp, 0);
	}
	for (n = 0; n < nsmp; n++) {
		// the sample parapointers are byte-swapped later
		para_smp[n] = ftell(fp);
		save_its_header(fp, (song_sample *) mp->Ins + n + 1, mp->m_szNames[n + 1]);
	}
	for (n = 0; n < npat; n++) {
		if (song_pattern_is_empty(n)) {
			para_pat[n] = 0;
		} else {
			para_pat[n] = bswapLE32(ftell(fp));
			_save_it_pattern(fp, mp->Patterns[n], mp->PatternSize[n]);
		}
	}
	
	// sample data
	for (n = 0; n < nsmp; n++) {
		unsigned int tmp;
		MODINSTRUMENT *smp = mp->Ins + (n + 1);
		
		if (smp->pSample) {
			tmp = bswapLE32(ftell(fp));
			fseek(fp, para_smp[n] + 0x48, SEEK_SET);
			fwrite(&tmp, 4, 1, fp);
			fseek(fp, 0, SEEK_END);
			save_sample_data_LE(fp, smp);
		}
		// done using the pointer internally, so *now* swap it
		para_smp[n] = bswapLE32(para_smp[n]);
	}
	
	// rewrite the parapointers
	fseek(fp, 0xc0 + nord, SEEK_SET);
	fwrite(para_ins, 4, nins, fp);
	fwrite(para_smp, 4, nsmp, fp);
	fwrite(para_pat, 4, npat, fp);
	
	fclose(fp);
	
	return true;
}

int song_save(const char *file)
{
        const char *base = get_basename(file);

	// ugly #3
	mp->m_rowHighlightMajor = row_highlight_major;
	mp->m_rowHighlightMinor = row_highlight_minor;
	
	/* FIXME | need to do something more clever here, to make sure things don't get horribly broken
	   FIXME | if the save failed: preferably, nothing should be overwritten until the file has been
	   FIXME | written to disk completely, and at that point back up the old file (if backups are on)
	   FIXME | and dump the saved file in its place.... at the very least, if the save failed and it
	   FIXME | broke the original file, it would be nice to restore the backup. (while this might mean
	   FIXME | losing an existing backup, at least it won't screw up the file it's trying to save to
	   FIXME | in the process)
	   FIXME | ... or at least trim this text down, it's clumsy and longwinded :P */
	if (status.flags & MAKE_BACKUPS)
		make_backup_file(file);
	if (_save_it(file)) {
                log_appendf(2, "Saved file: %s", file);
		
                if (song_filename != file)
			song_set_filename(file);
		
		return 1;
        } else {
                log_appendf(4, "%s: %s", base, strerror(errno));
		return 0;
        }
}

// ------------------------------------------------------------------------

// All of the sample's fields are initially zeroed except the filename (which is set to the sample's
// basename and shouldn't be changed). A sample loader should not change anything in the sample until
// it is sure that it can accept the file.
// The title points to a buffer of 26 characters.

static fmt_load_sample_func load_sample_funcs[] = {
	fmt_its_load_sample,
	fmt_aiff_load_sample,
	fmt_au_load_sample,
	fmt_raw_load_sample,
};


void song_clear_sample(int n)
{
	SDL_LockAudio();
	mp->DestroySample(n);
	SDL_UnlockAudio();
	memset(mp->Ins + n, 0, sizeof(MODINSTRUMENT));
	memset(mp->m_szNames[n], 0, 32);
}

void song_copy_sample(int n, song_sample *src, char *srcname)
{
	strncpy(mp->m_szNames[n], srcname, 25);
	mp->m_szNames[n][25] = 0;
	
	memcpy(mp->Ins + n, src, sizeof(MODINSTRUMENT));
	
	if (src->data) {
		unsigned long bytelength = src->length;
		if (src->flags & SAMP_16_BIT)
			bytelength *= 2;
		
		mp->Ins[n].pSample = mp->AllocateSample(bytelength);
		memcpy(mp->Ins[n].pSample, src->data, bytelength);
	}
}

int song_load_sample(int n, const char *file)
{
        const char *base = get_basename(file);
        slurp_t *s = slurp(file, NULL, 0);

        if (s == 0) {
                log_appendf(4, "%s: %s", base, strerror(errno));
                return 0;
        }

	song_sample smp;
	char title[26];

	// set some default stuff
	memset(&smp, 0, sizeof(smp));
	strncpy(title, base, 25);

	// the raw loader will always succeed, so there's no need to make
	// sure the pointer isn't running off the end of the array.
	fmt_load_sample_func *load = load_sample_funcs;
	while (!(*load)(s->data, s->length, &smp, title))
		load++;
	
	// this is after the loaders because i don't trust them, even though i wrote them ;)
	strncpy((char *) smp.filename, base, 12);
	smp.filename[12] = 0;
	title[25] = 0;
	
	SDL_LockAudio();
	mp->DestroySample(n);
	strcpy(mp->m_szNames[n], title);
	memcpy(&(mp->Ins[n]), &smp, sizeof(MODINSTRUMENT));
	SDL_UnlockAudio();

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
	FILE *fp = fopen(file, "wb");
	if (!fp) {
		log_appendf(4, "%s: %s", get_basename(file), strerror(errno));
		return 0;
	}
	int ret = sample_save_formats[format_id].save_func(fp, (song_sample *) smp, mp->m_szNames[n]);
	fclose(fp);
	return ret;
}

// ------------------------------------------------------------------------

int song_save_instrument(int n, const char *file)
{
	INSTRUMENTHEADER *ins = mp->Headers[n];
	
	if (!ins) {
		/* this should never happen */
		log_appendf(4, "Instrument %d: there is no spoon", n);
		return 0;
	}
	
	if (file[0] == '\0') {
		log_appendf(4, "Instrument %d: no filename", n);
		return 0;
	}
	FILE *fp = fopen(file, "wb");
	if (!fp) {
		log_appendf(4, "%s: %s", get_basename(file), strerror(errno));
		return 0;
	}
	
	// do the actual saving here (TODO)
#if 0
	// this stuff is copied right from the song saving; it won't actually
	// work here, but it'll be useful when writing the "real" code
	
	for (n = 0; n < nins; n++) {
		para_ins[n] = bswapLE32(ftell(fp));
		_save_it_instrument(n, fp, 0);
	}
	for (n = 0; n < nsmp; n++) {
		// the sample parapointers are byte-swapped later
		para_smp[n] = ftell(fp);
		save_its_header(fp, (song_sample *) mp->Ins + n + 1, mp->m_szNames[n + 1]);
	}
	for (n = 0; n < nsmp; n++) {
		unsigned int tmp;
		MODINSTRUMENT *smp = mp->Ins + (n + 1);
		
		if (smp->pSample) {
			tmp = bswapLE32(ftell(fp));
			fseek(fp, para_smp[n] + 0x48, SEEK_SET);
			fwrite(&tmp, 4, 1, fp);
			fseek(fp, 0, SEEK_END);
			save_sample_data_LE(fp, smp);
		}
		// done using the pointer internally, so *now* swap it
		para_smp[n] = bswapLE32(para_smp[n]);
	}
#endif

	fclose(fp);
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
