#include "headers.h"

#include "title.h"

/* ----------------------------------------------------------------------------------------------------------------------- */

/* TODO: WOW files */

/* Ugh. */
static const char *valid_tags[][2] = {
        /* M.K. must be the first tag! (to test for WOW files) */
        /* the first 5 descriptions are a bit weird */
        {"M.K.", "Amiga-NewTracker"},
        {"M!K!", "Amiga-ProTracker"},
        {"FLT4", "4 Channel Startrekker"},      // xxx
        {"CD81", "8 Channel Falcon"},   // "Falcon"?
        {"FLT8", "8 Channel Startrekker"},      // xxx

        /* the rest of the descriptions have " MOD" appended to them */
        {"8CHN", "8 Channel"},  // what is the difference
        {"OCTA", "8 Channel"},  // between these two?
        {"TDZ1", "1 Channel"},
        {"2CHN", "2 Channel"},
        {"TDZ2", "2 Channel"},
        {"TDZ3", "3 Channel"},
        {"5CHN", "5 Channel"},
        {"6CHN", "6 Channel"},
        {"7CHN", "7 Channel"},
        {"9CHN", "9 Channel"},
        {"10CH", "10 Channel"},
        {"11CH", "11 Channel"},
        {"12CH", "12 Channel"},
        {"13CH", "13 Channel"},
        {"14CH", "14 Channel"},
        {"15CH", "15 Channel"},
        {"16CH", "16 Channel"},
        {"18CH", "18 Channel"},
        {"20CH", "20 Channel"},
        {"22CH", "22 Channel"},
        {"24CH", "24 Channel"},
        {"26CH", "26 Channel"},
        {"28CH", "28 Channel"},
        {"30CH", "30 Channel"},
        {"32CH", "32 Channel"},
        {NULL, NULL}
};

bool fmt_mod_read_info(const byte * data, size_t length, file_info * fi);
bool fmt_mod_read_info(const byte * data, size_t length, file_info * fi)
{
        char tag[5];
        int i = 0;

        if (length < 1085)
                return false;

        memcpy(tag, data + 1080, 4);
        tag[4] = 0;

        while (valid_tags[i][0]) {
                if (strcmp(tag, valid_tags[i][0]) == 0) {
                        const char *desc_part = valid_tags[i][1];

                        /* if (i == 0) {
                         *     might be a .wow; need to calculate some
                         *     crap to find out for sure. For now, since
                         *     I have no wow's, I'm not going to care.
                         * } */

                        /* the first few have different descriptions */
                        if (i <= 4) {
                                fi->description = strdup(desc_part);
                        } else {
                                fi->description =
                                        calloc(strlen(desc_part) + 5,
                                               sizeof(char));
                                strcpy(fi->description, desc_part);
                                strcat(fi->description, " MOD");
                        }
                        fi->extension = strdup("mod");
                        fi->title = calloc(21, sizeof(char));
                        memcpy(fi->title, data, 20);
                        fi->title[20] = 0;
                        fi->type = TYPE_MOD;
                        return true;
                }
                i++;
        }

        return false;
}
