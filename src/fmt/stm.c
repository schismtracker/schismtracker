#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

/* TODO: get more stm's and test this... one file's not good enough */

bool fmt_stm_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_stm_read_info(const byte * data, size_t length, file_info * fi)
{
        /* Hmm... checking the data in the tracker ID field doesn't seem to be the most reliable way to tell if it's
         * really an stm. Plus these are apparently cAsE iNsEnSiTiVe checks; both the modplug source codeand
         * /usr/share/magic/magic have "!SCREAM!" (in all caps) but the only stm I have (Purple Motion's "Fracture in
         * Space") uses "!Scream!". */

        /* FIXME: strncasecmp isn't very portable */
        if (!(length > 28 && data[28] == 0x1a && (data[29] == 1 || data[29] == 2)       // 1 = song, 2 = module
              && (strncasecmp(data + 14, "!SCREAM!", 8)
                  || strncasecmp(data + 14, "BMOD2STM", 8))
            ))
                return false;

        /* I used to check whether it was a 'song' or 'module' and set the description accordingly,
         * but it's fairly pointless information :) */
        fi->description = strdup("Scream Tracker 2");
        fi->extension = strdup("stm");
        fi->type = TYPE_MOD;
        fi->title = calloc(21, sizeof(char));
        memcpy(fi->title, data, 20);
        fi->title[20] = 0;
        return true;
}
