#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

/* TODO: make this not suck. I have no documentation for this format, and only one file to test with,
 * so it's kinda tricky at the moment.
 *
 * /usr/share/magic/magic says:
 *     0       string  MAS_UTrack_V00
 *     >14     string  >/0             ultratracker V1.%.1s module sound data */

bool fmt_ult_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_ult_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 48 && memcmp(data, "MAS_UTrack_V00", 14) == 0))
                return false;

        fi->description = strdup("UltraTracker Module");
        fi->type = TYPE_S3M;
        fi->extension = strdup("ult");
        fi->title = calloc(33, sizeof(char));
        memcpy(fi->title, data + 15, 32);
        fi->title[32] = 0;
        return true;
}
