/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#define NEED_BYTESWAP
#include "headers.h"

#include "title.h"

/* --------------------------------------------------------------------- */

/* Thanks to Olivier Lapicque, this function no longer sucks. A lot of
 * this code was lifted from ModPlug's 669 loader. I renamed about all
 * the data types (btw, what is the difference between DWORD and UINT?)
 * and changed the xgqImpossibleToReadVariableNames to ones that don't
 * give me a headache ;) but essentially it's the same... */

struct header_669 {
        char sig[2];
        char songmessage[108];
        byte samples;
        byte patterns;
        byte restartpos;
        byte orders[128];
        byte tempolist[128];
        byte breaks[128];
};

bool fmt_669_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_669_read_info(const byte * data, size_t length, file_info * fi)
{
        struct header_669 *header = (struct header_669 *) data;
        unsigned long i;
        const char *desc;
        
        if (length < sizeof(struct header_669))
                return false;
        
        /* Impulse Tracker identifies any 669 file as a "Composer
         * 669 Module", regardless of the signature tag. */
        if (memcmp(header->sig, "if", 2) == 0)
                desc = "Composer 669 Module";
        else if (memcmp(header->sig, "JN", 2) == 0)
                desc = "Extended 669 Module";
        else
                return false;
        
        if (header->samples == 0 || header->patterns == 0
            || header->samples > 64 || header->patterns > 128
            || header->restartpos > 127)
                return false;
        for (i = 0; i < 128; i++)
                if (header->breaks[i] > 0x3f)
                        return false;
        
        /* From my very brief observation, it seems the message of a 669
         * file is split into 3 lines. This (naively) takes the first
         * line of it as the title, as the format doesn't actually have
         * a field for a song title. */
        fi->title = (char *) calloc(37, sizeof(char));
        memcpy(fi->title, header->songmessage, 36);
        fi->title[36] = 0;
        
        fi->description = strdup(desc);
        fi->extension = strdup("669");
        fi->type = TYPE_S3M;
        
        return true;
}
