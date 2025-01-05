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

#ifndef SCHISM_IEEE_FLOAT_H_
#define SCHISM_IEEE_FLOAT_H_

/* These functions work on the raw, big endian versions of IEEE
 * floating point numbers. If you have little endian input or
 * output, you'll have to byteswap. */

SCHISM_PURE double float_decode_ieee_32(const unsigned char bytes[4]);
void float_encode_ieee_32(double num, unsigned char bytes[4]);

SCHISM_PURE double float_decode_ieee_64(const unsigned char bytes[8]);
void float_encode_ieee_64(double num, unsigned char bytes[8]);

SCHISM_PURE double float_decode_ieee_80(const unsigned char bytes[10]);
void float_encode_ieee_80(double num, unsigned char bytes[10]);

#endif /* SCHISM_IEEE_FLOAT_H_ */
