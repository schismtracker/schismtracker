#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

/* TODO: test this code */

bool fmt_f2r_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_f2r_read_info(const byte * data, size_t length, file_info * fi)
{
        if (!(length > 46 && memcmp(data, "F2R", 3) == 0))
                return false;

        fi->description = strdup("Farandole 2 (linear)");
        fi->extension = strdup("f2r");
        fi->title = calloc(41, sizeof(char));
        memcpy(fi->title, data + 6, 40);
        fi->title[40] = 0;
        fi->type = TYPE_S3M;
        return true;
}
