#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

/* TODO: test this code.
 * Modplug seems to have a totally different idea of ams than this.
 * I don't know what this data's supposed to be for :) */

/* btw: AMS stands for "Advanced Module System" */

bool fmt_ams_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_ams_read_info(const byte * data, size_t length, file_info * fi)
{
        byte n;

        if (!(length > 38 && memcmp(data, "AMShdr\x1a", 7) == 0))
                return false;

        n = data[7];
        if (n > 30)
                n = 30;
        fi->description = strdup("Velvet Studio");
        fi->extension = strdup("ams");
        fi->title = calloc(n + 1, sizeof(char));
        memcpy(fi->title, data + 8, n);
        fi->title[n] = 0;
        fi->type = TYPE_XM;
        return true;
}
