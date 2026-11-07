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

// RM: this was tested on the tracks distributed with FunkTracker GOLD v1.2,
// alongside the source code of some of JsNO's demos which include FNKs.

#include "headers.h"
#include "bits.h"
#include "slurp.h"
#include "fmt.h"
#include "str.h"
#include "mem.h"

#include "player/sndfile.h"

struct sheader_fnk {
	uint8_t name[19];
	uint32_t loop_point;
	uint32_t length;
	uint8_t volume;
	uint8_t panning;
	uint8_t portamento_type_sample_offset_shift;
	uint8_t vibrato_waveform_tremolo_waveform;
	uint8_t retrigger_arp_speed;
};

struct header_fnk {
	uint8_t preamble[4];
	uint8_t packed_info[4];
	uint32_t file_size;
	uint8_t file_type[4];
	uint8_t restartpos;
	uint8_t orders[256];
	uint8_t breaks[128];
};

enum {
	FNK_NOTE_SYSTEM_ACTIVE = 1 << 0,
	FNK_VOLUME_SYSTEM_ACTIVE = 1 << 1,
};

static int read_sample_header_fnk(struct sheader_fnk *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) return LOAD_FILE_ERROR

	READ_VALUE(name);
	READ_VALUE(loop_point);
	hdr->loop_point = bswapLE32(hdr->loop_point);
	READ_VALUE(length);
	hdr->length = bswapLE32(hdr->length);
	READ_VALUE(volume);
	READ_VALUE(panning);
	READ_VALUE(portamento_type_sample_offset_shift);
	READ_VALUE(vibrato_waveform_tremolo_waveform);
	READ_VALUE(retrigger_arp_speed);

#undef READ_VALUE
	return LOAD_SUCCESS;
}

static int read_header_fnk(struct header_fnk *hdr, slurp_t *fp)
{
#define READ_VALUE(name) \
	if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) return LOAD_FILE_ERROR

	READ_VALUE(preamble);
	if (memcmp(hdr->preamble, "Funk", 4) != 0) return LOAD_UNSUPPORTED;
	READ_VALUE(packed_info);
	READ_VALUE(file_size);
	hdr->file_size = bswapLE32(hdr->file_size);
	if (slurp_length(fp) != hdr->file_size) return LOAD_UNSUPPORTED;
	READ_VALUE(file_type);
	READ_VALUE(restartpos);
	if (hdr->restartpos != 0xff && hdr->restartpos > 0x7f) return 0;
	READ_VALUE(orders);
	READ_VALUE(breaks);
	for (int i = 0; i < sizeof(hdr->breaks); i++)
		if (hdr->breaks[i] > 0x3f)
			return LOAD_UNSUPPORTED;
#undef READ_VALUE
	return LOAD_SUCCESS;
}

static int fnk_card_allows_panning(const uint8_t card)
{
	switch (card)
	{
		// SoundBlaster 2.0
		case 0x0: return 0;
		// SoundBlaster Pro
		case 0x1: return 0;
		// Gravis UltraSound
		case 0x2: return 1;
		// SoundBlaster compatible
		case 0x3: return 0;
		// SoundBlaster 16
		case 0x4: return 0;
		// Gravis UltraSound (Unis 669 panning)
		case 0x5: return 0;
		// Ripped/converted
		case 0x6: return 0;
		// Pro Audio Spectrum
		case 0x7: return 0;
		// Voxkit 8-bit
		case 0x8: return 1;
		// Voxkit 16-bit
		case 0x9: return 1;
	}
	// RM: no other values are known to exist,
	// we'll just say it supports panning for our own sanity. ;)
	return 1;
}

static inline float fnk_pitch_slide_as_float(const uint8_t prm)
{
	// lifted from FUNKLITE.ASM, specifically for logarithmic
	const uint8_t amount[16] = {1,4,6,10,12,16,20,24,31,47,63,83,107,139,187,251};
	const float ticks = 1 + (prm >> 4);
	return (amount[prm & 0xF] / ticks);
}

static inline float fnk_volume_slide_as_float(const uint8_t prm)
{
	// lifted from FUNKLITE.ASM
	const uint8_t amount[16] = {1,2,3,4,5,6,7,8,10,12,16,20,24,32,48,64};
	const float ticks = 1 + (prm >> 4);
	return (amount[prm & 0xF] / ticks);
}

int fmt_fnk_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct header_fnk hdr;
	if (read_header_fnk(&hdr, fp) != LOAD_SUCCESS)
		return 0;

	const char *desc;
	if (hdr.file_type[0] == 'F')
		if (hdr.file_type[1] == '2')
			desc = "FunkTracker (R2) Module";
		else
			desc = "FunkTracker (R1, v1.05+) Module";
	else
		desc = "FunkTracker (R1, pre-v1.05) Module";


	file->description = desc;
	/*file->extension = str_dup("fnk");*/
	file->type = TYPE_MODULE_S3M;

	return 1;
}

int fmt_fnk_load_song(song_t *song, slurp_t *fp, uint32_t lflags)
{
	int nchn = 8, npat, read_result, gold = 0, allow_pan = 1, smp_16b = 0;
	int total_samples_using_linear = 0, total_samples_using_log = 0;
	struct header_fnk hdr;
	struct sheader_fnk fnk_sample[64];
	const int waveform_conversion[5] = {
		// sine
		VIB_SINE,
		// triangle
		VIB_SINE,
		// square
		VIB_SQUARE,
		// sawtooth
		VIB_RAMP_DOWN,
		// random
		VIB_RANDOM
	};

	read_result = read_header_fnk(&hdr, fp);
	if (read_result != LOAD_SUCCESS)
		return read_result;

	const char *desc;
	if (hdr.file_type[0] == 'F') {
		nchn = ((hdr.file_type[2] - '0') * 10) + (hdr.file_type[3] - '0');
		if (nchn > MAX_CHANNELS)
			return LOAD_UNSUPPORTED;

		if (hdr.file_type[1] == '2') {
			gold = 1;
			smp_16b = hdr.packed_info[3] & 1;
			strcpy(song->tracker_id, "FunkTracker (R2) Module");
		} else if (hdr.file_type[1] == 'k' || hdr.file_type[1] == 'v') {
			char panning_setup[32] = {0};
			allow_pan = fnk_card_allows_panning(hdr.packed_info[2] & 0xF);
			if (hdr.file_type[1] == 'k')
				allow_pan = 0;
			strcpy(song->tracker_id, "FunkTracker (R1, v1.05+) Module");
			sprintf(panning_setup, " (%s panning)",allow_pan ? "fixed" : "non-fixed");
			strcat(song->tracker_id, panning_setup);
		}
	} else {
		strcpy(song->tracker_id, "FunkTracker (R1, pre-v1.05) Module");
		allow_pan = 0;
	}

	for (int n = 0; n < sizeof(hdr.orders); n++) {
		if (hdr.orders[n] == 0xff) {
			continue;
		}
		if (hdr.orders[n] > 128)
			return LOAD_UNSUPPORTED;

		if (hdr.orders[n] > npat)
			npat = hdr.orders[n];
	}
	npat++;

	memcpy(song->orderlist, hdr.orders, sizeof(hdr.orders));

	for (int smp = 1; smp <= 64; smp++) {
		song_sample_t *sample = song->samples + smp;
		struct sheader_fnk* fnk_smp = &fnk_sample[smp - 1];
		read_sample_header_fnk(fnk_smp, fp);
		strncpy(sample->name, fnk_smp->name, 19);
		sample->c5speed = 8363;
		sample->length = fnk_smp->length;
		sample->loop_end = fnk_smp->length;
		if (fnk_smp->loop_point != 0xFFFFFFFF && fnk_smp->loop_point <= fnk_smp->length)
		{
			sample->flags |= CHN_LOOP;
			sample->loop_start = fnk_smp->loop_point;
		}
		sample->panning = fnk_smp->panning;
		sample->volume = fnk_smp->volume; // lucky that it's on the same scale!
		if (allow_pan)
			sample->flags |= CHN_PANNING;
		if (fnk_smp->vibrato_waveform_tremolo_waveform <= 5)
			sample->vib_type = waveform_conversion[fnk_smp->vibrato_waveform_tremolo_waveform >> 4];
	}

	for (int n = 0; n < 64; n++) {
		if (fnk_sample[n].length == 0)
			continue;
		const uint8_t port_type = fnk_sample[n].portamento_type_sample_offset_shift >> 4;
		total_samples_using_linear += port_type == 1;
		total_samples_using_log += port_type == 0;
	}

	if (total_samples_using_linear > total_samples_using_log)
		song->flags |= SONG_LINEARSLIDES;

	if (lflags & LOAD_NOPATTERNS)
		slurp_seek(fp, npat * 64 * nchn * 3, SEEK_CUR);
	else
		for (int pat = 0; pat < npat; pat++) {
			int row, chn;
			// RM: i imagine that there's probably a way easier and cleaner way to
			// emulate the multi-system paradigm, but eh...
			uint8_t cached_sample[MAX_CHANNELS] = {0}, cached_note[MAX_CHANNELS] = {0},
			active_system[MAX_CHANNELS] = {0}, cached_pitch_effect[MAX_CHANNELS] = {0}, cached_volume_effect[MAX_CHANNELS] = {0},
			cached_pitch_parameter[MAX_CHANNELS] = {0}, cached_volume_parameter[MAX_CHANNELS] = {0};
			const int rows = hdr.breaks[pat] + 1;
			if (rows > 64)
				return LOAD_UNSUPPORTED;

			song->patterns[pat] = csf_allocate_pattern(CLAMP(rows, 32, 64));
			song->pattern_size[pat] = song->pattern_alloc_size[pat] = CLAMP(rows, 32, 64);

			for(row = 0; row < rows; row++) {
				song_note_t *note = song->patterns[pat] + (row * 64);
				for (chn = 0; chn < nchn; chn++, note++)
				{
					uint8_t b[3], e, n, s;
					int note_effect[MAX_CHANNELS] = {0}, volume_effect[MAX_CHANNELS] = {0};
					slurp_read(fp, b, 3);
					n = b[0] >> 2;
					s = ((b[0] & 3) << 4) | (b[1] >> 4);
					if (fnk_sample[s].length != 0)
						cached_sample[chn] = s;
					if (n < 0x3D) {
						active_system[chn] = 0;
						cached_note[chn] = n + 36 + 1;
						note->note = cached_note[chn];
						note->instrument = cached_sample[chn] + 1;
					} else if (n == 0x3D) {
						note->instrument = cached_sample[chn] + 1;
					} else if (n == 0x3E) {
						note->note = cached_note[chn];
					} else {
						note->note = NOTE_NONE;
						note->instrument = 0;
					}

					e = (b[1] & 0xf) + 'a';
					if (e >= 'a' && e <= 'f' || e == 'l') {
						active_system[chn] |= FNK_NOTE_SYSTEM_ACTIVE;
						cached_pitch_effect[chn] = e;
						cached_pitch_parameter[chn] = b[2];
					} else if (e >= 'g' && e <= 'j' || e == 'k') {
						active_system[chn] |= FNK_VOLUME_SYSTEM_ACTIVE;
						cached_volume_effect[chn] = e;
						cached_volume_parameter[chn] = b[2];
					} else if (e == 'o' && (b[2] >> 4) == 0) {
						switch (b[2] & 0xF) {
							case 0xA:
								active_system[chn] &= ~FNK_NOTE_SYSTEM_ACTIVE;
								break;
							case 0xB:
								active_system[chn] &= ~FNK_VOLUME_SYSTEM_ACTIVE;
								break;
							case 0xC:
								active_system[chn] = 0;
								break;
						}
					}

					if ((active_system[chn] & FNK_NOTE_SYSTEM_ACTIVE) == 0)
						cached_pitch_effect[chn] = 0;
					if ((active_system[chn] & FNK_VOLUME_SYSTEM_ACTIVE) == 0)
						cached_volume_effect[chn] = 0;

	#define NOTE_EFFECT(x,n) \
		if (note->effect == FX_NONE) {note->effect = x; note->param = n; note_effect[chn] = 1;}
	#define VOL_EFFECT(x,n) \
		if (note->voleffect == FX_NONE) {note->voleffect = x; note->volparam = n; volume_effect[chn] = 1;}

					switch(cached_pitch_effect[chn]) {
						case 'a':
							if(gold) {
								NOTE_EFFECT(FX_PORTAMENTOUP, cached_pitch_parameter[chn]);
							} else {
								const float approx = fnk_pitch_slide_as_float(cached_pitch_parameter[chn]);
								if (approx < 1.f) {
									NOTE_EFFECT(FX_PORTAMENTOUP, CLAMP((int)(approx * 15.f), 0x1, 0xF) | 0xF0);
								} else {
									NOTE_EFFECT(FX_PORTAMENTOUP, CLAMP((int)approx, 0x01, 0xDF));
								}
							}
							break;
						case 'b':
							if(gold) {
								NOTE_EFFECT(FX_PORTAMENTODOWN, cached_pitch_parameter[chn]);
							} else {
								const float approx = fnk_pitch_slide_as_float(cached_pitch_parameter[chn]);
								if (approx < 1.f) {
									NOTE_EFFECT(FX_PORTAMENTODOWN, CLAMP((int)(approx * 15.f), 0x1, 0xF) | 0xF0);
								} else {
									NOTE_EFFECT(FX_PORTAMENTODOWN, CLAMP((int)approx, 0x01, 0xDF));
								}
							}
							break;
						case 'c':
							// RM: the sample actually **restarts**, which we can't do...
							if(gold) {
								NOTE_EFFECT(FX_TONEPORTAMENTO, cached_pitch_parameter[chn]);
							} else {
								NOTE_EFFECT(FX_TONEPORTAMENTO, (int)fnk_pitch_slide_as_float(cached_pitch_parameter[chn]));
							}
							break;
						case 'd':
							// RM: speed is backwards from what you expect
							NOTE_EFFECT(FX_VIBRATO, ((~(cached_pitch_parameter[chn] >> 4)) << 4) | (cached_pitch_parameter[chn] & 0x0F));
							break;
						case 'l':
							NOTE_EFFECT(FX_ARPEGGIO, cached_pitch_parameter[chn]);
							break;
					}

					switch(cached_volume_effect[chn]) {
						case 'g': {
							const float approx = fnk_volume_slide_as_float(cached_volume_parameter[chn]);
							if (!note_effect[chn]) {
								if (approx < 1.f) {
									NOTE_EFFECT(FX_VOLUMESLIDE, (CLAMP((int)(approx * 15.f), 0x1, 0xF) << 4) | 0x0F);
								} else {
									NOTE_EFFECT(FX_VOLUMESLIDE, CLAMP((int)(approx * 15.f / 64.f), 0x1, 0xF) << 4);
								}
							} else {
								if (approx < 1.f) {
									VOL_EFFECT(VOLFX_FINEVOLUP, CLAMP((int)(approx * 15.f), 0x1, 0x9));
								} else {
									VOL_EFFECT(VOLFX_VOLSLIDEUP, CLAMP((int)(approx * 15.f / 64.f), 0x1, 0x9));
								}
							}
						}
							break;
						case 'h': {
							const float approx = fnk_volume_slide_as_float(cached_volume_parameter[chn]);
							if (!note_effect[chn]) {
								if (approx < 1.f) {
									NOTE_EFFECT(FX_VOLUMESLIDE, CLAMP((int)(approx * 15.f), 0x1, 0xF) | 0xF0);
								} else {
									NOTE_EFFECT(FX_VOLUMESLIDE, CLAMP((int)(approx * 15.f / 64.f), 0x1, 0xF));
								}

							} else {
								if (approx < 1.f) {
									VOL_EFFECT(VOLFX_FINEVOLDOWN, CLAMP((int)(approx * 15.f), 0x1, 0x9));
								} else {
									VOL_EFFECT(VOLFX_VOLSLIDEDOWN, CLAMP((int)(approx * 15.f / 64.f), 0x1, 0x9));
								}
							}
						}
							break;
						case 'k':
							if(!note_effect[chn])
								NOTE_EFFECT(FX_VOLUMESLIDE, ((~(cached_volume_parameter[chn] >> 4)) << 4) | (cached_volume_parameter[chn] & 0x0F));
							break;
					}

					switch (e)
					{
						case 'm':
							note->effect = FX_OFFSET;
							note->param = (b[2] << fnk_sample[cached_sample[chn]].portamento_type_sample_offset_shift) >> 8;
							break;
						case 'n':
							note->voleffect = VOLFX_VOLUME;
							note->volparam = b[2] * 64 / 255;
							break;
						case 'o': {
							const uint8_t prm = b[2] & 0xF;
							switch (b[2] >> 4)
							{
								case 0x0:
									switch (b[2] & 0xF)
									{
										case 0x0:
										case 0x1:
										case 0x2:
										case 0x3:
										case 0x4:
											note->effect = FX_SPECIAL;
											note->param = 0x30 | waveform_conversion[prm];
											break;
										case 0x5:
										case 0x6:
										case 0x7:
										case 0x8:
										case 0x9:
											note->effect = FX_SPECIAL;
											note->param = 0x40 | waveform_conversion[prm - 5];
											break;
										case 0xE:
											note->effect = FX_PANNINGSLIDE;
											note->param = 0x4F;
											break;
										case 0xF:
											note->effect = FX_PANNINGSLIDE;
											note->param = 0xF4;
											break;
									}
									break;
								case 0x1:
									note->effect = FX_SPECIAL;
									note->param = 0xC0 | prm;
									break;
								case 0x2:
									if (gold) {
										note->effect = FX_SPECIAL;
										note->param = 0xD0 | prm;
									} else {
										note->effect = FX_PORTAMENTOUP;
										note->param = 0xF0 | prm;
									}
									break;
								case 0x4:
									note->effect = FX_PORTAMENTOUP;
									note->param = 0xF0 | prm;
									break;
								case 0x5:
									note->effect = FX_PORTAMENTODOWN;
									note->param = 0xF0 | prm;
									break;
								case 0x6:
									note->effect = FX_VOLUMESLIDE;
									note->param = (prm << 4) | 0x0F;
									break;
								case 0x7:
									note->effect = FX_VOLUMESLIDE;
									note->param = 0xF0 | prm;
									break;
								case 0xA:
									if (!gold) {
										note->effect = FX_GLOBALVOLUME;
										note->param = prm * 64 / 15;
									} else {
										note->effect = FX_GLOBALVOLSLIDE;
										if (prm & 8)
											note->param = (((prm & 7) << 1) << 4) | 0x0F;
										else
											note->param = 0xF0 | ((prm & 7) << 1);
									}
									break;
								case 0xD:
									note->effect = FX_RETRIG;
									note->param = prm;
									break;
								case 0xE:
									note->effect = FX_SPECIAL;
									note->param = 0x80 | prm;
									break;
								case 0xF:
									note->effect = FX_SPEED;
									note->param = prm + 1;
									break;
							}
							break;
						}
						case 0xf + 'a': break;
						default: break;
					}
				}
			}

#undef VOL_EFFECT
#undef NOTE_EFFECT
			if (row < 64) {
				/* skip the rest of the rows beyond the break position */
				slurp_seek(fp, 3 * nchn * (64 - row), SEEK_CUR);
			}
			/* handle the break position */
			if (row < 32) {
				//printf("adding pattern break for pattern %d\n", pat);
				song_note_t *note = song->patterns[pat] + MAX_CHANNELS * (row - 1);
				for (int chan = 0; chan < nchn; chan++, note++) {
					if (!note->effect) {
						note->effect = FX_PATTERNBREAK;
						note->param = 0;
						break;
					}
				}
			}
		}

	csf_insert_restart_pos(song, hdr.restartpos);

	/* sample data */
	if (!(lflags & LOAD_NOSAMPLES)) {
		for (int smp = 1; smp <= 64; smp++) {
			if (song->samples[smp].length == 0)
				continue;

			csf_read_sample(song->samples + smp, SF_LE | SF_M | SF_PCMS | (smp_16b ? SF_16 : SF_8), fp);
		}
	}

	/* set the rest of the stuff */
	song->initial_speed = 5;
	if (!gold) {
		song->initial_tempo = 125;
		if (!allow_pan) {
			for (int n = 0; n < nchn; n++)
				song->channels[n].panning = (n & 1) ? 256 : 0; //mphack
			song->pan_separation = 64;
		}
	} else {
		const uint8_t tmp = hdr.packed_info[3] >> 1;
		uint8_t tempo = 125;
		if (tmp & 64) tempo -= tmp & 63;
		else tempo += tmp & 63;
		song->initial_tempo = tempo;
	}
	song->flags = SONG_ITOLDEFFECTS;

	song->pan_separation = 128;
	for (int n = nchn; n < 64; n++)
		song->channels[n].flags = CHN_MUTE;

//      if (ferror(fp)) {
//              return LOAD_FILE_ERROR;
//      }

	/* done! */
	return LOAD_SUCCESS;
}