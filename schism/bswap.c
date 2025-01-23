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

#include "bswap.h"

/* these are defined here for compliance with C99's weird
 * inline behavior */
#ifdef SCHISM_NEED_EXTERN_DEFINE_BSWAP_64
SCHISM_CONST extern inline uint64_t bswap_64_schism_internal_(uint64_t x);
#endif
#ifdef SCHISM_NEED_EXTERN_DEFINE_BSWAP_32
SCHISM_CONST extern inline uint32_t bswap_32_schism_internal_(uint32_t x);
#endif
#ifdef SCHISM_NEED_EXTERN_DEFINE_BSWAP_16
SCHISM_CONST extern inline uint16_t bswap_16_schism_internal_(uint16_t x);
#endif
