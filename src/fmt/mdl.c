#include "headers.h"

#include <byteswap.h>

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

#ifdef WORDS_BIGENDIAN
# define bswapLE32(x) bswap_32(x)
#else
# define bswapLE32(x) (x)
#endif

/* ----------------------------------------------------------------------------------------------------------------------- */

/* MDL is nice, but it's a pain to read the title... */

/* TODO: this is another format with separate artist/title fields... */

bool fmt_mdl_read_info(byte * data, size_t length, file_info * fi);
bool fmt_mdl_read_info(byte * data, size_t length, file_info * fi)
{
        unsigned int position, block_length;
        UNUSED int n;
        char artist[21], title[33];

        if (!(length > 5 && ((data[4] & 0xf0) >> 4) <= 1        /* major version 0 or 1 */
              && memcmp(data, "DMDL", 4) == 0))
                return false;

        position = 5;
        while (position + 6 < length) {
                memcpy(&block_length, data + position + 2, 4);
                block_length = bswapLE32(block_length);
                if (block_length + position > length)
                        return false;
                if (memcmp(data + position, "IN", 2) == 0) {
                        /* hey! we have a winner */
                        memcpy(title, data + position + 6, 32);
                        memcpy(artist, data + position + 38, 20);
                        artist[20] = 0;
                        title[32] = 0;
                        trim_string(artist);
                        trim_string(title);

                        fi->description = strdup("Digitrakker");
                        fi->extension = strdup("mdl");
                        fi->title = (char *) calloc(56, sizeof(char));
                        sprintf(fi->title, "%s / %s", artist, title);
                        fi->type = TYPE_XM;
                        return true;
                }       /* else... */
                position += 6 + block_length;
        }

        return false;
}
