#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

bool fmt_s3m_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_s3m_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 48 && memcmp(data + 44, "SCRM", 4) == 0))
                return false;

        fi->description = strdup("Scream Tracker 3");
        fi->extension = strdup("s3m");
        fi->title = calloc(28, sizeof(char));
        memcpy(fi->title, data, 27);
        fi->title[27] = 0;
        fi->type = TYPE_S3M;
        return true;
}
