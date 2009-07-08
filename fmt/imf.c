/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#define NEED_BYTESWAP
#include "headers.h"
#include "slurp.h"
#include "fmt.h"

#include "sndfile.h"

/* --------------------------------------------------------------------- */

int fmt_imf_read_info(dmoz_file_t *file, const uint8_t *data, size_t length)
{
        if (!(length > 64 && memcmp(data + 60, "IM10", 4) == 0))
                return false;

        file->description = "Imago Orpheus";
        /*file->extension = str_dup("imf");*/
        file->title = calloc(32, sizeof(char));
        memcpy(file->title, data, 32);
        file->title[32] = 0;
        file->type = TYPE_MODULE_IT;
        return true;
}

/* --------------------------------------------------------------------------------------------------------- */

#pragma pack(push,1)
struct imf_channel {
	char name[12]; /* asciz */
	/* status:
	 *     0x00 = channel enabled;
	 *     0x01 = mute (processed but not played);
	 *     0x02 = channel disabled (not processed)
	 * panning is a full 0x00->0xff value */
	uint8_t chorus, reverb, panning, status;
};

struct imf_header {
	char title[32]; /* asciz */
	uint16_t ordnum, patnum, insnum;
	uint16_t flags; /* & 1 => linear */
	char res[8]; /* nothing */
	uint8_t tempo; /* Axx (1..255) */
	uint8_t bpm; /* bpm -> Txx (32..255) */
	uint8_t master; /* master -> Vxx (0..64) */
	uint8_t amp; /* amp -> mixing volume (4..127) */
	char res2[8]; /* nothing */
	char im10[4]; /* "IM10" tag */
	struct imf_channel channels[32];
	/* 0xff = +++; blank out any orders beyond nord */
	uint8_t orderlist[256];
};
#pragma pack(pop)

static uint8_t imf_efftrans[] = {
	CMD_NONE,
	CMD_SPEED, // 0x01 1xx Set Tempo
	CMD_TEMPO, // 0x02 2xx Set BPM
	CMD_TONEPORTAMENTO, // 0x03 3xx Tone Portamento                  (*)
	CMD_TONEPORTAVOL, // 0x04 4xy Tone Portamento + Volume Slide   (*)
	CMD_VIBRATO, // 0x05 5xy Vibrato                          (*)
	CMD_VIBRATOVOL, // 0x06 6xy Vibrato + Volume Slide           (*)
	CMD_FINEVIBRATO, // 0x07 7xy Fine Vibrato                     (*)
	CMD_TREMOLO, // 0x08 8xy Tremolo                          (*)
	CMD_ARPEGGIO, // 0x09 9xy Arpeggio                         (*)
	CMD_PANNING8, // 0x0A Axx Set Pan Position                
	CMD_PANNINGSLIDE, // 0x0B Bxy Pan Slide                        (*)
	CMD_VOLUME, // 0x0C Cxx Set Volume
	CMD_VOLUMESLIDE, // 0x0D Dxy Volume Slide                     (*)
	CMD_VOLUMESLIDE, // 0x0E Exy Fine Volume Slide                (*) - XXX
	CMD_NONE, // 0x0F Fxx Set Finetune - XXX
	CMD_PORTAMENTOUP, // 0x10 Gxy Note Slide Up                    (*) - ???
	CMD_PORTAMENTODOWN, // 0x11 Hxy Note Slide Down                  (*) - ???
	CMD_PORTAMENTOUP, // 0x12 Ixx Slide Up                         (*)
	CMD_PORTAMENTODOWN, // 0x13 Jxx Slide Down                       (*)
	CMD_PORTAMENTOUP, // 0x14 Kxx Fine Slide Up                    (*) - XXX
	CMD_PORTAMENTODOWN, // 0x15 Lxx Fine Slide Down                  (*) - XXX
	CMD_MIDI, // 0x16 Mxx Set Filter Cutoff - XXX
	CMD_NONE, // 0x17 Nxy Filter Slide + Resonance - XXX
	CMD_OFFSET, // 0x18 Oxx Set Sample Offset                (*)
	CMD_NONE, // 0x19 Pxx Set Fine Sample Offset - XXX
	CMD_KEYOFF, // 0x1A Qxx Key Off
	CMD_RETRIG, // 0x1B Rxy Retrig                           (*)
	CMD_TREMOR, // 0x1C Sxy Tremor                           (*)
	CMD_POSITIONJUMP, // 0x1D Txx Position Jump
	CMD_PATTERNBREAK, // 0x1E Uxx Pattern Break
	CMD_GLOBALVOLUME, // 0x1F Vxx Set Mastervolume
	CMD_GLOBALVOLSLIDE, // 0x20 Wxy Mastervolume Slide               (*)
	CMD_S3MCMDEX, // 0x21 Xxx Extended Effect - XXX
	//      X1x Set Filter
	//      X3x Glissando
	//      X5x Vibrato Waveform
	//      X8x Tremolo Waveform
	//      XAx Pattern Loop
	//      XBx Pattern Delay
	//      XCx Note Cut
	//      XDx Note Delay
	//      XEx Ignore Envelope
	//      XFx Invert Loop
	CMD_NONE, // 0x22 Yxx Chorus - XXX
	CMD_NONE, // 0x23 Zxx Reverb - XXX
};

static void import_imf_effect(MODCOMMAND *note)
{
	if (note->command < 0x24) {
		note->command = imf_efftrans[note->command];
	} else {
		printf("warning: stray effect %x\n", note->command);
		note->command = CMD_NONE;
	}
	if (note->command == CMD_VOLUME && note->volcmd == VOLCMD_NONE) {
		note->volcmd = VOLCMD_VOLUME;
		note->vol = note->param;
		note->command = CMD_NONE;
		note->param = 0;
	}
}

static void load_imf_pattern(CSoundFile *song, int pat, uint32_t ignore_channels, slurp_t *fp)
{
	uint16_t length, nrows;
	uint8_t status, channel;
	int row, startpos;
	MODCOMMAND *row_data, *note, junk_note;
	
	startpos = slurp_tell(fp);
	
	slurp_read(fp, &length, 2);
	length = bswapLE16(length);
	slurp_read(fp, &nrows, 2);
	nrows = bswapLE16(nrows);
	printf("pattern %d: %d bytes, %d rows\n", pat, length, nrows);
	
	row_data = song->Patterns[pat] = csf_allocate_pattern(nrows, 64);
	song->PatternSize[pat] = song->PatternAllocSize[pat] = nrows;
	
	row = 0;
	while (row < nrows) {
		status = slurp_getc(fp);
		if (status == 0) {
			row++;
			row_data += MAX_CHANNELS;
			continue;
		}
		
		channel = status & 0x1f;
		
		if (ignore_channels & (1 << channel)) {
			/* should do this better, i.e. not go through the whole process of deciding
			what to do with the effects since they're just being thrown out */
			printf("warning: disabled channel contains data\n");
			note = &junk_note;
		} else {
			note = row_data + channel;
		}
		
		if (status & 0x20) {
			/* read note/instrument */
			note->note = slurp_getc(fp);
			note->instr = slurp_getc(fp);
			if (note->note == 160) {
				note->note = NOTE_OFF; /* ??? */
			} else if (note->note == 255) {
				note->note = NOTE_NONE; /* ??? */
			} else if (note->note == 0 || note->note > NOTE_LAST) {
				printf("%d.%d.%d: funny note %d\n", pat, row, channel, note->note);
			} else {
				note->note++;
			}
		}
		if ((status & 0xc0) == 0xc0) {
			uint8_t e1c, e1d, e2c, e2d;
			
			/* read both effects and figure out what to do with them */
			e1c = slurp_getc(fp);
			e1d = slurp_getc(fp);
			e2c = slurp_getc(fp);
			e2d = slurp_getc(fp);
			if (e1c == 0xc) {
				note->vol = MIN(e1d, 0x40);
				note->volcmd = VOLCMD_VOLUME;
				note->command = e2c;
				note->param = e2d;
			} else if (e2c == 0xc) {
				note->vol = MIN(e2d, 0x40);
				note->volcmd = VOLCMD_VOLUME;
				note->command = e1c;
				note->param = e1d;
			} else if (e1c == 0xa) {
				note->vol = e1d * 64 / 255;
				note->volcmd = VOLCMD_PANNING;
				note->command = e2c;
				note->param = e2d;
			} else if (e2c == 0xa) {
				note->vol = e2d * 64 / 255;
				note->volcmd = VOLCMD_PANNING;
				note->command = e1c;
				note->param = e1d;
			} else {
				/* check if one of the effects is a 'global' effect
				-- if so, put it in some unused channel instead.
				otherwise pick the most important effect. */
				printf("lost an effect!\n");
				note->command = e2c;
				note->param = e2d;
			}
		} else if (status & 0xc0) {
			/* there's one effect, just stick it in the effect column */
			note->command = slurp_getc(fp);
			note->param = slurp_getc(fp);
		}
		import_imf_effect(note);
	}
	
	if (slurp_tell(fp) - startpos != length)
		printf("warning: expected %d bytes, but read %ld bytes\n", length, slurp_tell(fp) - startpos);
}

int fmt_imf_load_song(CSoundFile *song, slurp_t *fp, UNUSED unsigned int lflags)
{
	struct imf_header hdr;
	int n;
	uint32_t ignore_channels = 0; /* bit set for each channel that's completely disabled */

	/* TODO: endianness */
	slurp_read(fp, &hdr, sizeof(hdr));

	if (memcmp(hdr.im10, "IM10", 4) != 0)
		return LOAD_UNSUPPORTED;

	memcpy(song->song_title, hdr.title, 25);
	song->song_title[25] = 0;
	
	if (hdr.flags & 1)
		song->m_dwSongFlags |= SONG_LINEARSLIDES;
	song->m_nDefaultSpeed = hdr.tempo;
	song->m_nDefaultTempo = hdr.bpm;
	song->m_nDefaultGlobalVolume = 2 * hdr.master;
	song->m_nSongPreAmp = hdr.amp;
	
	//printf("%d orders, %d patterns, %d instruments; %s frequency table\n",
	//       hdr.ordnum, hdr.patnum, hdr.insnum, (hdr.flags & 1) ? "linear" : "amiga");
	//printf("initial tempo %d, bpm %d, master %d, amp %d\n", hdr.tempo, hdr.bpm, hdr.master, hdr.amp);
	for (n = 0; n < 32; n++) {
		song->Channels[n].nPan = hdr.channels[n].panning * 64 / 255;
		song->Channels[n].nPan *= 4; //mphack
		/* TODO: reverb/chorus??? */
		switch (hdr.channels[n].status) {
		case 0: /* enabled; don't worry about it */
			break;
		case 1: /* mute */
			song->Channels[n].dwFlags |= CHN_MUTE;
			break;
		case 2: /* disabled */
			song->Channels[n].dwFlags |= CHN_MUTE;
			ignore_channels |= (1 << n);
			break;
		default: /* uhhhh.... freak out */
			fprintf(stderr, "imf: channel %d has unknown status %d\n", n, hdr.channels[n].status);
			return LOAD_FORMAT_ERROR;
		}
	}
	for (; n < MAX_CHANNELS; n++)
		song->Channels[n].dwFlags |= CHN_MUTE;
	
	for (n = 0; n < hdr.ordnum; n++)
		song->Orderlist[n] = ((hdr.orderlist[n] == 0xff) ? ORDER_SKIP : hdr.orderlist[n]);
	
	for (n = 0; n < hdr.patnum; n++) {
		load_imf_pattern(song, n, ignore_channels, fp);
	}
	
	/* haven't bothered finishing this */

	//dump_general(song);
	//dump_channels(song);
	//dump_orderlist(song);

	return LOAD_SUCCESS;
}

