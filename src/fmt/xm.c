#include "headers.h"

#include "title.h"

/* --------------------------------------------------------------------- */

bool fmt_xm_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_xm_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 38 && memcmp(data, "Extended Module: ", 17) == 0))
                return false;

        fi->description = strdup("Fast Tracker 2 Module");
        fi->type = TYPE_XM;
        fi->extension = strdup("xm");
        fi->title = calloc(21, sizeof(char));
        memcpy(fi->title, data + 17, 20);
        fi->title[20] = 0;
        return true;
}
