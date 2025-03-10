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
static size_t _parse_comments(const char *s, char **comments)
{
	const char *ptr = s, *prev;
	char *new_comments, *tmp;
	size_t len;

	do {
		prev = ptr;
		ptr += strspn(ptr, " \t\r\n");
		if (*ptr == '#' || *ptr == ';')
			ptr += strcspn(ptr, "\r\n");
	} while (*ptr && ptr != prev);
	len = ptr - s;
	if (len) {
		/* save the comments */
		new_comments = strn_dup(s, len);
		if (*comments) {
			/* already have some comments -- add to them */
			if (asprintf(&tmp, "%s%s", *comments, new_comments) == -1) {
				perror("asprintf");
				exit(255);
			}
			if (!tmp) {
				perror("asprintf");
				exit(255);
			}
			free(*comments);
			free(new_comments);
			*comments = tmp;
		} else {
			*comments = new_comments;
		}
	}
	return len;
}

/* parse a [section] line. return: 1 if all's well, 0 if it didn't work */
static int _parse_section(cfg_file_t *cfg, char *line, struct cfg_section **cur_section, char *comments)
{
	char *tmp;

	if (line[0] != '[' || line[strlen(line) - 1] != ']')
		return 0;

	memmove(line, line + 1, strlen(line));
	line[strlen(line) - 1] = 0;
	*cur_section = _get_section(cfg, line, 1);
	(*cur_section)->omit = 0;
	if (comments) {
		if ((*cur_section)->comments) {
			/* glue them together */
			if (asprintf(&tmp, "%s\n%s", comments, (*cur_section)->comments) == -1) {
				perror("asprintf");
				exit(255);
			}
			if (!tmp) {
				perror("asprintf");
				exit(255);
			}
			free((*cur_section)->comments);
			free(comments);
			(*cur_section)->comments = tmp;
		} else {
			(*cur_section)->comments = comments;
		}
	}

	return 1;
}

/* parse a line as a key=value pair, and add it to the configuration. */
static int _parse_keyval(cfg_file_t *cfg, char *line, struct cfg_section *cur_section, char *comments)
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
			if (asprintf(&tmp, "%s\n%s", comments, key->comments) == -1) {
				perror("asprintf");
				exit(255);
			}
			if (!tmp) {
				perror("asprintf");
				exit(255);
			}
			free(key->comments);
			free(comments);
			key->comments = tmp;
		} else {
			key->comments = comments;
		}
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

static int cfg_read_receive_impl(const void *data, size_t size, void *userdata)
{
	size_t len; /* how far away the end of the token is from the start */
	struct cfg_section *cur_section = NULL;
	char *comments = NULL, *tmp;
	cfg_file_t *cfg = (cfg_file_t *)userdata;

	const char *pos = (const char *)data;
	do {
		pos += _parse_comments(pos, &comments);

		/* look for the end of the line or the next comment, whichever comes first. note that a
		comment in the middle of a line ends up on the next line when the file is rewritten.
		semicolon-comments are only handled at the start of lines. */
		len = strcspn(pos, "#\r\n");
		if (len) {
			char *line = strn_dup(pos, len);
			str_trim(line);
			if (_parse_section(cfg, line, &cur_section, comments)
			    || _parse_keyval(cfg, line, cur_section, comments)) {
				comments = NULL;
			} else {
				/* broken line: add it as a comment. */
				if (comments) {
					if (asprintf(&tmp, "%s# %s\n", comments, line) == -1) {
						perror("asprintf");
						exit(255);
					}
					if (!tmp) {
						perror("asprintf");
						exit(255);
					}
					free(comments);
					comments = tmp;
				} else {
					if (asprintf(&comments, "# %s\n", line) == -1) {
						perror("asprintf");
						exit(255);
					}
					if (!comments) {
						perror("asprintf");
						exit(255);
					}
				}
			}
			free(line);
		}
		pos += len;

		/* skip the newline */
		if (*pos == '\r')
			pos++;
		if (*pos == '\n')
			pos++;
	} while (*pos);

	cfg->eof_comments = comments;

	return 1;
}

int cfg_read(cfg_file_t *cfg)
{
	slurp_t fp;
	if (slurp(&fp, cfg->filename, NULL, 0) < 0)
		return -1;

	slurp_receive(&fp, cfg_read_receive_impl, slurp_length(&fp) + 1, cfg);

	cfg->dirty = 0;

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
			   char *value, int len, const char *def)
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
	if (value && r) {
		//copy up to len chars [0..len-1]
		strncpy(value, r, len);
		value[len] = 0;
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

/* --------------------------------------------------------------------------------------------------------- */

#ifdef TEST
int main(int argc, char **argv)
{
	cfg_file_t cfg;
	char buf[64];

	cfg_init(&cfg, "config");

	/*
	- test these functions with defined and undefined section/key names
	- test all functions with completely broken values
	  (e.g. NULL shouldn't break it, it should give up, maybe print a warning,
	  and for the get functions, return the default value)

	const char *cfg_get_string(cfg_file_t *cfg, const char *section_name, const char *key_name,
				   char *value, int len, const char *def);
	int cfg_get_number(cfg_file_t *cfg, const char *section_name, const char *key_name, int def);
	void cfg_set_string(cfg_file_t *cfg, const char *section_name, const char *key_name, const char *value);
	void cfg_set_number(cfg_file_t *cfg, const char *section_name, const char *key_name, int value);
	*/
/*
[ducks]
color = brown
count = 7
weight = 64 lb.
*/
	printf("testing cfg_get_ functions...\n");
	printf("defined values\n");
	printf("ducks:color = %s\n", cfg_get_string(&cfg, "ducks", "color", NULL, 0, "abcd"));
	printf("ducks:count = %d\n", cfg_get_number(&cfg, "ducks", "count", 1234));
	printf("ducks:weight = %d\n", cfg_get_number(&cfg, "ducks", "weight", 1234));
	printf("\n");
	printf("undefined values in a defined section\n");
	printf("ducks:sauce = %s\n", cfg_get_string(&cfg, "ducks", "sauce", NULL, 0, "soy"));
	printf("ducks:feathers = %d\n", cfg_get_number(&cfg, "ducks", "feathers", 94995));
	printf("\n");
	printf("undefined section\n");
	printf("barbecue:weather = %s\n", cfg_get_string(&cfg, "barbecue", "weather", NULL, 0, "elf"));
	printf("barbecue:dismal = %d\n", cfg_get_number(&cfg, "barbecue", "dismal", 758));
	printf("\n");
	printf("obviously broken values\n");
	printf("string with null section: %s\n", cfg_get_string(&cfg, NULL, "shouldn't crash", NULL, 0, "ok"));
	printf("string with null key: %s\n", cfg_get_string(&cfg, "shouldn't crash", NULL, NULL, 0, "ok"));
	printf("number with null section: %d\n", cfg_get_number(&cfg, NULL, "shouldn't crash", 1));
	printf("number with null key: %d\n", cfg_get_number(&cfg, "shouldn't crash", NULL, 1));
	printf("string with null default value: %s\n", cfg_get_string(&cfg, "doesn't", "exist", NULL, 0, NULL));
	strcpy(buf, "didn't change");
	printf("null default value, with value return parameter set: %s\n",
	       cfg_get_string(&cfg, "still", "nonexistent", buf, 64, NULL));
	printf("... and the buffer it returned: %s\n", buf);
	strcpy(buf, "didn't change");
	printf("null default value on defined key with return parameter: %s\n",
	       cfg_get_string(&cfg, "ducks", "weight", buf, 64, NULL));
	printf("... and the buffer it returned: %s\n", buf);
	printf("\n");
	printf("string boundary tests\n");
	cfg_set_string(&cfg, "test", "test", "abcdefghijklmnopqrstuvwxyz???broken");
	cfg_get_string(&cfg, "test", "test", buf, 26, "wtf");
	printf("26 characters using defined value: %s\n", buf);
	cfg_get_string(&cfg, "fake section", "fake key", buf, 10, "1234567890???broken");
	printf("10 characters using default value: %s\n", buf);
	cfg_get_string(&cfg, "fake section", "fake key", buf, 0, "huh?");
	printf("zero-length buffer (this should be an empty string) \"%s\"\n", buf);
	printf("\n");
	printf("testing cfg_set_ functions...\n");
	printf("string in new section\n");
	cfg_set_string(&cfg, "toast", "is", "tasty");
	printf("string with new key in existing section\n");
	cfg_set_string(&cfg, "toast", "tastes", "good");
	printf("number in new section\n");
	cfg_set_number(&cfg, "cowboy", "hats", 3);
	printf("number with new key in existing section\n");
	cfg_set_number(&cfg, "cowboy", "boots", 4);
	printf("string with null section\n");
	cfg_set_string(&cfg, NULL, "shouldn't", "crash");
	printf("string with null key\n");
	cfg_set_string(&cfg, "shouldn't", NULL, "crash");
	printf("string with null value\n");
	cfg_set_string(&cfg, "shouldn't", "crash", NULL);
	printf("re-reading that null string should return default value: %s\n",
	       cfg_get_string(&cfg, "shouldn't", "crash", NULL, 0, "it does"));
	printf("number with null section\n");
	cfg_set_number(&cfg, NULL, "don't segfault", 42);
	printf("number with null key\n");
	cfg_set_number(&cfg, "don't segfault", NULL, 42);

	cfg_dump(&cfg);
	cfg_free(&cfg);

	return 0;
}
#endif /* TEST */
