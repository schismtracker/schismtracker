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
#include "headers.h"
#include "song.h"
#include "it.h"

static int _cache_ok = 0;
void memused_songchanged(void)
{
        _cache_ok = 0;
}


/* packed patterns */
unsigned int memused_patterns(void)
{
        unsigned int i, nm, rows, q;
        static unsigned int p_cached;
        song_note *ptr;

        if (_cache_ok & 1) return p_cached;
        _cache_ok |= 1;

        q = 0;
        nm = song_get_num_patterns();
        for (i = 0; i < nm; i++) {
                if (song_pattern_is_empty(i)) continue;
                rows = song_get_pattern(i, &ptr);
                q += (rows*256);
        }
        return p_cached = q;
}

unsigned int memused_clipboard(void)
{
        unsigned int q = 0;
        static unsigned int c_cached;

        if (_cache_ok & 2) return c_cached;
        _cache_ok |= 2;

        memused_get_pattern_saved(&q, NULL);
        c_cached = q*256;
        return c_cached;
}
unsigned int memused_history(void)
{
        static unsigned int h_cached;
        unsigned int q = 0;
        if (_cache_ok & 4) return h_cached;
        _cache_ok |= 4;
        memused_get_pattern_saved(NULL, &q);
        return h_cached = (q * 256);
}
unsigned int memused_samples(void)
{
        song_sample *s;
        static unsigned int s_cache;
        unsigned int q;
        int i;

        if (_cache_ok & 8) return s_cache;
        _cache_ok |= 8;

        q = 0;
        for (i = 0; i < 99; i++) {
                s = song_get_sample(i, NULL);
                q += s->length;
                if (s->flags & SAMP_STEREO) q += s->length;
                if (s->flags & SAMP_16_BIT) q += s->length;
        }
        return s_cache = q;
}
unsigned int memused_instruments(void)
{
        static unsigned int i_cache;
        unsigned int q;
        int i;

        if (_cache_ok & 16) return i_cache;
        _cache_ok |= 16;

        q = 0;
        for (i = 0; i < 99; i++) {
                if (song_instrument_is_empty(i)) continue;
                q += 512;
        }
        return i_cache = q;
}
unsigned int memused_songmessage(void)
{
        char *p;
        static unsigned int m_cache;
        if (_cache_ok & 32) return m_cache;
        _cache_ok |= 32;
        if (!(p=song_get_message())) return m_cache = 0;
        return m_cache = strlen(p);
}


/* this makes an effort to calculate about how much memory IT would report
is being taken up by the current song.

it's pure, unadulterated crack, but the routines are useful for schism mode :)
*/
static unsigned int _align4k(unsigned int q) {
        return ((q + 0xfff) & ~0xfff);
}
unsigned int memused_ems(void)
{
        return _align4k(memused_samples())
                + _align4k(memused_history())
                + _align4k(memused_patterns());
}
unsigned int memused_lowmem(void)
{
        return memused_songmessage() + memused_instruments()
                + memused_clipboard();
}
