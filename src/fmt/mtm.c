#include "headers.h"

#include "title.h"

/* --------------------------------------------------------------------- */

bool fmt_mtm_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_mtm_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 24 && memcmp(data, "MTM", 3) == 0))
                return false;

        fi->description = strdup("MultiTracker Module");
        fi->extension = strdup("mtm");
        fi->title = calloc(21, sizeof(char));
        memcpy(fi->title, data + 4, 20);
        fi->title[20] = 0;
        fi->type = TYPE_MOD;
        return true;
}
