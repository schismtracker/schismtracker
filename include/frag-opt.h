/*----------------------------------------------------------------------------*\
	header for frag-opt 0.5.5
	frag-opt is distributed under the GNU LGPL
\*----------------------------------------------------------------------------*/

#include <stdio.h>	/* we need this for NULL */

#ifndef _FRAG_OPT_H
# define _FRAG_OPT_H
# ifdef __cplusplus
extern "C" {
# endif /* __cplusplus */

typedef struct {
	signed int id;		/* the id-number */
	char chr;		/* short option */
	const char *str;	/* long option */
	int type;		/* type */
	const char *arg;	/* argument name */
	const char *desc;	/* description */
} frag_option;

typedef struct {
	int index;		/* where are we going */
	int chr;		/* more specificly */
	int id;			/* the id-number of the option (or error) */
	int type;		/* the type the option was (enable or disable) */
	int flags;		/* dem flags */
	int argc;		/* number of args */
	int prog;		/* index (in fops) of program */
	char *arg;		/* the pointer to the arg */
	char **argv;		/* pointer to the args */
	frag_option *fops;	/* the options */
} FRAG;

/* frag->flags or flags passed to frag_init() */
# define FRAG_DISABLE_DOUBLEDASH	0x0001
# define FRAG_DISABLE_CLUSTERS		0x0002
# define FRAG_DISABLE_GROUPS		0x0002
# define FRAG_DISABLE_EQUALS_LONG	0x0004
# define FRAG_ENABLE_SPACED_LONG	0x0008
# define FRAG_DISABLE_SPACED_SHORT	0x0010
# define FRAG_ENABLE_NO_SPACE_SHORT	0x0020
# define FRAG_DISABLE_LONG_OPTIONS	0x0040
# define FRAG_DISABLE_SHORT_OPTIONS	0x0080
# define FRAG_DISABLE_NEGATION_OPTIONS	0x0100
# define FRAG_ENABLE_ONEDASH_LONG	0x0200
# define FRAG_QUIET			0x0400
# define FRAG_POSIX			0x0800
/* this just tells that we've parsed a "--" or an argument to the program and
 * frag-opt is operating in posix mode */
# define _FRAG_END_SCAN			0x8000

/* fops->type or the type passed with options */
# define _FRAG_TYPES			0x0003
# define FRAG_ARG			1
# define FRAG_OPT_ARG			2
# define FRAG_NEG			3
	/* visibility control */
# define FRAG_HIDDEN			0x0010
# define FRAG_ALIAS			0x0020
	/* end of array and the program */
# define _FRAG_EOA			0x8000
# define _FRAG_PROG			0x4000

/* frag->type or enabled or disabled */
# define FRAG_ENABLE			1
# define FRAG_DISABLE			0

/* frag->id errors */
	/* "--something-we-know-nothing-about" (unidentified flying option) */
# define FRAG_ERR_UFO			-1
	/* "a_program_that_takes_no_arg bla" */
# define FRAG_ERR_BAREWORD		-2
	/* "-xzvf" when groups are disabled */
# define FRAG_ERR_GROUP			-3
	/* "-xfv file" when spaced are and no-spaced arguments aren't allowed for sopts */
# define FRAG_ERR_ORDER			-4
	/* "--long-option=value" on a long option that doesn't take arguments */
# define FRAG_ERR_UNWANTED_ARG		-5
	/* "program --an-option-with-mandatory-argument" */
# define FRAG_ERR_ARG_MISSING		-6
	/* contains: "--=[.]*" */
# define FRAG_ERR_SYNTAX		-7
	/* we have fucked up somehow */
# define FRAG_ERR_INTERNAL		-1024

/* real macros */
# define FRAG_PROGRAM		     '\0', NULL,   _FRAG_PROG
# define FRAG_END_ARRAY		0,   '\0', NULL,   _FRAG_EOA,  NULL, NULL

FRAG       *frag_init(frag_option *fops, int argc, char **argv, int flags);
int         frag_parse(FRAG *frag);
const char *frag_err(FRAG *frag);
void        frag_usage(FRAG *frag);
void        frag_free(FRAG *frag);

# ifdef __cplusplus
}
# endif /* __cplusplus */
#endif /* _FRAG_OPT_H */
