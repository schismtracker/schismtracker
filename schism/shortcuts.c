#include "headers.h"

#include "config-parser.h"
#include "keyboard.h"
#include "shortcuts.h"

#define SHORTCUT_MOD_MASK (SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT | SCHISM_KEYMOD_SHIFT)

enum shortcut_action {
	SHORTCUT_SHOW_HELP = 0,
	SHORTCUT_OPEN_MIDI_CONFIG,
	SHORTCUT_OPEN_CONFIG,
	SHORTCUT_OPEN_SHORTCUT_CONFIG,
	SHORTCUT_PATTERN_EDITOR,
	SHORTCUT_SAMPLE_LIST,
	SHORTCUT_SAMPLE_LIBRARY,
	SHORTCUT_INSTRUMENT_LIST,
	SHORTCUT_INSTRUMENT_LIBRARY,
	SHORTCUT_INFO_PAGE,
	SHORTCUT_PREFERENCES_PAGE,
	SHORTCUT_PLAY_SONG,
	SHORTCUT_PLAY_PATTERN,
	SHORTCUT_PLAY_ORDER,
	SHORTCUT_PLAY_FROM_MARK,
	SHORTCUT_PAUSE_TOGGLE,
	SHORTCUT_STOP_SONG,
	SHORTCUT_LOAD_MODULE,
	SHORTCUT_SAVE_MODULE,
	SHORTCUT_EXPORT_MODULE,
	SHORTCUT_SONG_MESSAGE,
	SHORTCUT_SONG_VARIABLES,
	SHORTCUT_LOG_PAGE,
	SHORTCUT_PALETTE_PAGE,
	SHORTCUT_FONT_EDITOR,
	SHORTCUT_NEW_SONG,
	SHORTCUT_LOAD_MODULE_ALT_L,
	SHORTCUT_LOAD_MODULE_ALT_R,
	SHORTCUT_SAVE_MODULE_ALT_W,
	SHORTCUT_SAVE_SONG_OR_AS,
	SHORTCUT_EXIT_PROMPT,
	SHORTCUT_EXIT_NOW,
	SHORTCUT_SONG_TIMEJUMP,
	SHORTCUT_SONG_LENGTH,
	SHORTCUT_TOGGLE_FULLSCREEN,
	SHORTCUT_TOGGLE_MOUSE_CURSOR,
	SHORTCUT_TOGGLE_INPUT_GRAB,
	SHORTCUT_REINIT_AUDIO,
	SHORTCUT_REFRESH_DISPLAY,
	SHORTCUT_TIME_INFO_PAGE,
	SHORTCUT_PLAYBACK_TRACE_TOGGLE,
	SHORTCUT_MIDI_INPUT_TOGGLE,
	SHORTCUT_SPEED_DOWN,
	SHORTCUT_SPEED_UP,
	SHORTCUT_TEMPO_DOWN,
	SHORTCUT_TEMPO_UP,
	SHORTCUT_GLOBAL_VOL_DOWN,
	SHORTCUT_GLOBAL_VOL_UP,
	SHORTCUT_OCTAVE_DOWN,
	SHORTCUT_OCTAVE_UP,
	SHORTCUT_PATEDIT_PLAY_FROM_ROW,
	SHORTCUT_PATEDIT_SET_PLAY_MARK,
	SHORTCUT_PATEDIT_BLOCK_START,
	SHORTCUT_PATEDIT_BLOCK_END,
	SHORTCUT_PATEDIT_BLOCK_MARK_ALL,
	SHORTCUT_PATEDIT_BLOCK_UNMARK,
	SHORTCUT_PATEDIT_BLOCK_COPY,
	SHORTCUT_PATEDIT_BLOCK_PASTE,
	SHORTCUT_PATEDIT_BLOCK_PASTE_OVERWRITE,
	SHORTCUT_PATEDIT_BLOCK_CUT,
	SHORTCUT_PATEDIT_UNDO,
	SHORTCUT_PATEDIT_NOTE_FADE,
	SHORTCUT_CHAN_TOGGLE_1,
	SHORTCUT_CHAN_TOGGLE_2,
	SHORTCUT_CHAN_TOGGLE_3,
	SHORTCUT_CHAN_TOGGLE_4,
	SHORTCUT_CHAN_TOGGLE_5,
	SHORTCUT_CHAN_TOGGLE_6,
	SHORTCUT_CHAN_TOGGLE_7,
	SHORTCUT_CHAN_TOGGLE_8,
	SHORTCUT_COUNT
};

struct shortcut_binding {
	const char *name;
	const char *cfg_key;
	int default_sym;
	int default_mod;
	int sym;
	int mod;
};

static struct shortcut_binding shortcut_bindings[SHORTCUT_COUNT] = {
	{ "Show Help", "show_help", SCHISM_KEYSYM_F1, 0, SCHISM_KEYSYM_F1, 0 },
	{ "Open MIDI Config", "open_midi_config", SCHISM_KEYSYM_F1, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_F1, SCHISM_KEYMOD_SHIFT },
	{ "Open Configuration", "open_config", SCHISM_KEYSYM_F1, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_F1, SCHISM_KEYMOD_CTRL },
	{ "Open Shortcut Config", "open_shortcut_config", SCHISM_KEYSYM_F1, SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_F1, SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_SHIFT },
	{ "Pattern Editor", "pattern_editor", SCHISM_KEYSYM_F2, 0, SCHISM_KEYSYM_F2, 0 },
	{ "Sample List (Library)", "sample_library", SCHISM_KEYSYM_F3, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_F3, SCHISM_KEYMOD_CTRL },
	{ "Sample List", "sample_list", SCHISM_KEYSYM_F3, 0, SCHISM_KEYSYM_F3, 0 },
	{ "Instrument List (Library)", "instrument_library", SCHISM_KEYSYM_F4, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_F4, SCHISM_KEYMOD_CTRL },
	{ "Instrument List", "instrument_list", SCHISM_KEYSYM_F4, 0, SCHISM_KEYSYM_F4, 0 },
	{ "Info Page", "info_page", SCHISM_KEYSYM_F5, 0, SCHISM_KEYSYM_F5, 0 },
	{ "Preferences Page", "preferences_page", SCHISM_KEYSYM_F5, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_F5, SCHISM_KEYMOD_SHIFT },
	{ "Play Song", "play_song", SCHISM_KEYSYM_F5, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_F5, SCHISM_KEYMOD_CTRL },
	{ "Play Pattern", "play_pattern", SCHISM_KEYSYM_F6, 0, SCHISM_KEYSYM_F6, 0 },
	{ "Play from Order", "play_order", SCHISM_KEYSYM_F6, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_F6, SCHISM_KEYMOD_SHIFT },
	{ "Play from Mark", "play_from_mark", SCHISM_KEYSYM_F7, 0, SCHISM_KEYSYM_F7, 0 },
	{ "Pause", "pause_toggle", SCHISM_KEYSYM_F8, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_F8, SCHISM_KEYMOD_SHIFT },
	{ "Stop Song", "stop_song", SCHISM_KEYSYM_F8, 0, SCHISM_KEYSYM_F8, 0 },
	{ "Load Module", "load_module", SCHISM_KEYSYM_F9, 0, SCHISM_KEYSYM_F9, 0 },
	{ "Save Module", "save_module", SCHISM_KEYSYM_F10, 0, SCHISM_KEYSYM_F10, 0 },
	{ "Export Module", "export_module", SCHISM_KEYSYM_F11, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_F11, SCHISM_KEYMOD_SHIFT },
	{ "Song Message", "song_message", SCHISM_KEYSYM_F9, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_F9, SCHISM_KEYMOD_SHIFT },
	{ "Song Variables", "song_variables", SCHISM_KEYSYM_F12, 0, SCHISM_KEYSYM_F12, 0 },
	{ "Log Page", "log_page", SCHISM_KEYSYM_F11, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_F11, SCHISM_KEYMOD_CTRL },
	{ "Palette Page", "palette_page", SCHISM_KEYSYM_F12, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_F12, SCHISM_KEYMOD_CTRL },
	{ "Font Editor", "font_editor", SCHISM_KEYSYM_F12, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_F12, SCHISM_KEYMOD_SHIFT },
	{ "New Song", "new_song", SCHISM_KEYSYM_n, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_n, SCHISM_KEYMOD_CTRL },
	{ "Load Module (Ctrl+L)", "load_module_ctrl_l", SCHISM_KEYSYM_l, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_l, SCHISM_KEYMOD_CTRL },
	{ "Load Module (Ctrl+R)", "load_module_ctrl_r", SCHISM_KEYSYM_r, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_r, SCHISM_KEYMOD_CTRL },
	{ "Save Module (Ctrl+W)", "save_module_ctrl_w", SCHISM_KEYSYM_w, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_w, SCHISM_KEYMOD_CTRL },
	{ "Save Song", "save_song", SCHISM_KEYSYM_s, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_s, SCHISM_KEYMOD_CTRL },
	{ "Exit Prompt", "exit_prompt", SCHISM_KEYSYM_q, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_q, SCHISM_KEYMOD_CTRL },
	{ "Exit Now", "exit_now", SCHISM_KEYSYM_q, SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_q, SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_SHIFT },
	{ "Song Timejump", "song_timejump", SCHISM_KEYSYM_g, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_g, SCHISM_KEYMOD_CTRL },
	{ "Song Length", "song_length", SCHISM_KEYSYM_p, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_p, SCHISM_KEYMOD_CTRL },
	{ "Toggle Fullscreen", "toggle_fullscreen", SCHISM_KEYSYM_RETURN, SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_RETURN, SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT },
	{ "Toggle Mouse Cursor", "toggle_mouse_cursor", SCHISM_KEYSYM_m, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_m, SCHISM_KEYMOD_CTRL },
	{ "Toggle Input Grab", "toggle_input_grab", SCHISM_KEYSYM_d, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_d, SCHISM_KEYMOD_CTRL },
	{ "Reinit Audio", "reinit_audio", SCHISM_KEYSYM_i, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_i, SCHISM_KEYMOD_CTRL },
	{ "Refresh Display", "refresh_display", SCHISM_KEYSYM_e, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_e, SCHISM_KEYMOD_CTRL },
	{ "Time Information", "time_information", SCHISM_KEYSYM_t, SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_t, SCHISM_KEYMOD_CTRL | SCHISM_KEYMOD_ALT },
	{ "Toggle Playback Trace", "toggle_playback_trace", SCHISM_KEYSYM_f, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_f, SCHISM_KEYMOD_CTRL },
	{ "Toggle MIDI Input", "toggle_midi_input", SCHISM_KEYSYM_SCROLLLOCK, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_SCROLLLOCK, SCHISM_KEYMOD_ALT },
	{ "Speed Down", "speed_down", SCHISM_KEYSYM_LEFTBRACKET, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_LEFTBRACKET, SCHISM_KEYMOD_SHIFT },
	{ "Speed Up", "speed_up", SCHISM_KEYSYM_RIGHTBRACKET, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_RIGHTBRACKET, SCHISM_KEYMOD_SHIFT },
	{ "Tempo Down", "tempo_down", SCHISM_KEYSYM_LEFTBRACKET, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_LEFTBRACKET, SCHISM_KEYMOD_CTRL },
	{ "Tempo Up", "tempo_up", SCHISM_KEYSYM_RIGHTBRACKET, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_RIGHTBRACKET, SCHISM_KEYMOD_CTRL },
	{ "Global Volume Down", "global_vol_down", SCHISM_KEYSYM_LEFTBRACKET, 0, SCHISM_KEYSYM_LEFTBRACKET, 0 },
	{ "Global Volume Up", "global_vol_up", SCHISM_KEYSYM_RIGHTBRACKET, 0, SCHISM_KEYSYM_RIGHTBRACKET, 0 },
	{ "Octave Down", "octave_down", SCHISM_KEYSYM_SLASH, 0, SCHISM_KEYSYM_SLASH, 0 },
	{ "Octave Up", "octave_up", SCHISM_KEYSYM_ASTERISK, 0, SCHISM_KEYSYM_ASTERISK, 0 },
	{ "Pattern Play from Row", "patedit_play_from_row", SCHISM_KEYSYM_F6, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_F6, SCHISM_KEYMOD_CTRL },
	{ "Pattern Set Play Mark", "patedit_set_play_mark", SCHISM_KEYSYM_F7, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_F7, SCHISM_KEYMOD_CTRL },
	{ "Pattern Block Start", "patedit_block_start", SCHISM_KEYSYM_b, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_b, SCHISM_KEYMOD_ALT },
	{ "Pattern Block End", "patedit_block_end", SCHISM_KEYSYM_e, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_e, SCHISM_KEYMOD_ALT },
	{ "Pattern Block Mark All", "patedit_block_mark_all", SCHISM_KEYSYM_l, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_l, SCHISM_KEYMOD_ALT },
	{ "Pattern Block Unmark", "patedit_block_unmark", SCHISM_KEYSYM_u, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_u, SCHISM_KEYMOD_ALT },
	{ "Pattern Block Copy", "patedit_block_copy", SCHISM_KEYSYM_c, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_c, SCHISM_KEYMOD_ALT },
	{ "Pattern Block Paste", "patedit_block_paste", SCHISM_KEYSYM_p, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_p, SCHISM_KEYMOD_ALT },
	{ "Pattern Block Overwrite Paste", "patedit_block_paste_overwrite", SCHISM_KEYSYM_o, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_o, SCHISM_KEYMOD_ALT },
	{ "Pattern Block Cut", "patedit_block_cut", SCHISM_KEYSYM_z, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_z, SCHISM_KEYMOD_ALT },
	{ "Pattern Undo", "patedit_undo", SCHISM_KEYSYM_BACKSPACE, SCHISM_KEYMOD_CTRL, SCHISM_KEYSYM_BACKSPACE, SCHISM_KEYMOD_CTRL },
	{ "Pattern Note Fade", "patedit_note_fade", SCHISM_KEYSYM_BACKQUOTE, SCHISM_KEYMOD_SHIFT, SCHISM_KEYSYM_BACKQUOTE, SCHISM_KEYMOD_SHIFT },
	{ "Toggle Channel 1", "toggle_channel_1", SCHISM_KEYSYM_F1, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_F1, SCHISM_KEYMOD_ALT },
	{ "Toggle Channel 2", "toggle_channel_2", SCHISM_KEYSYM_F2, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_F2, SCHISM_KEYMOD_ALT },
	{ "Toggle Channel 3", "toggle_channel_3", SCHISM_KEYSYM_F3, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_F3, SCHISM_KEYMOD_ALT },
	{ "Toggle Channel 4", "toggle_channel_4", SCHISM_KEYSYM_F4, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_F4, SCHISM_KEYMOD_ALT },
	{ "Toggle Channel 5", "toggle_channel_5", SCHISM_KEYSYM_F5, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_F5, SCHISM_KEYMOD_ALT },
	{ "Toggle Channel 6", "toggle_channel_6", SCHISM_KEYSYM_F6, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_F6, SCHISM_KEYMOD_ALT },
	{ "Toggle Channel 7", "toggle_channel_7", SCHISM_KEYSYM_F7, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_F7, SCHISM_KEYMOD_ALT },
	{ "Toggle Channel 8", "toggle_channel_8", SCHISM_KEYSYM_F8, SCHISM_KEYMOD_ALT, SCHISM_KEYSYM_F8, SCHISM_KEYMOD_ALT },
};

static int shortcut_lookup_action(int sym, int mod)
{
	int i;
	int masked_mod = mod & SHORTCUT_MOD_MASK;

	for (i = 0; i < SHORTCUT_COUNT; i++) {
		if (shortcut_bindings[i].sym == sym && shortcut_bindings[i].mod == masked_mod) {
			return i;
		}
	}
	return -1;
}

static int shortcut_default_sym(int action)
{
	return shortcut_bindings[action].default_sym;
}

static int shortcut_default_mod(int action)
{
	return shortcut_bindings[action].default_mod;
}

void shortcuts_reset_defaults(void)
{
	int i;
	for (i = 0; i < SHORTCUT_COUNT; i++) {
		shortcut_bindings[i].sym = shortcut_bindings[i].default_sym;
		shortcut_bindings[i].mod = shortcut_bindings[i].default_mod;
	}
}

void shortcuts_load(cfg_file_t *cfg)
{
	int i;
	for (i = 0; i < SHORTCUT_COUNT; i++) {
		int sym = cfg_get_number(cfg, "Shortcuts", shortcut_bindings[i].cfg_key, shortcut_bindings[i].default_sym);
		char mod_key[64];
		int mod;

		snprintf(mod_key, sizeof(mod_key), "%s_mod", shortcut_bindings[i].cfg_key);
		mod = cfg_get_number(cfg, "Shortcuts", mod_key, shortcut_bindings[i].default_mod);
		mod &= SHORTCUT_MOD_MASK;

		if (sym <= 0) {
			sym = shortcut_bindings[i].default_sym;
			mod = shortcut_bindings[i].default_mod;
		}

		shortcut_bindings[i].sym = sym;
		shortcut_bindings[i].mod = mod;
	}
}

void shortcuts_save(cfg_file_t *cfg)
{
	int i;
	for (i = 0; i < SHORTCUT_COUNT; i++) {
		char mod_key[64];
		cfg_set_number(cfg, "Shortcuts", shortcut_bindings[i].cfg_key, shortcut_bindings[i].sym);
		snprintf(mod_key, sizeof(mod_key), "%s_mod", shortcut_bindings[i].cfg_key);
		cfg_set_number(cfg, "Shortcuts", mod_key, shortcut_bindings[i].mod & SHORTCUT_MOD_MASK);
	}
}

void shortcuts_remap_key_event(struct key_event *k)
{
	int action = shortcut_lookup_action(k->sym, k->mod);
	if (action < 0) {
		return;
	}

	k->sym = shortcut_default_sym(action);
	k->mod = (k->mod & ~SHORTCUT_MOD_MASK) | shortcut_default_mod(action);
}

int shortcuts_count(void)
{
	return SHORTCUT_COUNT;
}

const char *shortcuts_action_name(int idx)
{
	if (idx < 0 || idx >= SHORTCUT_COUNT) {
		return "";
	}
	return shortcut_bindings[idx].name;
}

int shortcuts_get_binding(int idx, int *sym, int *mod)
{
	if (idx < 0 || idx >= SHORTCUT_COUNT || !sym || !mod) {
		return 0;
	}

	*sym = shortcut_bindings[idx].sym;
	*mod = shortcut_bindings[idx].mod;
	return 1;
}

int shortcuts_set_binding(int idx, int sym, int mod)
{
	int masked_mod = mod & SHORTCUT_MOD_MASK;

	if (idx < 0 || idx >= SHORTCUT_COUNT || sym <= 0) {
		return 0;
	}

	if (shortcuts_find_conflict(idx, sym, masked_mod) >= 0)
		return 0;

	shortcut_bindings[idx].sym = sym;
	shortcut_bindings[idx].mod = masked_mod;
	return 1;
}

int shortcuts_find_conflict(int idx, int sym, int mod)
{
	int i;
	int masked_mod = mod & SHORTCUT_MOD_MASK;

	for (i = 0; i < SHORTCUT_COUNT; i++) {
		if (i == idx) {
			continue;
		}
		if (shortcut_bindings[i].sym == sym && shortcut_bindings[i].mod == masked_mod) {
			return i;
		}
	}
	return -1;
}

const char *shortcuts_format_binding(int sym, int mod, char *buf, size_t buflen)
{
	const char *key_name = NULL;
	size_t n = 0;

	if (!buf || buflen == 0) {
		return "";
	}
	buf[0] = 0;

	if (mod & SCHISM_KEYMOD_CTRL) {
		n += strlcpy(buf + n, "Ctrl+", (n < buflen) ? (buflen - n) : 0);
	}
	if (mod & SCHISM_KEYMOD_ALT) {
		n += strlcpy(buf + n, "Alt+", (n < buflen) ? (buflen - n) : 0);
	}
	if (mod & SCHISM_KEYMOD_SHIFT) {
		n += strlcpy(buf + n, "Shift+", (n < buflen) ? (buflen - n) : 0);
	}

	switch (sym) {
	case SCHISM_KEYSYM_F1: key_name = "F1"; break;
	case SCHISM_KEYSYM_F2: key_name = "F2"; break;
	case SCHISM_KEYSYM_F3: key_name = "F3"; break;
	case SCHISM_KEYSYM_F4: key_name = "F4"; break;
	case SCHISM_KEYSYM_F5: key_name = "F5"; break;
	case SCHISM_KEYSYM_F6: key_name = "F6"; break;
	case SCHISM_KEYSYM_F7: key_name = "F7"; break;
	case SCHISM_KEYSYM_F8: key_name = "F8"; break;
	case SCHISM_KEYSYM_F9: key_name = "F9"; break;
	case SCHISM_KEYSYM_F10: key_name = "F10"; break;
	case SCHISM_KEYSYM_F11: key_name = "F11"; break;
	case SCHISM_KEYSYM_F12: key_name = "F12"; break;
	case SCHISM_KEYSYM_RETURN: key_name = "Enter"; break;
	case SCHISM_KEYSYM_SCROLLLOCK: key_name = "ScrollLock"; break;
	case SCHISM_KEYSYM_PAUSE: key_name = "Pause"; break;
	case SCHISM_KEYSYM_HOME: key_name = "Home"; break;
	case SCHISM_KEYSYM_END: key_name = "End"; break;
	case SCHISM_KEYSYM_PAGEUP: key_name = "PgUp"; break;
	case SCHISM_KEYSYM_PAGEDOWN: key_name = "PgDn"; break;
	case SCHISM_KEYSYM_SPACE: key_name = "Space"; break;
	case SCHISM_KEYSYM_TAB: key_name = "Tab"; break;
	case SCHISM_KEYSYM_BACKSPACE: key_name = "Backspace"; break;
	case SCHISM_KEYSYM_BACKQUOTE: key_name = "`"; break;
	case SCHISM_KEYSYM_LEFTBRACKET: key_name = "["; break;
	case SCHISM_KEYSYM_RIGHTBRACKET: key_name = "]"; break;
	case SCHISM_KEYSYM_SLASH: key_name = "/"; break;
	case SCHISM_KEYSYM_ASTERISK: key_name = "*"; break;
	default:
		if (sym >= 32 && sym <= 126) {
			static char ascii_buf[2];
			ascii_buf[0] = (char)sym;
			ascii_buf[1] = 0;
			key_name = ascii_buf;
		}
		break;
	}

	if (!key_name) {
		char fallback[24];
		snprintf(fallback, sizeof(fallback), "Key%d", sym);
		key_name = fallback;
	}

	strlcpy(buf + n, key_name, (n < buflen) ? (buflen - n) : 0);
	return buf;
}
