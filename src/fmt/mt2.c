#include "headers.h"

#include "title.h"

/* FIXME:
 * - this is wrong :)
 * - look for an author name; if it's not "Unregistered" use it */

/* --------------------------------------------------------------------- */

bool fmt_mt2_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_mt2_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 106 && memcmp(data, "MT20", 4) == 0))
                return false;

        fi->description = strdup("MadTracker 2 Module");
        fi->extension = strdup("mt2");
        fi->title = calloc(65, sizeof(char));
        memcpy(fi->title, data + 42, 64);
        fi->title[64] = 0;
        fi->type = TYPE_XM;
        return true;
}
