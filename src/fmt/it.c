#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

/* FIXME: MMCMP isn't IT-specific, and I know nothing about it */

bool fmt_it_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_it_read_info(const byte * data, size_t length, file_info * fi)
{
        bool mmcmp;

        /* "Bart just said I-M-P! He's made of pee!" */
        if (length > 30 && memcmp(data, "IMPM", 4) == 0) {
                mmcmp = false;
                /* I had an snprintf here that stuck the CMWT in the description, but I got rid of it because it really
                 * doesn't add a whole lot... */
                if (data[42] >= 0x14)
                        fi->description =
                                strdup("Compressed Impulse Tracker");
                else
                        fi->description = strdup("Impulse Tracker");
        } else if (length > 164 && memcmp(data + 132, "IMPM", 4) == 0
                   && memcmp(data, "ziRCONia", 8) == 0) {
                mmcmp = true;
                fi->description = strdup("Impulse Tracker");
        } else {
                return false;
        }

        fi->extension = strdup("it");
        fi->title = calloc(26, sizeof(char));
        memcpy(fi->title, data + (mmcmp ? 136 : 4), 25);
        fi->title[25] = 0;
        fi->type = TYPE_IT;
        return true;
}
