#include "keybinds.h"

enum keybinds_menu_ids {
	IDM_FILE_NEW = 101,
	IDM_FILE_LOAD = 102,
	IDM_FILE_SAVE_CURRENT = 103,
	IDM_FILE_SAVE_AS = 104,
	IDM_FILE_EXPORT = 105,
	IDM_FILE_MESSAGE_LOG = 106,
	IDM_FILE_QUIT = 107,
	IDM_PLAYBACK_SHOW_INFOPAGE = 201,
	IDM_PLAYBACK_PLAY_SONG = 202,
	IDM_PLAYBACK_PLAY_PATTERN = 203,
	IDM_PLAYBACK_PLAY_FROM_ORDER = 204,
	IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR = 205,
	IDM_PLAYBACK_STOP = 206,
	IDM_PLAYBACK_CALCULATE_LENGTH = 207,
	IDM_SAMPLES_SAMPLE_LIST = 301,
	IDM_SAMPLES_SAMPLE_LIBRARY = 302,
	IDM_SAMPLES_RELOAD_SOUNDCARD = 303,
	IDM_INSTRUMENTS_INSTRUMENT_LIST = 401,
	IDM_INSTRUMENTS_INSTRUMENT_LIBRARY = 402,
	IDM_VIEW_HELP = 501,
	IDM_VIEW_VIEW_PATTERNS = 502,
	IDM_VIEW_ORDERS_PANNING = 503,
	IDM_VIEW_VARIABLES = 504,
	IDM_VIEW_MESSAGE_EDITOR = 505,
	IDM_VIEW_TOGGLE_FULLSCREEN = 506,
	IDM_SETTINGS_PREFERENCES = 601,
	IDM_SETTINGS_MIDI_CONFIGURATION = 602,
	IDM_SETTINGS_PALETTE_EDITOR = 603,
	IDM_SETTINGS_FONT_EDITOR = 604,
	IDM_SETTINGS_SYSTEM_CONFIGURATION = 605,
};

struct keybinds_menu keybinds_menus[] = {
	{
		.type = KEYBINDS_MENU_REGULAR,
		.info = {
			.regular = {
				.name = "&File",
			},
		},
		.items = (struct keybinds_menu_item[]){
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&New", .bind = &global_keybinds_list.global.new_song, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Load", .bind = &global_keybinds_list.global.load_module, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Save", .bind = &global_keybinds_list.global.save, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "Save &As...", .bind = &global_keybinds_list.global.save_module, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Export...", .bind = &global_keybinds_list.global.export_module, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Message Log", .bind = &global_keybinds_list.global.schism_logging, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_SEPARATOR,
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.no_osx = 1, // hax
				.info = { .regular = { .name = "&Quit", .bind = &global_keybinds_list.global.quit, }, },
			},
			{0},
		},
	},
	{
		.type = KEYBINDS_MENU_APPLE,
		.info = {
			.regular = {
				.name = "&View",
			},
		},
		.items = (struct keybinds_menu_item[]){
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Help", .bind = &global_keybinds_list.global.help, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_SEPARATOR,
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&View Patterns", .bind = &global_keybinds_list.global.pattern_edit, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Orders/Panning", .bind = &global_keybinds_list.global.order_list, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Variables", .bind = &global_keybinds_list.global.song_variables, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Message Editor", .bind = &global_keybinds_list.global.message_editor, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_SEPARATOR,
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Toggle Fullscreen", .bind = &global_keybinds_list.global.fullscreen, }, },
			},
			{0},
		},
	},
	{
		.type = KEYBINDS_MENU_REGULAR,
		.info = {
			.regular = {
				.name = "&Playback",
			},
		},
		.items = (struct keybinds_menu_item[]){
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Show Infopage", .bind = &global_keybinds_list.global.play_information_or_play_song, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Play Song", .bind = &global_keybinds_list.global.play_song, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "Play Pa&ttern", .bind = &global_keybinds_list.global.play_current_pattern, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "Play From &Order", .bind = &global_keybinds_list.global.play_song_from_order, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "Play From &Mark / Cursor", .bind = &global_keybinds_list.global.play_song_from_mark, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Stop", .bind = &global_keybinds_list.global.stop_playback, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Calculate Length", .bind = &global_keybinds_list.global.calculate_song_length, }, },
			},
			{0},
		},
	},
	{
		.type = KEYBINDS_MENU_REGULAR,
		.info = {
			.regular = {
				.name = "&Samples",
			},
		},
		.items = (struct keybinds_menu_item[]){
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Sample List", .bind = &global_keybinds_list.global.sample_list, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "Sample &Library", .bind = &global_keybinds_list.global.sample_library, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Reload Soundcard", .bind = &global_keybinds_list.global.audio_reset, }, },
			},
			{0},
		},
	},
	{
		.type = KEYBINDS_MENU_REGULAR,
		.info = {
			.regular = {
				.name = "&Instruments",
			},
		},
		.items = (struct keybinds_menu_item[]){
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Instrument List", .bind = &global_keybinds_list.global.instrument_list, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "Instrument &Library", .bind = &global_keybinds_list.global.instrument_library, }, },
			},
			{0},
		},
	},
	{
		.type = KEYBINDS_MENU_REGULAR,
		.info = {
			.regular = {
				.name = "S&ettings",
			},
		},
		.items = (struct keybinds_menu_item[]){
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Preferences", .bind = &global_keybinds_list.global.preferences, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&MIDI Configuration", .bind = &global_keybinds_list.global.midi, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Palette Editor", .bind = &global_keybinds_list.global.palette_config, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&Font Editor", .bind = &global_keybinds_list.global.font_editor, }, },
			},
			{
				.type = KEYBINDS_MENU_ITEM_REGULAR,
				.info = { .regular = { .name = "&System Configuration", .bind = &global_keybinds_list.global.system_configure, }, },
			},
			{0},
		},
	},
	{0},
};

struct keybinds_menu_item *keybinds_menu_find_item_from_id(uint16_t id)
{
	for (struct keybinds_menu *m = keybinds_menus; m->type != KEYBINDS_MENU_NULL; m++)
		for (struct keybinds_menu_item *i = m->items; i->type != KEYBINDS_MENU_ITEM_NULL; i++)
			if (i->type == KEYBINDS_MENU_REGULAR && i->info.regular.id == id)
				return i;

	return NULL;
}

void keybinds_menu_item_pressed(struct keybinds_menu_item *i, SDL_Event *event)
{
	SDL_Event e = {0};
	e.type = SCHISM_EVENT_NATIVE;
	e.user.code = SCHISM_EVENT_NATIVE_SCRIPT;
	switch (i->info.regular.id) {
		case IDM_FILE_NEW:
			e.user.data1 = "new";
			break;
		case IDM_FILE_LOAD:
			e.user.data1 = "load";
			break;
		case IDM_FILE_SAVE_CURRENT:
			e.user.data1 = "save";
			break;
		case IDM_FILE_SAVE_AS:
			e.user.data1 = "save_as";
			break;
		case IDM_FILE_EXPORT:
			e.user.data1 = "export_song";
			break;
		case IDM_FILE_MESSAGE_LOG:
			e.user.data1 = "logviewer";
			break;
		case IDM_FILE_QUIT:
			e.type = SDL_QUIT;
			break;
		case IDM_PLAYBACK_SHOW_INFOPAGE:
			e.user.data1 = "info";
			break;
		case IDM_PLAYBACK_PLAY_SONG:
			e.user.data1 = "play";
			break;
		case IDM_PLAYBACK_PLAY_PATTERN:
			e.user.data1 = "play_pattern";
			break;
		case IDM_PLAYBACK_PLAY_FROM_ORDER:
			e.user.data1 = "play_order";
			break;
		case IDM_PLAYBACK_PLAY_FROM_MARK_CURSOR:
			e.user.data1 = "play_mark";
			break;
		case IDM_PLAYBACK_STOP:
			e.user.data1 = "stop";
			break;
		case IDM_PLAYBACK_CALCULATE_LENGTH:
			e.user.data1 = "calc_length";
			break;
		case IDM_SAMPLES_SAMPLE_LIST:
			e.user.data1 = "sample_page";
			break;
		case IDM_SAMPLES_SAMPLE_LIBRARY:
			e.user.data1 = "sample_library";
			break;
		case IDM_SAMPLES_RELOAD_SOUNDCARD:
			e.user.data1 = "init_sound";
			break;
		case IDM_INSTRUMENTS_INSTRUMENT_LIST:
			e.user.data1 = "inst_page";
			break;
		case IDM_INSTRUMENTS_INSTRUMENT_LIBRARY:
			e.user.data1 = "inst_library";
			break;
		case IDM_VIEW_HELP:
			e.user.data1 = "help";
			break;
		case IDM_VIEW_VIEW_PATTERNS:
			e.user.data1 = "pattern";
			break;
		case IDM_VIEW_ORDERS_PANNING:
			e.user.data1 = "orders";
			break;
		case IDM_VIEW_VARIABLES:
			e.user.data1 = "variables";
			break;
		case IDM_VIEW_MESSAGE_EDITOR:
			e.user.data1 = "message_edit";
			break;
		case IDM_VIEW_TOGGLE_FULLSCREEN:
			e.user.data1 = "fullscreen";
			break;
		case IDM_SETTINGS_PREFERENCES:
			e.user.data1 = "preferences";
			break;
		case IDM_SETTINGS_MIDI_CONFIGURATION:
			e.user.data1 = "midi_config";
			break;
		case IDM_SETTINGS_PALETTE_EDITOR:
			e.user.data1 = "palette_page";
			break;
		case IDM_SETTINGS_FONT_EDITOR:
			e.user.data1 = "font_editor";
			break;
		case IDM_SETTINGS_SYSTEM_CONFIGURATION:
			e.user.data1 = "system_config";
			break;
		default:
			break;
	}
	*event = e;
}
