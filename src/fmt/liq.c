#include "headers.h"

#include "title.h"

/* --------------------------------------------------------------------- */

bool fmt_liq_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_liq_read_info(const byte * data, size_t length, file_info * fi)
{
        char artist[21], title[31];

        if (!
            (length > 64 && data[64] == 0x1a
             && memcmp(data, "Liquid Module:", 14) == 0))
                return false;

        fi->description = strdup("Liquid Tracker");
        fi->extension = strdup("liq");
        fi->title = calloc(54, sizeof(char));

        memcpy(artist, data + 44, 20);
        memcpy(title, data + 14, 30);
        artist[20] = 0;
        title[30] = 0;
        trim_string(artist);
        trim_string(title);
        /* TODO: separate artist/title fields: make this customizable */
        sprintf(fi->title, "%s / %s", artist, title);
        fi->type = TYPE_S3M;

        return true;
}
