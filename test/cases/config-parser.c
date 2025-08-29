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

#include "test.h"
#include "test-tempfile.h"
#include "test-assertions.h"

#include "charset.h"

#include "config-parser.h"

static const char test_config_file_content[] =
	"[ducks]\n"
	"colour = brown\n"
	"count = 7\n"
	"weight = 64 lb.\n";

#define ARRANGE_TEST_CONFIG_FILE(CFG) \
	do \
	{ \
		char test_config_file[TEST_TEMP_FILE_NAME_LENGTH]; \
\
		REQUIRE(test_temp_file(test_config_file, test_config_file_content, ARRAY_SIZE(test_config_file_content) - 1)); \
\
		int init_result = cfg_init(&cfg, test_config_file); \
\
		if (init_result < 0) \
		{ \
			test_log_printf("cfg_init(\"%s\") failed with result %d\n", test_config_file, init_result); \
			RETURN_FAIL; \
		} \
	} while (0)

static testresult_t test_config_file_defined_values(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	const char *colour = cfg_get_string(&cfg, "ducks", "colour", NULL, 0, "abcd");
	int count = cfg_get_number(&cfg, "ducks", "count", 1234);
	int weight = cfg_get_number(&cfg, "ducks", "weight", 1234);

	// Assert
	ASSERT_EQUAL_S(colour, "brown");
	ASSERT_EQUAL_D(count, 7);
	ASSERT_EQUAL_D(weight, 64);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_undefined_values_in_defined_section(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	const char *sauce = cfg_get_string(&cfg, "ducks", "sauce", NULL, 0, "soy");
	int feathers = cfg_get_number(&cfg, "ducks", "feathers", 94995);

	// Assert
	ASSERT_EQUAL_S(sauce, "soy");
	ASSERT_EQUAL_D(feathers, 94995);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_undefined_section(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	const char *weather = cfg_get_string(&cfg, "barbecue", "weather", NULL, 0, "elf");
	int dismal = cfg_get_number(&cfg, "barbecue", "dismal", 758);

	// Assert
	ASSERT_EQUAL_S(weather, "elf");
	ASSERT_EQUAL_D(dismal, 758);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_obviously_broken_values(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	// could be separate tests...
	const char *str_null_section = cfg_get_string(&cfg, NULL, "shouldn't crash", NULL, 0, "ok");
	const char *str_null_key = cfg_get_string(&cfg, "shouldn't crash", NULL, NULL, 0, "ok");
	int num_null_section = cfg_get_number(&cfg, NULL, "shouldn't crash", 1);
	int num_null_key = cfg_get_number(&cfg, "shouldn't crash", NULL, 1);
	const char *str_null_default_value = cfg_get_string(&cfg, "doesn't", "exist", NULL, 0, NULL);

	// Assert
	ASSERT_EQUAL_S(str_null_section, "ok");
	ASSERT_EQUAL_S(str_null_key, "ok");
	ASSERT_EQUAL_D(num_null_section, 1);
	ASSERT_EQUAL_D(num_null_key, 1);
	ASSERT_NULL(str_null_default_value);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_null_default_with_value_set(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	char buf[64];

	const char *semaphore = "didn't change";

	strcpy(buf, semaphore);

	// Act
	cfg_get_string(&cfg, "still", "nonexistent", buf, 64, NULL);

	// Assert
	ASSERT_EQUAL_S(buf, semaphore);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_null_default_with_value_set_defined_key(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	char buf[64];

	const char *semaphore = "didn't change";

	strcpy(buf, semaphore);

	// Act
	cfg_get_string(&cfg, "ducks", "weight", buf, 64, NULL);

	// Assert
	ASSERT_NOT_EQUAL_S(buf, semaphore);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_string_boundary_defined_key(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	char buf[64];

	const char *semaphore = "didn't change";

	strcpy(buf, semaphore);

	cfg_set_string(&cfg, "test", "test", "abcdefghijklmnopqrstuvwxyz???broken");

	// Act
	cfg_get_string(&cfg, "test", "test", buf, 26 + 1 /* '\0' */, NULL);

	// Assert
	ASSERT_STRLEN(buf, 26);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_string_boundary_default_value(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	char buf[64];

	const char *semaphore = "didn't change";

	strcpy(buf, semaphore);

	// Act
	cfg_get_string(&cfg, "fake section", "fake key", buf, 10 + 1 /* '\0' */, "1234567890???broken");

	// Assert
	ASSERT_STRLEN(buf, 10);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_string_boundary_zero(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	cfg_set_string(&cfg, "shouldn't", "crash", "daikon");

	char buf[64];

	const char *semaphore = "didn't change";

	strcpy(buf, semaphore);

	// Act
	const char *result = cfg_get_string(&cfg, "shouldn't", "crash", buf, 0, "it doesn't");

	// Assert
	ASSERT_EQUAL_S(result, "daikon");
	ASSERT_EQUAL_S(buf, semaphore);

	cfg_free(&cfg);

	RETURN_PASS;
}

/* --------------------------------------------------------------------- */

static int grep_config_impl(const cfg_file_t *cfg, const char *section_expr, const char *key_expr, const char *value_expr, struct cfg_section **section, struct cfg_key **key)
{
	int matches = 0;

	*section = NULL;
	*key = NULL;

	if (!cfg->sections)
		return 0;

	if (section_expr != NULL) {
		struct cfg_section *trace_section = cfg->sections;

		while (trace_section != NULL) {
			if (charset_fnmatch(section_expr, CHARSET_UTF8, trace_section->name, CHARSET_UTF8, CHARSET_FNM_CASEFOLD) == 0) {
				*section = trace_section;
				matches++;
			}

			trace_section = trace_section->next;
		}
	}

	if (key_expr != NULL) {
		struct cfg_section *trace_section = *section ? *section : cfg->sections;

		while (trace_section != NULL) {
			struct cfg_key *trace_key = trace_section->keys;

			while (trace_key != NULL) {
				if (charset_fnmatch(key_expr, CHARSET_UTF8, trace_key->name, CHARSET_UTF8, CHARSET_FNM_CASEFOLD) == 0) {
					*key = trace_key;
					matches++;
				}

				trace_key = trace_key->next;
			}

			if (*section) break;

			trace_section = trace_section->next;
		}
	}

	if (value_expr != NULL) {
		struct cfg_section *trace_section = *section ? *section : cfg->sections;

		while (trace_section != NULL) {
			struct cfg_key *trace_key = *key ? *key : trace_section->keys;

			while (trace_key != NULL) {
				if (charset_fnmatch(value_expr, CHARSET_UTF8, trace_key->value, CHARSET_UTF8, CHARSET_FNM_CASEFOLD) == 0)
					matches++;

				if (*key)
					break;

				trace_key = trace_key->next;
			}

			if (*section || *key) break;

			trace_section = trace_section->next;
		}
	}

	return matches;
}

// grep_config
// -----------
// Searches the configuration for the specified sections. Increments a counter on every match and returns it.
// If section pattern is specified, only the last matching section is searched for key.
// If key pattern is specified, only the last matching key in searched for value.
//
// For instance, if you grep for ("section", NULL, NULL):
// - section doesn't exist -> 0
// - section does exist -> 1
//
// If you grep for ("section", "key", NULL):
// - section doesn't exist -> 0
// - section does exist, key doesn't exist -> 1
// - section and key both exist -> 2
// - section exists, key exists but only in other sections -> 1
//
// If you grep for ("section", "key", "value"):
// - section doesn't exist -> 0
// - section does exist, key doesn't exist -> 1
// - section exists, key exists within section, value does not match -> 2
// - section exists, key exists within section, value does match -> 3
// - section exists, key exists within section, matching value in unrelated key -> 2
//
// If you grep for (NULL, "key", NULL):
// - no sections exist with key -> 0
// - 3 sections exist with key -> 3
//
// If you grep for (NULL, "key", "value"):
// - no sections exist with key -> 0
// - 3 sections exist with key, none of their values match -> 3
// - 3 sections exist with key, all of their values match -> 6
// - 3 sections exist with key, none of their values match, but there are unrelated values that do match -> 3
//
// If you grep for (NULL, NULL, "value"):
// - across all sections and keys, none of the values match -> 0
// - across all sections and keys, 5 of the values match -> 5
static int grep_config(const cfg_file_t *cfg, const char *section_expr, const char *key_expr, const char *value_expr)
{
	struct cfg_section *section;
	struct cfg_key *key;

	return grep_config_impl(cfg, section_expr, key_expr, value_expr, &section, &key);
}

// grep_config_ex
// --------------
// Return value is equivalent to grep_config(cfg, section_expr, key_expr, NULL).
// Takes two additional output parameters:
// - out_section: Receives a pointer to the last matching section.
// - out_key: Receiver a pointer to the last matching key.
static int grep_config_ex(const cfg_file_t *cfg, const char *section_expr, const char *key_expr, struct cfg_section **out_section, struct cfg_key **out_key)
{
	struct cfg_section *section;
	struct cfg_key *key;

	return grep_config_impl(cfg, section_expr, key_expr, NULL, out_section ? out_section : &section, out_key ? out_key : &key);
}

static testresult_t test_config_file_set_string_in_new_section(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_string(&cfg, "toast", "is", "tasty");

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, "toast", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "toast", "is", NULL), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, "toast", "is", "tasty"), 3);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "is", NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "is", "tasty"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, NULL, "tasty"), 1);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_set_new_string_in_existing_section(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_string(&cfg, "ducks", "are", "tasty");

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, "toast", NULL, NULL), 0);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "are", NULL), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "are", "tasty"), 3);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "are", NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "are", "tasty"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, NULL, "tasty"), 1);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_set_number_in_new_section(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_number(&cfg, "cowboy", "hats", 3);

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, "cowboy", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "cowboy", "hats", NULL), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, "cowboy", "hats", "3"), 3);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "hats", NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "hats", "3"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, NULL, "3"), 1);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_set_new_number_in_existing_section(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_number(&cfg, "ducks", "boots", 4);

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, "toast", NULL, NULL), 0);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "boots", NULL), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "boots", "4"), 3);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "boots", NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "boots", "4"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, NULL, "4"), 1);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_set_string_in_null_section(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_string(&cfg, NULL, "shouldn't", "crash");

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "shouldn't", NULL), 0);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, NULL, "crash"), 0);

	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "*", NULL), 4);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "colour", "brown"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "count", "7"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "weight", "64*"), 2);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_set_number_in_null_section(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_number(&cfg, NULL, "don't segfault", 42);

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "don't segfault", NULL), 0);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, NULL, "42"), 0);

	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "*", NULL), 4);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "colour", "brown"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "count", "7"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "weight", "64*"), 2);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_set_string_with_null_key(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_string(&cfg, "shouldn't", NULL, "crash");

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, "shouldn't", NULL, NULL), 0);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, NULL, "crash"), 0);

	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "*", NULL), 4);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "colour", "brown"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "count", "7"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "weight", "64*"), 2);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_set_number_with_null_key(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_number(&cfg, "don't segfault", NULL, 42);

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, "*segfault", NULL, NULL), 0);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, NULL, "42"), 0);

	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "*", NULL), 4);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "colour", "brown"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "count", "7"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "weight", "64*"), 2);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_set_string_with_null_value(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	// Act
	cfg_set_string(&cfg, "shouldn't", "crash", NULL);

	// Assert
	ASSERT_EQUAL_D(grep_config(&cfg, "shouldn't", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "crash", NULL), 1);

	struct cfg_key *key;
	struct cfg_section *q;

	grep_config_ex(&cfg, "shouldn't", "crash", NULL, &key);

	ASSERT(key != NULL);
	ASSERT(key->value == NULL);

	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", NULL, NULL), 1);
	ASSERT_EQUAL_D(grep_config(&cfg, "ducks", "*", NULL), 4);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "colour", "brown"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "count", "7"), 2);
	ASSERT_EQUAL_D(grep_config(&cfg, NULL, "weight", "64*"), 2);

	cfg_free(&cfg);

	RETURN_PASS;
}

static testresult_t test_config_file_get_string_with_null_value(void)
{
	// Arrange
	cfg_file_t cfg;

	ARRANGE_TEST_CONFIG_FILE(cfg);

	cfg_set_string(&cfg, "shouldn't", "crash", NULL);

	// Act
	const char *result = cfg_get_string(&cfg, "shouldn't", "crash", NULL, 0, "it doesn't");

	// Assert
	ASSERT_EQUAL_S(result, "it doesn't");

	cfg_free(&cfg);

	RETURN_PASS;
}

#include "config-parser.f"