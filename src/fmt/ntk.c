#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

bool fmt_ntk_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_ntk_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 25 && memcmp(data, "TWNNSNG2", 8) == 0))
                return false;

        fi->description = strdup("NoiseTrekker");
        fi->extension = strdup("ntk");
        fi->title = calloc(16, sizeof(char));
        memcpy(fi->title, data + 9, 15);
        fi->title[15] = 0;
        fi->type = TYPE_MOD;    /* ??? */
        return true;
}
