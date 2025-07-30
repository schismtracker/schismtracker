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

#include "keybinds.h"

#include "log.h"
#include "str.h"
#include "mem.h"
#include "config-parser.h"
#include "config.h"
#include "dmoz.h"

enum {
	KEYBIND_TYPE_SCANCODE,
	KEYBIND_TYPE_KEYCODE,
};

enum {
	KEYBIND_STATE_PRESSED = (0x01),
	KEYBIND_STATE_RELEASED = (0x02),
	KEYBIND_STATE_REPEATED = (0x04),
};

/* Try to keep this structure reasonably small!
 *
 * I've cobbled this together so that it only takes
 * up 12 bytes for now.
 *
 * If more stuff gets added, try to fit it so where
 * the structure will take up the least space after
 * alignment.
 *
 * NOTE: this not only holds the (somewhat read-only)
 * keybind info, but also the current state. */
struct keybind {
	/* how many times the current key has been repeated */
	uint16_t repeats;

	/* bitwise OR of the state values defined above */
	uint8_t state;

	/* scancode & keycode are mutually exclusive */
	uint8_t type;
	union {
		/* these are actually the same size, so there's no difference
		 * besides some code hints to show WHICH one we're using. */
		schism_scancode_t scan;
		schism_keysym_t key;
	} code;

	/* bitwise OR of the modifier values defined above */
	uint16_t modifiers;

	/* how many repeats we need to count this bind as pressed.
	 * this is an ugly thing derived from old hardcoded keybinds. */
	uint16_t repeats_needed;
};

struct keybinds {
	/* XXX we should be able to make as many binds as we want */
	struct keybind b[5];

	uint8_t n;
};

struct keybind_section {
	struct keybinds *binds;

	size_t max_binds;

	size_t num_binds;
};

static struct keybinds keybinds_midi[KEYBIND_BIND_MIDI_MAX_];
static struct keybinds keybinds_load_sample[KEYBIND_BIND_LOAD_SAMPLE_MAX_];
static struct keybinds keybinds_load_stereo_sample[KEYBIND_BIND_LOAD_STEREO_SAMPLE_MAX_];
static struct keybinds keybinds_message_editor[KEYBIND_BIND_MESSAGE_EDITOR_MAX_];
static struct keybinds keybinds_waterfall[KEYBIND_BIND_WATERFALL_MAX_];
static struct keybinds keybinds_time_information[KEYBIND_BIND_TIME_INFORMATION_MAX_];
static struct keybinds keybinds_load_module[KEYBIND_BIND_LOAD_MODULE_MAX_];
static struct keybinds keybinds_palette_editor[KEYBIND_BIND_PALETTE_EDITOR_MAX_];
static struct keybinds keybinds_order_list[KEYBIND_BIND_ORDER_LIST_MAX_];
static struct keybinds keybinds_order_list_pan[KEYBIND_BIND_ORDER_LIST_PAN_MAX_];
static struct keybinds keybinds_info_page[KEYBIND_BIND_INFO_PAGE_MAX_];
static struct keybinds keybinds_instrument_list[KEYBIND_BIND_INSTRUMENT_LIST_MAX_];
static struct keybinds keybinds_instrument_note_translation[KEYBIND_BIND_INSTRUMENT_NOTE_TRANSLATION_MAX_];
static struct keybinds keybinds_instrument_envelope[KEYBIND_BIND_INSTRUMENT_ENVELOPE_MAX_];
static struct keybinds keybinds_sample_list[KEYBIND_BIND_SAMPLE_LIST_MAX_];
static struct keybinds keybinds_pattern_editor[KEYBIND_BIND_PATTERN_EDITOR_MAX_];
static struct keybinds keybinds_track_view[KEYBIND_BIND_TRACK_VIEW_MAX_];
static struct keybinds keybinds_block_functions[KEYBIND_BIND_BLOCK_FUNCTIONS_MAX_];
static struct keybinds keybinds_playback_functions[KEYBIND_BIND_PLAYBACK_FUNCTIONS_MAX_];
static struct keybinds keybinds_file_list[KEYBIND_BIND_FILE_LIST_MAX_];
static struct keybinds keybinds_global[KEYBIND_BIND_GLOBAL_MAX_];
static struct keybinds keybinds_dialog[KEYBIND_BIND_DIALOG_MAX_];
static struct keybinds keybinds_notes[KEYBIND_BIND_NOTES_MAX_];

static struct keybind_section keybind_sections[KEYBIND_SECTION_MAX_] = {
	{keybinds_midi, KEYBIND_BIND_MIDI_MAX_},
	{keybinds_load_sample, KEYBIND_BIND_LOAD_SAMPLE_MAX_},
	{keybinds_load_stereo_sample, KEYBIND_BIND_LOAD_STEREO_SAMPLE_MAX_},
	{keybinds_message_editor, KEYBIND_BIND_MESSAGE_EDITOR_MAX_},
	{keybinds_waterfall, KEYBIND_BIND_WATERFALL_MAX_},
	{keybinds_time_information, KEYBIND_BIND_TIME_INFORMATION_MAX_},
	{keybinds_load_module, KEYBIND_BIND_LOAD_MODULE_MAX_},
	{keybinds_palette_editor, KEYBIND_BIND_PALETTE_EDITOR_MAX_},
	{keybinds_order_list, KEYBIND_BIND_ORDER_LIST_MAX_},
	{keybinds_order_list_pan, KEYBIND_BIND_ORDER_LIST_PAN_MAX_},
	{keybinds_info_page, KEYBIND_BIND_INFO_PAGE_MAX_},
	{keybinds_instrument_list, KEYBIND_BIND_INSTRUMENT_LIST_MAX_},
	{keybinds_instrument_note_translation, KEYBIND_BIND_INSTRUMENT_NOTE_TRANSLATION_MAX_},
	{keybinds_instrument_envelope, KEYBIND_BIND_INSTRUMENT_ENVELOPE_MAX_},
	{keybinds_sample_list, KEYBIND_BIND_SAMPLE_LIST_MAX_},
	{keybinds_pattern_editor, KEYBIND_BIND_PATTERN_EDITOR_MAX_},
	{keybinds_track_view, KEYBIND_BIND_TRACK_VIEW_MAX_},
	{keybinds_block_functions, KEYBIND_BIND_BLOCK_FUNCTIONS_MAX_},
	{keybinds_playback_functions, KEYBIND_BIND_PLAYBACK_FUNCTIONS_MAX_},
	{keybinds_file_list, KEYBIND_BIND_FILE_LIST_MAX_},
	{keybinds_global, KEYBIND_BIND_GLOBAL_MAX_},
	{keybinds_dialog, KEYBIND_BIND_DIALOG_MAX_},
	{keybinds_notes, KEYBIND_BIND_NOTES_MAX_},
};

/* ------------------------------------------------------------------------ */
/* initialization code */

struct keybind_bind_info {
	const char *name;

	/* The default value, e.g. "Ctrl-Shift-A" */
	const char *def;
};

static const struct keybind_bind_info keybind_bind_midi_info[KEYBIND_BIND_MIDI_MAX_] = {
	[KEYBIND_BIND_MIDI_TOGGLE_PORT] = {"Toggle port", "SPACE"},
};

static const struct keybind_bind_info keybind_bind_load_sample_info[KEYBIND_BIND_LOAD_SAMPLE_MAX_] = {
	[KEYBIND_BIND_LOAD_SAMPLE_TOGGLE_MULTICHANNEL] = {"Toggle multichannel mode", "Alt-N"},
};

static const struct keybind_bind_info keybind_bind_load_stereo_sample_info[KEYBIND_BIND_LOAD_STEREO_SAMPLE_MAX_] = {
	[KEYBIND_BIND_LOAD_STEREO_SAMPLE_LOAD_LEFT] = {"Load left channel", "L"},
	[KEYBIND_BIND_LOAD_STEREO_SAMPLE_LOAD_RIGHT] = {"Load right channel", "R"},
	[KEYBIND_BIND_LOAD_STEREO_SAMPLE_LOAD_BOTH] = {"Load both channels", "B,S"},
};

static const struct keybind_bind_info keybind_bind_global_info[KEYBIND_BIND_GLOBAL_MAX_] = {
	[KEYBIND_BIND_GLOBAL_HELP] = {"Help (context sensitive!)", "F1"},
	[KEYBIND_BIND_GLOBAL_MIDI] = {"MIDI screen", "Shift+F1"},
	[KEYBIND_BIND_GLOBAL_SYSTEM_CONFIGURE] = {"System configuration", "Ctrl+F1"},
	[KEYBIND_BIND_GLOBAL_PATTERN_EDIT] = {"Pattern editor / pattern editor options", "F2"},
	[KEYBIND_BIND_GLOBAL_SAMPLE_LIST] = {"Sample list", "F3"},
	[KEYBIND_BIND_GLOBAL_SAMPLE_LIBRARY] = {"Sample library", "Ctrl+F3"},
	[KEYBIND_BIND_GLOBAL_INSTRUMENT_LIST] = {"Instrument list", "F4"},
	[KEYBIND_BIND_GLOBAL_INSTRUMENT_LIBRARY] = {"Instrument library", "Ctrl+F4"},
	[KEYBIND_BIND_GLOBAL_PLAY_INFORMATION_OR_PLAY_SONG] = {"Play information or play song", "F5"},
	[KEYBIND_BIND_GLOBAL_PLAY_SONG] = {"Play song", "Ctrl+F5"},
	[KEYBIND_BIND_GLOBAL_PREFERENCES] = {"Preferences", "Shift+F5"},
	[KEYBIND_BIND_GLOBAL_PLAY_CURRENT_PATTERN] = {"Play current pattern", "F6"},
	[KEYBIND_BIND_GLOBAL_PLAY_SONG_FROM_ORDER] = {"Play song from current order", "Shift+F6"},
	[KEYBIND_BIND_GLOBAL_PLAY_SONG_FROM_MARK] = {"Play from mark / current row", "F7"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_PLAYBACK] = {"Pause / resume playback", "Shift+F8"},
	[KEYBIND_BIND_GLOBAL_STOP_PLAYBACK] = {"Stop playback", "F8"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_PLAYBACK_TRACING] = {"Toggle playback tracing", "SCROLLLOCK,Ctrl+SCROLLLOCK,Ctrl+F"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_MIDI_INPUT] = {"Toggle MIDI input", "Alt+SCROLLLOCK"},
	[KEYBIND_BIND_GLOBAL_LOAD_MODULE] = {"Load module", "F9,Ctrl+L"},
	[KEYBIND_BIND_GLOBAL_MESSAGE_EDITOR] = {"Message editor", "Shift+F9"},
	[KEYBIND_BIND_GLOBAL_SAVE_MODULE] = {"Save module", "F10,Ctrl+W"},
	[KEYBIND_BIND_GLOBAL_EXPORT_MODULE] = {"Export module (to WAV, AIFF)", "Shift+F10"},
	[KEYBIND_BIND_GLOBAL_ORDER_LIST_PANNING] = {"Order list and panning", "F11"},
	[KEYBIND_BIND_GLOBAL_ORDER_LIST_VOLUME] = {"Order list and channel volume", "F11 x2"}, /* hmm :) */
	[KEYBIND_BIND_GLOBAL_SCHISM_LOGGING] = {"Schism logging", "Ctrl+F11"},
	[KEYBIND_BIND_GLOBAL_ORDER_LIST_LOCK] = {"Lock/unlock order list", "Alt+F11"},
	[KEYBIND_BIND_GLOBAL_SONG_VARIABLES] = {"Song variables & directory configuration", "F12"},
	[KEYBIND_BIND_GLOBAL_PALETTE_CONFIG] = {"Palette configuration", "Ctrl+F12"},
	[KEYBIND_BIND_GLOBAL_FONT_EDITOR] = {"Font editor", "Shift+F12"},
	[KEYBIND_BIND_GLOBAL_WATERFALL] = {"Waterfall", "Alt+F12"},
	[KEYBIND_BIND_GLOBAL_TIME_INFORMATION] = {"Time Information", "LShift+LAlt+RAlt+RCtrl+Pause,Ctrl+Shift+T"},
	[KEYBIND_BIND_GLOBAL_OCTAVE_DECREASE] = {"Decrease octave", "KP_DIVIDE,Alt+HOME"},
	[KEYBIND_BIND_GLOBAL_OCTAVE_INCREASE] = {"Increase octave", "KP_MULTIPLY,Alt+END"},
	[KEYBIND_BIND_GLOBAL_DECREASE_PLAYBACK_SPEED] = {"Decrease playback speed", "Shift+LEFTBRACKET"},
	[KEYBIND_BIND_GLOBAL_INCREASE_PLAYBACK_SPEED] = {"Increase playback speed", "Shift+RIGHTBRACKET"},
	[KEYBIND_BIND_GLOBAL_DECREASE_PLAYBACK_TEMPO] = {"Decrease playback tempo", "Ctrl+LEFTBRACKET"},
	[KEYBIND_BIND_GLOBAL_INCREASE_PLAYBACK_TEMPO] = {"Increase playback tempo", "Ctrl+RIGHTBRACKET"},
	[KEYBIND_BIND_GLOBAL_DECREASE_GLOBAL_VOLUME] = {"Decrease global volume", "LEFTBRACKET"},
	[KEYBIND_BIND_GLOBAL_INCREASE_GLOBAL_VOLUME] = {"Increase global volume", "RIGHTBRACKET"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_CHANNEL_1] = {"Toggle channel 1", "Alt+F1"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_CHANNEL_2] = {"Toggle channel 2", "Alt+F2"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_CHANNEL_3] = {"Toggle channel 3", "Alt+F3"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_CHANNEL_4] = {"Toggle channel 4", "Alt+F4"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_CHANNEL_5] = {"Toggle channel 5", "Alt+F5"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_CHANNEL_6] = {"Toggle channel 6", "Alt+F6"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_CHANNEL_7] = {"Toggle channel 7", "Alt+F7"},
	[KEYBIND_BIND_GLOBAL_TOGGLE_CHANNEL_8] = {"Toggle channel 8", "Alt+F8"},
	[KEYBIND_BIND_GLOBAL_MOUSE_GRAB] = {"Toggle mouse and keyboard grab", "Ctrl+D"},
	[KEYBIND_BIND_GLOBAL_DISPLAY_RESET] = {"Refresh screen and reset cache identification", "Ctrl+E"},
	[KEYBIND_BIND_GLOBAL_GO_TO_TIME] = {"Go to order / pattern / row given time", "Ctrl+G"},
	[KEYBIND_BIND_GLOBAL_AUDIO_RESET] = {"Reinitialize sound driver", "Ctrl+I"},
	[KEYBIND_BIND_GLOBAL_MOUSE] = {"Toggle mouse cursor", "Ctrl+M"},
	[KEYBIND_BIND_GLOBAL_NEW_SONG] = {"New song", "Ctrl+N"},
	[KEYBIND_BIND_GLOBAL_CALCULATE_SONG_LENGTH] = {"Calculate approximate song length", "Ctrl+P"},
	[KEYBIND_BIND_GLOBAL_QUIT] = {"Quit Schism Tracker", "Ctrl+Q"},
	[KEYBIND_BIND_GLOBAL_QUIT_NO_CONFIRM] = {"Quit without confirmation", "Ctrl+Shift+Q"},
	[KEYBIND_BIND_GLOBAL_SAVE] = {"Save current song", "Ctrl+S"},
	[KEYBIND_BIND_GLOBAL_PREVIOUS_ORDER] = {"Previous order (while playing, not on pattern edit)", "Ctrl+LEFT"},
	[KEYBIND_BIND_GLOBAL_NEXT_ORDER] = {"Next order (while playing, not on pattern edit)", "Ctrl+RIGHT"},
	[KEYBIND_BIND_GLOBAL_FULLSCREEN] = {"Toggle fullscreen", "Ctrl+Alt+ENTER"},
	[KEYBIND_BIND_GLOBAL_OPEN_MENU] = {"Open menu", "ESCAPE"},

	/* err, these should be in a separate "Navigation" section */
	[KEYBIND_BIND_GLOBAL_NAV_LEFT] = {"Navigate left", "LEFT"},
	[KEYBIND_BIND_GLOBAL_NAV_RIGHT] = {"Navigate right", "RIGHT"},
	[KEYBIND_BIND_GLOBAL_NAV_UP] = {"Navigate up", "UP"},
	[KEYBIND_BIND_GLOBAL_NAV_DOWN] = {"Navigate down", "DOWN"},
	[KEYBIND_BIND_GLOBAL_NAV_PAGE_UP] = {"Navigate page up", "PAGEUP"},
	[KEYBIND_BIND_GLOBAL_NAV_PAGE_DOWN] = {"Navigate page down", "PAGEDOWN"},
	[KEYBIND_BIND_GLOBAL_NAV_ACCEPT] = {"Navigate accept", "ENTER"},
	[KEYBIND_BIND_GLOBAL_NAV_CANCEL] = {"Navigate cancel", "ESCAPE"},
	[KEYBIND_BIND_GLOBAL_NAV_HOME] = {"Navigate home (start of line/first in list)", "HOME"},
	[KEYBIND_BIND_GLOBAL_NAV_END] = {"Navigate end (end of line/last in list)", "END"},
	[KEYBIND_BIND_GLOBAL_NAV_TAB] = {"Navigate to next item right", "TAB"},
	[KEYBIND_BIND_GLOBAL_NAV_BACKTAB] = {"Navigate to next item left", "Shift+TAB"},

	/* lol ew; this also should have its own section :) */
	[KEYBIND_BIND_GLOBAL_THUMBBAR_MAX_VALUE] = {"Thumbbar max value", "END"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_INCREASE_VALUE] = {"Thumbbar increase value", "RIGHT"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_INCREASE_VALUE_2X] = {"Thumbbar increase value 2x", "Ctrl+RIGHT"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_INCREASE_VALUE_4X] = {"Thumbbar increase value 4x", "Shift+RIGHT"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_INCREASE_VALUE_8X] = {"Thumbbar increase value 8x", "Alt+RIGHT"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_MIN_VALUE] = {"Thumbbar min value", "HOME"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_DECREASE_VALUE] = {"Thumbbar decrease value", "LEFT"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_DECREASE_VALUE_2X] = {"Thumbbar decrease value 2x", "Ctrl+LEFT"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_DECREASE_VALUE_4X] = {"Thumbbar decrease value 4x", "Shift+LEFT"},
	[KEYBIND_BIND_GLOBAL_THUMBBAR_DECREASE_VALUE_8X] = {"Thumbbar decrease value 8x", "Alt+LEFT"},

	[KEYBIND_BIND_GLOBAL_NUMENTRY_INCREASE_VALUE] = {"Number increase value", "KP_PLUS,Shift+EQUALS"},
	[KEYBIND_BIND_GLOBAL_NUMENTRY_DECREASE_VALUE] = {"Number decrease value 8x", "KP_MINUS,Shift+MINUS"},
};

struct keybind_section_info {
	const char *name;

	const struct keybind_bind_info *binds;

	uint32_t num_binds;
};

static const struct keybind_section_info keybind_sections_info[KEYBIND_SECTION_MAX_] = {
	[KEYBIND_SECTION_MIDI] = {"MIDI", keybind_bind_midi_info, ARRAY_SIZE(keybind_bind_midi_info)},
	[KEYBIND_SECTION_LOAD_SAMPLE] = {"Load Sample", keybind_bind_load_sample_info, ARRAY_SIZE(keybind_bind_load_sample_info)},
	[KEYBIND_SECTION_LOAD_STEREO_SAMPLE] = {"Load Stereo Sample", keybind_bind_load_stereo_sample_info, ARRAY_SIZE(keybind_bind_load_stereo_sample_info)},
	[KEYBIND_SECTION_MESSAGE_EDITOR] = {"Message Editor", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_WATERFALL] = {"Waterfall", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_TIME_INFORMATION] = {"Time Information", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_LOAD_MODULE] = {"Load Module", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_PALETTE_EDITOR] = {"Palette Editor", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_ORDER_LIST] = {"Order List", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_ORDER_LIST_PAN] = {"Order List/Panning", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_INFO_PAGE] = {"Info Page", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_INSTRUMENT_LIST] = {"Instrument List", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_INSTRUMENT_NOTE_TRANSLATION] = {"Instrument Note Translation", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_INSTRUMENT_ENVELOPE] = {"Instrument Envelope", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_SAMPLE_LIST] = {"Sample List", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_PATTERN_EDITOR] = {"Pattern Editor", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_TRACK_VIEW] = {"Track View", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_BLOCK_FUNCTIONS] = {"Block Functions", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_PLAYBACK_FUNCTIONS] = {"Playback Functions", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_FILE_LIST] = {"File List", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_GLOBAL] = {"Global", keybind_bind_global_info, ARRAY_SIZE(keybind_bind_global_info)},
	[KEYBIND_SECTION_DIALOG] = {"Dialog", NULL, 0 /* TODO */},
	[KEYBIND_SECTION_NOTES] = {"Notes", NULL, 0 /* TODO */},
};

static const char *keybinds_get_section_name(int section)
{
	SCHISM_RUNTIME_ASSERT(section < ARRAY_SIZE(keybind_sections_info), "section must not overflow");
	SCHISM_RUNTIME_ASSERT(keybind_sections_info[section].name, "section must have a name");

	return keybind_sections_info[section].name;
}

static const char *keybinds_get_bind_name(int section, int bind)
{
	SCHISM_RUNTIME_ASSERT(section < ARRAY_SIZE(keybind_sections_info), "section must not overflow");
	SCHISM_RUNTIME_ASSERT(keybind_sections_info[section].binds, "section must have its own lookup");
	SCHISM_RUNTIME_ASSERT(keybind_sections_info[section].binds[bind].name, "bind must be filled within the lookup");

	return keybind_sections_info[section].binds[bind].name;
}

static int keybinds_parse_bind(const char *str, struct keybind *b)
{
	/* '+' and '-' are both treated as valid delimiters, because
	 * both are used within Schism ('+' is used for the menu bars,
	 * while '-' is used in the program itself) */
	static const char *delims = "+-";
	static const char *whitespace = " \t";
	const char *last_pch, *pch;

	for (last_pch = str, pch = str; pch; last_pch = pch + 1) {
		int r;
		uint16_t mod;
		char *trimmed;

		pch = strpbrk(last_pch, delims);
		if (!pch)
			/* ok, we're on the last one... skip until a space */
			pch = strpbrk(last_pch, whitespace);

		trimmed = (pch) ? strn_dup(last_pch, pch - last_pch) : str_dup(last_pch);
		str_trim(trimmed);

		if (keybinds_parse_modkey(trimmed, &mod)) {
			b->modifiers |= mod;
		} else {
			/* we're on a keycode/scancode?
			 *
			 * note: we should always return (or goto the x2 parsing branch),
			 * modifiers are required to be in front of the keycode/scancode */
			if (keybinds_parse_keycode(trimmed, &b->code.key)) {
				b->type = KEYBIND_TYPE_KEYCODE;
				free(trimmed);
				goto KB_have_code;
			}

			if (keybinds_parse_scancode(trimmed, &b->code.scan)) {
				b->type = KEYBIND_TYPE_SCANCODE;
				free(trimmed);
				goto KB_have_code;
			}

			/* invalid */
			free(trimmed);
			return 0;
		}

		free(trimmed);
	}

	/* wut? */
	return 0;

KB_have_code:
	if (!pch)
		return 1;

	/* increment the pointer until we're at the end of the spaces */
	while (*pch && isspace(*pch))
		pch++;

	if (!*pch)
		return 1; /* we're done here */

	if (*pch == 'x') {
		long x;

		pch++;

		x = strtol(pch, NULL, 10);

		if (!x)
			return 0; /* invalid */

		b->repeats = x;

		return 1;
	}

	return 1; /* ...??? */
}

static int keybinds_parse_binds(const char *str, struct keybinds *b)
{
	int r = 1;
	const char *last_pch, *pch;

	for (last_pch = str, pch = str; pch; last_pch = pch + 1) {
		char *trimmed;

		pch = strchr(last_pch, ',');

		trimmed = (pch) ? strn_dup(last_pch, pch - last_pch) : str_dup(last_pch);
		str_trim(trimmed);

		SCHISM_RUNTIME_ASSERT(b->n + 1 < ARRAY_SIZE(b->b),
			"not enough space for new keybind");

		if (keybinds_parse_bind(trimmed, &b->b[b->n])) {
			b->n++;
		} else {
			r = 0;
		}

		free(trimmed);
	}

	return r;
}

static int keybinds_handle_cfg_entry(cfg_file_t *cfg, int section, int bind)
{
	const char *section_name = keybinds_get_section_name(section);
	const char *bind_name = keybinds_get_bind_name(section, bind);
	const char *str, *def;
	struct keybinds *b;

	SCHISM_RUNTIME_ASSERT(section < ARRAY_SIZE(keybind_sections),
		"keybinds section is out of bounds");
	SCHISM_RUNTIME_ASSERT(bind == keybind_sections[section].num_binds,
		"keybinds must be parsed in order as in keybinds.h");
	SCHISM_RUNTIME_ASSERT(keybind_sections_info[section].binds[bind].def,
		"a default keybind is required");
	SCHISM_RUNTIME_ASSERT(bind < keybind_sections[section].max_binds,
		"keybinds bind is out of bounds");

	b = &keybind_sections[section].binds[bind];
	def = keybind_sections_info[section].binds[bind].def;

	{
		/* prefix any keybind config options with "Keybinds/", as to
		 * prevent any name collision */
		char *s, *ss;
		size_t i, len;

		if (asprintf(&s, "Keybinds/%s", section_name) < 0)
			return -1;

		len = strlen(bind_name);
		ss = strn_dup(bind_name, len);

		/* convert to lowercase, and replace any non-alphanumeric characters with underscores */
		for (i = 0; i < len; i++)
			ss[i] = (!isalnum(ss[i])) ? '_' : tolower(ss[i]);

		str = cfg_get_string(cfg, s, ss, NULL, 0, NULL);

		/*log_appendf(1, "checking config, %s, %s: %s", s, ss, str ? str : "(null)");*/

		free(s);
		free(ss);
	}

	if (str) {
		if (keybinds_parse_binds(str, b)) {
			keybind_sections[section].num_binds++;
			return 0;
		}

		log_appendf(1, "KEYBINDS: Parsing keybind %s/%s failed: '%s'",
			section_name, bind_name, str);
	}

	if (keybinds_parse_binds(def, b)) {
		keybind_sections[section].num_binds++;
		return 0;
	}

	printf("%s\n", def);

	SCHISM_RUNTIME_ASSERT(0, "default keybind is bogus");

	return -1;
}

int keybinds_init(void)
{
	char *ptr;
	cfg_file_t cfg;
	int i, j;

	ptr = dmoz_path_concat(cfg_dir_dotschism, "config");
	cfg_init(&cfg, ptr);
	free(ptr);

	/* keybinds are always parsed in the order they are defined.
	 * this is by design, and changing it will probably break everything. */
	for (i = 0; i < ARRAY_SIZE(keybind_sections_info); i++)
		for (j = 0; j < keybind_sections_info[i].num_binds; j++)
			keybinds_handle_cfg_entry(&cfg, i, j);

	cfg_free(&cfg);

	return 0;
}

/* ------------------------------------------------------------------------ */
/* event handler */

static inline int keybinds_modifier_held(const struct keybind *b, schism_keymod_t mod)
{
	/* only see the modifiers we care about... */
	mod &= (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_SHIFT);

	/* now, remove any bits that we need */
#define CHECK_MODIFIER(x) \
	do { \
		if (b->modifiers & KEYBIND_MOD_##x) { \
			if (!(mod & SCHISM_KEYMOD_##x)) \
				return 0; \
		\
			mod &= ~(SCHISM_KEYMOD_##x); \
		} \
	} while (0)

#define CHECK_LRMODIFIERS(x) \
	CHECK_MODIFIER(x); \
	CHECK_MODIFIER(L##x); \
	CHECK_MODIFIER(R##x)

	CHECK_LRMODIFIERS(CTRL);
	CHECK_LRMODIFIERS(SHIFT);
	CHECK_LRMODIFIERS(ALT);

#undef CHECK_MODIFIER
#undef CHECK_LRMODIFIERS

	/* if we have ANYTHING left, we have extra modifiers being held.
	 * this means there is probably some other keybind that this should
	 * map to, so punt. */
	if (mod)
		return 0;

	return 1;
}

static inline int keybind_check_code(const struct keybind *b, const struct key_event *kk)
{
	switch (b->type) {
	case KEYBIND_TYPE_KEYCODE:  return (kk->sym      == b->code.key);
	case KEYBIND_TYPE_SCANCODE: return (kk->scancode == b->code.scan);
	}

	SCHISM_UNREACHABLE;
	return 0;
}

static int keybind_check(const struct keybind *b, const struct key_event *kk)
{
	return (keybind_check_code(b, kk) && keybinds_modifier_held(b, kk->mod));
}

void keybinds_event(const struct key_event *kk)
{
	size_t i, j, k;

	for (i = 0; i < ARRAY_SIZE(keybind_sections); i++) {
		for (j = 0; j < keybind_sections[i].num_binds; j++) {
			for (k = 0; k < keybind_sections[i].binds[j].n; k++) {
				struct keybind *b = &keybind_sections[i].binds[j].b[k];

				if (keybind_check(b, kk)) {
					switch (kk->state) {
					case KEY_PRESS:
						if (b->state & KEYBIND_STATE_RELEASED) {
							b->state |= KEYBIND_STATE_REPEATED;
							/* maybe handle overflow here? */
							b->repeats++;
						}

						b->state |= KEYBIND_STATE_PRESSED;
						b->state &= ~KEYBIND_STATE_RELEASED;
						break;
					case KEY_RELEASE:
						b->state |= KEYBIND_STATE_RELEASED;
						b->state &= ~KEYBIND_STATE_PRESSED;
						break;
					default:
						SCHISM_UNREACHABLE;
						break;
					}
				} else {
					/* keybind isn't pressed or released */
					b->state = 0;
					b->repeats = 0;
				}

				/*log_appendf(1, "KEYBIND/%" PRIuSZ "/%" PRIuSZ ": state: %" PRIx8 ", repeats: %" PRIu16,
					i, j, b->state, b->repeats);*/
			}
		}
	}
}

/* ------------------------------------------------------------------------ */
/* ahem. */

static int keybinds_check_state(int section, int bind, uint8_t x)
{
	size_t i;

	SCHISM_RUNTIME_ASSERT(section < ARRAY_SIZE(keybind_sections),
		"keybinds section is out of bounds");
	SCHISM_RUNTIME_ASSERT(bind < keybind_sections[section].num_binds,
		"keybinds bind for section is out of bounds");

	for (i = 0; i < keybind_sections[section].binds[bind].n; i++) {
		struct keybind *b = &keybind_sections[section].binds[bind].b[i];

		if ((b->state & x) && (b->repeats >= b->repeats_needed))
			return 1;
	}

	return 0;
}

/* NOTE: would it be more sane behavior for repeats to be considered based
 * off of the modulo of the repeats needed? I know at least the order/pan
 * stuff does that... oh well. */
int keybinds_pressed(int section, int bind)
{
	return keybinds_check_state(section, bind, KEYBIND_STATE_PRESSED);
}

int keybinds_released(int section, int bind)
{
	return keybinds_check_state(section, bind, KEYBIND_STATE_RELEASED);
}

int keybinds_handled(int section, int bind)
{
	return keybinds_check_state(section, bind, KEYBIND_STATE_PRESSED | KEYBIND_STATE_RELEASED);
}
