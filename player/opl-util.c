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
#include "bshift.h"

/**
 * @file   opl-util.cpp
 * @brief  Utility functions related to OPL chips.
 *
 * Copyright (C) 2010-2013 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

//Stripped down version for Schismtracker, in C.

// this really should be in a header but it's only used in one other file
int32_t fnumToMilliHertz(uint32_t fnum, uint32_t block,
		uint32_t conversionFactor);
void milliHertzToFnum(uint32_t milliHertz,
		uint32_t *fnum, uint32_t *block, uint32_t conversionFactor);


/// Convert the given f-number and block into a note frequency.
/**
* @param fnum
* Input frequency number, between 0 and 1023 inclusive. Values outside this
* range will cause assertion failures.
*
* @param block
* Input block number, between 0 and 7 inclusive. Values outside this range
* will cause assertion failures.
*
* @param conversionFactor
* Conversion factor to use. Normally will be 49716 and occasionally 50000.
*
* @return The converted frequency in milliHertz.
*/
int32_t fnumToMilliHertz(uint32_t fnum, uint32_t block, uint32_t conversionFactor)
{
	// Original formula
	//return 1000 * conversionFactor * (double)fnum * pow(2, (double)((signed)block - 20));

	// More efficient version
	return (1000ull * conversionFactor * fnum) >> (20 - block);
}
/// Convert a frequency into an OPL f-number
/**
* @param milliHertz
* Input frequency.
*
* @param fnum
* Output frequency number for OPL chip. This is a 10-bit number, so it will
* always be between 0 and 1023 inclusive.
*
* @param block
* Output block number for OPL chip. This is a 3-bit number, so it will
* always be between 0 and 7 inclusive.
*
* @param conversionFactor
* Conversion factor to use. Normally will be 49716 and occasionally 50000.
*
* @post fnum will be set to a value between 0 and 1023 inclusive. block will
* be set to a value between 0 and 7 inclusive. assert() calls inside this
* function ensure this will always be the case.
*
* @note As the block value increases, the frequency difference between two
* adjacent fnum values increases. This means the higher the frequency,
* the less precision is available to represent it. Therefore, converting
* a value to fnum/block and back to milliHertz is not guaranteed to reproduce
* the original value.
*/
void milliHertzToFnum(uint32_t milliHertz, uint32_t *fnum, uint32_t *block, uint32_t conversionFactor)
{
	// Special case to avoid divide by zero
	if (milliHertz <= 0) {
		*block = 0; // actually any block will work
		*fnum = 0;
		return;
	}

	// Special case for frequencies too high to produce
	if (milliHertz > 6208431) {
		*block = 7;
		*fnum = 1023;
		return;
	}

	/// This formula will provide a pretty good estimate as to the best block to
	/// use for a given frequency.  It tries to use the lowest possible block
	/// number that is capable of representing the given frequency.  This is
	/// because as the block number increases, the precision decreases (i.e. there
	/// are larger steps between adjacent note frequencies.)  The 6M constant is
	/// the largest frequency (in milliHertz) that can be represented by the
	/// block/fnum system.
	//int invertedBlock = log2(6208431 / milliHertz);

	// Very low frequencies will produce very high inverted block numbers, but
	// as they can all be covered by inverted block 7 (block 0) we can just clip
	// the value.
	//if (invertedBlock > 7) invertedBlock = 7;
	//*block = 7 - invertedBlock;

	// This is a bit more efficient and doesn't need log2() from math.h
	if (milliHertz > 3104215) *block = 7;
	else if (milliHertz > 1552107) *block = 6;
	else if (milliHertz > 776053) *block = 5;
	else if (milliHertz > 388026) *block = 4;
	else if (milliHertz > 194013) *block = 3;
	else if (milliHertz > 97006) *block = 2;
	else if (milliHertz > 48503) *block = 1;
	else *block = 0;

	// Original formula
	//*fnum = milliHertz * pow(2, 20 - *block) / 1000 / conversionFactor + 0.5;

	// Slightly more efficient version
	*fnum = ((uint64_t)milliHertz << (20 - *block)) / (conversionFactor * 1000.0) + 0.5;

	if (*fnum > 1023) {
		(*block)++;
		*fnum = ((uint64_t)milliHertz << (20 - *block)) / (conversionFactor * 1000.0) + 0.5;
	}

	return;
}
