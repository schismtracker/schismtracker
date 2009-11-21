/*----------------------------------------------------------------------------*\
    frag-opt - frag-opt rather ain't getopt - or popt, technically
    Version 0.5.5

    Copyright (C) 2004 Ville Jokela
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU Library General Public License as published
    by the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
\*----------------------------------------------------------------------------*/

/* This was written on a 100x37 terminal, so this might get a bit wide.
 * Also a word of warning, you will find here plenty of conditionals of the form
        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_NEG
        &&  frag->argv[frag->index][frag->chr] == frag->fops[i].chr)
                stuff();
 * ie. a logic operator right below "if".
 * Also, I'm pretty damn paranoid when it comes to operator precedance, so you'll see a lot of
 * useless parentheses.
 */

/* TODO POSIX ?? (ie. stop parsing when encountered the first non-option thingy)
 */

#if HAVE_CONFIG_H
# include <config.h>
#endif

#if STDC_HEADERS
# include <stdio.h>
# include <stdlib.h>
#endif

#if HAVE_STRING_H
# include <string.h>
#elif HAVE_STRINGS_H
# include <strings.h>
#else
# warning no string header found
  /* hope for the best */
#endif

/* these are used by the wrapper */
#define INDENT_OPTIONS          2
#define INDENT_DESC             16
#define SCREEN_WIDTH            80

#include <frag-opt.h>

/* helper functions */
static inline void _frag_parse_bare(FRAG *frag);
static void     _frag_parse_sopt(FRAG *frag, const char *str);
static void     _frag_parse_lopt(FRAG *frag, const char *str);
static void     _frag_parse_nopt(FRAG *frag, const char *str);
static void     _frag_do_opt(FRAG *frag, int i);
static void     _frag_do_sopt(FRAG *frag, int i);
static void     _frag_do_lopt(FRAG *frag, int i);
static void     _frag_do_nopt(FRAG *frag, int i);
static inline int _frag_print_options(FRAG *frag, const int i, int col);
static int      _frag_print_sopt(FRAG *frag, const int i, int col);
static int      _frag_print_lopt(FRAG *frag, const int i, int col);
static int      _frag_print_wrapped(const char *str, const int indent, int col);
static void     _frag_print_error(FRAG *frag);

FRAG *frag_init(frag_option *fops, int argc, char **argv, int flags)
{
        FRAG *frag;
        int i, j;

        if (argc == 0 || argv == NULL || fops == NULL) {
                fprintf(stderr, "frag_init: argc, argv or fops is invalid\n");
                return NULL;
        }

        /* check for id's below zero */
        for (i = 0; fops[i].type != _FRAG_EOA; i++)
                if (fops[i].id < 0) {
                        fprintf(stderr, "frag_init: option %d's id is %d (below zero)\n", i,
                                        fops[i].id);
                        return NULL;
                }

        /* check that there's no duplicate id's with different flags */
        for (i = 0; fops[i + 1].type != _FRAG_EOA; i++)
                for (j = i + 1; fops[j].type != _FRAG_EOA; j++)
                        if (fops[i].id == fops[j].id
                        && (fops[i].type & _FRAG_TYPES) != (fops[j].type & _FRAG_TYPES)) {
                                fprintf(stderr, "frag_init: options %d and %d use the same id "
                                                "but have different types\n", i, j);
                                return NULL;
                        }

        frag = malloc(sizeof(FRAG));
        if (frag == NULL)
                return NULL;

        /* see if the program takes args */
        for (i = 0, j = 0; fops[i].type != _FRAG_EOA; i++)
                if (fops[i].type == _FRAG_PROG) {
                        frag->prog = i;
                        j++;
                }

        if (j == 0)             /* no _FRAG_PROG's */
                frag->prog = -1;
        else if (j > 1) {       /* 1+ _FRAG_PROG's */
                fprintf(stderr, "frag_init: too many FRAG_PROGRAMs in array\n");
                return NULL;
        }

        /* we don't want no invalid flags in here */
        while (flags) {                 /* this might be pretty, but at least its clear */
                if (flags &             FRAG_DISABLE_DOUBLEDASH) {
                        frag->flags |=  FRAG_DISABLE_DOUBLEDASH;
                        flags ^=        FRAG_DISABLE_DOUBLEDASH;
                } else if (flags &      FRAG_DISABLE_GROUPS) {
                        frag->flags |=  FRAG_DISABLE_GROUPS;
                        flags ^=        FRAG_DISABLE_GROUPS;
                } else if (flags &      FRAG_DISABLE_EQUALS_LONG) {
                        frag->flags |=  FRAG_DISABLE_EQUALS_LONG;
                        flags ^=        FRAG_DISABLE_EQUALS_LONG;
                } else if (flags &      FRAG_ENABLE_SPACED_LONG) {
                        frag->flags |=  FRAG_ENABLE_SPACED_LONG;
                        flags ^=        FRAG_ENABLE_SPACED_LONG;
                } else if (flags &      FRAG_DISABLE_SPACED_SHORT) {
                        frag->flags |=  FRAG_DISABLE_SPACED_SHORT;
                        flags ^=        FRAG_DISABLE_SPACED_SHORT;
                } else if (flags &      FRAG_ENABLE_NO_SPACE_SHORT) {
                        frag->flags |=  FRAG_ENABLE_NO_SPACE_SHORT;
                        flags ^=        FRAG_ENABLE_NO_SPACE_SHORT;
                } else if (flags &      FRAG_DISABLE_LONG_OPTIONS) {
                        frag->flags |=  FRAG_DISABLE_LONG_OPTIONS;
                        flags ^=        FRAG_DISABLE_LONG_OPTIONS;
                } else if (flags &      FRAG_DISABLE_SHORT_OPTIONS) {
                        frag->flags |=  FRAG_DISABLE_SHORT_OPTIONS;
                        flags ^=        FRAG_DISABLE_SHORT_OPTIONS;
                } else if (flags &      FRAG_DISABLE_NEGATION_OPTIONS) {
                        frag->flags |=  FRAG_DISABLE_NEGATION_OPTIONS;
                        flags ^=        FRAG_DISABLE_NEGATION_OPTIONS;
                } else if (flags &      FRAG_ENABLE_ONEDASH_LONG) {
                        frag->flags |=  FRAG_ENABLE_ONEDASH_LONG;
                        flags ^=        FRAG_ENABLE_ONEDASH_LONG;
                } else if (flags &      FRAG_QUIET) {
                        frag->flags |=  FRAG_QUIET;
                        flags ^=        FRAG_QUIET;
                } else if (flags &      FRAG_POSIX) {
                        frag->flags |=  FRAG_POSIX;
                        flags ^=        FRAG_POSIX;
                } else {
                        free(frag);
                        fprintf(stderr, "frag_init: unidentified flags given\n");
                        return NULL;
                }
        }

        frag->index = 0;
        frag->chr = 0;
        frag->id = FRAG_ERR_INTERNAL;   /* this is here in case something actually goes wrong */
        frag->type = 0;
        frag->arg = NULL;
        frag->argc = argc;
        frag->argv = argv;
        frag->fops = fops;

        return frag;
}

int frag_parse(FRAG *frag)
{
        /* clear frag->* set by previous call */
        frag->id = FRAG_ERR_INTERNAL;   /* this will be later changed to the real value and
                                         * if not, it already contains the error-value */
        frag->type = FRAG_ENABLE;       /* only needs to be changed for DISABLEs */
        frag->arg = NULL;               /* NULL by default */

        if (frag->chr) {                /* the previous was a short option */
                if (frag->argv[frag->index][frag->chr + 1] == '\0'
                && !(frag->flags & FRAG_DISABLE_GROUPS)) {
                        frag->chr = 0;
                        frag->index++;
                } else                  /* there's still stuff left */
                        frag->chr++;    /* move to the next character */
        } else                          /* move to the next argument */
                frag->index++;

        if (frag->index == frag->argc)  /* everything is parsed */
                return 0;

        /* END_SCAN won't be enabled unless the program takes args */   /* regex */
        if (frag->flags & _FRAG_END_SCAN)
                _frag_do_opt(frag, frag->prog);
        else if (frag->argv[frag->index][0] == '-') {                   /* -.* */
                if (frag->argv[frag->index][1] == '\0')                 /* - */
                        _frag_parse_bare(frag);
                else if (frag->argv[frag->index][1] == '-'
                &&       frag->argv[frag->index][2] == '\0') {          /* -- */
                        if (!(frag->flags & FRAG_DISABLE_DOUBLEDASH)) {
                                frag->flags |= _FRAG_END_SCAN;
                                /* parse the next so we'll have something to return */
                                return frag_parse(frag);
                        } else
                                _frag_parse_bare(frag);
                } else if (frag->argv[frag->index][1] == '-'            /* --.+ */
                &&       !(frag->flags & FRAG_DISABLE_LONG_OPTIONS))
                        _frag_parse_lopt(frag, frag->argv[frag->index] + 2);
                else {                                                  /* -[^-].+ */
                        if (frag->flags & FRAG_ENABLE_ONEDASH_LONG
                        && !(frag->flags & FRAG_DISABLE_LONG_OPTIONS)) {
                                _frag_parse_lopt(frag, frag->argv[frag->index] + 1);
                                if (frag->id == FRAG_ERR_UFO)
                                        _frag_parse_sopt(frag, frag->argv[frag->index] + 1);
                        } else if (!(frag->flags & FRAG_DISABLE_SHORT_OPTIONS))
                                _frag_parse_sopt(frag, frag->argv[frag->index] + 1);
                        else
                                _frag_parse_bare(frag);
                }
        } else if (frag->argv[frag->index][0] == '+'                    /* \+.* */
        &&         frag->argv[frag->index][1] != '\0'
        &&       !(frag->flags & FRAG_DISABLE_NEGATION_OPTIONS))
                _frag_parse_nopt(frag, frag->argv[frag->index] + 1);
        else                                                            /* ^[-+].* */
                _frag_parse_bare(frag);

        if (frag->id < 0)
                _frag_print_error(frag);

        return frag->argc - frag->index;
}

static inline void _frag_parse_bare(FRAG *frag)
{
        if (frag->prog != -1) {
                if (frag->flags & FRAG_POSIX)
                        frag->flags |= _FRAG_END_SCAN;
                _frag_do_opt(frag, frag->prog);
        } else
                frag->id = FRAG_ERR_BAREWORD;
}

static void _frag_parse_sopt(FRAG *frag, const char *str)
{
        int i;

        if (!frag->chr)         /* first character of this group */
                frag->chr++;

        for (i = 0; frag->fops[i].type != _FRAG_EOA; i++)
                if (str[frag->chr - 1] == frag->fops[i].chr) {
                        _frag_do_opt(frag, i);
                        return;
                }

        frag->id = FRAG_ERR_UFO;
}

static void _frag_parse_lopt(FRAG *frag, const char *str)
{
        int i, n;
        const char *tmp = NULL;

        if (!strncmp(str, "no-", 3))    /* check for "no-" in the arg */
                for (i = 0; frag->fops[i].type != _FRAG_EOA; i++)
                        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_NEG
                        &&   frag->fops[i].str != NULL)
                                if (!strcmp(str + 3, frag->fops[i].str)) {
                                        _frag_do_nopt(frag, i);
                                        return;
                                }

        n = strlen(str);
        if (!(frag->flags & FRAG_DISABLE_EQUALS_LONG)) {
                tmp = strchr(str, '=');
                if (tmp != NULL) {
                        n -= strlen(tmp);
                        if (n == 0) {                                   /* --=.* */
                                frag->id = FRAG_ERR_SYNTAX;
                                return;
                        }
                }
        }

        for (i = 0; frag->fops[i].type != _FRAG_EOA; i++)
                if (frag->fops[i].str != NULL)
                        if (!strncmp(str, frag->fops[i].str, n)) {
                                if ((frag->fops[i].type & _FRAG_TYPES) != FRAG_ARG
                                &&  (frag->fops[i].type & _FRAG_TYPES) != FRAG_OPT_ARG
                                &&  tmp != NULL) {
                                        frag->id = FRAG_ERR_UNWANTED_ARG;
                                        return;
                                }
                                _frag_do_opt(frag, i);
                                return;
                        }

        frag->id = FRAG_ERR_UFO;
}

static void _frag_parse_nopt(FRAG *frag, const char *str)
{
        int i;

        if (frag->chr == 0)     /* first character of this group */
                frag->chr++;

        for (i = 0; frag->fops[i].type != _FRAG_EOA; i++)
                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_NEG)
                        if (str[frag->chr - 1] == frag->fops[i].chr) {
                                _frag_do_nopt(frag, i);
                                return;
                        }

        frag->id = FRAG_ERR_UFO;
}

static void _frag_do_opt(FRAG *frag, int i)
{
        frag->id = frag->fops[i].id;

        if (frag->fops[i].type == _FRAG_PROG) {
                frag->arg = frag->argv[frag->index];
                return;
        }

        if (frag->chr)          /* it's a short one */
                _frag_do_sopt(frag, i);
        else                    /* it's a long one */
                _frag_do_lopt(frag, i);
}

static void _frag_do_sopt(FRAG *frag, int i)
{
        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG
        ||  (frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                if (frag->flags & FRAG_ENABLE_NO_SPACE_SHORT) {
                        if (frag->argv[frag->index][frag->chr + 1] != '\0') {
                                frag->arg = frag->argv[frag->index] + frag->chr + 1;
                                frag->chr = strlen(frag->argv[frag->index]) - 1;
                        } else if (!(frag->flags & FRAG_ENABLE_SPACED_LONG)) {
                                frag->arg = frag->argv[frag->index] + frag->chr + 1;
                                frag->chr = strlen(frag->argv[frag->index]) - 1;
                        } else {
                                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG
                                &&   (frag->argv[frag->index][0] == '-'
                                ||    frag->argv[frag->index][0] == '+'))
                                        return;
                                frag->index++;
                                frag->chr = 0;
                                if (frag->index == frag->argc) {
                                        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG) {
                                                frag->id = FRAG_ERR_ARG_MISSING;
                                                frag->index--;
                                        }
                                        return;
                                }
                                frag->arg = frag->argv[frag->index];
                        }
                } else {
                        if (frag->argv[frag->index][frag->chr + 1] != '\0') {
                                if (!((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG))
                                        frag->id = FRAG_ERR_ORDER;
                                return;
                        }
                        frag->index++;
                        frag->chr = 0;
                        if (frag->index == frag->argc) {
                                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG) {
                                        frag->id = FRAG_ERR_ARG_MISSING;
                                        frag->index--;
                                }
                                return;
                        }
                        frag->arg = frag->argv[frag->index];
                }
        }
}

static void _frag_do_lopt(FRAG *frag, int i)
{
        char *equals = strchr(frag->argv[frag->index], '=');

        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG
        ||  (frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                if (frag->flags & FRAG_DISABLE_EQUALS_LONG
                || ((frag->flags & FRAG_ENABLE_SPACED_LONG) && equals == NULL)) {
                        if (frag->index + 1 == frag->argc) {
                                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG)
                                        frag->id = FRAG_ERR_ARG_MISSING;
                                return;
                        }
                        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG
                        &&   (frag->argv[frag->index + 1][0] == '-'
                        ||    frag->argv[frag->index + 1][0] == '+'))
                                return;
                        frag->index++;
                        frag->chr = 0;
                        frag->arg = frag->argv[frag->index];
                } else {
                        equals++;       /* skip the '=' */
                        frag->arg = equals;
                }
        }
}

static void _frag_do_nopt(FRAG *frag, int i)
{       /* negation options can't take arguments so this is pretty simple */
        frag->id = frag->fops[i].id;
        frag->type = FRAG_DISABLE;
}

void frag_usage(FRAG *frag)
{
        int i, col;

        printf("usage: %s [OPTIONS]", frag->argv[0]);
        if (frag->prog != -1) {
                if (frag->fops[frag->prog].arg != NULL)
                        printf(" %s\n", frag->fops[frag->prog].arg);
                if (frag->fops[frag->prog].desc != NULL) {
                        _frag_print_wrapped(frag->fops[frag->prog].desc, 0, 0);
                        putchar('\n');
                }
        }
        putchar('\n');

        for (i = 0, col = 0; frag->fops[i].type != _FRAG_EOA; i++) {
                if (frag->fops[i].type != _FRAG_PROG
                && !(frag->fops[i].type & FRAG_HIDDEN)
                && !(frag->fops[i].type & FRAG_ALIAS)) {
                        col = _frag_print_options(frag, i, col);
                        /* we want some space before the description */
                        if (col >= INDENT_DESC - 1) {
                                putchar('\n');
                                col = 0;
                        }
                        while (col < INDENT_DESC) {
                                putchar(' ');
                                col++;
                        }
                        _frag_print_wrapped(frag->fops[i].desc, INDENT_DESC, col);
                        putchar('\n');
                        col = 0;
                }
        }
        putchar('\n');
}

static inline int _frag_print_options(FRAG *frag, const int i, int col)
{
        int j;

        while (col < INDENT_OPTIONS) {
                putchar(' ');
                col++;
        }
        if (frag->fops[i].chr != '\0') {
                col = _frag_print_sopt(frag, i, col);
                for (j = i + 1; frag->fops[j].type & FRAG_ALIAS; j++)
                        col = _frag_print_sopt(frag, j, col);
        }
        if (frag->fops[i].str != NULL) {
                col = _frag_print_lopt(frag, i, col);
                for (j = i + 1; frag->fops[j].type & FRAG_ALIAS; j++)
                        col = _frag_print_lopt(frag, j, col);
        }

        return col;
}

static int _frag_print_sopt(FRAG *frag, const int i, int col)
{
        int req = 2;    /* "-c" is required by everything */

        if (col != INDENT_OPTIONS) {
                printf(", ");
                col += 2;
        }

        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG              /* count how much */
        ||  (frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {       /* space this option */
                if (!(frag->flags & FRAG_DISABLE_SPACED_SHORT))         /* needs */
                        req++;                                  /* the space */
                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG)
                        req += 2;                               /* '[' and ']' */
                if (frag->fops[i].arg != NULL)
                        req += strlen(frag->fops[i].arg);       /* the length of the string */
                else
                        req += 5;                               /* "VALUE" */
        }

        if (col + req > SCREEN_WIDTH - 1) {
                putchar('\n');
                for (col = 0; col < INDENT_OPTIONS; col++)
                        putchar(' ');
        }

        printf("-%c", frag->fops[i].chr);
        col += 2;
        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG
        ||  (frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                if (!(frag->flags & FRAG_DISABLE_SPACED_SHORT)) {
                        putchar(' ');
                        col++;
                }
                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                        putchar('[');
                        col++;
                }
                if (frag->fops[i].arg != NULL) {
                        printf("%s", frag->fops[i].arg);
                        col += strlen(frag->fops[i].arg);
                } else {
                        printf("VALUE");
                        col += 5;
                }
                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                        putchar(']');
                        col++;
                }
        } else if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_NEG) {
                if (col + 4 > SCREEN_WIDTH - 1) {
                        putchar(',');
                        putchar('\n');
                        for (col = 0; col < INDENT_OPTIONS; col++)
                                putchar(' ');
                        printf("+%c", frag->fops[i].chr);
                        col += 2;
                } else {
                        printf(", +%c", frag->fops[i].chr);
                        col += 4;
                }
        }

        return col;
}

static int _frag_print_lopt(FRAG *frag, const int i, int col)
{       /* TODO: think about the possibility of doing the wrapping thing with
         * sprintf() & _frag_print_wrapped() */
        int req = 2;    /* everything requires "--" */

        if (col != INDENT_OPTIONS) {
                printf(", ");
                col += 2;
        }

        req += strlen(frag->fops[i].str);
        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG
        ||  (frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                req++;                                          /* '=' or ' ' */
                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG)
                        req += 2;                               /* '[' and ']' */
                if (frag->fops[i].arg != NULL)
                        req += strlen(frag->fops[i].arg);       /* length of string */
                else
                        req += 5;                               /* "VALUE" */
        }

        if (col + req > SCREEN_WIDTH - 1) {
                putchar('\n');
                for (col = 0; col < INDENT_OPTIONS; col++)
                        putchar(' ');
        }

        printf("--%s", frag->fops[i].str);
        col += 2 + strlen(frag->fops[i].str);
        if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_ARG
        ||  (frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                        putchar('[');
                        col++;
                }
                if (!(frag->flags & FRAG_DISABLE_EQUALS_LONG))
                        putchar('=');
                else
                        putchar(' ');
                col++;
                if (frag->fops[i].arg != NULL) {
                        printf("%s", frag->fops[i].arg);
                        col += strlen(frag->fops[i].arg);
                } else {
                        printf("VALUE");
                        col += 5;
                }
                if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_OPT_ARG) {
                        putchar(']');
                        col++;
                }
        } else if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_NEG) {
                if (col + 7 + strlen(frag->fops[i].str) > SCREEN_WIDTH - 1) {
                        putchar(',');
                        putchar('\n');
                        for (col = 0; col < INDENT_OPTIONS; col++)
                                putchar(' ');
                        printf("--no-%s", frag->fops[i].str);
                        col += 5 + strlen(frag->fops[i].str);
                } else {
                        printf(", --no-%s", frag->fops[i].str);
                        col += 7 + strlen(frag->fops[i].str);
                }
        }

        return col;
}

static int _frag_print_wrapped(const char *str, const int indent, int col)
{       /* TODO: break at dash? */
        int i, tail, head;

        for (tail = head = 0; str[tail] != '\0'; head++) {
                if (str[head] == ' '  || str[head] == '\t'
                ||  str[head] == '\n' || str[head] == '\0') {
                        if (col + (head - tail) >= SCREEN_WIDTH - 1) {
                                if (col != indent) {
                                        putchar('\n');
                                        for (col = 0; col < indent; col++)
                                                putchar(' ');
                                }
                                while (str[tail] == ' ')        /* skip the whitespace */
                                        tail++;
                        }
                        for (i = 0; tail + i < head; i++) {
                                putchar(str[tail + i]);
                                if (col == SCREEN_WIDTH - 1) {
                                        putchar('\n');
                                        for (col = 0; col < indent; col++)
                                                putchar(' ');
                                }
                                if (str[tail + i] == '\t')
                                        col += 8 - (col % 8);
                                else if (str[tail + i] == '\n') {
                                        for (col = 0; col < indent; col++)
                                                putchar(' ');
                                } else
                                        col++;
                        }
                        tail = head;
                }
        }

        return col;
}

const char *frag_err(FRAG *frag)
{
        switch (frag->id) {
                case FRAG_ERR_UFO:
                        return "unidentified option";
                case FRAG_ERR_BAREWORD:
                        return "unexpected bareword";
                case FRAG_ERR_GROUP:
                        return "grouping short options is not allowed";
                case FRAG_ERR_ORDER:
                        return "invalid ordering of options";
                case FRAG_ERR_UNWANTED_ARG:
                        return "argument given for an option that doesn't take one";
                case FRAG_ERR_ARG_MISSING:
                        return "mandatory argument missing";
                case FRAG_ERR_SYNTAX:
                        return "syntax error";
                case FRAG_ERR_INTERNAL:
                        return "internal error";
                default:
                        if (frag->id >= 0)
                                return "no error";
                        else
                                return "unrecognized error";
        }
}

static void _frag_print_error(FRAG *frag)
{
        fprintf(stderr, "error: ");
        switch (frag->id) {
                case FRAG_ERR_UFO:
                        if (frag->chr) {
                                if (frag->argv[frag->index][0] == '-') {
                                        fprintf(stderr, "unidentified short option \"-%c\"",
                                                                frag->argv[frag->index][frag->chr]);
                                        if (frag->chr != 1
                                        ||  frag->argv[frag->index][frag->chr + 1] != '\0')
                                                fprintf(stderr, " in group \"%s\"",
                                                                frag->argv[frag->index]);
                                } else {
                                        fprintf(stderr, "unidentified negation option \"+%c\"",
                                                                frag->argv[frag->index][frag->chr]);
                                        if (frag->chr != 1
                                        ||  frag->argv[frag->index][frag->chr + 1] != '\0')
                                                fprintf(stderr, " in group \"%s\"",
                                                                frag->argv[frag->index]);
                                }
                                fputc('\n', stderr);
                        } else
                                fprintf(stderr, "unidentified long option %s\n",
                                                        frag->argv[frag->index]);
                        return;
                case FRAG_ERR_BAREWORD:
                        fprintf(stderr, "unexpected bareword \"%s\"\n", frag->argv[frag->index]);
                        return;
                case FRAG_ERR_GROUP:
                        fprintf(stderr, "groups of short options are not allowed \"%s\"\n",
                                                        frag->argv[frag->index]);
                        return;
                case FRAG_ERR_ORDER:
                        if (frag->argv[frag->index + 1])
                                fprintf(stderr, "invalid ordering of options in \"%s %s\"\n",
                                        frag->argv[frag->index], frag->argv[frag->index + 1]);
                        else
                                fprintf(stderr, "invalid ordering of options in \"%s\" followed "
                                        "by a missing argument\n", frag->argv[frag->index]);

                        return;
                case FRAG_ERR_UNWANTED_ARG:
                        fprintf(stderr, "argument given for an option that doesn't take one in "
                                        "\"%s\"\n", frag->argv[frag->index]);
                        return;
                case FRAG_ERR_ARG_MISSING:
                        fprintf(stderr, "\"%s\" is missing an argument\n",
                                                        frag->argv[frag->index]);
                        return;
                case FRAG_ERR_SYNTAX:
                        fprintf(stderr, "invalid syntax in \"%s\"\n", frag->argv[frag->index]);
                        return;
                case FRAG_ERR_INTERNAL:
                        fprintf(stderr, "internal error");
                        return;
                default:
                        fprintf(stderr, "unidentified flying error #%d\n", frag->id);
                        return;
        }
}

void frag_free(FRAG *frag)      /* maybe a macro would be better? nah. could break binary */
{                               /* compatibility if we need an actual function for this later */
        free(frag);
}
