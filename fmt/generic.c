/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010 Storlek
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
#include "fmt.h"

/* --------------------------------------------------------------------------------------------------------- */

static int _mod_period_to_note(int period)
{
        int n;

        if (period)
                for (n = 0; n <= NOTE_LAST; n++)
                        if (period >= (32 * FreqS3MTable[n % 12] >> (n / 12 + 2)))
                                return n + 1;
        return NOTE_NONE;
}

void mod_import_note(const uint8_t p[4], MODCOMMAND *note)
{
        note->note = _mod_period_to_note(((p[0] & 0xf) << 8) + p[1]);
        note->instr = (p[0] & 0xf0) + (p[2] >> 4);
        note->volcmd = VOLCMD_NONE;
        note->vol = 0;
        note->command = p[2] & 0xf;
        note->param = p[3];
}

