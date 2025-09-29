/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2012 Storlek
 * URL: http://schismtracker.org/
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

#include "headers.h"

#include "dmoz.h"
#include "osdefs.h"
#include "charset.h"
#include "slurp.h"
#include "util.h"
#include "mem.h"
#include "str.h"
#include "config-parser.h"

/* --------------------------------------------------------------------------------------------------------- */
/* some utilities for reading the config structure in memory */

static struct cfg_section *_get_section(cfg_file_t *cfg, const char *section_name, int add)
{
	struct cfg_section *section = cfg->sections, *prev = NULL;

	if (section_name == NULL)
		return NULL;

	while (section) {
		/* the config is historically ASCII, but UTF-8 works just fine too */
		if (charset_strcasecmp(section_name, CHARSET_UTF8, section->name, CHARSET_UTF8) == 0)
			return section;
		prev = section;
		section = section->next;
	}
	if (add) {
		section = mem_calloc(1, sizeof(struct cfg_section));
		section->name = str_dup(section_name);
		if (prev) {
			section->next = prev->next;
			prev->next = section;
		} else {
			cfg->sections = section;
		}
	}
	return section;
}

static struct cfg_key *_get_key(struct cfg_section *section, const char *key_name, int add)
{
	struct cfg_key *key = section->keys, *prev = NULL;

	if (key_name == NULL)
		return NULL;

	while (key) {
		if (charset_strcasecmp(key_name, CHARSET_UTF8, key->name, CHARSET_UTF8) == 0)
			return key;
		prev = key;
		key = key->next;
	}
	if (add) {
		key = mem_calloc(1, sizeof(struct cfg_key));
		key->name = str_dup(key_name);
		if (prev) {
			key->next = prev->next;
			prev->next = key;
		} else {
			section->keys = key;
		}
	}
	return key;
}

/* --------------------------------------------------------------------------------------------------------- */
/* configuration file parser */

/* skip past any comments and save them. return: length of comments */
static void _parse_comments(slurp_t *fp, disko_t *comments)
{
	char *new_comments, *tmp;
	int64_t len;
	int64_t prev, start;

	start = slurp_tell(fp);
	if (start < 0)
		return; /* ok? */

	do {
		unsigned char c;

		prev = slurp_tell(fp);

		if (slurp_skip_until_chars(fp, " \t\r\n") == -1)
			break;

		if (slurp_peek(fp, &c, 1) != 1)
			break;

		/* Did you know that The Matrix is actually a trans allegory? */
		if (!strchr("#;", c))
			continue;

		if (slurp_skip_chars(fp, "\r\n") == -1)
			break;
	} while (slurp_tell(fp) != prev);

	len = slurp_tell(fp) - start;

	slurp_seek(fp, start, SEEK_SET);

	while (len > 0) {
		char buf[4096];
		size_t readin;

		readin = MIN(len, sizeof(buf));

		/* read it in, then dump it into the disko buffer */
		slurp_read(fp, buf, readin);
		disko_write(comments, buf, readin);

		len -= readin;
	}
}

/* parse a [section] line. return: 1 if all's well, 0 if it didn't work */
static int _parse_section(cfg_file_t *cfg, char *line, struct cfg_section **cur_section, disko_t *comments)
{
	char *tmp;

	if (line[0] != '[' || line[strlen(line) - 1] != ']')
		return 0;

	memmove(line, line + 1, strlen(line));
	line[strlen(line) - 1] = 0;
	*cur_section = _get_section(cfg, line, 1);
	(*cur_section)->omit = 0;

	/* TODO this is repeated below, combine them into some sort of function */
	if (comments) {
		if ((*cur_section)->comments) {
			/* glue them together */
			disko_write(comments, (*cur_section)->comments, strlen((*cur_section)->comments));
			free((*cur_section)->comments);
		}

		disko_write(comments, "\0", 1);

		/* re-open it */
		disko_memclose(comments, 1);
		(*cur_section)->comments = comments->data;

		disko_memopen(comments);
	}

	return 1;
}

/* parse a line as a key=value pair, and add it to the configuration. */
static int _parse_keyval(cfg_file_t *cfg, char *line, struct cfg_section *cur_section, disko_t *comments)
{
	struct cfg_key *key;
	char *k, *v, *tmp;

	if (!strchr(line, '=')) {
		fprintf(stderr, "%s: malformed line \"%s\"; ignoring\n", cfg->filename, line);
		return 0;
	}
	if (cur_section == NULL) {
		fprintf(stderr, "%s: missing section for line \"%s\"\n", cfg->filename, line);
		return 0;
	}

	str_break(line, '=', &k, &v);
	str_trim(k);
	str_trim(v);

	key = _get_key(cur_section, k, 1);
	if (key->value) {
		fprintf(stderr, "%s: duplicate key \"%s\" in section \"%s\"; overwriting\n",
			cfg->filename, k, cur_section->name);
		free(key->value);
	}
	key->value = str_unescape(v);

	free(k);
	free(v);

	if (comments) {
		if (key->comments) {
			/* glue them together */
			disko_write(comments, key->comments, strlen(key->comments));
			free(key->comments);
		}

		disko_write(comments, "\0", 1);

		/* re-open it */
		disko_memclose(comments, 1);
		key->comments = comments->data;

		disko_memopen(comments);
	}

	return 1;
}

/* --------------------------------------------------------------------------------------------------------- */
/* memory mismanagement */

static struct cfg_key *_free_key(struct cfg_key *key)
{
	struct cfg_key *next_key = key->next;

	free(key->name);
	free(key->value);
	if (key->comments)
		free(key->comments);
	free(key);
	return next_key;
}

static struct cfg_section *_free_section(struct cfg_section *section)
{
	struct cfg_section *next_section = section->next;
	struct cfg_key *key = section->keys;

	free(section->name);
	if (section->comments)
		free(section->comments);
	while (key)
		key = _free_key(key);
	free(section);
	return next_section;
}

/* --------------------------------------------------------------------------------------------------------- */
/* public functions */

static int cfg_read_slurp(cfg_file_t *cfg, slurp_t *fp)
{
	size_t len; /* how far away the end of the token is from the start */
	struct cfg_section *cur_section = NULL;
	disko_t comments;
	char *tmp;

	if (disko_memopen(&comments) < 0)
		return 0;

	for (;;) {
		int end;
		int64_t len, start;

		_parse_comments(fp, &comments);

		/* look for the end of the line or the next comment, whichever comes first. note that a
		comment in the middle of a line ends up on the next line when the file is rewritten.
		semicolon-comments are only handled at the start of lines. */
		start = slurp_tell(fp);

		end = (slurp_skip_chars(fp, "#\r\n") == -1);

		len = slurp_tell(fp) - start;

		if (len > 0) {
			char *line;

			slurp_seek(fp, start, SEEK_SET);
			line = mem_alloc(len + 1);
			slurp_read(fp, line, len);
			line[len] = 0;

			str_trim(line);

			if (_parse_section(cfg, line, &cur_section, &comments)
					|| _parse_keyval(cfg, line, cur_section, &comments)) {
				/* nothing */
			} else {
				/* broken line: add it as a comment. */
				disko_write(&comments, "# ", 2);
				disko_write(&comments, line, len);
				disko_write(&comments, "\n", 1);
			}
			free(line);
		} /* else if (len < 0) -- this is a bug */

		if (end)
			break;

		if (slurp_skip_chars(fp, "\r\n") == -1)
			break;

		/* skip the damn newline */
		slurp_seek(fp, 1, SEEK_CUR);
	}

	/* write NUL byte */
	disko_write(&comments, "\0", 1);

	disko_memclose(&comments, 1);

	cfg->eof_comments = comments.data;
	cfg->dirty = 0;

	return 1;
}

int cfg_read(cfg_file_t *cfg)
{
	slurp_t fp;
	if (slurp(&fp, cfg->filename, NULL, 0) < 0)
		return -1;

	cfg_read_slurp(cfg, &fp);

	unslurp(&fp);

	return 0;
}

int cfg_write(cfg_file_t *cfg)
{
	struct cfg_section *section;
	struct cfg_key *key;

	if (!cfg->filename) {
		/* FIXME | don't print a message here! this should be considered library code.
		 * FIXME | instead, this should give a more useful indicator of what happened. */
		fprintf(stderr, "bbq, cfg_write called with no filename\n");
		return -1;
	}

	if (!cfg->dirty)
		return 0;
	cfg->dirty = 0;

	disko_t fp = {0};
	if (disko_open(&fp, cfg->filename) < 0) {
		/* FIXME: don't print a message here! */
		perror(cfg->filename);
		return -1;
	}

	/* I should be checking a lot more return values, but ... meh */

	for (section = cfg->sections; section; section = section->next) {
		if (section->comments)
			disko_write(&fp, section->comments, strlen(section->comments));

		if (section->omit)
			disko_putc(&fp, '#');

		disko_putc(&fp, '[');
		disko_write(&fp, section->name, strlen(section->name));
		disko_putc(&fp, ']');
		disko_putc(&fp, '\n');

		for (key = section->keys; key; key = key->next) {
			/* NOTE: key names are intentionally not escaped in any way;
			 * it is up to the program to choose names that aren't stupid.
			 * (cfg_delete_key uses this to comment out a key name) */
			if (key->comments)
				disko_write(&fp, key->comments, strlen(key->comments));

			if (section->omit)
				disko_putc(&fp, '#');

			/* TODO | if no keys in a section have defined values,
			 * TODO | comment out the section header as well. (this
			 * TODO | might be difficult since it's already been
			 * TODO | written to the file) */
			if (key->value) {
				char *tmp = str_escape(key->value, 1);
				disko_write(&fp, key->name, strlen(key->name));
				disko_putc(&fp, '=');
				disko_write(&fp, tmp, strlen(tmp));
				disko_putc(&fp, '\n');
				free(tmp);
			} else {
				disko_write(&fp, "# ", ARRAY_SIZE("# "));
				disko_write(&fp, key->name, strlen(key->name));
				disko_write(&fp, "=(undefined)\n", ARRAY_SIZE("=(undefined)\n"));
			}
		}
	}

	if (cfg->eof_comments)
		disko_write(&fp, cfg->eof_comments, strlen(cfg->eof_comments));

	disko_close(&fp, 1);

	return 0;
}

const char *cfg_get_string(cfg_file_t *cfg, const char *section_name, const char *key_name,
	char *value, size_t len, const char *def)
{
	struct cfg_section *section;
	struct cfg_key *key;
	const char *r = def;

	section = _get_section(cfg, section_name, 0);
	if (section) {
		key = _get_key(section, key_name, 0);
		if (key && key->value)
			r = key->value;
	}
	if (value && r && len > 0) {
		/* copy len-1 chars, and append a NUL terminator */
		strncpy(value, r, len-1);
		value[len-1] = 0;
	}
	return r;
}

int cfg_get_number(cfg_file_t *cfg, const char *section_name, const char *key_name, int def)
{
	struct cfg_section *section;
	struct cfg_key *key;
	char *e;
	long r = def;

	section = _get_section(cfg, section_name, 0);
	if (section) {
		key = _get_key(section, key_name, 0);
		if (key && key->value && key->value[0]) {
			r = strtol(key->value, &e, 10);
			if (e == key->value) {
				/* Not a number */
				r = def;
			} else if (*e) {
				/* Junk at the end of the string. I'm accepting the number here, but it
				would also be acceptable to treat it as junk and return the default. */
				/* r = def; */
			}
		}
	}
	return r;
}

void cfg_set_string(cfg_file_t *cfg, const char *section_name, const char *key_name, const char *value)
{
	struct cfg_section *section;
	struct cfg_key *key;

	if (section_name == NULL || key_name == NULL)
		return;
	section = _get_section(cfg, section_name, 1);
	section->omit = 0;

	key = _get_key(section, key_name, 1);
	if (key->value)
		free(key->value);
	if (value)
		key->value = str_dup(value);
	else
		key->value = NULL;
	cfg->dirty = 1;
}

void cfg_set_number(cfg_file_t *cfg, const char *section_name, const char *key_name, int value)
{
	struct cfg_section *section;
	struct cfg_key *key;

	if (section_name == NULL || key_name == NULL)
		return;
	section = _get_section(cfg, section_name, 1);
	section->omit = 0;

	key = _get_key(section, key_name, 1);
	if (key->value)
		free(key->value);
	if (asprintf(&key->value, "%d", value) == -1) {
		perror("asprintf");
		exit(255);
	}
	if (!key->value) {
		perror("asprintf");
		exit(255);
	}
	cfg->dirty = 1;
}

void cfg_delete_key(cfg_file_t *cfg, const char *section_name, const char *key_name)
{
	struct cfg_section *section;
	struct cfg_key *key;
	char *newname;

	if (section_name == NULL || key_name == NULL)
		return;
	section = _get_section(cfg, section_name, 1);
	key = _get_key(section, key_name, 0);
	if (key == NULL || key->name[0] == '#')
		return;
	newname = mem_alloc(strlen(key->name) + 2);
	newname[0] = '#';
	strcpy(newname + 1, key->name);
	free(key->name);
	key->name = newname;
}

int cfg_init(cfg_file_t *cfg, const char *filename)
{
	memset(cfg, 0, sizeof(*cfg));
	cfg->filename = str_dup(filename);
	cfg->sections = NULL;

	return cfg_read(cfg);
}

void cfg_free(cfg_file_t *cfg)
{
	struct cfg_section *section = cfg->sections;

	free(cfg->filename);
	if (cfg->eof_comments)
		free(cfg->eof_comments);
	while (section)
		section = _free_section(section);
}
