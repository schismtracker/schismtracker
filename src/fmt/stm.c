#include "headers.h"

#include "title.h"

/* --------------------------------------------------------------------- */

/* TODO: get more stm's and test this... one file's not good enough */

bool fmt_stm_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_stm_read_info(const byte * data, size_t length, file_info * fi)
{
        // data[29] is the type: 1 = song, 2 = module (with samples)
        if (!(length > 28 && data[28] == 0x1a
              && (data[29] == 1 || data[29] == 2)
              && (memcmp(data + 14, "!Scream!", 8)
                  || memcmp(data + 14, "BMOD2STM", 8))
            ))
                return false;

        /* I used to check whether it was a 'song' or 'module' and set
         * the description accordingly, but it's fairly pointless
         * information :) */
        fi->description = strdup("Scream Tracker 2");
        fi->extension = strdup("stm");
        fi->type = TYPE_MOD;
        fi->title = calloc(21, sizeof(char));
        memcpy(fi->title, data, 20);
        fi->title[20] = 0;
        return true;
}
