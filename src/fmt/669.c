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

/* Thanks to Olivier Lapicque, this function no longer sucks. A lot of
 * this code was lifted from ModPlug's 669 loader. I renamed about all
 * the data types (btw, what is the difference between DWORD and UINT?)
 * and changed the xgqImpossibleToReadVariableNames to ones that don't
 * give me a headache ;) but essentially it's the same... */

struct header_669 {
        char sig[2];
        char songmessage[108];
        byte samples;
        byte patterns;
        byte restartpos;
        byte orders[128];
        byte tempolist[128];
        byte breaks[128];
};

struct sample_669 {
        byte filename[13];
        byte length[4];
        byte loopstart[4];
        byte loopend[4];
};

bool fmt_669_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_669_read_info(const byte * data, size_t length, file_info * fi)
{
        struct header_669 *header = (struct header_669 *) data;
        struct sample_669 *samples =
                (struct sample_669 *) (data + sizeof(struct header_669));
        unsigned long n, i;
        const char *desc;

        if (length < sizeof(struct header_669))
                return false;

        /* Impulse Tracker identifies any 669 file as a "Composer
         * 669 Module", regardless of the signature tag. */
        if (memcmp(header->sig, "if", 2) == 0)
                desc = "Composer 669 Module";
        else if (memcmp(header->sig, "JN", 2) == 0)
                desc = "Extended 669 Module";
        else
                return false;

        if (header->samples == 0 || header->patterns == 0
            || header->samples > 64 || header->patterns > 128
            || header->restartpos > 127)
                return false;

        n = (sizeof(struct header_669) +
             (sizeof(struct sample_669) * header->samples) +
             (1536 * header->patterns));
        if (n > length)
                return false;

        for (i = 0; i < header->samples; i++)
                n += bswapLE32(*((unsigned long *) (&samples[i].length)));

        if (n > length)
                return false;

        /* From my (very brief) observation it seems the message of a 669
         * file is split into 3 lines. This (naively) takes the first line
         * of it as the title, as the format doesn't actually have a field
         * for a song title. */
        fi->title = (char *) calloc(37, sizeof(char));
        memcpy(fi->title, header->songmessage, 36);
        fi->title[36] = 0;

        fi->description = strdup(desc);
        fi->extension = strdup("669");
        fi->type = TYPE_S3M;

        return true;
}
