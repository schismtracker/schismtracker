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

/* This header describes the fft tables for the waterfall page.
 * Mainly so that we can have everything neatly here and have macros
 * handle the "one big allocation" for us.
 * Note that these should be in order of size, from biggest to lowest,
 * in order to reduce possible weirdness. */
FFT_VAR(uint32_t, bit_reverse, bufsize)
FFT_VAR(float, window, bufsize)
FFT_VAR(float, presin, size)
FFT_VAR(float, precos, size)
FFT_VAR(float, state_real, bufsize)
FFT_VAR(float, state_imag, bufsize)
FFT_VAR(int16_t, incomingl, bufsize)
FFT_VAR(int16_t, incomingr, bufsize)
/* fft data is in range 0..128 */
FFT_VAR(uint8_t, current_fft_datal, size)
FFT_VAR(uint8_t, current_fft_datar, size)

#undef FFT_VAR
