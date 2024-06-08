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

#include "event.h"

#include "clippy.h"
#include "disko.h"

#include "version.h"
#include "song.h"
#include "midi.h"
#include "dmoz.h"
#include "charset.h"

#include "osdefs.h"

#include <errno.h>

#include "sdlmain.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef SCHISM_WIN32
# include <signal.h>
#endif

#include <getopt.h>

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
static int did_fullscreen = 0;
static int did_classic = 0;

/* --------------------------------------------------------------------- */

static void display_print_info(void)
{
	log_append(2, 0, "Video initialised");
	log_underline(17);
	video_report();
}

/* If we're not not debugging, don't not dump core. (Have I ever mentioned
 * that NDEBUG is poorly named -- or that identifiers for settings in the
 * negative form are a bad idea?) */
#if defined(NDEBUG)
# define SDL_INIT_FLAGS SDL_INIT_TIMER | SDL_INIT_VIDEO
#else
# define SDL_INIT_FLAGS SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE
#endif

static void sdl_init(void)
{
	const char *err;
	if (SDL_Init(SDL_INIT_FLAGS) == 0)
		return;
	err = SDL_GetError();
	if (strstr(err, "mouse")) {
		// see if we can start up mouseless
		status.flags |= NO_MOUSE;
		put_env_var("SDL_NOMOUSE", "1");
		if (SDL_Init(SDL_INIT_FLAGS) == 0)
			return;
	}
	fprintf(stderr, "SDL_Init: %s\n", err);
	schism_exit(1);
}

static void display_init(void)
{
	SDL_SysWMinfo	info;
	SDL_VERSION(&info.version);

	video_startup();

	if (SDL_GetWindowWMInfo(video_window(), &info)) {
		status.flags |= WM_AVAILABLE;
	}

	display_print_info();
	SDL_StartTextInput();
}

static void check_update(void);

void toggle_display_fullscreen(void)
{
	video_fullscreen(-1);
	status.flags |= (NEED_UPDATE);
}

/* --------------------------------------------------------------------- */

static void handle_window_event(SDL_WindowEvent *w)
{
	switch (w->event) {
	case SDL_WINDOWEVENT_SHOWN:
		status.flags |= (IS_VISIBLE|SOFTWARE_MOUSE_MOVED);
		video_mousecursor(MOUSE_RESET_STATE);
		break;
	case SDL_WINDOWEVENT_HIDDEN:
		status.flags &= ~IS_VISIBLE;
		break;
	case SDL_WINDOWEVENT_FOCUS_GAINED:
		status.flags |= IS_FOCUSED;
		video_mousecursor(MOUSE_RESET_STATE);
		break;
	case SDL_WINDOWEVENT_FOCUS_LOST:
		status.flags &= ~IS_FOCUSED;
		SDL_ShowCursor(SDL_ENABLE);
		break;
	case SDL_WINDOWEVENT_RESIZED:
	case SDL_WINDOWEVENT_SIZE_CHANGED: /* tiling window managers */
		video_update();
		/* fallthrough */
	case SDL_WINDOWEVENT_EXPOSED:
		status.flags |= (NEED_UPDATE);
		break;
	default:
#if 0
		/* ignored currently */
		SDL_WINDOWEVENT_NONE,           /**< Never used */
		SDL_WINDOWEVENT_MINIMIZED,      /**< Window has been minimized */
		SDL_WINDOWEVENT_MAXIMIZED,      /**< Window has been maximized */
		SDL_WINDOWEVENT_RESTORED,       /**< Window has been restored to normal size
						 and position */
		SDL_WINDOWEVENT_ENTER,          /**< Window has gained mouse focus */
		SDL_WINDOWEVENT_LEAVE,          /**< Window has lost mouse focus */
		SDL_WINDOWEVENT_CLOSE,          /**< The window manager requests that the window be closed */
		SDL_WINDOWEVENT_TAKE_FOCUS,     /**< Window is being offered a focus (should SetWindowInputFocus() on itself or a subwindow, or ignore) */
		SDL_WINDOWEVENT_HIT_TEST        /**< Window had a hit test that wasn't SDL_HITTEST_NORMAL. */
#endif
		break;
	}
}

/* --------------------------------------------------------------------- */

#if ENABLE_HOOKS
static void run_startup_hook(void)
{
	run_hook(cfg_dir_dotschism, "startup-hook", NULL);
}
static void run_disko_complete_hook(void)
{
	run_hook(cfg_dir_dotschism, "diskwriter-hook", NULL);
}

static void run_exit_hook(void)
{
	run_hook(cfg_dir_dotschism, "exit-hook", NULL);
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
			audio_driver = str_dup(optarg);
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
			video_fullscreen(1);
			did_fullscreen = 1;
			break;
		case O_NO_FULLSCREEN:
			video_fullscreen(0);
			did_fullscreen = 1;
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

	char *cwd = get_current_directory();
	for (; optind < argc; optind++) {
		char *arg = argv[optind];
		char *tmp = dmoz_path_concat(cwd, arg);
		if (!tmp) {
			perror(arg);
			continue;
		}
		char *norm = dmoz_path_normal(tmp);
		free(tmp);
		if (is_directory(arg)) {
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
	static unsigned long next = 0;

	/* is there any reason why we'd want to redraw
	   the screen when it's not even visible? */
	if ((status.flags & (NEED_UPDATE | IS_VISIBLE)) == (NEED_UPDATE | IS_VISIBLE)) {
		status.flags &= ~(NEED_UPDATE | SOFTWARE_MOUSE_MOVED);
		if ((status.flags & (IS_FOCUSED | LAZY_REDRAW)) == LAZY_REDRAW) {
			if (SDL_GetTicks() < next)
				return;
			next = SDL_GetTicks() + 500;
		} else if (status.flags & (DISKWRITER_ACTIVE | DISKWRITER_ACTIVE_PATTERN)) {
			if (SDL_GetTicks() < next)
				return;
			next = SDL_GetTicks() + 100;
		}
		redraw_screen();
		video_refresh();
		video_blit();
	} else if (status.flags & SOFTWARE_MOUSE_MOVED) {
		video_blit();
		status.flags &= ~(SOFTWARE_MOUSE_MOVED);
	}
}

static void _do_clipboard_paste_op(SDL_Event *e)
{
	if (ACTIVE_WIDGET.clipboard_paste
	&& ACTIVE_WIDGET.clipboard_paste(e->user.code,
				e->user.data1)) return;

	if (ACTIVE_PAGE.clipboard_paste
	&& ACTIVE_PAGE.clipboard_paste(e->user.code,
				e->user.data1)) return;

	handle_text_input((uint8_t*)e->user.data1);
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

/* -------------------------------------------------- */

/* These are here for linking text input to keyboard inputs.
 * If no keyboard input can be found, then the text will
 * be sent to the already existing handlers written for the
 * original port to SDL2.
 *
 * - paper */

static struct key_event pending_keydown;
static int have_pending_keydown = 0;

static void push_pending_keydown_event(struct key_event* kk) {
	if (!have_pending_keydown) {
		pending_keydown = *kk;
		have_pending_keydown = 1;
	}
}

static void pop_pending_keydown_event(const uint8_t* text) {
	/* text can and will be NULL here. when it is not NULL, it
	 * should be in CP437 */
	if (have_pending_keydown) {
		pending_keydown.text = text;
		handle_key(&pending_keydown);
		cache_key_repeat(&pending_keydown);
		have_pending_keydown = 0;
	}
}

/* -------------------------------------------- */

static void event_loop(void)
{
	SDL_Event event;
	unsigned int lx = 0, ly = 0; /* last x and y position (character) */
	uint32_t last_mouse_down, ticker;
	SDL_Keycode last_key = 0;
	int modkey;
	time_t startdown;
#ifdef os_screensaver_deactivate
	time_t last_ss;
#endif
	int downtrip;
	int wheel_x;
	int wheel_y;
	int sawrep;
	int fix_numlock_key;
	struct key_event kk;

	fix_numlock_key = status.fix_numlock_setting;

	downtrip = 0;
	last_mouse_down = 0;
	startdown = 0;
	status.last_keysym = 0;

	modkey = SDL_GetModState();
	os_get_modkey(&modkey);
	SDL_SetModState(modkey);

#ifdef os_screensaver_deactivate
	time(&last_ss);
#endif
	time(&status.now);
	localtime_r(&status.now, &status.tmnow);
	for (;;) {
		while (SDL_PollEvent(&event)) {
			/* handle the current event queue */
			if (!os_sdlevent(&event))
				continue;

			key_event_reset(&kk, kk.sx, kk.sy);

			sawrep = 0;
			if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
				kk.state = KEY_PRESS;
			} else if (event.type == SDL_KEYUP || event.type == SDL_MOUSEBUTTONUP) {
				kk.state = KEY_RELEASE;
			}
			if (event.type == SDL_KEYDOWN || event.type == SDL_TEXTINPUT || event.type == SDL_KEYUP) {
				if (event.key.keysym.sym == 0) {
					// XXX when does this happen?
					kk.mouse = MOUSE_NONE;
					kk.is_repeat = 0;
				}
			}
			switch (event.type) {
#if defined(SCHISM_WIN32)
#define _ALTTRACKED_KMOD        (KMOD_NUM|KMOD_CAPS)
#else
#define _ALTTRACKED_KMOD        0
#endif
			case SDL_TEXTINPUT: {
				uint8_t* input_text = NULL;

				charset_error_t err = charset_iconv((uint8_t*)event.text.text, &input_text, CHARSET_UTF8, CHARSET_CP437);
				if (err || !input_text) {
					log_appendf(4, " [ERROR] failed to convert SDL text input event");
					log_appendf(4, "  %s", event.text.text);
					log_appendf(4, " into CP437 with error %s.", charset_iconv_error_lookup(err));
					log_appendf(4, " please report this on the github!");
					break;
				}

				if (have_pending_keydown) {
					pop_pending_keydown_event(input_text);
				} else {
					/* wtf? whatever, send it to the text input
					 * handlers I guess, don't throw it out! */
					handle_text_input(input_text);
				}

				free(input_text);
				break;
			}
			case SDL_KEYDOWN:
				/* we have our own repeat handler now */
				if (event.key.repeat)
					break;

				/* fallthrough */
			case SDL_KEYUP:
				pop_pending_keydown_event(NULL);
				switch (event.key.keysym.sym) {
				case SDLK_NUMLOCKCLEAR:
					modkey ^= KMOD_NUM;
					break;
				case SDLK_CAPSLOCK:
					if (event.type == SDL_KEYDOWN) {
						status.flags |= CAPS_PRESSED;
					} else {
						status.flags &= ~CAPS_PRESSED;
					}
					modkey ^= KMOD_CAPS;
					break;
				case SDLK_LSHIFT: case SDLK_RSHIFT:
					if (event.type == SDL_KEYDOWN)
						status.flags |= SHIFT_KEY_DOWN;
					else
						status.flags &= ~SHIFT_KEY_DOWN;
					break;
				default:
					break;
				};

				if (kk.state == KEY_PRESS) {
					modkey = (event.key.keysym.mod
						& ~(_ALTTRACKED_KMOD))
						| (modkey & _ALTTRACKED_KMOD);
				}

				os_get_modkey(&modkey);

				kk.sym = event.key.keysym.sym;
				kk.scancode = event.key.keysym.scancode;

				switch (fix_numlock_key) {
				case NUMLOCK_GUESS:
					/* should be handled per OS */
					break;
				case NUMLOCK_ALWAYS_OFF:
					modkey &= ~KMOD_NUM;
					break;
				case NUMLOCK_ALWAYS_ON:
					modkey |= KMOD_NUM;
					break;
				};

				kk.mod = modkey;
				kk.mouse = MOUSE_NONE;
				key_translate(&kk);

				if (event.type == SDL_KEYUP) {
					handle_key(&kk);

					/* only empty the key repeat if
					 * the last keydown is the same sym */
					if (last_key == kk.sym)
						empty_key_repeat();
				} else {
					push_pending_keydown_event(&kk);

					/* reset key repeat regardless */
					empty_key_repeat();

					/* TODO this ought to be handled in
					 * pop_pending_keydown_event() */
					status.last_keysym = last_key;
					last_key = kk.sym;
				}
				break;
			case SDL_QUIT:
				show_exit_prompt();
				break;
			case SDL_WINDOWEVENT:
				/* reset this... */
				modkey = SDL_GetModState();
				os_get_modkey(&modkey);
				SDL_SetModState(modkey);

				handle_window_event(&event.window);
				break;
			case SDL_MOUSEWHEEL:
				kk.state = -1;  /* neither KEY_PRESS nor KEY_RELEASE */
#if SDL_VERSION_ATLEAST(2, 26, 0)
				/* SDL just sends this to us anyway, don't ask for it again */
				wheel_x = event.wheel.mouseX;
				wheel_y = event.wheel.mouseY;
#else
				SDL_GetMouseState(&wheel_x, &wheel_y);
				video_get_logical_coordinates(wheel_x, wheel_y, &wheel_x, &wheel_y);
#endif
				video_translate(wheel_x, wheel_y, &kk.fx, &kk.fy);
				kk.mouse = (event.wheel.y > 0) ? MOUSE_SCROLL_UP : MOUSE_SCROLL_DOWN;
			case SDL_MOUSEMOTION:
			case SDL_MOUSEBUTTONDOWN:
			case SDL_MOUSEBUTTONUP:
				if (kk.state == KEY_PRESS) {
					modkey = SDL_GetModState();
					os_get_modkey(&modkey);
				}

				kk.sym = 0;
				kk.mod = 0;

				if (event.type != SDL_MOUSEWHEEL)
					video_translate(event.button.x, event.button.y, &kk.fx, &kk.fy);

				/* character resolution */
				kk.x = kk.fx / kk.rx;
				/* half-character selection */
				if ((kk.fx / (kk.rx/2)) % 2 == 0) {
					kk.hx = 0;
				} else {
					kk.hx = 1;
				}
				kk.y = kk.fy / kk.ry;
				if (event.type == SDL_MOUSEWHEEL) {
					handle_key(&kk);
					break; /* nothing else to do here */
				}
				if (event.type == SDL_MOUSEBUTTONDOWN) {
					kk.sx = kk.x;
					kk.sy = kk.y;
				}
				if (startdown) startdown = 0;

				switch (event.button.button) {
				case SDL_BUTTON_RIGHT:
				case SDL_BUTTON_MIDDLE:
				case SDL_BUTTON_LEFT:
					if ((modkey & KMOD_CTRL)
					|| event.button.button == SDL_BUTTON_RIGHT) {
						kk.mouse_button = MOUSE_BUTTON_RIGHT;
					} else if ((modkey & (KMOD_ALT|KMOD_GUI))
					|| event.button.button == SDL_BUTTON_MIDDLE) {
						kk.mouse_button = MOUSE_BUTTON_MIDDLE;
					} else {
						kk.mouse_button = MOUSE_BUTTON_LEFT;
					}
					if (kk.state == KEY_RELEASE) {
						ticker = SDL_GetTicks();
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
					if (change_focus_to_xy(kk.x, kk.y)) {
						kk.on_target = 1;
					} else {
						kk.on_target = 0;
					}
					if (event.type == SDL_MOUSEBUTTONUP && downtrip) {
						downtrip = 0;
						break;
					}
					handle_key(&kk);
					break;
				};
			/* Our own custom events.
			 *
			 * XXX:
			 * SDL docs say that we have to register these events.
			 *
			 * We can probably get away with doing this though, since
			 * we don't have any dependencies that use SDL anyway. */
			case SCHISM_EVENT_MIDI:
				midi_engine_handle_event(&event);
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
				_do_clipboard_paste_op(&event);
				free(event.user.data1);
				break;
			case SCHISM_EVENT_NATIVE:
				/* used by native system scripting */
				switch (event.user.code) {
				case SCHISM_EVENT_NATIVE_OPEN: /* open song */
					song_load(event.user.data1);
					break;
				case SCHISM_EVENT_NATIVE_SCRIPT:
					/* destroy any active dialog before changing pages */
					dialog_destroy();
					if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "new", CHARSET_UTF8) == 0) {
						new_song_dialog();
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "save", CHARSET_UTF8) == 0) {
						save_song_or_save_as();
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "save_as", CHARSET_UTF8) == 0) {
						set_page(PAGE_SAVE_MODULE);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "export_song", CHARSET_UTF8) == 0) {
						set_page(PAGE_EXPORT_MODULE);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "logviewer", CHARSET_UTF8) == 0) {
						set_page(PAGE_LOG);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "font_editor", CHARSET_UTF8) == 0) {
						set_page(PAGE_FONT_EDIT);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "load", CHARSET_UTF8) == 0) {
						set_page(PAGE_LOAD_MODULE);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "help", CHARSET_UTF8) == 0) {
						set_page(PAGE_HELP);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "pattern", CHARSET_UTF8) == 0) {
						set_page(PAGE_PATTERN_EDITOR);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "orders", CHARSET_UTF8) == 0) {
						set_page(PAGE_ORDERLIST_PANNING);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "variables", CHARSET_UTF8) == 0) {
						set_page(PAGE_SONG_VARIABLES);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "message_edit", CHARSET_UTF8) == 0) {
						set_page(PAGE_MESSAGE);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "info", CHARSET_UTF8) == 0) {
						set_page(PAGE_INFO);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "play", CHARSET_UTF8) == 0) {
						song_start();
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "play_pattern", CHARSET_UTF8) == 0) {
						song_loop_pattern(get_current_pattern(), 0);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "play_order", CHARSET_UTF8) == 0) {
						song_start_at_order(get_current_order(), 0);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "play_mark", CHARSET_UTF8) == 0) {
						play_song_from_mark();
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "stop", CHARSET_UTF8) == 0) {
						song_stop();
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "calc_length", CHARSET_UTF8) == 0) {
						show_song_length();
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "sample_page", CHARSET_UTF8) == 0) {
						set_page(PAGE_SAMPLE_LIST);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "sample_library", CHARSET_UTF8) == 0) {
						set_page(PAGE_LIBRARY_SAMPLE);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "init_sound", CHARSET_UTF8) == 0) {
						/* does nothing :) */
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "inst_page", CHARSET_UTF8) == 0) {
						set_page(PAGE_INSTRUMENT_LIST);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "inst_library", CHARSET_UTF8) == 0) {
						set_page(PAGE_LIBRARY_INSTRUMENT);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "preferences", CHARSET_UTF8) == 0) {
						set_page(PAGE_PREFERENCES);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "system_config", CHARSET_UTF8) == 0) {
						set_page(PAGE_CONFIG);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "midi_config", CHARSET_UTF8) == 0) {
						set_page(PAGE_MIDI);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "palette_page", CHARSET_UTF8) == 0) {
						set_page(PAGE_PALETTE_EDITOR);
					} else if (charset_strcasecmp(event.user.data1, CHARSET_UTF8, "fullscreen", CHARSET_UTF8) == 0) {
						toggle_display_fullscreen();
					}
					break;
				}
				break;
			default:
				/* SDL provides many other events that we quite frankly
				 * don't need (or want) to care about. */
				break;
			}
		}

		/* when using a real keyboard SDL will send a keydown first
		 * and text input after it; if a text input event is NOT sent,
		 * we should just send the keydown as is */
		pop_pending_keydown_event(NULL);

		/* handle key repeats */
		handle_key_repeat();

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
#ifdef os_screensaver_deactivate
			if ((status.now-last_ss) > 14) {
				last_ss=status.now;
				os_screensaver_deactivate();
			}
#endif
			break;
		default:
			break;
		};

		if (status.flags & DISKWRITER_ACTIVE) {
			int q = disko_sync();
			while (q == DW_SYNC_MORE && !SDL_PollEvent(NULL)) {
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
		while (!(status.flags & NEED_UPDATE) && dmoz_worker() && !SDL_PollEvent(NULL));

		/* delay until there's an event OR 10 ms have passed */
		int t;
		for (t = 0; t < 10 && !SDL_PollEvent(NULL); t++)
			SDL_Delay(1);
	}
	schism_exit(0);
}


void schism_exit(int status)
{
#if ENABLE_HOOKS
	if (shutdown_process & EXIT_HOOK)
		run_exit_hook();
#endif

	if (shutdown_process & EXIT_SAVECFG)
		cfg_atexit_save();

	if (shutdown_process & EXIT_SDLQUIT) {
		song_lock_audio();
		song_stop_unlocked(1);
		song_unlock_audio();

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
		SDL_Quit();
	}
	os_sysexit();
	exit(status);
}

extern void vis_init(void);

/* wart */
#ifdef SCHISM_MACOSX
int SDL_main(int argc, char** argv)
#else
int main(int argc, char **argv)
#endif
{
	os_sysinit(&argc, &argv);

	vis_init();

	video_fullscreen(0);

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
	log_nl();

	song_initialise();
	cfg_load();

	if (did_classic) {
		status.flags &= ~CLASSIC_MODE;
		if (startup_flags & SF_CLASSIC) status.flags |= CLASSIC_MODE;
	}

	if (!did_fullscreen) {
		video_fullscreen(cfg_video_fullscreen);
	}

	if (!(startup_flags & SF_NETWORK)) {
		status.flags |= NO_NETWORK;
	}

	shutdown_process |= EXIT_SAVECFG;

	sdl_init();
	/* make SDL_SetWindowGrab grab the keyboard too */
	SDL_SetHint(SDL_HINT_GRAB_KEYBOARD, "1");
	shutdown_process |= EXIT_SDLQUIT;
	os_sdlinit();

	display_init();
	palette_apply();
	font_init();
	midi_engine_start();
	log_nl();
	audio_init(audio_driver);
	song_init_modplug();

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
		initial_dir = get_parent_directory(initial_song);
		if (!initial_dir) {
			initial_dir = get_current_directory();
		}
	}
	if (initial_dir) {
		strncpy(cfg_dir_modules, initial_dir, PATH_MAX);
		cfg_dir_modules[PATH_MAX] = 0;
		strncpy(cfg_dir_samples, initial_dir, PATH_MAX);
		cfg_dir_samples[PATH_MAX] = 0;
		strncpy(cfg_dir_instruments, initial_dir, PATH_MAX);
		cfg_dir_instruments[PATH_MAX] = 0;
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
