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
#include "song.h"
#include "it.h"
#include "fakemem.h"

static int _cache_ok = 0;
void memused_songchanged(void)
{
	_cache_ok = 0;
}


/* packed patterns */
uint32_t memused_patterns(void)
{
	uint32_t i, nm, rows, q;
	static uint32_t p_cached;
	song_note_t *ptr;

	if (_cache_ok & 1) return p_cached;
	_cache_ok |= 1;

	q = 0;
	nm = csf_get_num_patterns(current_song);
	for (i = 0; i < nm; i++) {
		if (csf_pattern_is_empty(current_song, i)) continue;
		rows = song_get_pattern(i, &ptr);
		q += (rows*256);
	}
	return p_cached = q;
}

uint32_t memused_clipboard(void)
{
	uint32_t q = 0;
	static uint32_t c_cached;

	if (_cache_ok & 2) return c_cached;
	_cache_ok |= 2;

	memused_get_pattern_saved(&q, NULL);
	c_cached = q*256;
	return c_cached;
}
uint32_t memused_history(void)
{
	static uint32_t h_cached;
	uint32_t q = 0;
	if (_cache_ok & 4) return h_cached;
	_cache_ok |= 4;
	memused_get_pattern_saved(NULL, &q);
	return h_cached = (q * 256);
}
uint32_t memused_samples(void)
{
	song_sample_t *s;
	static uint32_t s_cache;
	uint32_t q, qs;
	int i;

	if (_cache_ok & 8) return s_cache;
	_cache_ok |= 8;

	q = 0;
	for (i = 0; i < MAX_SAMPLES; i++) {
		s = song_get_sample(i);
		qs = s->length;
		if (s->flags & CHN_STEREO) qs *= 2;
		if (s->flags & CHN_16BIT) qs *= 2;
		q += qs;
	}
	return s_cache = q;
}
uint32_t memused_instruments(void)
{
	static uint32_t i_cache;
	uint32_t q;
	int i;

	if (_cache_ok & 16) return i_cache;
	_cache_ok |= 16;

	q = 0;
	for (i = 0; i < MAX_INSTRUMENTS; i++) {
		if (csf_instrument_is_empty(current_song->instruments[i])) continue;
		q += 512;
	}
	return i_cache = q;
}
uint32_t memused_songmessage(void)
{
	static uint32_t m_cache;
	if (_cache_ok & 32) return m_cache;
	_cache_ok |= 32;
	return m_cache = strlen(current_song->message);
}


/* this makes an effort to calculate about how much memory IT would report
is being taken up by the current song.

it's pure, unadulterated crack, but the routines are useful for schism mode :)
*/
static uint32_t _align4k(uint32_t q) {
	return ((q + 0xfff) & ~(uint32_t)0xfff);
}
uint32_t memused_ems(void)
{
	return _align4k(memused_samples())
		+ _align4k(memused_history())
		+ _align4k(memused_patterns());
}
uint32_t memused_lowmem(void)
{
	return memused_songmessage() + memused_instruments()
		+ memused_clipboard();
}
