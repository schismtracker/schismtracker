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

#include "test.h"

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
#include "mt.h"
#include "mem.h"
#include "vgamem.h"

#include "osdefs.h"

#if !defined(SCHISM_WIN32) && !defined(SCHISM_OS2) && !defined(SCHISM_XBOX)
# include <signal.h>
#endif

#if !defined(__amigaos4__) && !defined(SCHISM_WII)
# define ENABLE_HOOKS 1
#endif

/* --------------------------------------------------------------------- */
/* globals */

enum {
	EXIT_HOOK = 1,
	EXIT_SAVECFG = 4,
	EXIT_SDLQUIT = 16,
};
static int shutdown_process = 0;

static char *video_driver = NULL;
static char *audio_driver = NULL;
static char *audio_device = NULL;
static int want_fullscreen = -1;
static int did_classic = 0;

/* --------------------------------------------------------------------- */

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
	SF_HEADLESS = 32,
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
	O_HEADLESS,
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
		{"headless", 0, NULL, O_HEADLESS},
		{"version", 0, NULL, O_VERSION},
		{"help", 0, NULL, O_HELP},
		{NULL, 0, NULL, 0},
	};
	int opt;

	optind = 1; /* Devkitppc bug ??? what the hell */

	while ((opt = getopt_long(argc, argv, SHORT_OPTIONS, long_options, NULL)) != -1) {
		switch (opt) {
		case O_SDL_AUDIODRIVER:
			audio_parse_driver_spec(optarg, (char**)&audio_driver, (char**)&audio_device);
			break;
		case O_SDL_VIDEODRIVER:
			// XXX this has been broken for ages
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
		case O_HEADLESS:
			startup_flags |= SF_HEADLESS;
			break;
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
				"      --headless\n"
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
	static timer_ticks_t next = 0;
	timer_ticks_t now = timer_ticks();

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
	kk->rx = NATIVE_SCREEN_WIDTH / VGAMEM_COLUMNS;
	kk->ry = NATIVE_SCREEN_HEIGHT / VGAMEM_ROWS;
	/* preserve the start position */
	kk->sx = start_x;
	kk->sy = start_y;
}

/* -------------------------------------------- */

SCHISM_NORETURN static void event_loop(void)
{
	unsigned int lx = 0, ly = 0; /* last x and y position (character) */
	timer_ticks_t last_mouse_down, ticker, last_audio_poll;
	time_t startdown;
	int downtrip;
	int fix_numlock_key;
	int screensaver;
	int button = -1;
	struct key_event kk;

	int keyboard_focus = 0;

	fix_numlock_key = status.fix_numlock_setting;

	downtrip = 0;
	last_mouse_down = 0;
	last_audio_poll = timer_ticks();
	startdown = 0;
	status.last_keysym = 0;
	status.last_orig_keysym = 0;

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
				handle_text_input(se.text.text);
				break;
			}
			case SCHISM_KEYDOWN:
				if (se.key.repeat) {
					// Only use the system repeat if we don't have our own configuration
					if (kbd_key_repeat_enabled())
						break;

					kk.is_repeat = 1;
				}

				/* normalize the text for processing */
				kk.text = charset_compose_to_set(se.key.text, CHARSET_UTF8,
					CHARSET_UTF8);

				SCHISM_FALLTHROUGH;
			case SCHISM_KEYUP:
				kk.sym = se.key.sym;
				kk.scancode = se.key.scancode;

				kk.mouse = MOUSE_NONE;

				/* grab the keymod */
				kk.mod = se.key.mod;
				/* apply OS-specific fixups to account for bugs in SDL */
				os_get_modkey(&kk.mod);
				/* numlock hacks, mostly here because of macs */
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
				/* copy the keymod into the status global */
				status.keymod = kk.mod;

				/* apply translations for different keyboard layouts,
				 * e.g. shift-8 -> asterisk on standard U.S. */
				kbd_key_translate(&kk);
				/* airball */
				handle_key(&kk);

				if (se.type == SCHISM_KEYUP) {
					/* only empty the key repeat if
					 * the last keydown is the same sym */
					if ((status.last_orig_keysym == kk.orig_sym)
						|| kbd_is_modifier_key(&kk))
						kbd_empty_key_repeat();
				} else {
					kbd_cache_key_repeat(&kk);

					if (!kbd_is_modifier_key(&kk)) {
						status.last_keysym = kk.sym;
						status.last_orig_keysym = kk.orig_sym;
						status.last_keymod = kk.mod;
					}
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
					kk.state = KEY_DRAG; /* ummm */
					video_translate(se.wheel.mouse_x, se.wheel.mouse_y, &kk.fx, &kk.fy);
					kk.mouse = (se.wheel.y > 0) ? MOUSE_SCROLL_UP : MOUSE_SCROLL_DOWN;
					break;
				case SCHISM_MOUSEMOTION:
					kk.state = KEY_DRAG;
					video_translate(se.motion.x, se.motion.y, &kk.fx, &kk.fy);
					break;
				case SCHISM_MOUSEBUTTONDOWN:
					video_translate(se.button.x, se.button.y, &kk.fx, &kk.fy);
					// we also have to update the current button
					if (se.button.button == MOUSE_BUTTON_LEFT) {
						/* macosx cruft: Ctrl-LeftClick = RightClick */
						button = (status.keymod & SCHISM_KEYMOD_CTRL) ? (MOUSE_BUTTON_RIGHT)
							: (status.keymod & (SCHISM_KEYMOD_ALT|SCHISM_KEYMOD_GUI)) ? (MOUSE_BUTTON_MIDDLE)
							: MOUSE_BUTTON_LEFT;
					} else {
						button = se.button.button;
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
						if (lx == kk.x && ly == kk.y && (ticker - last_mouse_down) < 300) {
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
					switch (kk.state) {
					case KEY_PRESS:
						if (widget_change_focus_to_xy(kk.x, kk.y)) {
							kk.on_target = 1;
						} else {
							kk.on_target = 0;
						}
						break;
					case KEY_DRAG:
						kk.on_target = (widget_find_xy(kk.x, kk.y) == *selected_widget);
					}
					if (se.type == SCHISM_MOUSEBUTTONUP && downtrip) {
						downtrip = 0;
						break;
					}
					//if (se.type != SCHISM_MOUSEMOTION)
						handle_key(&kk);
					break;
				default:
					break;
				};
				break;
			}
			/* this logic sucks, but it does what should probably be considered the "right" thing to do */
			case SCHISM_WINDOWEVENT_FOCUS_GAINED:
				keyboard_focus = 1;
				SCHISM_FALLTHROUGH;
			case SCHISM_WINDOWEVENT_SHOWN:
				video_mousecursor(MOUSE_RESET_STATE);
				break;
			case SCHISM_WINDOWEVENT_ENTER:
				if (keyboard_focus)
					video_mousecursor(MOUSE_RESET_STATE);
				break;
			case SCHISM_WINDOWEVENT_FOCUS_LOST:
				keyboard_focus = 0;
				SCHISM_FALLTHROUGH;
			case SCHISM_WINDOWEVENT_LEAVE:
				video_show_cursor(1);
				break;
			case SCHISM_WINDOWEVENT_RESIZED:
			case SCHISM_WINDOWEVENT_SIZE_CHANGED: /* tiling window managers */
				video_resize(se.window.data.resized.width, se.window.data.resized.height);
				SCHISM_FALLTHROUGH;
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

		// Check for new audio devices every 5 seconds.
		if (timer_ticks_passed(timer_ticks(), last_audio_poll + 5000)) {
			refresh_audio_device_list();
			status.flags |= NEED_UPDATE;
			last_audio_poll = timer_ticks();
		}

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

		{
			/* This takes a LOT of time if we just base it on files.
			 * So here I've just based it on how LONG we've spent.
			 *
			 * I'm just setting it to a maximum of 10ms, which I think
			 * is an okay amount of time to spend on it.
			 *
			 * Ideally we would do this stuff in a different thread, so
			 * that the main thread doesn't get hogged by it at all, but
			 * that would mean guarding it all behind mutexes. :( */
			timer_ticks_t start = timer_ticks();

			while (start + 10 > timer_ticks() && dmoz_worker() && !events_have_event());
		}

		/* sleep for a little bit to not hog CPU time */
		if (!events_have_event())
			timer_msleep(5);
	}

	schism_exit(0);
}

#ifndef NDEBUG
/* schism equivalent to built-in C assert() macro.
 *
 * In general this mimics the Visual C++ assert() implementation, which
 * also tries (but seemingly fails) to include the current process name.
 * We don't care about that, so we just give the file, line, and the
 * expression that caused the assertion. */
void schism_assert_fail(const char *msg, const char *exp, const char *file, int line)
{
	const char *format =
		"Assertion failed: \"%s\".\n"
		"\n"
		"File: %s\n"
		"Line: %d\n"
		"\n"
		"Expression: %s\n"
		"\n"
		"Schism Tracker will now terminate.";

	if (msgbox(OS_MESSAGE_BOX_ERROR, "Assertion triggered!", format, msg, file, line, exp) < 0) {
		/* fall back to printing to stderr, I guess */
		fprintf(stderr, format, msg, file, line, exp);
		fputc('\n', stderr);
	}

	/* XXX should this use schism_exit ?
	 * I mean, it's not like it's totally necessary to exit everything.
	 * Especially if we're coming from mem.c or something. */
	exit(1);
}
#endif

/* basic AF crash handler
 * platform-specific stuff can be printed out into the logfile in the log
 * callback. */
void schism_crash(void (*log_cb)(FILE *f, void *userdata), void *userdata)
{
	char name_template[512]; /* should be plenty */
	size_t name_template_len;

	{
		time_t thetime;
		struct tm tm;

		thetime = time(NULL);
		localtime_r(&thetime, &tm);

		/* TODO its more safe to shove it in LocalAppData or similar
		 * make sure we have enough space to put in a file extension */
		name_template_len = strftime(name_template, sizeof(name_template) - 16, "schism_crash_%Y-%m-%d_%H-%M-%S", &tm);
	}

	if (log_cb) {
		FILE *log;

		memcpy(name_template + name_template_len, ".log", 5);
		log = fopen(name_template, "w");

		log_cb(log, userdata);

		fclose(log);
	}

	/* now that we've sufficiently dumped the stuff, at least try to
	 * save the current song */
	if (current_song && (status.flags & SONG_NEEDS_SAVE)) {
		int i;
		int try_again = 1;

		const char *extension = dmoz_path_get_extension(song_get_filename());
		if (extension && *extension) {
			char type[4];
			strncpy(type, extension + 1, 3);
			type[3] = 0;

			/* convert to uppercase */
			for (i = 0; i < 3; i++)
				type[i] = toupper(type[i]);

			strncpy(name_template + name_template_len, extension, 15);
			name_template[name_template_len + 15] = 0;

			try_again = (song_save(name_template, type) != SAVE_SUCCESS);
		}

		if (try_again) {
			/* welp, either we have no extension, or no big hints, so
			 * at least attempt to save SOMETHING */
			const char *type = "IT";
			int len = 2;

			/* Instrument data is probably more important? */
			if (!(current_song->flags & SONG_INSTRUMENTMODE)) {
				int i;

				/* look for any possible adlib samples; if we do have some,
				 * we have to save as S3M .. */
				for (i = 1; i <= MAX_SAMPLES; i++) {
					if (current_song->samples[i].flags & CHN_ADLIB) {
						type = "S3M";
						len = 3;
						break;
					}
				}
			}

			name_template[name_template_len] = '.';
			for (i = 0; i < len; i++)
				name_template[name_template_len + 1 + i] = tolower(type[i]);
			name_template[name_template_len + 1 + len] = 0;

			try_again = (song_save(name_template, type) != SAVE_SUCCESS);
		}
	}

	{
		char *dir = dmoz_get_current_directory();
		const char *thebox;

		if (log_cb && current_song && (status.flags & SONG_NEEDS_SAVE)) {
			thebox = "a crash log and a copy of the current song";
		} else if (log_cb) {
			thebox = "a crash log";
		} else if (current_song && (status.flags & SONG_NEEDS_SAVE)) {
			thebox = "a copy of the current song";
		} else {
			thebox = "absolutely nothing";
		}

		msgbox(OS_MESSAGE_BOX_ERROR, "Schism Tracker has crashed!",
			"If all went well, there should be %s saved in %s.\n"
			"\n"
			"You should report this to the bug tracker at "
			"https://github.com/schismtracker/schismtracker/issues",
			thebox, dir);

		free(dir);
	}
}

void schism_exit(int x)
{
#if ENABLE_HOOKS
	if (shutdown_process & EXIT_HOOK)
		run_exit_hook();
#endif

	free_audio_device_list();

#ifdef USE_FLAC
	flac_quit();
#endif
#ifdef USE_AVFORMAT
	avformat_quit();
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

	midi_engine_stop();

	dmoz_quit();
	audio_quit();
	clippy_quit();
	events_quit();
#ifndef HAVE_LOCALTIME_R
	localtime_r_quit();
#endif
	timer_quit();
	mt_quit();

	os_sysexit();

	exit(x);
}

extern void vis_init(void);

/* the real main function is called per-platform */
int schism_main(int argc, char** argv)
{
	os_sysinit(&argc, &argv);

	vis_init();

	ver_init();

#if defined(HAVE_TZSET) && !defined(SCHISM_WIN32) /* ok */
	tzset(); // localtime_r wants this
#endif
	srand(time(NULL));
	parse_options(argc, argv); /* shouldn't this be like, first? */

	if (startup_flags & SF_HEADLESS)
		status.flags |= STATUS_IS_HEADLESS; // for the log

	/* Eh. */
	log_append2(0, 3, 0, schism_banner(0));
	log_nl();

#ifndef SCHISM_MACOS /* done in macos_sysinit */
	if (!dmoz_init()) {
		log_appendf(4, "Failed to initialize a filesystem backend!");
		log_appendf(4, "Portable mode will not work properly!");
		log_nl();
	}

	cfg_init_dir();
#endif

#if ENABLE_HOOKS
	if (startup_flags & SF_HOOKS) {
		run_startup_hook();
		shutdown_process |= EXIT_HOOK;
	}
#endif

	SCHISM_RUNTIME_ASSERT(mt_init(), "Failed to initialize a multithreading backend!");
	SCHISM_RUNTIME_ASSERT(timer_init(), "Failed to initialize a timers backend!");

#ifndef HAVE_LOCALTIME_R
	SCHISM_RUNTIME_ASSERT(localtime_r_init(), "Failed to initialize localtime_r replacement!");
#endif

	song_initialise();
	cfg_load();

	if (!clippy_init()) {
		log_appendf(4, "Failed to initialize a clipboard backend!");
		log_appendf(4, "Copying to the system clipboard will not work properly!");
		log_nl();
	}

	if (did_classic) {
		status.flags &= ~CLASSIC_MODE;
		if (startup_flags & SF_CLASSIC) status.flags |= CLASSIC_MODE;
	}

	if (!(startup_flags & SF_NETWORK))
		status.flags |= NO_NETWORK;

	shutdown_process |= EXIT_SAVECFG;
	shutdown_process |= EXIT_SDLQUIT;

	if (startup_flags & SF_HEADLESS) {
		if (!diskwrite_to) {
			fprintf(stderr, "Error: --headless requires --diskwrite\n");
			return 1;
		}
		if (!initial_song) {
			fprintf(stderr, "Error: --headless requires an input song file\n");
			return 1;
		}

		// Initialize modplug only
		song_init_modplug();

		// Load and export song
		if (song_load_unchecked(initial_song)) {
			const char *multi = strcasestr(diskwrite_to, "%c");
			const char *driver = (strcasestr(diskwrite_to, ".aif")
					  ? (multi ? "MAIFF" : "AIFF")
					  : (multi ? "MWAV" : "WAV"));
			if (song_export(diskwrite_to, driver) != SAVE_SUCCESS) {
				schism_exit(1);
			}

			// Wait for diskwrite to complete
			while (status.flags & DISKWRITER_ACTIVE) {
				int q = disko_sync();
				if (q == DW_SYNC_DONE) {
					break;
				} else if (q != DW_SYNC_MORE) {
					fprintf(stderr, "Error: Diskwrite failed\n");
					schism_exit(1);
				}
			}
			schism_exit(0);
		} else {
			fprintf(stderr, "Error: Failed to load song %s\n", initial_song);
			schism_exit(1);
		}
	}

	SCHISM_RUNTIME_ASSERT(!video_startup(), "Failed to initialize video!");
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
#ifdef USE_AVFORMAT
	avformat_init();
#endif

#if !defined(SCHISM_WIN32) && !defined(SCHISM_OS2) && !defined(SCHISM_XBOX)
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

#if defined(SCHISM_TEST_BUILD)
int main(int argc, char *argv[])
{
	return schism_test_main(argc, argv);
}
#elif defined(SCHISM_MACOSX)
// sys/macosx/macosx-sdlmain.m
#else
int main(int argc, char **argv)
{
	// do nothing special
	return schism_main(argc, argv);
}
#endif
