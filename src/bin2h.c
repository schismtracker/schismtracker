/*
 * bin2h - Utility to include binary files in source code
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include "util.h"

#include <ctype.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define BIN2H_VERSION "0.6"

/* --------------------------------------------------------------------- */

/* TODO:
 * - char format ('a', 'b', 'c') with dec/hex/oct for non-ascii chars
 * - option to use three or four spaces for decimal (four looks nicer
 *   with other bin2h output in hex/octal format, three occupies less
 *   space) */


enum { HEXADECIMAL, DECIMAL, OCTAL };
enum { LINE_BYTE, LAST_BYTE, POSITION_COMMENT };

const char *format_strings[3][3] = {
	{"0x%02x, ", "0x%02x,", "/* 0x%08x - 0x%08x */ "},
	{"%4d, ", "%4d,", "/* %10d - %10d */ "},
	{"%04o, ", "%04o,", "/* %010o - %010o */ "},
};

/* these can be changed by command-line options */
int print_mode = HEXADECIMAL;
int max_width = 16;	/* number of chars to write per line */
int start_pos = 0;	/* where to start in the file */
char *variable_name = NULL;
int position_comments = 0;
const char *output_filename = "-";
const char *data_type = "static unsigned const char";
#define VARIABLE_CHARS "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"

/* --------------------------------------------------------------------- */

static void print_version(FILE *f)
{
	static int did_this = 0;

	if (did_this)
		return;
	did_this = 1;

	fprintf(f, "bin2h v" BIN2H_VERSION "\n"
		"Written by chisel <someguy@here.is>.\n"
		"\n"
		"Copyright (C) 2004-2005 chisel.\n"
		"This is free software; see the source for copying conditions.  There is NO\n"
		"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"
		);
}

static void print_help(FILE *f)
{
	static int did_this = 0;

	if (did_this)
		return;
	did_this = 1;

	print_version(f);
	fprintf(f, "\n"
		"Usage: bin2h [<options>] [infile]\n"
		"\t-D -O -X     output mode: decimal, octal, hex\n"
		"\t-P -p        enable/disable position comments\n"
		"\t-o <file>    name of output file (default stdout)\n"
		"\t-w <num>     maximum (input) characters per line\n"
		"\t-s <num>     starting byte position\n"
		"\t-n <var>     variable name\n"
		"\t-t <str>     data type (e.g. \"static const char\")\n"
		"\t-v           print version (look about ten lines up)\n"
		"\t-h           print help (you're reading it now)\n");
}

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

static void handle_options(int argc, char **argv)
{
	int opt, should_exit = -1;

	while ((opt = getopt(argc, argv, "DOXPpo:w:s:n:t:vh")) != EOF) {
		switch (opt) {
		case 'D':
			print_mode = DECIMAL;
			break;
		case 'O':
			print_mode = OCTAL;
			break;
		case 'X':
			print_mode = HEXADECIMAL;
			break;
		case 'P':
			position_comments = 1;
			break;
		case 'p':
			position_comments = 0;
			break;
		case 'o':
			output_filename = optarg;
			break;
		case 'w':
			max_width = get_number(optarg, max_width);
			break;
		case 's':
			start_pos = get_number(optarg, 0);
			start_pos = MAX(0, start_pos);
			break;
		case 'n':
			if (variable_name)
				free(variable_name);
			variable_name = strdup(optarg);
			break;
		case 't':
			data_type = optarg;
			break;
		case 'v':
			print_version(stdout);
			should_exit = 0;
			break;
		case 'h':
			print_help(stdout);
			should_exit = 0;
			break;
		case '?':      /* unknown option */
		case ':':      /* missing parameter */
			print_help(stderr);
			should_exit = 1;
			break;
		default:
			fprintf(stderr, "bin2h: unhandled option 0x%02x\n", opt);
		}
	}

	if (should_exit >= 0)
		exit(should_exit);
}

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

	do {
		ptr += strspn(ptr, variable_chars);
		if (*ptr)
			*ptr = '_';
	} while (*ptr);

	return ret;
}

/* --------------------------------------------------------------------- */

int main(int argc, char **argv)
{
	FILE *in = stdin, *out = stdout;
	int cur_byte = 0;       /* in the file */
	unsigned char *buf;     /* the line buffer */
	char *filename;

	handle_options(argc, argv);

	switch (argc - optind) {
	case 0:
		break;
	default:
		print_help(stderr);
		exit(1);
	case 1:
		filename = argv[optind];
		if (strcmp(filename, "-") == 0)
			break;
		in = fopen(filename, "rb");
		if (!in) {
			perror(filename);
			exit(2);
		}
		if (!variable_name)
			variable_name = get_variable_name(filename);
		break;
	}
	
	if (strcmp(output_filename, "-") != 0) {
		/* TODO: prompt for overwrite if the file already exists
		 * (add an -f option like with cp, ln, etc.) */
		out = fopen(output_filename, "w");
		if (!out) {
			perror(filename);
			exit(2);
		}
	}

	if (isatty(fileno(in))) {
		fprintf(stderr, "bin2h: I won't read binary data from a terminal.\n");
		exit(1);
	}
	/* create the read buffer */
	buf = malloc(max_width);

	/* skip to the start position */
	if (start_pos > 0) {
		if (fseek(in, 0, SEEK_END) != 0) {
			fprintf(stderr, "bin2h: starting position requested, but input is not seekable\n");
			exit(1);
		}
		if (ftell(in) >= start_pos) {
			fprintf(stderr, "bin2h: starting position is after end of input\n");
			exit(1);
		}
		if (fseek(in, start_pos, SEEK_SET) != 0) {
			perror("bin2h: fseek failed");
			exit(1);
		}
	}
	cur_byte = start_pos;

	fprintf(out, "%s %s[] = {\n", data_type, variable_name ? variable_name : "bin_data");

	for (;;) {
		int pos, nbuf;

		nbuf = fread(buf, 1, max_width, in);
		if (nbuf <= 0) {
			if (ferror(in)) {
				perror("bin2h: input error");
				exit(1);
			} else {
				break;
			}
		}

		fprintf(out, "\t");
		if (position_comments) {
			fprintf(out, format_strings[print_mode][POSITION_COMMENT],
				cur_byte, cur_byte + nbuf - 1);
		}
		for (pos = 0; pos < nbuf - 1; pos++)
			fprintf(out, format_strings[print_mode][LINE_BYTE], buf[pos]);
		fprintf(out, format_strings[print_mode][LAST_BYTE], buf[pos]);
		fprintf(out, "\n");

		cur_byte += nbuf;
	}

	fprintf(out, "};\n");

	/* is this stuff even useful at the end of the program? */
	free(buf);

	if (variable_name)
		free(variable_name);

	if (in != stdin)
		fclose(in);
	if (out != stdout)
		fclose(out);

	return 0;
}
