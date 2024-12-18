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

/* This is the first thing I *ever* did with SDL. :) */

/* Warning: spaghetti follows.... it just kind of ended up this way.
   In other news, TODO: clean this mess on a rainy day.
   and yes, that in fact rhymed :P */

#include "headers.h"

#include "backend/events.h"
#include "events.h"

#include "clippy.h"
#include "disko.h"
#include "fakemem.h"

#include "config.h"
#include "version.h"
#include "song.h"
#include "midi.h"
#include "dmoz.h"
#include "charset.h"
#include "keyboard.h"
#include "palettes.h"
#include "fonts.h"
#include "dialog.h"
#include "widget.h"
#include "fmt.h"
#include "timer.h"
#include "threads.h"

#include "osdefs.h"

#include <errno.h>

#include <fcntl.h>
#include <unistd.h>

#ifndef SCHISM_WIN32
# include <signal.h>
#endif

#if !defined(__amigaos4__) && !defined(SCHISM_WII)
# define ENABLE_HOOKS 1
#endif

#define NATIVE_SCREEN_WIDTH     640
#define NATIVE_SCREEN_HEIGHT    400

/* --------------------------------------------------------------------- */
/* globals */

enum {
	EXIT_HOOK = 1,
	EXIT_SAVECFG = 4,
	EXIT_SDLQUIT = 16,
};
static int shutdown_process = 0;

static const char *video_driver = NULL;
static const char *audio_driver = NULL;
static const char *audio_device = NULL;
static int want_fullscreen = -1;
static int did_classic = 0;

/* --------------------------------------------------------------------- */

#define SDL_INIT_FLAGS SDL_INIT_TIMER | SDL_INIT_VIDEO

static void display_init(void)
{
	video_startup();
}

static void check_update(void);

void toggle_display_fullscreen(void)
{
	video_fullscreen(-1);
	status.flags |= (NEED_UPDATE);
}

/* --------------------------------------------------------------------- */

#if ENABLE_HOOKS
static void run_startup_hook(void)
{
	os_run_hook(cfg_dir_dotschism, "startup-hook", NULL);
}
static void run_disko_complete_hook(void)
{
	os_run_hook(cfg_dir_dotschism, "diskwriter-hook", NULL);
}

static void run_exit_hook(void)
{
	os_run_hook(cfg_dir_dotschism, "exit-hook", NULL);
}
#endif

/* --------------------------------------------------------------------- */
/* arg parsing */

/* filename of song to load on startup, or NULL for none */
#ifdef SCHISM_MACOSX
/* this gets written to by the custom main function on macosx */
char *initial_song = NULL;
#else
static char *initial_song = NULL;
#endif

/* initial module directory */
static char *initial_dir = NULL;

/* diskwrite? */
static char *diskwrite_to = NULL;

/* startup flags */
enum {
	SF_PLAY = 1, /* -p: start playing after loading initial_song */
	SF_HOOKS = 2, /* --no-hooks: don't run startup/exit scripts */
	SF_FONTEDIT = 4,
	SF_CLASSIC = 8,
	SF_NETWORK = 16,
};
static int startup_flags = SF_HOOKS | SF_NETWORK;

/* getopt ids */
#define SHORT_OPTIONS "a:v:fFpPh" // these should correspond to the characters below (trailing colon indicates argument)
enum {
	// short options
	O_SDL_AUDIODRIVER = 'a',
	O_SDL_VIDEODRIVER = 'v',
	O_FULLSCREEN = 'f', O_NO_FULLSCREEN = 'F',
	O_PLAY = 'p', O_NO_PLAY = 'P',
	O_HELP = 'h',
	// ids for long options with no corresponding short option
	O_VIDEO_YUVLAYOUT = 256,
	O_VIDEO_RESOLUTION,
	O_VIDEO_STRETCH, O_NO_VIDEO_STRETCH,
	O_VIDEO_DEPTH,
#if USE_NETWORK
	O_NETWORK, O_NO_NETWORK,
#endif
	O_CLASSIC_MODE, O_NO_CLASSIC_MODE,
	O_FONTEDIT, O_NO_FONTEDIT,
#if ENABLE_HOOKS
	O_HOOKS, O_NO_HOOKS,
#endif
	O_DISKWRITE,
	O_DEBUG,
	O_VERSION,
};

#define USAGE "Usage: %s [OPTIONS] [DIRECTORY] [FILE]\n"

// Remember to update the manpage when changing the command-line options!

static void parse_options(int argc, char **argv)
{
	struct option long_options[] = {
		{"audio-driver", 1, NULL, O_SDL_AUDIODRIVER},
		{"video-driver", 1, NULL, O_SDL_VIDEODRIVER},

#if USE_NETWORK
		{"network", 0, NULL, O_NETWORK},
		{"no-network", 0, NULL, O_NO_NETWORK},
#endif
		{"classic", 0, NULL, O_CLASSIC_MODE},
		{"no-classic", 0, NULL, O_NO_CLASSIC_MODE},
		{"fullscreen", 0, NULL, O_FULLSCREEN},
		{"no-fullscreen", 0, NULL, O_NO_FULLSCREEN},
		{"play", 0, NULL, O_PLAY},
		{"no-play", 0, NULL, O_NO_PLAY},
		{"diskwrite", 1, NULL, O_DISKWRITE},
		{"font-editor", 0, NULL, O_FONTEDIT},
		{"no-font-editor", 0, NULL, O_NO_FONTEDIT},
#if ENABLE_HOOKS
		{"hooks", 0, NULL, O_HOOKS},
		{"no-hooks", 0, NULL, O_NO_HOOKS},
#endif
		{"version", 0, NULL, O_VERSION},
		{"help", 0, NULL, O_HELP},
		{NULL, 0, NULL, 0},
	};
	int opt;

	while ((opt = getopt_long(argc, argv, SHORT_OPTIONS, long_options, NULL)) != -1) {
		switch (opt) {
		case O_SDL_AUDIODRIVER:
			audio_parse_driver_spec(optarg, (char**)&audio_driver, (char**)&audio_device);
			break;
		case O_SDL_VIDEODRIVER:
			video_driver = str_dup(optarg);
			break;
#if USE_NETWORK
		case O_NETWORK:
			startup_flags |= SF_NETWORK;
			break;
		case O_NO_NETWORK:
			startup_flags &= ~SF_NETWORK;
			break;
#endif
		case O_CLASSIC_MODE:
			startup_flags |= SF_CLASSIC;
			did_classic = 1;
			break;
		case O_NO_CLASSIC_MODE:
			startup_flags &= ~SF_CLASSIC;
			did_classic = 1;
			break;
		case O_FULLSCREEN:
			want_fullscreen = 1;
			break;
		case O_NO_FULLSCREEN:
			want_fullscreen = 0;
			break;
		case O_PLAY:
			startup_flags |= SF_PLAY;
			break;
		case O_NO_PLAY:
			startup_flags &= ~SF_PLAY;
			break;
		case O_FONTEDIT:
			startup_flags |= SF_FONTEDIT;
			break;
		case O_NO_FONTEDIT:
			startup_flags &= ~SF_FONTEDIT;
			break;
		case O_DISKWRITE:
			diskwrite_to = optarg;
			break;
#if ENABLE_HOOKS
		case O_HOOKS:
			startup_flags |= SF_HOOKS;
			break;
		case O_NO_HOOKS:
			startup_flags &= ~SF_HOOKS;
			break;
#endif
		case O_VERSION:
			puts(schism_banner(0));
			puts(ver_short_copyright);
			exit(0);
		case O_HELP:
			// XXX try to keep this stuff to one screen (78x20 or so)
			printf(USAGE, argv[0]);
			printf(
				"  -a, --audio-driver=DRIVER\n"
				"  -v, --video-driver=DRIVER\n"
				"      --classic (--no-classic)\n"
				"  -f, --fullscreen (-F, --no-fullscreen)\n"
				"  -p, --play (-P, --no-play)\n"
				"      --diskwrite=FILENAME\n"
				"      --font-editor (--no-font-editor)\n"
#if ENABLE_HOOKS
				"      --hooks (--no-hooks)\n"
#endif
				"      --version\n"
				"  -h, --help\n"
			);
			printf("Refer to the documentation for complete usage details.\n");
			exit(0);
		case '?': // unknown option
			fprintf(stderr, USAGE, argv[0]);
			exit(2);
		default: // unhandled but known option
			fprintf(stderr, "how did this get here i am not good with computer\n");
			exit(2);
		}
	}

	char *cwd = dmoz_get_current_directory();
	for (; optind < argc; optind++) {
		char *arg = argv[optind];
		char *tmp = dmoz_path_concat(cwd, arg);
		if (!tmp) {
			perror(arg);
			continue;
		}
		char *norm = dmoz_path_normal(tmp);
		free(tmp);
		if (dmoz_path_is_directory(arg)) {
			free(initial_dir);
			initial_dir = norm;
		} else {
			free(initial_song);
			initial_song = norm;
		}
	}
	free(cwd);
}

/* --------------------------------------------------------------------- */

static void check_update(void)
{
	static schism_ticks_t next = 0;
	schism_ticks_t now = timer_ticks();

	/* is there any reason why we'd want to redraw
	   the screen when it's not even visible? */
	if (video_is_visible() && (status.flags & NEED_UPDATE)) {
		status.flags &= ~NEED_UPDATE;

		if (!video_is_focused() && (status.flags & LAZY_REDRAW)) {
			if (!timer_ticks_passed(now, next))
				return;

			next = now + 500;
		} else if (status.flags & (DISKWRITER_ACTIVE | DISKWRITER_ACTIVE_PATTERN)) {
			if (!timer_ticks_passed(now, next))
				return;

			next = now + 100;
		}

		redraw_screen();
		video_refresh();
		video_blit();
	} else if (status.flags & SOFTWARE_MOUSE_MOVED) {
		video_blit();
		status.flags &= ~(SOFTWARE_MOUSE_MOVED);
	}
}

static void _do_clipboard_paste_op(schism_event_t *e)
{
	if (ACTIVE_WIDGET.clipboard_paste
	&& ACTIVE_WIDGET.clipboard_paste(e->type,
				e->clipboard.clipboard)) return;

	if (ACTIVE_PAGE.clipboard_paste
	&& ACTIVE_PAGE.clipboard_paste(e->type,
				e->clipboard.clipboard)) return;

	handle_text_input(e->clipboard.clipboard);
}

static void key_event_reset(struct key_event *kk, int start_x, int start_y)
{
	memset(kk, 0, sizeof(*kk));

	kk->midi_volume = -1;
	kk->midi_note = -1;
	/* X/Y resolution */
	kk->rx = NATIVE_SCREEN_WIDTH / 80;
	kk->ry = NATIVE_SCREEN_HEIGHT / 50;
	/* preserve the start position */
	kk->sx = start_x;
	kk->sy = start_y;
}

/* -------------------------------------------- */

static void event_loop(void)
{
	unsigned int lx = 0, ly = 0; /* last x and y position (character) */
	schism_ticks_t last_mouse_down, ticker;
	schism_keysym_t last_key = 0;
	time_t startdown;
	int downtrip;
	int wheel_x;
	int wheel_y;
	int fix_numlock_key;
	int screensaver;
	int button = -1;
	int i;
	struct key_event kk;

	fix_numlock_key = status.fix_numlock_setting;

	downtrip = 0;
	last_mouse_down = 0;
	startdown = 0;
	status.last_keysym = 0;

	status.keymod = events_get_keymod_state();
	os_get_modkey(&status.keymod);

	video_toggle_screensaver(1);
	screensaver = 1;

	time(&status.now);
	localtime_r(&status.now, &status.tmnow);
	for (;;) {
		schism_event_t se;
		while (events_poll_event(&se)) {
			if (midi_engine_handle_event(&se))
				continue;

			key_event_reset(&kk, kk.sx, kk.sy);

			if (se.type == SCHISM_KEYDOWN || se.type == SCHISM_MOUSEBUTTONDOWN) {
				kk.state = KEY_PRESS;
			} else if (se.type == SCHISM_KEYUP || se.type == SCHISM_MOUSEBUTTONUP) {
				kk.state = KEY_RELEASE;
			}
			if (se.type == SCHISM_KEYDOWN || se.type == SCHISM_TEXTINPUT || se.type == SCHISM_KEYUP) {
				if (se.key.sym == 0) {
					// XXX when does this happen?
					kk.mouse = MOUSE_NONE;
					kk.is_repeat = 0;
				}
			}

			switch (se.type) {
			case SCHISM_QUIT:
				show_exit_prompt();
				break;
			case SCHISM_AUDIODEVICEADDED:
			case SCHISM_AUDIODEVICEREMOVED:
				refresh_audio_device_list();
				status.flags |= NEED_UPDATE;
				break;
			case SCHISM_TEXTINPUT: {
				char *input_text = NULL;

				charset_error_t err = charset_iconv(se.text.text, &input_text, CHARSET_UTF8, CHARSET_CP437, ARRAY_SIZE(se.text.text));
				if (err || !input_text) {
					log_appendf(4, " [ERROR] failed to convert text input event");
					log_appendf(4, "  %s", se.text.text);
					log_appendf(4, " into CP437 with error %s.", charset_iconv_error_lookup(err));
					log_appendf(4, " please report this on the github!");
					break;
				}

				handle_text_input(input_text);

				free(input_text);
				break;
			}
			case SCHISM_KEYDOWN:
				if (se.key.repeat) {
					// Only use the system repeat if we don't have our own configuration
					if (kbd_key_repeat_enabled())
						break;

					kk.is_repeat = 1;
				}

				/* fallthrough */
			case SCHISM_KEYUP:
				switch (se.key.sym) {
				case SCHISM_KEYSYM_NUMLOCKCLEAR:
					status.keymod ^= SCHISM_KEYMOD_NUM;
					break;
				case SCHISM_KEYSYM_CAPSLOCK:
					if (se.type == SCHISM_KEYDOWN) {
						status.keymod |= SCHISM_KEYMOD_CAPS_PRESSED;
					} else {
						status.keymod &= ~SCHISM_KEYMOD_CAPS_PRESSED;
					}
					status.keymod ^= SCHISM_KEYMOD_CAPS;
					break;
				default:
					break;
				};

				// grab the keymod
				status.keymod = se.key.mod;
				// fix it
				os_get_modkey(&status.keymod);

				kk.sym = se.key.sym;
				kk.scancode = se.key.scancode;

				switch (fix_numlock_key) {
				case NUMLOCK_GUESS:
					/* should be handled per OS */
					break;
				case NUMLOCK_ALWAYS_OFF:
					status.keymod &= ~SCHISM_KEYMOD_NUM;
					break;
				case NUMLOCK_ALWAYS_ON:
					status.keymod |= SCHISM_KEYMOD_NUM;
					break;
				};

				kk.mod = status.keymod;
				kk.mouse = MOUSE_NONE;
				kbd_key_translate(&kk);

				if (*se.key.text)
					charset_iconv(se.key.text, &kk.text, CHARSET_UTF8, CHARSET_CP437, ARRAY_SIZE(se.key.text));

				handle_key(&kk);

				if (se.type == SCHISM_KEYUP) {
					/* only empty the key repeat if
					 * the last keydown is the same sym */
					if (last_key == kk.sym)
						kbd_empty_key_repeat();
				} else {
					kbd_cache_key_repeat(&kk);

					status.last_keysym = last_key;
					last_key = kk.sym;
				}
				break;
			case SCHISM_MOUSEMOTION:
			case SCHISM_MOUSEWHEEL:
			case SCHISM_MOUSEBUTTONDOWN:
			case SCHISM_MOUSEBUTTONUP: {
				if (kk.state == KEY_PRESS) {
					status.keymod = events_get_keymod_state();
					os_get_modkey(&status.keymod);
				}

				kk.sym = 0;
				kk.mod = 0;

				// Handle the coordinate translation
				switch (se.type) {
				case SCHISM_MOUSEWHEEL:
					kk.state = -1;  /* neither KEY_PRESS nor KEY_RELEASE (???) */
					video_translate(se.wheel.mouse_x, se.wheel.mouse_y, &kk.fx, &kk.fy);
					kk.mouse = (se.wheel.y > 0) ? MOUSE_SCROLL_UP : MOUSE_SCROLL_DOWN;
					break;
				case SCHISM_MOUSEMOTION:
					video_translate(se.motion.x, se.motion.y, &kk.fx, &kk.fy);
					break;
				case SCHISM_MOUSEBUTTONDOWN:
					video_translate(se.button.x, se.button.y, &kk.fx, &kk.fy);
					// we also have to update the current button
					if ((status.keymod & SCHISM_KEYMOD_CTRL)
					|| se.button.button == MOUSE_BUTTON_RIGHT) {
						button = MOUSE_BUTTON_RIGHT;
					} else if ((status.keymod & (SCHISM_KEYMOD_ALT|SCHISM_KEYMOD_GUI))
					|| se.button.button == MOUSE_BUTTON_MIDDLE) {
						button = MOUSE_BUTTON_MIDDLE;
					} else {
						button = MOUSE_BUTTON_LEFT;
					}
					break;
				case SCHISM_MOUSEBUTTONUP:
					video_translate(se.button.x, se.button.y, &kk.fx, &kk.fy);
					break;
				}

				/* character resolution */
				kk.x = kk.fx / kk.rx;
				/* half-character selection */
				if ((kk.fx / (kk.rx/2)) % 2 == 0) {
					kk.hx = 0;
				} else {
					kk.hx = 1;
				}
				kk.y = kk.fy / kk.ry;

				if (se.type == SCHISM_MOUSEWHEEL) {
					handle_key(&kk);
					break; /* nothing else to do here */
				}
				if (se.type == SCHISM_MOUSEBUTTONDOWN) {
					kk.sx = kk.x;
					kk.sy = kk.y;
				}

				// what?
				if (startdown) startdown = 0;

				switch (button) {
				case MOUSE_BUTTON_RIGHT:
				case MOUSE_BUTTON_MIDDLE:
				case MOUSE_BUTTON_LEFT:
					kk.mouse_button = button;

					if (kk.state == KEY_RELEASE) {
						ticker = timer_ticks();
						if (lx == kk.x
						&& ly == kk.y
						&& (ticker - last_mouse_down) < 300) {
							last_mouse_down = 0;
							kk.mouse = MOUSE_DBLCLICK;
						} else {
							last_mouse_down = ticker;
							kk.mouse = MOUSE_CLICK;
						}
						lx = kk.x;
						ly = kk.y;

						// dirty hack
						button = -1;
					} else {
						kk.mouse = MOUSE_CLICK;
					}
					if (status.dialog_type == DIALOG_NONE) {
						if (kk.y <= 9 && status.current_page != PAGE_FONT_EDIT) {
							if (kk.state == KEY_RELEASE && kk.mouse_button == MOUSE_BUTTON_RIGHT) {
								menu_show();
								break;
							} else if (kk.state == KEY_PRESS && kk.mouse_button == MOUSE_BUTTON_LEFT) {
								time(&startdown);
							}
						}
					}
					if (widget_change_focus_to_xy(kk.x, kk.y)) {
						kk.on_target = 1;
					} else {
						kk.on_target = 0;
					}
					if (se.type == SCHISM_MOUSEBUTTONUP && downtrip) {
						downtrip = 0;
						break;
					}
					handle_key(&kk);
					break;
				default:
					break;
				};
				break;
			}
			case SCHISM_WINDOWEVENT_SHOWN:
			case SCHISM_WINDOWEVENT_FOCUS_GAINED:
				video_mousecursor(MOUSE_RESET_STATE);
				break;
			case SCHISM_WINDOWEVENT_RESIZED:
			case SCHISM_WINDOWEVENT_SIZE_CHANGED: /* tiling window managers */
				video_resize(se.window.data.resized.width, se.window.data.resized.height);
				/* fallthrough */
			case SCHISM_WINDOWEVENT_EXPOSED:
				status.flags |= (NEED_UPDATE);
				break;
			case SCHISM_DROPFILE:
				dialog_destroy();
				switch(status.current_page) {
				case PAGE_SAMPLE_LIST:
				case PAGE_LOAD_SAMPLE:
				case PAGE_LIBRARY_SAMPLE:
					song_load_sample(sample_get_current(), se.drop.file);
					memused_songchanged();
					status.flags |= SONG_NEEDS_SAVE;
					set_page(PAGE_SAMPLE_LIST);
					break;
				case PAGE_INSTRUMENT_LIST_GENERAL:
				case PAGE_INSTRUMENT_LIST_VOLUME:
				case PAGE_INSTRUMENT_LIST_PANNING:
				case PAGE_INSTRUMENT_LIST_PITCH:
				case PAGE_LOAD_INSTRUMENT:
				case PAGE_LIBRARY_INSTRUMENT:
					song_load_instrument_with_prompt(instrument_get_current(), se.drop.file);
					memused_songchanged();
					status.flags |= SONG_NEEDS_SAVE;
					set_page(PAGE_INSTRUMENT_LIST);
					break;
				default:
					song_load(se.drop.file);
					break;
				}
				free(se.drop.file);
				break;
			case SCHISM_EVENT_UPDATE_IPMIDI:
				status.flags |= (NEED_UPDATE);
				midi_engine_poll_ports();
				break;
			case SCHISM_EVENT_PLAYBACK:
				/* this is the sound thread */
				midi_send_flush();
				if (!(status.flags & (DISKWRITER_ACTIVE | DISKWRITER_ACTIVE_PATTERN)))
					playback_update();
				break;
			case SCHISM_EVENT_PASTE:
				/* handle clipboard events */
				_do_clipboard_paste_op(&se);
				free(se.clipboard.clipboard);
				break;
			case SCHISM_EVENT_NATIVE_OPEN: /* open song */
				song_load(se.open.file);
				free(se.open.file);
				break;
			case SCHISM_EVENT_NATIVE_SCRIPT:
				/* destroy any active dialog before changing pages */
				dialog_destroy();
				if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "new", CHARSET_UTF8) == 0) {
					new_song_dialog();
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "save", CHARSET_UTF8) == 0) {
					save_song_or_save_as();
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "save_as", CHARSET_UTF8) == 0) {
					set_page(PAGE_SAVE_MODULE);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "export_song", CHARSET_UTF8) == 0) {
					set_page(PAGE_EXPORT_MODULE);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "logviewer", CHARSET_UTF8) == 0) {
					set_page(PAGE_LOG);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "font_editor", CHARSET_UTF8) == 0) {
					set_page(PAGE_FONT_EDIT);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "load", CHARSET_UTF8) == 0) {
					set_page(PAGE_LOAD_MODULE);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "help", CHARSET_UTF8) == 0) {
					set_page(PAGE_HELP);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "pattern", CHARSET_UTF8) == 0) {
					set_page(PAGE_PATTERN_EDITOR);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "orders", CHARSET_UTF8) == 0) {
					set_page(PAGE_ORDERLIST_PANNING);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "variables", CHARSET_UTF8) == 0) {
					set_page(PAGE_SONG_VARIABLES);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "message_edit", CHARSET_UTF8) == 0) {
					set_page(PAGE_MESSAGE);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "info", CHARSET_UTF8) == 0) {
					set_page(PAGE_INFO);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "play", CHARSET_UTF8) == 0) {
					song_start();
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "play_pattern", CHARSET_UTF8) == 0) {
					song_loop_pattern(get_current_pattern(), 0);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "play_order", CHARSET_UTF8) == 0) {
					song_start_at_order(get_current_order(), 0);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "play_mark", CHARSET_UTF8) == 0) {
					play_song_from_mark();
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "stop", CHARSET_UTF8) == 0) {
					song_stop();
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "calc_length", CHARSET_UTF8) == 0) {
					show_song_length();
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "sample_page", CHARSET_UTF8) == 0) {
					set_page(PAGE_SAMPLE_LIST);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "sample_library", CHARSET_UTF8) == 0) {
					set_page(PAGE_LIBRARY_SAMPLE);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "init_sound", CHARSET_UTF8) == 0) {
					/* does nothing :) */
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "inst_page", CHARSET_UTF8) == 0) {
					set_page(PAGE_INSTRUMENT_LIST);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "inst_library", CHARSET_UTF8) == 0) {
					set_page(PAGE_LIBRARY_INSTRUMENT);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "preferences", CHARSET_UTF8) == 0) {
					set_page(PAGE_PREFERENCES);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "system_config", CHARSET_UTF8) == 0) {
					set_page(PAGE_CONFIG);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "midi_config", CHARSET_UTF8) == 0) {
					set_page(PAGE_MIDI);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "palette_page", CHARSET_UTF8) == 0) {
					set_page(PAGE_PALETTE_EDITOR);
				} else if (charset_strcasecmp(se.script.which, CHARSET_UTF8, "fullscreen", CHARSET_UTF8) == 0) {
					toggle_display_fullscreen();
				}
				free(se.script.which);
				break;
			default:
				break;
			}
		}

		/* handle key repeats */
		kbd_handle_key_repeat();

		/* now we can do whatever we need to do */
		time(&status.now);
		localtime_r(&status.now, &status.tmnow);

		if (status.dialog_type == DIALOG_NONE
		    && startdown && (status.now - startdown) > 1) {
			menu_show();
			startdown = 0;
			downtrip = 1;
		}
		if (status.flags & (CLIPPY_PASTE_SELECTION|CLIPPY_PASTE_BUFFER)) {
			clippy_paste((status.flags & CLIPPY_PASTE_BUFFER)
				     ? CLIPPY_BUFFER : CLIPPY_SELECT);
			status.flags &= ~(CLIPPY_PASTE_BUFFER|CLIPPY_PASTE_SELECTION);
		}

		check_update();

		switch (song_get_mode()) {
		case MODE_PLAYING:
		case MODE_PATTERN_LOOP:
			if (screensaver) {
				video_toggle_screensaver(0);
				screensaver = 0;
			}
			break;
		default:
			if (!screensaver) {
				video_toggle_screensaver(1);
				screensaver = 1;
			}
			break;
		};

		if (status.flags & DISKWRITER_ACTIVE) {
			int q = disko_sync();
			while (q == DW_SYNC_MORE && !events_have_event()) {
				check_update();
				q = disko_sync();
			}
			if (q == DW_SYNC_DONE) {
#ifdef ENABLE_HOOKS
				run_disko_complete_hook();
#endif
				if (diskwrite_to) {
					printf("Diskwrite complete, exiting...\n");
					schism_exit(0);
				}
			}
		}

		/* let dmoz build directory lists, etc
		 *
		 * as long as there's no user-event going on... */
		while (!(status.flags & NEED_UPDATE) && dmoz_worker() && !events_have_event());

		/* delay until there's an event OR 10 ms have passed */
		int t;
		for (t = 0; t < 10 && !events_have_event(); t++)
			timer_msleep(1);
	}
	
	schism_exit(0);
}

void schism_exit(int status)
{
#if ENABLE_HOOKS
	if (shutdown_process & EXIT_HOOK)
		run_exit_hook();
#endif

	free_audio_device_list();

#ifdef USE_FLAC
	flac_quit();
#endif

	if (shutdown_process & EXIT_SAVECFG)
		cfg_atexit_save();

	if (shutdown_process & EXIT_SDLQUIT) {
		// Clear to black on exit (nicer on Wii; I suppose it won't hurt elsewhere)
		video_refresh();
		video_blit();
		video_shutdown();
		/*
		Don't use this function as atexit handler, because that will cause
		segfault when MESA runs on Wayland or KMS/DRM: Never call SDL_Quit()
		inside an atexit handler.
		You're probably still on X11 if this has not bitten you yet.
		See long-standing bug: https://github.com/libsdl-org/SDL/issues/3184
			/ Vanfanel
		*/
	}

	song_lock_audio();
	song_stop_unlocked(1);
	song_unlock_audio();

	dmoz_quit();
	audio_quit();
	clippy_quit();
	events_quit();
#ifndef HAVE_LOCALTIME_R
	localtime_r_quit();
#endif
	mt_quit();
	timer_quit();

	os_sysexit();

	exit(status);
}

extern void vis_init(void);

/* the real main function is called per-platform */
int schism_main(int argc, char** argv)
{
	os_sysinit(&argc, &argv);

	vis_init();

	ver_init();

	tzset(); // localtime_r wants this
	srand(time(NULL));
	parse_options(argc, argv); /* shouldn't this be like, first? */

	cfg_init_dir();

#if ENABLE_HOOKS
	if (startup_flags & SF_HOOKS) {
		run_startup_hook();
		shutdown_process |= EXIT_HOOK;
	}
#endif

	/* Eh. */
	log_append2(0, 3, 0, schism_banner(0));
	log_nl();

	if (!dmoz_init()) {
		log_nl();
		log_appendf(4, "Failed to initialize a filesystem backend!");
		log_appendf(4, "Portable mode will not work properly!");
	}

	if (!timer_init()) {
		os_show_message_box("Critical error!", "Failed to initialize a timers backend!");
		return 1;
	}

	if (!mt_init()) {
		os_show_message_box("Critical error!", "Failed to initialize a multithreading backend!");
		return 1;
	}

#ifndef HAVE_LOCALTIME_R
	if (!localtime_r_init()) {
		os_show_message_box("Critical error!", "Failed to initialize localtime_r replacement!");
		return 1;
	}
#endif

	song_initialise();
	cfg_load();

	if (!clippy_init()) {
		log_nl();
		log_appendf(4, "Failed to initialize a clipboard backend!");
		log_appendf(4, "Copying to the system clipboard will not work properly!");
	}

	if (did_classic) {
		status.flags &= ~CLASSIC_MODE;
		if (startup_flags & SF_CLASSIC) status.flags |= CLASSIC_MODE;
	}

	if (!(startup_flags & SF_NETWORK))
		status.flags |= NO_NETWORK;

	shutdown_process |= EXIT_SAVECFG;
	shutdown_process |= EXIT_SDLQUIT;

	display_init();
	if (want_fullscreen >= 0)
		video_fullscreen(want_fullscreen);

	palette_apply();
	font_init();
	midi_engine_start();
	audio_init(audio_driver, audio_device);
	song_init_modplug();

#ifdef USE_FLAC
	flac_init();
#endif

#ifndef SCHISM_WIN32
	signal(SIGINT, exit);
	signal(SIGQUIT, exit);
	signal(SIGTERM, exit);
#endif

	video_mousecursor(cfg_video_mousecursor);
	status_text_flash(" "); /* silence the mouse cursor message */

	load_pages();
	main_song_changed_cb();

	if (initial_song && !initial_dir) {
		initial_dir = dmoz_path_get_parent_directory(initial_song);
		if (!initial_dir) {
			initial_dir = dmoz_get_current_directory();
		}
	}
	if (initial_dir) {
		strncpy(cfg_dir_modules, initial_dir, ARRAY_SIZE(cfg_dir_modules) - 1);
		cfg_dir_modules[ARRAY_SIZE(cfg_dir_modules) - 1] = 0;
		strncpy(cfg_dir_samples, initial_dir, ARRAY_SIZE(cfg_dir_samples) - 1);
		cfg_dir_samples[ARRAY_SIZE(cfg_dir_samples) - 1] = 0;
		strncpy(cfg_dir_instruments, initial_dir, ARRAY_SIZE(cfg_dir_instruments) - 1);
		cfg_dir_instruments[ARRAY_SIZE(cfg_dir_instruments) - 1] = 0;
		free(initial_dir);
	}

	if (startup_flags & SF_FONTEDIT) {
		status.flags |= STARTUP_FONTEDIT;
		set_page(PAGE_FONT_EDIT);
		free(initial_song);
	} else if (initial_song) {
		set_page(PAGE_LOG);
		if (song_load_unchecked(initial_song)) {
			if (diskwrite_to) {
				// make a guess?
				const char *multi = strcasestr(diskwrite_to, "%c");
				const char *driver = (strcasestr(diskwrite_to, ".aif")
						      ? (multi ? "MAIFF" : "AIFF")
						      : (multi ? "MWAV" : "WAV"));
				if (song_export(diskwrite_to, driver) != SAVE_SUCCESS) {
					schism_exit(1);
				}
			} else if (startup_flags & SF_PLAY) {
				song_start();
				set_page(PAGE_INFO);
			}
		}
		free(initial_song);
	} else {
		set_page(PAGE_ABOUT);
	}

#if HAVE_NICE
	if (nice(1) == -1) {
	}
#endif
	/* poll once */
	midi_engine_poll_ports();

	event_loop();

	return 0; /* blah */
}

#if defined(SCHISM_WIN32)
# include "loadso.h"
# include <windows.h>

static char **utf8_argv = NULL;
static int utf8_argc = 0;

void win32_atexit(void)
{
	for (int i = 0; i < utf8_argc; i++)
		free(utf8_argv[i]);

	free(utf8_argv);
}

int main(int argc, char **argv)
{
	int i;

	// We only want to get the Unicode arguments if we are able to
	LPWSTR *(WINAPI *WIN32_CommandLineToArgvW)(LPCWSTR lpCmdLine,int *pNumArgs);
	LPWSTR (WINAPI *WIN32_GetCommandLineW)(void);

	void *lib_kernel32 = loadso_object_load("KERNEL32.DLL");
	void *lib_shell32 = loadso_object_load("SHELL32.DLL");

	if (lib_kernel32 && lib_shell32) {
		WIN32_CommandLineToArgvW = loadso_function_load(lib_shell32, "CommandLineToArgvW");
		WIN32_GetCommandLineW = loadso_function_load(lib_kernel32, "GetCommandLineW");

		if (WIN32_CommandLineToArgvW && WIN32_GetCommandLineW) {
			int argcw;
			LPWSTR *argvw = CommandLineToArgvW(WIN32_GetCommandLineW(), &argcw);

			utf8_argc = argcw;

			if (argvw) {
				// now we have Unicode arguments!...

				utf8_argv = mem_alloc(sizeof(char *) * utf8_argc);

				for (i = 0; i < argcw; i++) {
					charset_iconv(argvw[i], &utf8_argv[i], CHARSET_WCHAR_T, CHARSET_CHAR, SIZE_MAX);
					if (!utf8_argv[i])
						utf8_argv[i] = str_dup(""); // ...
				}

				LocalFree(argvw);

				goto have_utf8_args;
			}
		}
	}

	// fallback, ANSI
	utf8_argc = argc;
	utf8_argv = mem_alloc(sizeof(char *) * utf8_argc);

	for (i = 0; i < argc; i++) {
		charset_iconv(argv[i], &utf8_argv[i], CHARSET_ANSI, CHARSET_CHAR, SIZE_MAX);
		if (!utf8_argv[i])
			utf8_argv[i] = str_dup(""); // ...
	}

have_utf8_args: ;
	if (lib_kernel32)
		loadso_object_unload(lib_kernel32);

	if (lib_shell32)
		loadso_object_unload(lib_shell32);

	// copy so our arguments don't get warped by main
	char *utf8_argv_cp[utf8_argc];

	for (i = 0; i < utf8_argc; i++)
		utf8_argv_cp[i] = utf8_argv[i];

	// make sure our arguments actually get free'd
	atexit(win32_atexit);

	schism_main(argc, utf8_argv_cp);

	// never happens; we use exit()
	return 0;
}
#elif defined(SCHISM_MACOSX)
// handled in its own file
#else
int main(int argc, char **argv)
{
	// do nothing special
	return schism_main(argc, argv);
}
#endif
