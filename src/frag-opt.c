/*----------------------------------------------------------------------------*\
    frag-opt - frag-opt rather ain't getopt - or popt, technically
    Version 0.5.4

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
 */

/* TODO POSIX ??
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

#define _FRAG_BUILDE
#include <frag-opt.h>

/* helper functions */
inline void	_frag_parse_bare(FRAG * frag);
void		_frag_parse_sopt(FRAG * frag, const char * str);
void		_frag_parse_lopt(FRAG * frag, const char * str);
void		_frag_parse_nopt(FRAG * frag, const char * str);
void		_frag_do_opt(FRAG * frag, int i);
void		_frag_do_sopt(FRAG * frag, int i);
void		_frag_do_lopt(FRAG * frag, int i);
void		_frag_do_nopt(FRAG * frag, int i);
int		_frag_print_options(FRAG * frag, int i, int col);
int		_frag_print_sopt(FRAG * frag, int i, int col);
int		_frag_print_lopt(FRAG * frag, int i, int col);
int		_frag_print_wrapped(const char * str, int indent, int col);
void		_frag_print_error(FRAG * frag);

FRAG * frag_init(frag_option * fops, int argc, char ** argv, int flags)
{
	FRAG * frag;
	int i, j;

	if (argc == 0 || argv == NULL || fops == NULL)
		return NULL;

	/* check that there's no duplicate id's with different flags */
	for (i = 0; fops[i + 1].type != _FRAG_EOA; i++)
		for (j = i + 1; fops[j].type != _FRAG_EOA; j++)
			if (fops[i].id == fops[j].id
			&& (fops[i].type & _FRAG_TYPES) != (fops[j].type & _FRAG_TYPES))
				return NULL;

	frag = (FRAG *) malloc(sizeof(FRAG));
	if (frag == NULL)
		return NULL;

	/* see if the program takes args */
	for (i = 0, j = 0; fops[i].type != _FRAG_EOA; i++)
		if (fops[i].type == _FRAG_PROG) {
			frag->prog = i;
			j++;
		}

	if (j == 0)		/* no _FRAG_PROG's */
		frag->prog = -1;
	else if (j > 1) {	/* 1+ _FRAG_PROG's */
		fprintf(stderr, "frag_init: too many FRAG_PROGRAMs\n");
		return NULL;
	}

	while (flags) {		/* we don't want no invalid flags in here */
		if (flags & FRAG_DISABLE_DOUBLEDASH) {
			frag->flags |= FRAG_DISABLE_DOUBLEDASH;
			flags ^= FRAG_DISABLE_DOUBLEDASH;
		} else if (flags & FRAG_DISABLE_CLUSTERS) {
			frag->flags |= FRAG_DISABLE_CLUSTERS;
			flags ^= FRAG_DISABLE_CLUSTERS;
		} else if (flags & FRAG_DISABLE_EQUALS_LONG) {
			frag->flags |= FRAG_DISABLE_EQUALS_LONG;
			flags ^= FRAG_DISABLE_EQUALS_LONG;
		} else if (flags & FRAG_ENABLE_SPACED_LONG) {
			frag->flags |= FRAG_ENABLE_SPACED_LONG;
			flags ^= FRAG_ENABLE_SPACED_LONG;
		} else if (flags & FRAG_DISABLE_SPACED_SHORT) {
			frag->flags |= FRAG_DISABLE_SPACED_SHORT;
			flags ^= FRAG_DISABLE_SPACED_SHORT;
		} else if (flags & FRAG_ENABLE_NO_SPACE_SHORT) {
			frag->flags |= FRAG_ENABLE_NO_SPACE_SHORT;
			flags ^= FRAG_ENABLE_NO_SPACE_SHORT;
		} else if (flags & FRAG_DISABLE_LONG_OPTIONS) {
			frag->flags |= FRAG_DISABLE_LONG_OPTIONS;
			flags ^= FRAG_DISABLE_LONG_OPTIONS;
		} else if (flags & FRAG_DISABLE_SHORT_OPTIONS) {
			frag->flags |= FRAG_DISABLE_SHORT_OPTIONS;
			flags ^= FRAG_DISABLE_SHORT_OPTIONS;
		} else if (flags & FRAG_DISABLE_NEGATION_OPTIONS) {
			frag->flags |= FRAG_DISABLE_NEGATION_OPTIONS;
			flags ^= FRAG_DISABLE_NEGATION_OPTIONS;
		} else if (flags & FRAG_ENABLE_ONEDASH_LONG) {
			frag->flags |= FRAG_ENABLE_ONEDASH_LONG;
			flags ^= FRAG_ENABLE_ONEDASH_LONG;
		} else if (flags & FRAG_QUIET) {
			frag->flags |= FRAG_QUIET;
			flags ^= FRAG_QUIET;
		} else {
			free(frag);
			fprintf(stderr, "frag_init: unidentified flags given\n");
			return NULL;
		}
	}

	frag->index = 0;
	frag->chr = 0;
	frag->id = -1;
	frag->type = 0;
	frag->arg = NULL;
	frag->argc = argc;
	frag->argv = argv;
	frag->fops = fops;

	return frag;
}

int frag_parse(FRAG * frag)
{
	/* clear frag->* set by previous call */
	frag->id = -1;
	frag->type = 1;
	frag->arg = NULL;

	if (frag->chr) {	/* the previous was a short [negation] option */
		if (frag->argv[frag->index][frag->chr + 1] == '\0'
		&& !(frag->flags & FRAG_DISABLE_CLUSTERS)) {
				/* the previous was the last of it's cluster */
			frag->chr = 0;
			frag->index++;
		} else		/* there's still stuff left */
			frag->chr++;
	} else			/* move to the next argument */
		frag->index++;

	if (frag->index == frag->argc) {	/* we've parsed everything */
		frag->id = FRAG_MAGIC_NO_ERR;
		return 0;
	}

	/* we won't enable DOUBLEDASH_ENCOUNTERED unless the program takes args */
	if (frag->flags & _FRAG_DOUBLEDASH_ENCOUNTERED)
		_frag_do_opt(frag, frag->prog);
	else if (frag->argv[frag->index][0] == '-') {			/* -.* */
		if (frag->argv[frag->index][1] == '\0')			/* - */
			_frag_parse_bare(frag);
		else if (frag->argv[frag->index][1] == '-'
		&&	   frag->argv[frag->index][2] == '\0') {	/* -- */
			if (!(frag->flags & FRAG_DISABLE_DOUBLEDASH)) {
				frag->flags |= _FRAG_DOUBLEDASH_ENCOUNTERED;
				frag_parse(frag);	/* gotta parse the next */
			} else
				_frag_parse_bare(frag);
		} else if (frag->argv[frag->index][1] == '-'		/* --.+ */
		&&	!(frag->flags & FRAG_DISABLE_LONG_OPTIONS))
			_frag_parse_lopt(frag, frag->argv[frag->index] + 2);
		else {							/* -[^-]+ */
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
	} else if (frag->argv[frag->index][0] == '+'			/* \+.* */
	&&	   frag->argv[frag->index][1] != '\0'
	&&	 !(frag->flags & FRAG_DISABLE_NEGATION_OPTIONS))
		_frag_parse_nopt(frag, frag->argv[frag->index] + 1);
	else							/* ^[-+].* */
		_frag_parse_bare(frag);

	if (frag->id < 0)
		_frag_print_error(frag);

	return frag->argc - frag->index;
}

inline void _frag_parse_bare(FRAG * frag)
{
	if (frag->prog != -1)                                      /* ^[-+].* */
		_frag_do_opt(frag, frag->prog);
	else
		frag->id = FRAG_ERR_BAREWORD;
}

void _frag_parse_sopt(FRAG * frag, const char * str)
{
	int i;

	if (!frag->chr)		/* first of this cluster */
		frag->chr++;

	for (i = 0; frag->fops[i].type != _FRAG_EOA; i++)
		if (str[frag->chr - 1] == frag->fops[i].chr) {
			_frag_do_opt(frag, i);
			return;
		}

	frag->id = FRAG_ERR_UFO;
}

void _frag_parse_lopt(FRAG * frag, const char * str)
{
	int i, n;
	const char * tmp;

	if (!strncmp(str, "no-", 3))	/* check for "no-" in the arg */
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
			if (n == 0) {					/* --=.* */
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

void _frag_parse_nopt(FRAG * frag, const char * str)
{
	int i;

	if (!frag->chr)	/* first of this cluster */
		frag->chr++;

	for (i = 0; frag->fops[i].type != _FRAG_EOA; i++)
		if ((frag->fops[i].type & _FRAG_TYPES) == FRAG_NEG)
			if (str[frag->chr - 1] == frag->fops[i].chr) {
				_frag_do_nopt(frag, i);
				return;
			}

	frag->id = FRAG_ERR_UFO;
}

void _frag_do_opt(FRAG * frag, int i)
{
	frag->id = frag->fops[i].id;
	frag->type = FRAG_ENABLE;

	if (frag->fops[i].type == _FRAG_PROG) {
		frag->arg = frag->argv[frag->index];
		return;
	}

	if (frag->chr)		/* it's a short one */
		_frag_do_sopt(frag, i);
	else 			/* it's a long one */
		_frag_do_lopt(frag, i);
}

void _frag_do_sopt(FRAG * frag, int i)
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

void _frag_do_lopt(FRAG * frag, int i)
{
	char * equals = strchr(frag->argv[frag->index], '=');

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
			equals++;	/* skip the '=' */
			frag->arg = equals;
		}
	}
}

void _frag_do_nopt(FRAG * frag, int i)
{
	frag->id = frag->fops[i].id;
	frag->type = FRAG_DISABLE;
}

void frag_usage(FRAG * frag)
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
			putchar('\t');
			col = _frag_print_options(frag, i, 8);
			if (col >= 22) {	/* we want some space before the description */
				printf("\n\t\t\t");
				col = 24;
			} else {
				while (col < 24) {
					putchar('\t');
					col += 8 - (col % 8);
				}
			}
			_frag_print_wrapped(frag->fops[i].desc, col, col);
			putchar('\n');
		}
	}
	putchar('\n');
}

int _frag_print_options(FRAG * frag, int i, int col)
{
	if (frag->fops[i].chr != '\0') {
		col = _frag_print_sopt(frag, i, col);
		if (frag->fops[i + 1].type & FRAG_ALIAS)
			col = _frag_print_sopt(frag, i + 1, col);
	}
	if (frag->fops[i].str != NULL) {
		col = _frag_print_lopt(frag, i, col);
		if (frag->fops[i + 1].type & FRAG_ALIAS)
			col = _frag_print_lopt(frag, i + 1, col);
	}

	return col;
}

int _frag_print_sopt(FRAG * frag, int i, int col)
{	/* TODO: take col into account when printing */
	if (col != 8) {
		printf(", ");
		col += 2;
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
		printf(", +%c", frag->fops[i].chr);
		col += 4;
	}

	return col;
}

int _frag_print_lopt(FRAG * frag, int i, int col)
{	/* TODO: take col into account when printing */
	if (col != 8) {
		printf(", ");
		col += 2;
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
		printf(", --no-%s", frag->fops[i].str);
		col += 5 + strlen(frag->fops[i].str);
	}

	return col;
}

int _frag_print_wrapped(const char * str, int indent, int col)
{
	int i, last, ahead, j;

	for (i = 0, last = 0, ahead = 0; str[last] != '\0'; ahead++) {
		if (str[ahead] == ' ' ||  str[ahead] == '\t'
		||  str[ahead] == '\n' ||  str[ahead] == '\0') {
			if (!(col + (ahead - last) < 80)) {
				if (col != indent) {
					putchar('\n');
					for (j = 0; j < indent; j++)
						putchar(' ');
					col = indent;
				}
				while (str[last] == ' ')
					last++;		/* skip the whitespace */
			}
			for (i = 0; i < ahead - last; i++) {
				putchar(str[last + i]);
				if (!(col < 80)) {
					putchar('\n');
					for (j = 0; j < indent; j++)
						putchar(' ');
					col = indent;
				}
				if (str[last + i] == '\t')
					col += 8 - (col % 8);
				else if (str[last + i] == '\n') {
					for (j = 0; j < indent; j++)
						putchar(' ');
					col = indent;
				} else
					col++;
			}
			last = ahead;
		}
	}

	return col;
}

const char * frag_err(FRAG * frag)
{
	switch (frag->id) {
		case FRAG_ERR_UFO:
			return "unidentified option";
		case FRAG_ERR_BAREWORD:
			return "unexpected bareword";
		case FRAG_ERR_CLUSTER:
			return "invalid cluster of short options";
		case FRAG_ERR_ORDER:
			return "invalid ordering of options";
		case FRAG_ERR_UNWANTED_ARG:
			return "argument given for an option that doesn't take one";
		case FRAG_ERR_ARG_MISSING:
			return "mandatory argument missing";
		case FRAG_ERR_SYNTAX:
			return "syntax error";
		default:
			return "unrecognized error";
	}
}

void _frag_print_error(FRAG * frag)
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
						fprintf(stderr, " in cluster \"%s\"",
								frag->argv[frag->index]);
				} else {
					fprintf(stderr, "unidentified negation option \"+%c\"",
								frag->argv[frag->index][frag->chr]);
					if (frag->chr != 1
					||  frag->argv[frag->index][frag->chr + 1] != '\0')
						fprintf(stderr, " in cluster \"%s\"",
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
		case FRAG_ERR_CLUSTER:
			fprintf(stderr, "invalid cluster of short options in \"%s\"\n",
							frag->argv[frag->index]);
			return;
		case FRAG_ERR_ORDER:
			fprintf(stderr, "invalid ordering of options in \"%s %s\"\n",
					frag->argv[frag->index], frag->argv[frag->index + 1]);
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
		default:
			fprintf(stderr, "unidentified flying error #%d\n", frag->id);
			return;
	}
}

void frag_free(FRAG * frag)	/* maybe a macro would be better? nah. could break binary */
{				/* compatibility if we need an actual function for this later */
	free(frag);
}
