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

#include "bshift.h"

#ifndef HAVE_SANE_SIGNED_LSHIFT
SCHISM_CONST extern inline int8_t   schism_signed_lshift_8_(int8_t x, unsigned int y);
SCHISM_CONST extern inline int16_t  schism_signed_lshift_16_(int16_t x, unsigned int y);
SCHISM_CONST extern inline int32_t  schism_signed_lshift_32_(int32_t x, unsigned int y);
SCHISM_CONST extern inline int64_t  schism_signed_lshift_64_(int64_t x, unsigned int y);
SCHISM_CONST extern inline intmax_t schism_signed_lshift_max_(intmax_t x, unsigned int y);
#endif

#ifndef HAVE_ARITHMETIC_RSHIFT
SCHISM_CONST extern inline int8_t   schism_signed_rshift_8_(int8_t x, unsigned int y);
SCHISM_CONST extern inline int16_t  schism_signed_rshift_16_(int16_t x, unsigned int y);
SCHISM_CONST extern inline int32_t  schism_signed_rshift_32_(int32_t x, unsigned int y);
SCHISM_CONST extern inline int64_t  schism_signed_rshift_64_(int64_t x, unsigned int y);
SCHISM_CONST extern inline intmax_t schism_signed_rshift_max_(intmax_t x, unsigned int y);
#endif
