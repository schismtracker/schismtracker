#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

bool fmt_far_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_far_read_info(const byte * data, size_t length, file_info * fi)
{
        /* the magic for this format is truly weird (which I suppose is good, as the chance of it being "accidentally"
         * correct is pretty low) */
        if (!
            (length > 47 && memcmp(data + 44, "\x0d\x0a\x1a", 3) == 0
             && memcmp(data, "FAR\xfe", 4) == 0))
                return false;

        fi->description = strdup("Farandole Module");
        fi->extension = strdup("far");
        fi->title = calloc(41, sizeof(char));
        memcpy(fi->title, data + 4, 40);
        fi->title[40] = 0;
        fi->type = TYPE_S3M;
        return true;
}
