#include "headers.h"

#include "it.h"
#include "helptext.h"

#if HAVE_LIBBZ2
# include <bzlib.h>
#else
# error "TODO: gzip/null help text compression"
#endif

/* --------------------------------------------------------------------- */

static char compressed_help_text[] = COMPRESSED_HELP_TEXT;
static char uncompressed_help_text[UNCOMPRESSED_HELP_TEXT_SIZE];

char *help_text_pointers[HELP_NUM_ITEMS] = { NULL };

/* --------------------------------------------------------------------- */

static void decompress_help_text(void)
{
        int insize = sizeof(compressed_help_text);
        int outsize = sizeof(uncompressed_help_text);
        int ret =
                BZ2_bzBuffToBuffDecompress(uncompressed_help_text,
                                           &outsize, compressed_help_text,
                                           insize, 0, 0);

        if (ret != BZ_OK) {
                fprintf(stderr, "error decompressing help text:"
                                " libbz2 returned %d\n",
                        ret);
                exit(1);
        }
}

/* --------------------------------------------------------------------- */

void setup_help_text_pointers(void)
{
        int n;
        char *ptr = uncompressed_help_text;

        decompress_help_text();
        for (n = 0; n < HELP_NUM_ITEMS; n++) {
                help_text_pointers[n] = ptr;
                ptr = strchr(ptr, 0) + 1;
        }
}
