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

#include "test.h"
#include "test-assertions.h"

#include "util.h"

testresult_t test_mem_xor(void)
{
	unsigned char x[] = {
		0x8B, 0xBE, 0xB4, 0xBA, 0x8B, 0xAD, 0xBE, 0xBC,
		0xB4, 0xBA, 0xAD, 0xBA, 0xBB, 0xFF, 0xA8, 0xB6,
		0xAB, 0xB7, 0xFF, 0xA9, 0xBA, 0xAD, 0xAC, 0xB6,
		0xB0, 0xB1, 0xFF, 0xEF, 0xF1, 0xE6, 0xBA, 0xFE,
		0xFE, 0xFE, 0xFE, 0xFE,
	};

	mem_xor(x, sizeof(x), 0xDF);

	if (memcmp(x, "TakeTrackered with version 0.9e!!!!!", sizeof(x)))
		RETURN_FAIL;

	RETURN_PASS;
}