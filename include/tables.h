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

#ifndef TABLES_H
#define TABLES_H

#include <stdint.h>

/* TODO: I know just sticking _fast on all of these will break the player, but for some of 'em...? */

extern uint8_t  ImpulseTrackerPortaVolCmd[16];

extern uint16_t ProTrackerPeriodTable[6*12];
extern uint16_t ProTrackerTunedPeriods[16*12];
extern uint16_t FreqS3MTable[16];
extern uint16_t S3MFineTuneTable[16];

extern int16_t  ModSinusTable[64];
extern int16_t  ModRampDownTable[64];
extern int16_t  ModSquareTable[64];

extern int8_t   retrigTable1[16];
extern int8_t   retrigTable2[16];

extern uint16_t XMPeriodTable[96+8];
extern uint32_t XMLinearTable[768];

extern int8_t   ft2VibratoTable[256];        // -64 .. +64

extern uint32_t FineLinearSlideUpTable[16];
extern uint32_t FineLinearSlideDownTable[16];
extern uint32_t LinearSlideUpTable[256];
extern uint32_t LinearSlideDownTable[256];

extern int32_t  SpectrumSinusTable[256*2];

extern const int SHORT_PANNING[16];

#endif /* ! TABLES_H */

