#include "headers.h"

#include "title.h"

/* --------------------------------------------------------------------- */

bool fmt_rtm_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_rtm_read_info(UNUSED const byte * data, UNUSED size_t length,
                       UNUSED file_info * fi)
{
        /* FIXME */
        return false;
}
