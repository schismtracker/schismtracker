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

#define NEED_TIME
#include "headers.h"

#include "event.h"

#include "clippy.h"
#include "disko.h"

#include "version.h"
#include "song.h"
#include "midi.h"
#include "dmoz.h"
#include "charset.h"

#ifdef USE_LUA
#include "lua-engine.h"
#endif

#include "osdefs.h"

#include <errno.h>

#include "sdlmain.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef WIN32
# include <signal.h>
#endif

#include <getopt.h>

#if !defined(__amigaos4__) && !defined(GEKKO)
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

	clippy_init();

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
	case SDL_WINDOWEVENT_SIZE_CHANGED:  // tiling window managers
		video_resize(w->data1, w->data2);
		/* fall through */
	case SDL_WINDOWEVENT_EXPOSED:
		status.flags |= (NEED_UPDATE);
		break;
	case SDL_WINDOWEVENT_MOVED:
		video_update();
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
#ifdef MACOSX
char *initial_song = NULL;
#else
static char *initial_song = NULL;
#endif

#ifdef MACOSX
static int ibook_helper = -1;
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
#if USE_OPENGL
	O_VIDEO_GLPATH,
#endif
	O_VIDEO_DEPTH,
#if HAVE_SYS_KD_H
	O_VIDEO_FBDEV,
#endif
#if USE_NETWORK
	O_NETWORK, O_NO_NETWORK,
#endif
#ifdef USE_X11
	O_DISPLAY,
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

#if defined(WIN32)
# define OPENGL_PATH "\\path\\to\\opengl.dll"
#elif defined(MACOSX)
# define OPENGL_PATH "/path/to/opengl.dylib"
#else
# define OPENGL_PATH "/path/to/opengl.so"
#endif

#define USAGE "Usage: %s [OPTIONS] [DIRECTORY] [FILE]\n"

// Remember to update the manpage when changing the command-line options!

static void parse_options(int argc, char **argv)
{
	struct option long_options[] = {
		{"audio-driver", 1, NULL, O_SDL_AUDIODRIVER},
		{"video-driver", 1, NULL, O_SDL_VIDEODRIVER},

		{"video-yuvlayout", 1, NULL, O_VIDEO_YUVLAYOUT},
		{"video-size", 1, NULL, O_VIDEO_RESOLUTION},
		{"video-stretch", 0, NULL, O_VIDEO_STRETCH},
		{"no-video-stretch", 0, NULL, O_NO_VIDEO_STRETCH},
#if USE_OPENGL
		{"video-gl-path", 1, NULL, O_VIDEO_GLPATH},
#endif
		{"video-depth", 1, NULL, O_VIDEO_DEPTH},
#if HAVE_SYS_KD_H
		{"video-fb-device", 1, NULL, O_VIDEO_FBDEV},
#endif
#if USE_NETWORK
		{"network", 0, NULL, O_NETWORK},
		{"no-network", 0, NULL, O_NO_NETWORK},
#endif
		{"classic", 0, NULL, O_CLASSIC_MODE},
		{"no-classic", 0, NULL, O_NO_CLASSIC_MODE},
#ifdef USE_X11
		{"display", 1, NULL, O_DISPLAY},
#endif
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
		{"debug", 1, NULL, O_DEBUG},
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

		// FIXME remove all these env vars, and put these things into a global struct or something instead

		case O_VIDEO_YUVLAYOUT:
			put_env_var("SCHISM_YUVLAYOUT", optarg);
			break;
		case O_VIDEO_RESOLUTION:
			put_env_var("SCHISM_VIDEO_RESOLUTION", optarg);
			break;
		case O_VIDEO_STRETCH:
			put_env_var("SCHISM_VIDEO_ASPECT", "full");
			break;
		case O_NO_VIDEO_STRETCH:
			put_env_var("SCHISM_VIDEO_ASPECT", "fixed");
			break;
#if USE_OPENGL
		case O_VIDEO_GLPATH:
			put_env_var("SDL_VIDEO_GL_DRIVER", optarg);
			break;
#endif
		case O_VIDEO_DEPTH:
			put_env_var("SCHISM_VIDEO_DEPTH", optarg);
			break;
#if HAVE_SYS_KD_H
		case O_VIDEO_FBDEV:
			put_env_var("SDL_FBDEV", optarg);
			break;
#endif
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

		case O_DEBUG:
			put_env_var("SCHISM_DEBUG", optarg);
			break;
#ifdef USE_X11
		case O_DISPLAY:
			put_env_var("DISPLAY", optarg);
			break;
#endif
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
				"      --video-yuvlayout=LAYOUT\n"
				"      --video-size=WIDTHxHEIGHT\n"
				"      --video-stretch (--no-video-stretch)\n"
#if USE_OPENGL
				"      --video-gl-path=/path/to/opengl.so\n"
#endif
				"      --video-depth=DEPTH\n"
#if HAVE_SYS_KD_H
				"      --video-fb-device=/dev/fb0\n"
#endif
#if USE_NETWORK
				"      --network (--no-network)\n"
#endif
				"      --classic (--no-classic)\n"
#ifdef USE_X11
				"      --display=DISPLAYNAME\n"
#endif
				"  -f, --fullscreen (-F, --no-fullscreen)\n"
				"  -p, --play (-P, --no-play)\n"
				"      --diskwrite=FILENAME\n"
				"      --font-editor (--no-font-editor)\n"
#if ENABLE_HOOKS
				"      --hooks (--no-hooks)\n"
#endif
				//"      --debug=OPS\n"
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

	switch (ACTIVE_WIDGET.type) {
		case WIDGET_TEXTENTRY:
		case WIDGET_NUMENTRY:
		case WIDGET_OTHER:
			if (!(status.flags & ACCEPTING_INPUT)) {
				status.flags |= (ACCEPTING_INPUT);
				SDL_StartTextInput();
			}
			break;
		default:
			if (status.flags & ACCEPTING_INPUT) {
				status.flags &= ~(ACCEPTING_INPUT);
				SDL_StopTextInput();
			}
			break;
	}
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

static void _synthetic_paste(const char *cbptr, int is_textinput)
{
	struct key_event kk;
	int isy = 2;
	memset(&kk, 0, sizeof(kk));
	kk.midi_volume = -1;
	kk.midi_note = -1;
	kk.mouse = MOUSE_NONE;
	for (; cbptr && *cbptr; cbptr++) {
		/* Win32 will have \r\n, everyone else \n */
		if (*cbptr == '\r') continue;
		/* simulate paste */
		kk.scancode = -1;
		kk.sym.sym = kk.orig_sym.sym = 0;
		if (*cbptr == '\n') {
			/* special attention to newlines */
			kk.unicode = '\r';
			kk.sym.sym = SDLK_RETURN;
		} else {
			kk.unicode = *cbptr;
		}
		kk.mod = is_textinput ? SDL_GetModState() : 0;
		kk.is_repeat = 0;
		if (cbptr[1])
			kk.is_synthetic = isy;
		else
			kk.is_synthetic = 3;
		kk.is_textinput = is_textinput;
		kk.state = KEY_PRESS;
		handle_key(&kk);
		kk.state = KEY_RELEASE;
		handle_key(&kk);
		isy = 1;
	}
}
static void _do_clipboard_paste_op(SDL_Event *e)
{
	if (ACTIVE_PAGE.clipboard_paste
	&& ACTIVE_PAGE.clipboard_paste(e->user.code,
				e->user.data1)) return;
	if (ACTIVE_WIDGET.clipboard_paste
	&& ACTIVE_WIDGET.clipboard_paste(e->user.code,
				e->user.data1)) return;
	_synthetic_paste((const char *)e->user.data1, 0);
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

static void event_loop(void) NORETURN;
static void event_loop(void)
{
	SDL_Event event;
	unsigned int lx = 0, ly = 0; /* last x and y position (character) */
	uint32_t last_mouse_down, ticker;
	SDL_Keysym last_key = {};
	int modkey;
	time_t startdown;
#ifdef USE_X11
	time_t last_ss;
#endif
	int downtrip;
	int wheel_x;
	int wheel_y;
	int sawrep;
	char *debug_s;
	int fix_numlock_key;
	struct key_event kk;

	fix_numlock_key = status.fix_numlock_setting;

	debug_s = getenv("SCHISM_DEBUG");

	downtrip = 0;
	last_mouse_down = 0;
	startdown = 0;
	status.last_keysym.sym = 0;

	modkey = SDL_GetModState();
#if defined(WIN32)
	win32_get_modkey(&modkey);
#endif
	SDL_SetModState(modkey);

#ifdef USE_X11
	time(&last_ss);
#endif
	time(&status.now);
	localtime_r(&status.now, &status.tmnow);
	while (SDL_WaitEvent(&event)) {
		if (!os_sdlevent(&event))
			continue;

		key_event_reset(&kk, kk.sx, kk.sy);

		sawrep = 0;
		if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
			kk.state = KEY_PRESS;
		} else if (event.type == SDL_KEYUP || event.type == SDL_MOUSEBUTTONUP) {
			kk.state = KEY_RELEASE;
		}
		if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP) {
			if (event.key.keysym.sym == 0) {
				// XXX when does this happen?
				kk.mouse = MOUSE_NONE;
				kk.unicode = 0;
				kk.is_repeat = 0;
			}
		}
		switch (event.type) {
		case SDL_AUDIODEVICEADDED:
		case SDL_SYSWMEVENT:
			/* todo... */
			break;
#if defined(WIN32)
#define _ALTTRACKED_KMOD        (KMOD_NUM|KMOD_CAPS)
#else
#define _ALTTRACKED_KMOD        0
#endif
		case SDL_TEXTINPUT: {
			char* input_text = str_unicode_to_cp437(event.text.text);
			if (input_text != NULL) {
				if (input_text[0] != '\0' && (status.flags & ACCEPTING_INPUT))
					_synthetic_paste(input_text, 1);
				free(input_text);
			}
			break;
		}
		case SDL_KEYUP:
		case SDL_KEYDOWN:
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
#if defined(WIN32)
			win32_get_modkey(&modkey);
#endif
			kk.sym.sym = event.key.keysym.sym;
			kk.scancode = event.key.keysym.scancode;

			switch (fix_numlock_key) {
			case NUMLOCK_GUESS:
#ifdef MACOSX
				// FIXME can this be moved to macosx_sdlevent?
				if (ibook_helper != -1) {
					if (ACTIVE_PAGE.selected_widget > -1
					    && ACTIVE_PAGE.selected_widget < ACTIVE_PAGE.total_widgets
					    && ACTIVE_PAGE_WIDGET.accept_text) {
						/* text is more likely? */
						modkey |= KMOD_NUM;
					} else {
						modkey &= ~KMOD_NUM;
					}
				} /* otherwise honor it */
#endif
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
			if (debug_s && strstr(debug_s, "key")) {
				log_appendf(12, "[DEBUG] Key%s sym=%d scancode=%d",
						(event.type == SDL_KEYDOWN) ? "Down" : "Up",
						(int)event.key.keysym.sym,
						(int)event.key.keysym.scancode);
			}
			key_translate(&kk);
			if (debug_s && strstr(debug_s, "translate")
					&& kk.orig_sym.sym != kk.sym.sym) {
				log_appendf(12, "[DEBUG] Translate Key%s sym=%d scancode=%d -> %d (%c)",
						(event.type == SDL_KEYDOWN) ? "Down" : "Up",
						(int)event.key.keysym.sym,
						(int)event.key.keysym.scancode,
						kk.sym.sym, kk.unicode);
			}
			if (event.type == SDL_KEYDOWN && last_key.sym == kk.sym.sym) {
				sawrep = kk.is_repeat = 1;
			} else {
				kk.is_repeat = 0;
			}
			if (kk.sym.sym == SDLK_RETURN)
				kk.unicode = '\r';
			handle_key(&kk);
			if (event.type == SDL_KEYUP) {
				status.last_keysym.sym = kk.sym.sym;
				last_key.sym = 0;
			} else {
				status.last_keysym.sym = 0;
				last_key.sym = kk.sym.sym;
			}
			break;
		case SDL_QUIT:
			show_exit_prompt();
			break;
		case SDL_WINDOWEVENT:
			/* reset this... */
			modkey = SDL_GetModState();
#if defined(WIN32)
			win32_get_modkey(&modkey);
#endif
			SDL_SetModState(modkey);

			handle_window_event(&event.window);
			break;
		case SDL_MOUSEWHEEL:
			kk.state = -1;  /* neither KEY_PRESS nor KEY_RELEASE */
			SDL_GetMouseState(&wheel_x, &wheel_y);
			video_translate(wheel_x, wheel_y, &kk.fx, &kk.fy);
			kk.mouse = (event.wheel.y > 0) ? MOUSE_SCROLL_UP : MOUSE_SCROLL_DOWN;
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (kk.state == KEY_PRESS) {
				modkey = SDL_GetModState();
#if defined(WIN32)
				win32_get_modkey(&modkey);
#endif
			}

			kk.sym.sym = 0;
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
			if (debug_s && strstr(debug_s, "mouse")) {
				log_appendf(12, "[DEBUG] Mouse%s button=%d x=%d y=%d",
					(event.type == SDL_MOUSEBUTTONDOWN) ? "Down" : "Up",
						(int)event.button.button,
						(int)event.button.x,
						(int)event.button.y);
			}

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
			break;
		default:
			if (event.type == SCHISM_EVENT_MIDI) {
				midi_engine_handle_event(&event);
			} else if (event.type == SCHISM_EVENT_UPDATE_IPMIDI) {
				status.flags |= (NEED_UPDATE);
				midi_engine_poll_ports();
			} else if (event.type == SCHISM_EVENT_PLAYBACK) {
				/* this is the sound thread */
				midi_send_flush();
				if (!(status.flags & (DISKWRITER_ACTIVE|DISKWRITER_ACTIVE_PATTERN))) {
					playback_update();
				}
			} else if (event.type == SCHISM_EVENT_PASTE) {
				/* handle clipboard events */
				_do_clipboard_paste_op(&event);
				free(event.user.data1);
			} else if (event.type == SCHISM_EVENT_NATIVE) {
				/* used by native system scripting */
				switch (event.user.code) {
				case SCHISM_EVENT_NATIVE_OPEN: /* open song */
					song_load(event.user.data1);
					break;
				case SCHISM_EVENT_NATIVE_SCRIPT:
					/* TODO: hash the string's value and do a switch() on it */
					if (strcasecmp(event.user.data1, "new") == 0) {
						new_song_dialog();
					} else if (strcasecmp(event.user.data1, "save") == 0) {
						save_song_or_save_as();
					} else if (strcasecmp(event.user.data1, "save_as") == 0) {
						set_page(PAGE_SAVE_MODULE);
					} else if (strcasecmp(event.user.data1, "export_song") == 0) {
						set_page(PAGE_EXPORT_MODULE);
					} else if (strcasecmp(event.user.data1, "logviewer") == 0) {
						set_page(PAGE_LOG);
					} else if (strcasecmp(event.user.data1, "font_editor") == 0) {
						set_page(PAGE_FONT_EDIT);
					} else if (strcasecmp(event.user.data1, "load") == 0) {
						set_page(PAGE_LOAD_MODULE);
					} else if (strcasecmp(event.user.data1, "help") == 0) {
						set_page(PAGE_HELP);
					} else if (strcasecmp(event.user.data1, "pattern") == 0) {
						set_page(PAGE_PATTERN_EDITOR);
					} else if (strcasecmp(event.user.data1, "orders") == 0) {
						set_page(PAGE_ORDERLIST_PANNING);
					} else if (strcasecmp(event.user.data1, "variables") == 0) {
						set_page(PAGE_SONG_VARIABLES);
					} else if (strcasecmp(event.user.data1, "message_edit") == 0) {
						set_page(PAGE_MESSAGE);
					} else if (strcasecmp(event.user.data1, "info") == 0) {
						set_page(PAGE_INFO);
					} else if (strcasecmp(event.user.data1, "play") == 0) {
						song_start();
					} else if (strcasecmp(event.user.data1, "play_pattern") == 0) {
						song_loop_pattern(get_current_pattern(), 0);
					} else if (strcasecmp(event.user.data1, "play_order") == 0) {
						song_start_at_order(get_current_order(), 0);
					} else if (strcasecmp(event.user.data1, "play_mark") == 0) {
						play_song_from_mark();
					} else if (strcasecmp(event.user.data1, "stop") == 0) {
						song_stop();
					} else if (strcasecmp(event.user.data1, "calc_length") == 0) {
						show_song_length();
					} else if (strcasecmp(event.user.data1, "sample_page") == 0) {
						set_page(PAGE_SAMPLE_LIST);
					} else if (strcasecmp(event.user.data1, "sample_library") == 0) {
						set_page(PAGE_LIBRARY_SAMPLE);
					} else if (strcasecmp(event.user.data1, "init_sound") == 0) {
						/* does nothing :) */
					} else if (strcasecmp(event.user.data1, "inst_page") == 0) {
						set_page(PAGE_INSTRUMENT_LIST);
					} else if (strcasecmp(event.user.data1, "inst_library") == 0) {
						set_page(PAGE_LIBRARY_INSTRUMENT);
					} else if (strcasecmp(event.user.data1, "preferences") == 0) {
						set_page(PAGE_PREFERENCES);
					} else if (strcasecmp(event.user.data1, "system_config") == 0) {
						set_page(PAGE_CONFIG);
					} else if (strcasecmp(event.user.data1, "midi_config") == 0) {
						set_page(PAGE_MIDI);
					} else if (strcasecmp(event.user.data1, "palette_page") == 0) {
						set_page(PAGE_PALETTE_EDITOR);
					} else if (strcasecmp(event.user.data1, "fullscreen") == 0) {
						toggle_display_fullscreen();
					}
				};
			} else {
				printf("received unknown event %x\n", event.type);
			}
			break;
		}
		if (sawrep || !SDL_PollEvent(NULL)) {
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
			as long as there's no user-event going on...
			*/
			while (!(status.flags & NEED_UPDATE) && dmoz_worker() && !SDL_PollEvent(NULL))
				/* nothing */;
		}

		continue_lua_eval();
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

#ifdef MACOSX
	if (ibook_helper != -1)
		macosx_ibook_fnswitch(ibook_helper);
#endif
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

#ifdef MACOSX
int SDL_main(int argc, char** argv)
#else
int main(int argc, char **argv)
#endif
{
	if (! SDL_VERSION_ATLEAST(2,0,5)) {
		SDL_Log("SDL_VERSION %i.%i.%i less than required!", SDL_MAJOR_VERSION, SDL_MINOR_VERSION, SDL_PATCHLEVEL);
		return 1;
	}
	os_sysinit(&argc, &argv);

	/* this needs to be done very early, because the version is used in the help text etc.
	Also, this needs to happen before any locale stuff is initialized
	(but we don't do that at all yet, anyway) */
	ver_init();

	/* FIXME: make a config option for this, and stop abusing frickin' environment variables! */
	put_env_var("SCHISM_VIDEO_ASPECT", "full");

	vis_init();

	video_fullscreen(0);

	tzset(); // localtime_r wants this
	srand(time(NULL));
	parse_options(argc, argv); /* shouldn't this be like, first? */

#ifdef USE_DLTRICK_ALSA
	alsa_dlinit();
#endif

	cfg_init_dir();

#if ENABLE_HOOKS
	if (startup_flags & SF_HOOKS) {
		run_startup_hook();
		shutdown_process |= EXIT_HOOK;
	}
#endif
#ifdef MACOSX
	ibook_helper = macosx_ibook_fnswitch(1);
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
	shutdown_process |= EXIT_SDLQUIT;
	os_sdlinit();

	display_init();
	palette_apply();
	font_init();
	midi_engine_start();
	log_nl();
	audio_init(audio_driver);
	song_init_modplug();

#ifdef USE_LUA
	log_nl();
	lua_init();
#endif

#ifndef WIN32
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

