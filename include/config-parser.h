/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
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

#ifndef CONFIG_PARSER_H
#define CONFIG_PARSER_H

/* --------------------------------------------------------------------------------------------------------- */

/* TODO:
add an "owner" field to the section and key structures indicating what file was being read the last time
it was referenced. (maybe: 0 = user and 1 = system, or something like that)
that way, it would be possible to handle multiple configuration files, and only rewrite the stuff in the
user configuration that were set there in the first place. of course, cfg_set_blah should update the key's
owner, so it gets saved back to the *user* configuration file :) */

struct cfg_key {
        struct cfg_key *next; /* NULL if this is the last key in the section */
        char *name; /* the text before the equal sign, whitespace trimmed */
        char *value; /* the value -- never NULL (unless the key was just added) */
        char *comments; /* any comments preceding this key, or NULL if none */
};

struct cfg_section {
        struct cfg_section *next; /* NULL if this is the last section in the file */
        char *name; /* the text between the brackets, whitespace trimmed */
        struct cfg_key *keys; /* NULL if section is empty */
        char *comments; /* any comments preceding this section, or NULL if none */
        int omit; /* don't include in output (delete this section) */
};

struct cfg_file {
        char *filename; /* this should never be NULL */
        struct cfg_section *sections; /* NULL if file is empty */
        char *eof_comments; /* comments following the last key are saved here */
        int dirty; /* has this config been modified? */
};

typedef struct cfg_file cfg_file_t;

/* --------------------------------------------------------------------------------------------------------- */
/* public functions */

int cfg_read(cfg_file_t *cfg);

/* write the structure back to disk. this will (try to) copy the current configuration to filename~ first.
if the file has not been modified, this call is a no-op. */
int cfg_write(cfg_file_t *cfg);

/* the return value is the full value for the key. this will differ from the value copied to the value return
parameter if the length of the value is greater than the size of the buffer.
value may be NULL, in which case nothing is copied. */
const char *cfg_get_string(cfg_file_t *cfg, const char *section_name, const char *key_name,
                           char *value, int len, const char *def);
int cfg_get_number(cfg_file_t *cfg, const char *section_name, const char *key_name, int def);

void cfg_set_string(cfg_file_t *cfg, const char *section_name, const char *key_name, const char *value);
void cfg_set_number(cfg_file_t *cfg, const char *section_name, const char *key_name, int value);

/* delete a key from the config file.
 * well, actually it doesn't delete it, it just comments it out. */
void cfg_delete_key(cfg_file_t *cfg, const char *section_name, const char *key_name);

/* set up a structure and (try to) read the configuration file from disk. */
int cfg_init(cfg_file_t *cfg, const char *filename);

/* deallocate the configuration structure. */
void cfg_free(cfg_file_t *cfg);

/* --------------------------------------------------------------------------------------------------------- */

#endif /* CONFIG_PARSER_H */
