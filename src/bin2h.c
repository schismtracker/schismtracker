#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <locale.h>
#include <ctype.h>
#include <getopt.h>
#include <unistd.h>
#include "util.h"

/* TODO:
 * - optionally start lines with some number of spaces instead of a tab
 * - optionally surround with #ifdef and #endif in define mode
 * - different escape modes: perl, ...
 * - "bracket" mode: instead of "binary data\n"
 *   output {'b', 'i', 'n', 'a', 'r', 'y', ' ', 'd', 'a', 't', 'a', '\n'}
 *   (would be much bigger, but might be needed for stupid compilers)
 * - check read/write operations for errors
 * - check return value of fclose()s */

/* bleh, this got ugly really fast */

/* --------------------------------------------------------------------- */

#define VARIABLE_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"\
        "_abcdefghijklmnopqrstuvwxyz"

char *variable_name = NULL;

int max_width = 126;
int tab_size = 8;
int bytes = 0;
int start = 0;

enum {
        /* use a #define statement (rather than a variable)? */
        DEFINE_MODE = (1 << 0),
        /* line up the backslashes at the end of the line in define mode? */
        ALIGN_BACKSLASHES = (1 << 1),
        /* cram the backslashes against the close quote in define mode? */
        CRAM_BACKSLASHES = (1 << 2),
        /* escape all chars? (this makes the file bigger...) */
        ESCAPE_ALL_CHARS = (1 << 3),
        /* use the locale's definition of a printable char? */
        LOCALE_CHARS = (1 << 4),
        /* display digits in octal? (default is hex) */
        USE_OCTAL = (1 << 5),
        /* write comments indicating byte positions? (for debugging) */
        POSITION_COMMENTS = (1 << 6),
};

int flags = DEFINE_MODE | ALIGN_BACKSLASHES;

/* --------------------------------------------------------------------- */

/* *INDENT-OFF* */
const struct {
        char from;
        const char *to;
} escapes[] = {
        {'\0', "\\0"},
        {'\a', "\\a"},
        {'\b', "\\b"},
        {'\t', "\\t"},
        {'\n', "\\n"},
        {'\v', "\\v"},
        {'\f', "\\f"},
        {'\r', "\\r"},
        /* {'\e', "\\e"}, */
        {'"', "\\\""},
        {'\\', "\\\\"},
        {0, NULL}
};
/* *INDENT-ON* */

/* --------------------------------------------------------------------- */

/* *INDENT-OFF* */
static const struct option long_options[] = {
        {"no-define-mode",       no_argument, NULL, 'D'},
        {   "define-mode",       no_argument, NULL, 'd'},
        {"no-align-backslashes", no_argument, NULL, 'A'},
        {   "align-backslashes", no_argument, NULL, 'a'},
        {"no-cram-backslashes",  no_argument, NULL, 'C'},
        {   "cram-backslashes",  no_argument, NULL, 'c'},
        {"no-escape-all-chars",  no_argument, NULL, 'E'},
        {   "escape-all-chars",  no_argument, NULL, 'e'},
        {"no-locale-chars",      no_argument, NULL, 'L'},
        {   "locale-chars",      no_argument, NULL, 'l'},
        {"use-octal",            no_argument, NULL, 'o'},
        {"use-hexadecimal",      no_argument, NULL, 'x'},
        {"no-position-comments", no_argument, NULL, 'P'},
        {   "position-comments", no_argument, NULL, 'p'},
        
        {"max-width",      required_argument, NULL, 'w'},
        {"tab-size",       required_argument, NULL, 't'},
        {"bytes",          required_argument, NULL, -'b'},
        {"start",          required_argument, NULL, -'s'},
        {"name",           required_argument, NULL, 'n'},
        
        {"version",              no_argument, NULL, -'v'},
        {"help",                 no_argument, NULL, -'h'},

        {NULL, 0, NULL, 0}
};
/* *INDENT-ON* */

/* --------------------------------------------------------------------- */

static char *get_variable_name(char *filename)
{
        const char *base;
        char *ret, *ptr;
        const char *variable_chars = VARIABLE_CHARS;

        base = get_basename(filename);
        if (isdigit(base[0])) {
                ret = calloc(strlen(base) + 2, sizeof(char));
                sprintf(ret, "_%s", base);
        } else {
                ret = strdup(base);
        }

        ptr = ret;

        if (flags & DEFINE_MODE) {
                do {
                        if (strchr(variable_chars, *ptr)) {
                                *ptr = toupper(*ptr);
                        } else {
                                *ptr = '_';
                        }
                        ptr++;
                } while (*ptr);
        } else {
                do {
                        ptr += strspn(ptr, variable_chars);
                        if (*ptr) {
                                *ptr = '_';
                        }
                } while (*ptr);
        }

        return ret;
}

static inline int need_escape(int c)
{
        return ((flags & USE_OCTAL)
                ? (c >= '0' && c <= '7')
                : ((c >= '0' && c <= '9') || (c >= 'A' && c <= 'F')
                   || (c >= 'a' && c <= 'f')));
}

static int escape_char(int c, char *buf, int escape_if_digit)
{
        int e;

        if (!(flags & ESCAPE_ALL_CHARS)) {
                for (e = 0; escapes[e].to; e++) {
                        if (escapes[e].from == c) {
                                strcpy(buf, escapes[e].to);
                                return need_escape(buf[strlen(buf) - 1]);
                        }
                }

                if ((!(escape_if_digit && need_escape(c)))
                    && ((flags & LOCALE_CHARS) ? isprint(c)
                        : (c >= 32 && c < 127))) {
                        buf[0] = c;
                        buf[1] = 0;
                        return 0;
                }
        }
        sprintf(buf, ((flags & USE_OCTAL) ? "\\%o" : "\\x%x"), c);
        return need_escape(buf[strlen(buf) - 1]);
}

/* --------------------------------------------------------------------- */

static void print_version(FILE * f)
{
        static int did_this = 0;

        if (did_this)
                return;
        did_this = 1;

        fprintf(f, "blah blah version blah gpl blah blah no warranty"
                " blah blah sandwich blah blah blah\n");
}

static void print_help(FILE * f)
{
        const struct option *o = long_options;
        static int did_this = 0;

        if (did_this)
                return;
        did_this = 1;

        print_version(f);
        fprintf(f, "\nUsage: bin2h [<options>] [infile [outfile]]\n");
        while (o->name) {
                switch (o->has_arg) {
                case required_argument:
                        if (o->val > 0)
                                fprintf(f, "\t-%c --%s <arg>\n", o->val,
                                        o->name);
                        else
                                fprintf(f, "\t   --%s <arg>\n", o->name);
                        break;
                case optional_argument:
                        if (o->val > 0)
                                fprintf(f, "\t-%c --%s [arg]\n", o->val,
                                        o->name);
                        else
                                fprintf(f, "\t   --%s [arg]\n", o->name);
                        break;
                default:
                        if (o->val > 0)
                                fprintf(f, "\t-%c --%s\n", o->val,
                                        o->name);
                        else
                                fprintf(f, "\t   --%s\n", o->name);
                        break;
                }
                o++;
        }
        fprintf(f,
                "\nSee the imaginary documentation for less vague help.\n");
}

/* --------------------------------------------------------------------- */

static long get_number(const char *str, long error_value)
{
        char *endptr;
        long n = strtol(str, &endptr, 0);

        if (endptr == str || *endptr) {
                fprintf(stderr, "bin2h: invalid number \"%s\"\n", str);
                return error_value;
        }

        return n;
}

/* --------------------------------------------------------------------- */

static void handle_options(int argc, char **argv)
{
        int opt, should_exit = 0;

        while ((opt =
                getopt_long(argc, argv, "DdAaCcEeLloxPpw:t:n:",
                            long_options, NULL)) != EOF) {
                switch (opt) {
                case 'D':
                        flags &= ~DEFINE_MODE;
                        break;
                case 'd':
                        flags |= DEFINE_MODE;
                        break;
                case 'A':
                        flags &= ~ALIGN_BACKSLASHES;
                        break;
                case 'a':
                        flags |= ALIGN_BACKSLASHES;
                        break;
                case 'C':
                        flags &= ~CRAM_BACKSLASHES;
                        break;
                case 'c':
                        flags |= CRAM_BACKSLASHES;
                        break;
                case 'E':
                        flags &= ~ESCAPE_ALL_CHARS;
                        break;
                case 'e':
                        flags |= ESCAPE_ALL_CHARS;
                        break;
                case 'L':
                        flags &= ~LOCALE_CHARS;
                        break;
                case 'l':
                        flags |= LOCALE_CHARS;
                        break;
                case 'o':
                        flags |= USE_OCTAL;
                        break;
                case 'x':
                        flags &= ~USE_OCTAL;
                        break;
                case 'P':
                        flags &= ~POSITION_COMMENTS;
                        break;
                case 'p':
                        flags |= POSITION_COMMENTS;
                        break;
                case 'w':
                        max_width = get_number(optarg, max_width);
                        break;
                case 't':
                        tab_size = get_number(optarg, tab_size);
                        break;
                case -'b':
                        bytes = get_number(optarg, 0);
                        bytes = MAX(0, bytes);
                        break;
                case -'s':
                        start = get_number(optarg, 0);
                        start = MAX(0, start);
                        break;
                case 'n':
                        if (variable_name)
                                free(variable_name);
                        variable_name = strdup(optarg);
                        break;
                case -'v':
                        print_version(stdout);
                        should_exit = 1;
                        break;
                case -'h':
                        print_help(stdout);
                        should_exit = 1;
                        break;
                case '?':      /* unknown option */
                case ':':      /* missing parameter */
                        print_help(stderr);
                        should_exit = 2;
                        break;
                default:
                        fprintf(stderr, "bin2h: unhandled option 0x%02x\n",
                                opt);
                }
        }

        if (should_exit)
                exit(should_exit - 1);
}

/* --------------------------------------------------------------------- */

int main(int argc, char **argv)
{
        FILE *in = stdin, *out = stdout;
        int c, width, escape_if_digit = 0, cur_byte;
        int byte_at_bol = 0;
        char buf[16];
        char *filename;

        setlocale(LC_ALL, "");

        handle_options(argc, argv);
        if (argc - optind > 2) {
                print_help(stderr);
                exit(1);
        }

        tab_size = MAX(0, tab_size);
        max_width = MAX(tab_size + 8, max_width);

        switch (argc - optind) {
        case 2:
                filename = argv[optind + 1];
                if (strcmp(filename, "-") != 0) {
                        out = fopen(filename, "w");
                        if (out == NULL) {
                                perror(filename);
                                exit(2);
                        }
                }
        case 1:
                filename = argv[optind];
                if (strcmp(filename, "-") != 0) {
                        in = fopen(filename, "rb");
                        if (in == NULL) {
                                perror(filename);
                                exit(2);
                        }
                        if (!variable_name) {
                                variable_name =
                                        get_variable_name(filename);
                        }
                }
        }

        if (isatty(fileno(in))) {
                fprintf(stderr, "bin2h: I won't read binary data"
                        " from a terminal.\n");
                exit(1);
        }

        for (cur_byte = 0; cur_byte < start; cur_byte++) {
                if (fgetc(in) == EOF) {
                        fprintf(stderr,
                                "error: EOF before start position\n");
                        exit(1);
                }
        }

        if (flags & DEFINE_MODE) {
                fprintf(out, "#define %s \\\n\t\"",
                        variable_name ? variable_name : "BIN_DATA");
        } else {
                fprintf(out, "static const unsigned char *%s =\n\t\"",
                        variable_name ? variable_name : "bin_data");
        }
        width = tab_size + 1;

        cur_byte = 0;
        while ((c = fgetc(in)) != EOF) {
                int printlen, newlen;

                if (bytes > 0 && cur_byte >= bytes)
                        break;
                cur_byte++;

                escape_if_digit = escape_char(c, buf, escape_if_digit);
                newlen = strlen(buf);

                if (flags & DEFINE_MODE) {
                        if (width + newlen +
                            ((flags & CRAM_BACKSLASHES) ? 2 : 3) >
                            max_width) {
                                fprintf(out, "\"");
                                width++;
                                if (!(flags & CRAM_BACKSLASHES)) {
                                        fprintf(out, " ");
                                        width++;
                                }
                                if (flags & ALIGN_BACKSLASHES) {
                                        while (width < max_width - 1) {
                                                fprintf(out, " ");
                                                width++;
                                        }
                                }

                                if (flags & POSITION_COMMENTS) {
                                        if (flags & USE_OCTAL)
                                                fprintf(out,
                                                        "/* 0%o - 0%o */",
                                                        byte_at_bol,
                                                        cur_byte - 1);
                                        else
                                                fprintf(out,
                                                        "/* 0x%x - 0x%x */",
                                                        byte_at_bol,
                                                        cur_byte - 1);
                                }

                                fprintf(out, "\\\n\t\"");
                                width = tab_size + 1;
                                byte_at_bol = cur_byte;
                        }
                } else {
                        if (width + newlen + 1 > max_width) {
                                if (flags & POSITION_COMMENTS) {
                                        if (flags & USE_OCTAL)
                                                fprintf(out,
                                                        "\" /* 0%o - 0%o */\n\t\"",
                                                        byte_at_bol,
                                                        cur_byte - 1);
                                        else
                                                fprintf(out,
                                                        "\" /* 0x%x - 0x%x */\n\t\"",
                                                        byte_at_bol,
                                                        cur_byte - 1);
                                } else {
                                        fprintf(out, "\"\n\t\"");
                                }
                                width = tab_size + 1;
                                byte_at_bol = cur_byte;
                        }
                }

                printlen = fprintf(out, "%s", buf);
                width += printlen;
        }

        fprintf(out, ((flags & DEFINE_MODE) ? "\"\n" : "\"\n\t;\n"));

        /* is this stuff even useful at the end of the program? */
        if (variable_name)
                free(variable_name);

        if (in != stdin)
                fclose(in);
        if (out != stdout)
                fclose(out);

        return 0;
}
