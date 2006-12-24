/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
 * URL: http://rigelseven.com/schism/
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
#include "diskwriter.h"

#include "song.h"
#include "mixer.h"
#include "midi.h"
#include "dmoz.h"
#include "frag-opt.h"

#include <errno.h>

#if HAVE_SYS_KD_H
# include <sys/kd.h>
#endif
#if HAVE_LINUX_FB_H
# include <linux/fb.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#include "sdlmain.h"

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#ifdef USE_DLTRICK_ALSA
#include <dlfcn.h>
void *_dltrick_handle = 0;
static void *_alsaless_sdl_hack = 0;
#endif

/* FIXME: don't declare functions here -- this stuff goes in .h files :P */
#if defined(WIN32)
extern void win32_get_modkey(int*);
#endif
#if defined(MACOSX)
int macosx_ibook_fnswitch(int setting);
#endif
#if defined(USE_X11)
void xscreensaver_deactivate(void);
#endif

/* wee... */
#if defined(USE_X11) || defined(WIN32) || defined(MACOSX)
unsigned key_repeat_rate(void);
unsigned key_repeat_delay(void);
#endif

#if !defined(WIN32) && !defined(__amigaos4__)
# define ENABLE_HOOKS 1
#endif

#define NATIVE_SCREEN_WIDTH	640
#define NATIVE_SCREEN_HEIGHT	400

/* --------------------------------------------------------------------- */
/* globals */

enum {
	EXIT_HOOK = 1,
	EXIT_CONSOLEFONT = 2,
	EXIT_SAVECFG = 4,
	EXIT_SDLQUIT = 16,
};
static int shutdown_process = 0;

static const char *video_driver = 0;
static const char *audio_driver = 0;
static int did_fullscreen = 0;
static int did_classic = 0;

/* ugly hack... */
void (*shift_release)(void) = NULL;

/* --------------------------------------------------------------------- */
/* stuff SDL should already be doing but isn't */

#if HAVE_SYS_KD_H
static byte console_font[512 * 32];
static int font_saved = 0;
static void save_font(void)
{
	int t = open("/dev/tty", O_RDONLY);
	if (t < 0)
		return;
	if (ioctl(t, GIO_FONT, &console_font) >= 0)
		font_saved = 1;
	close(t);
}
static void restore_font(void)
{
	int t;
	
	if (!font_saved)
		return;
	t = open("/dev/tty", O_RDONLY);
	if (t < 0)
		return;
	if (ioctl(t, PIO_FONT, &console_font) < 0)
		perror("set font");
	close(t);
}
#else
static void save_font(void)
{
}
static void restore_font(void)
{
}
#endif

/* --------------------------------------------------------------------- */

static void display_print_info(void)
{
	log_append(2, 0, "Video initialised");
	log_append(2, 0, "\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81");
	video_report();
	log_append(0, 0, "");
}

/* If we're not not debugging, don't not dump core. (Have I ever mentioned
 * that NDEBUG is poorly named -- or that identifiers for settings in the
 * negative form are a bad idea?) */
#if defined(NDEBUG)
# define SDL_INIT_FLAGS SDL_INIT_TIMER | SDL_INIT_VIDEO
#else
# define SDL_INIT_FLAGS SDL_INIT_TIMER | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE
#endif

static void display_init(void)
{
	if (SDL_Init(SDL_INIT_FLAGS) < 0) {
		fprintf(stderr, "init test %s\n", SDL_GetError());
		exit(1);
	}
	shutdown_process |= EXIT_SDLQUIT;

	video_init(video_driver);

	if (SDL_GetVideoInfo()->wm_available) {
		status.flags |= WM_AVAILABLE;
	}

	clippy_init();

	display_print_info();
#if 0
	SDL_EnableKeyRepeat(125, 25);
#endif
#if defined(USE_X11) || defined(WIN32) || defined(MACOSX)
	SDL_EnableKeyRepeat(key_repeat_delay(), key_repeat_rate());
#else
	SDL_EnableKeyRepeat(SDL_DEFAULT_REPEAT_DELAY,
			SDL_DEFAULT_REPEAT_INTERVAL);
#endif

	//SDL_WM_SetCaption(schism_banner(), "Schism Tracker");
	SDL_EnableUNICODE(1);
}

static void check_update(void);

void toggle_display_fullscreen(void)
{
	video_fullscreen(-1);
	status.flags |= (NEED_UPDATE);
}

/* --------------------------------------------------------------------- */

static void handle_active_event(SDL_ActiveEvent * a)
{
	if (a->state & SDL_APPACTIVE) {
		if (a->gain) {
			status.flags |= (IS_VISIBLE|SOFTWARE_MOUSE_MOVED);
			video_mousecursor(-2);
		} else {
			status.flags &= ~IS_VISIBLE;
		}
	}

	if (a->state & SDL_APPINPUTFOCUS) {
		if (a->gain) {
			status.flags |= IS_FOCUSED;
			video_mousecursor(-2);
		} else {
			status.flags &= ~IS_FOCUSED;
			SDL_ShowCursor(SDL_ENABLE);
		}
	}
}

/* --------------------------------------------------------------------- */

#if ENABLE_HOOKS
static void run_startup_hook(void)
{
	char *ptr = dmoz_path_concat(cfg_dir_dotschism, "startup-hook");
	if (access(ptr, X_OK) == 0)
		system(ptr);
	free(ptr);
}
static void run_diskwriter_complete_hook(void)
{
	char *ptr = dmoz_path_concat(cfg_dir_dotschism, "diskwriter-hook");
	if (access(ptr, X_OK) == 0)
		system(ptr);
	free(ptr);
}

static void run_exit_hook(void)
{
	char *ptr = dmoz_path_concat(cfg_dir_dotschism, "exit-hook");
	if (access(ptr, X_OK) == 0)
		system(ptr);
	free(ptr);
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

/* startup flags */
enum {
	SF_PLAY = 1, /* -p: start playing after loading initial_song */
	SF_HOOKS = 2, /* --no-hooks: don't run startup/exit scripts */
	SF_FONTEDIT = 4,
	SF_CLASSIC = 8,
};
static int startup_flags = SF_HOOKS;

/* frag_option ids */
enum {
	O_ARG,
	O_SDL_AUDIODRIVER,
	O_SDL_VIDEODRIVER,
#ifdef USE_X11
	O_DISPLAY,
#endif
	O_CLASSIC_MODE,
	O_FULLSCREEN,
	O_FONTEDIT,
	O_PLAY,
#if ENABLE_HOOKS
	O_HOOKS,
#endif
	O_VERSION,
	O_HELP,
};

static void parse_options(int argc, char **argv)
{
	FRAG *frag;
	frag_option opts[] = {
		{O_ARG, FRAG_PROGRAM, "[DIRECTORY] [FILE]", NULL},
		{O_SDL_AUDIODRIVER, 'a', "audio-driver", FRAG_ARG, "DRIVER", "SDL audio driver (or \"none\")"},
		{O_SDL_VIDEODRIVER, 'v', "video-driver", FRAG_ARG, "DRIVER", "SDL video driver"},
		{O_CLASSIC_MODE, 0, "classic", FRAG_NEG, NULL, "start schism in classic mode" },
#ifdef USE_X11
		{O_DISPLAY, 0, "display", FRAG_ARG, "DISPLAYNAME", "X11 display to use (e.g. \":0.0\")"},
#endif
		{O_FULLSCREEN, 'f', "fullscreen", FRAG_NEG, NULL, "start in fullscreen mode"},
		{O_PLAY, 'p', "play", FRAG_NEG, NULL, "start playing after loading song on command line"},
		{O_FONTEDIT, 0, "font-editor", FRAG_NEG, NULL, "start in font-editor (itf)"},
#if ENABLE_HOOKS
		{O_HOOKS, 0, "hooks", FRAG_NEG, NULL, "run startup/exit hooks (default: enabled)"},
#endif
		{O_VERSION, 0, "version", 0, NULL, "display version information"},
		{O_HELP, 'h', "help", 0, NULL, "print this stuff"},
		{FRAG_END_ARRAY}
	};
	
	frag = frag_init(opts, argc, argv, FRAG_ENABLE_NO_SPACE_SHORT | FRAG_ENABLE_SPACED_LONG);
	if (!frag) {
		fprintf(stderr, "Error during frag_init (no memory?)\n");
		exit(1);
	}
	
	while (frag_parse(frag)) {
		switch (frag->id) {
		case O_ARG:
			printf("[%s]\n", frag->arg);
			if (is_directory(frag->arg)) {
				initial_dir = dmoz_path_normal(frag->arg);
				if (!initial_dir)
					perror(frag->arg);
			} else {
				initial_song = dmoz_path_normal(frag->arg);
				if (!initial_song)
					perror(frag->arg);
			}
			break;
		case O_SDL_AUDIODRIVER:
			audio_driver = strdup(frag->arg);
			break;
		case O_SDL_VIDEODRIVER:
			video_driver = strdup(frag->arg);
			break;
		case O_CLASSIC_MODE:
			if (frag->type)
				startup_flags |= SF_CLASSIC;
			else
				startup_flags &= ~SF_CLASSIC;
			did_classic = 1;
			break;
#ifdef USE_X11
		case O_DISPLAY:
			put_env_var("DISPLAY", frag->arg);
			break;
#endif
		case O_FULLSCREEN:
			did_fullscreen = 1;
			video_fullscreen(!!frag->type);
			break;
		case O_PLAY:
			if (frag->type)
				startup_flags |= SF_PLAY;
			else
				startup_flags &= ~SF_PLAY;
			break;
		case O_FONTEDIT:
			if (frag->type)
				startup_flags |= SF_FONTEDIT;
			else
				startup_flags &= ~SF_FONTEDIT;
			break;
#if ENABLE_HOOKS
		case O_HOOKS:
			if (frag->type)
				startup_flags |= SF_HOOKS;
			else
				startup_flags &= ~SF_HOOKS;
			break;
#endif
		case O_VERSION:
#ifndef RELEASE_VERSION
			puts("Schism Tracker CVS");
			//puts("This is a CVS build of Schism Tracker. This should be a warning.\n");
#else
			puts("Schism Tracker v" VERSION "\n");
#endif
			printf("Copyright (c) 2003-2007 Storlek and Mrs. Brisby\n"
			       "This program is free software; you can redistribute it and/or modify\n"
			       "it under the terms of the GNU General Public License as published by\n"
			       "the Free Software Foundation; either version 2 of the License, or\n"
			       "(at your option) any later version.\n");
			frag_free(frag);
			exit(0);
		case O_HELP:
			frag_usage(frag);
			frag_free(frag);
			exit(0);
		default:
			frag_usage(frag);
			frag_free(frag);
			exit(2);
		}
	}
	frag_free(frag);
}

/* --------------------------------------------------------------------- */

static void check_update(void)
{
	static unsigned long next = 0;

	/* is there any reason why we'd want to redraw
	   the screen when it's not even visible? */
	if (status.flags & (NEED_UPDATE | IS_VISIBLE)
	    == (NEED_UPDATE | IS_VISIBLE)) {
		status.flags &= ~(NEED_UPDATE | SOFTWARE_MOUSE_MOVED);
		if (!(status.flags & IS_FOCUSED)) {
			if (SDL_GetTicks() < next)
				return;
			next = SDL_GetTicks() + 300;
		}
		redraw_screen();
		video_refresh();
		video_blit();
	} else if (status.flags & SOFTWARE_MOUSE_MOVED) {
		video_blit();
		status.flags &= ~(SOFTWARE_MOUSE_MOVED);
	}
}

static void _synthetic_paste(const char *cbptr)
{
	struct key_event kk;
	int isy = 2;
	kk.mouse = 0;
	for (; cbptr && *cbptr; cbptr++) {
		/* Win32 will have \r\n, everyone else \n */
		if (*cbptr == '\r') continue;
		/* simulate paste */
		kk.sym = kk.orig_sym = 0;
		if (*cbptr == '\n') {
			/* special attention to newlines */
			kk.unicode = '\r';
			kk.sym = SDLK_RETURN;
		} else {
			kk.unicode = *cbptr;
		}
		kk.mod = 0;
		kk.is_repeat = 0;
		if (cbptr[1])
			kk.is_synthetic = isy;
		else
			kk.is_synthetic = 3;
		kk.state = 0;
		handle_key(&kk);
		kk.state = 1;
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
	_synthetic_paste((const char *)e->user.data1);
}


static void event_loop(void) NORETURN;
static void event_loop(void)
{
	SDL_Event event;
	struct key_event kk;
	Uint32 last_mouse_down, ticker;
	SDLKey last_key = 0;
	int modkey;
	time_t startdown;
        struct tm *tmr;
#ifdef USE_X11
	time_t last_ss;
#endif
	int downtrip;
	int sawrep;
	char *debug_s;
	int q;

	debug_s = getenv("SCHISM_DEBUG");

	downtrip = 0;
	last_mouse_down = 0;
	startdown = 0;
	status.last_keysym = 0;
	kk.midi_volume = -1;
	kk.is_synthetic = 0;
	kk.midi_bend = 0;
	kk.midi_note = -1;

	/* X/Y resolution */
	kk.rx = NATIVE_SCREEN_WIDTH / 80;
	kk.ry = NATIVE_SCREEN_HEIGHT / 50;

	modkey = SDL_GetModState();
#if defined(WIN32)
	win32_get_modkey(&modkey);
#endif
	SDL_SetModState(modkey);

#ifdef USE_X11
	time(&last_ss);
#endif
	time(&status.now);
	tmr = localtime(&status.now);
	status.h = tmr->tm_hour;
	status.m = tmr->tm_min;
	status.s = tmr->tm_sec;
	while (SDL_WaitEvent(&event)) {
		sawrep = 0;
		if (event.type == SDL_KEYDOWN || event.type == SDL_MOUSEBUTTONDOWN) {
			kk.state = 0;
		} else if (event.type == SDL_KEYUP || event.type == SDL_MOUSEBUTTONUP) {
			kk.state = 1;
		}
		switch (event.type) {
		case SDL_SYSWMEVENT:
			/* todo... */
			break;
		case SDL_VIDEORESIZE:
			video_resize(event.resize.w, event.resize.h);
			/* fall through */
		case SDL_VIDEOEXPOSE:
			status.flags |= (NEED_UPDATE);
			break;
		case SDL_KEYUP:
			if (event.key.keysym.sym == SDLK_LSHIFT
			|| event.key.keysym.sym == SDLK_RSHIFT) {
				if (shift_release) shift_release();
			}
#define _ALTTRACKED_KMOD	(KMOD_NUM|KMOD_CAPS)
		case SDL_KEYDOWN:
			switch (event.key.keysym.sym) {
			case SDLK_NUMLOCK:
				modkey ^= KMOD_NUM;
				break;
			case SDLK_CAPSLOCK:
				modkey ^= KMOD_CAPS;
				break;
			default:
				break;
			};
			if (!kk.state) {
				modkey = (event.key.keysym.mod
					& ~(_ALTTRACKED_KMOD))
					| (modkey & _ALTTRACKED_KMOD);
			}
#if defined(WIN32)
			win32_get_modkey(&modkey);
#endif
			kk.sym = event.key.keysym.sym;
			kk.mod = modkey;
			kk.unicode = event.key.keysym.unicode;
			kk.mouse = 0;
			if (debug_s && strstr(debug_s, "key")) {
				log_appendf(12, "[DEBUG] Key%s sym=%d",
						(event.type == SDL_KEYDOWN) ? "Down" : "Up",
						event.key.keysym.sym);
			}
			key_translate(&kk);
			if (event.type == SDL_KEYDOWN
			&& last_key == kk.sym) {
				sawrep = kk.is_repeat = 1;
			} else {
				kk.is_repeat = 0;
			}
			handle_key(&kk);
			if (event.type == SDL_KEYUP) {
				status.last_keysym = kk.sym;
				last_key = 0;
			} else {
				status.last_keysym = 0;
				last_key = kk.sym;
			}
			break;
		case SDL_QUIT:
			show_exit_prompt();
			break;
		case SDL_ACTIVEEVENT:
			/* reset this... */
			modkey = SDL_GetModState();
#if defined(WIN32)
			win32_get_modkey(&modkey);
#endif
			SDL_SetModState(modkey);

			handle_active_event(&(event.active));
			break;
		case SDL_MOUSEMOTION:
		case SDL_MOUSEBUTTONDOWN:
		case SDL_MOUSEBUTTONUP:
			if (!kk.state) {
				modkey = event.key.keysym.mod;
#if defined(WIN32)
				win32_get_modkey(&modkey);
#endif
			}

			kk.sym = 0;
			kk.mod = 0;
			kk.unicode = 0;

			video_translate(
				event.button.x,
				event.button.y,
				&kk.fx, &kk.fy);

			/* character resolution */
			kk.x = kk.fx / kk.rx;
			/* half-character selection */
			if ((kk.fx / (kk.rx/2)) % 2 == 0) {
				kk.hx = 0;
			} else {
				kk.hx = 1;
			}
			kk.y = kk.fy / kk.ry;
			if (event.type == SDL_MOUSEBUTTONDOWN) {
				kk.sx = kk.x;
				kk.sy = kk.y;
			}
			if (startdown) startdown = 0;

			switch (event.button.button) {
#ifdef SDL_BUTTON_WHEELUP
			case SDL_BUTTON_WHEELUP:
				kk.mouse = MOUSE_SCROLL_UP;
				handle_key(&kk);
				break;
#endif

#ifdef SDL_BUTTON_WHEELDOWN
			case SDL_BUTTON_WHEELDOWN:
				kk.mouse = MOUSE_SCROLL_DOWN;
				handle_key(&kk);
				break;
#endif

			case SDL_BUTTON_RIGHT:
			case SDL_BUTTON_MIDDLE:
			case SDL_BUTTON_LEFT:
				if ((modkey & KMOD_CTRL)
				|| event.button.button == SDL_BUTTON_RIGHT) {
					kk.mouse_button = MOUSE_BUTTON_RIGHT;
				} else if ((modkey & (KMOD_ALT|KMOD_META))
				|| event.button.button == SDL_BUTTON_MIDDLE) {
					kk.mouse_button = MOUSE_BUTTON_MIDDLE;
				} else {
					kk.mouse_button = MOUSE_BUTTON_LEFT;
				}
				if (kk.state) {
					ticker = SDL_GetTicks();
					if (kk.sx == kk.x
					&& kk.sy == kk.y
					&& (ticker - last_mouse_down) < 300) {
						last_mouse_down = 0;
						kk.mouse = MOUSE_DBLCLICK;
					} else {
						last_mouse_down = ticker;
						kk.mouse = MOUSE_CLICK;
					}
				} else {
					kk.mouse = MOUSE_CLICK;
				}
				if (!(status.dialog_type & DIALOG_MENU)) {
					if (kk.y <= 9 && status.current_page != PAGE_FONT_EDIT) {
						if (kk.state
						&& kk.mouse_button == MOUSE_BUTTON_RIGHT) {
							menu_show();
							break;
						} else if (!kk.state
						&& kk.mouse_button == MOUSE_BUTTON_LEFT) {
							time(&startdown);
						}
					}

					if (change_focus_to_xy(kk.x, kk.y)) {
						kk.on_target = 1;
					} else {
						kk.on_target = 0;
					}
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
				midi_engine_handle_event((void*)&event);
			} else if (event.type == SCHISM_EVENT_PLAYBACK) {
				/* this is the sound thread */
				midi_send_flush();
				playback_update();

			} else if (event.type == SCHISM_EVENT_PASTE) {
				/* handle clipboard events */
				_do_clipboard_paste_op(&event);
				free(event.user.data1);

			} else if (event.type == SCHISM_EVENT_NATIVE) {
				/* used by native system scripting */
				switch (event.user.code) {
				case SCHISM_EVENT_NATIVE_OPEN: /* open song */
					if (song_load(event.user.data1)) {
						set_page(PAGE_BLANK);
					}
					break;
				case SCHISM_EVENT_NATIVE_SCRIPT:
					/* TODO: hash the string's value and do a switch() on it */
					if (strcasecmp(event.user.data1, "new") == 0) {
						new_song_dialog();
					} else if (strcasecmp(event.user.data1, "save") == 0) {
						save_song_or_save_as();
					} else if (strcasecmp(event.user.data1, "save_as") == 0) {
						set_page(PAGE_SAVE_MODULE);
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
		if (!SDL_PollEvent(0) || sawrep) {
			time(&status.now);
			tmr = localtime(&status.now);
			status.h = tmr->tm_hour;
			status.m = tmr->tm_min;
			status.s = tmr->tm_sec;

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

#ifdef USE_X11
			switch (song_get_mode()) {
			case MODE_PLAYING:
			case MODE_PATTERN_LOOP:
				if ((status.now-last_ss) > 14) {
					last_ss=status.now;
					xscreensaver_deactivate();
				}
				break;
			default:
				break;
			}
#endif

			while ((q=diskwriter_sync()) == 1 && !SDL_PollEvent(0))
				check_update();

			if (q == -1) {
				log_appendf(4, "Error running diskwriter: %s", strerror(errno));
				(void)diskwriter_finish();
			} else if (q == 0) {
				switch (diskwriter_finish()) {
				case -1: /* wasn't running */	
					break;
				case 0:
					log_appendf(4, "Error shutting down diskwriter: %s", strerror(errno));
					break;
				case 1:
					log_appendf(2, "Diskwriter completed successfully");
#ifdef ENABLE_HOOKS
					run_diskwriter_complete_hook();
#endif
	
				};
			}

			/* let dmoz build directory lists, etc
			as long as there's no user-event going on...
			*/
			while (dmoz_worker() && !SDL_PollEvent(0)) {
			}
		}
	}
	exit(0); /* atexit :) */
}


static void schism_shutdown(void)
{
#if ENABLE_HOOKS
	if (shutdown_process & EXIT_HOOK)
		run_exit_hook();
#endif
	if (shutdown_process & EXIT_CONSOLEFONT)
		restore_font();
	if (shutdown_process & EXIT_SAVECFG)
		cfg_atexit_save();
#ifdef MACOSX
	if (ibook_helper != -1)
		macosx_ibook_fnswitch(ibook_helper);
#endif
	if (shutdown_process & EXIT_SDLQUIT) {
		video_shutdown();
		/*
		If this is the atexit() handler, why are we calling SDL_Quit?

		Simple, SDL's documentation says always call SDL_Quit. :) In
		fact, in the examples they recommend writing atexit(SDL_Quit)
		directly after SDL_Init. I'm not sure exactly why, but I don't
		see a reason *not* to do it...
			/ Storlek
		*/
		SDL_Quit();
	}
}

static void dump_misc_about_text(void)
{
	const char *text[] = {
		"Schism Tracker is Copyright (C) 2003-2007 Storlek and Mrs. Brisby.",
		"",
		"Contains additional code by Olivier Lapicque, Markus Fick, Adam Goode,",
		"Ville Joleka, Juan Linietsky, Juha Niemimaki, and others.",
		"Based on Impulse Tracker which is Copyright (C) 1995-1998 Jeffrey Lim.",
		"",
		"This program is free software; you can redistribute it and/or modify it",
		"under the terms of the GNU General Public License as published by the",
		"Free Software Foundation; either version 2 of the License, or (at your",
		"option) any later version.",
		"",
		"You should have received a copy of the GNU General Public License along",
		"with this program; if not, write to the Free Software Foundation, Inc.,",
		"59 Temple Place, Suite 330, Boston, MA 02111-1307  USA",
		"",
		NULL
	};
	int n;
	
	log_append(3, 0, schism_banner());
	log_nl();

	for (n = 0; text[n]; n++)
		log_append(6, 0, text[n]);
	log_nl();
}

int main(int argc, char **argv) NORETURN;
int main(int argc, char **argv)
{
	atexit(schism_shutdown);

	kbd_init();
	video_fullscreen(0);

	srand(time(0));
	parse_options(argc, argv); /* shouldn't this be like, first? */

#ifdef USE_DLTRICK_ALSA
	_dltrick_handle = dlopen("libasound.so.2", RTLD_NOW);
	if (!_dltrick_handle)
		_dltrick_handle = dlopen("libasound.so", RTLD_NOW);
	if (!getenv("SDL_AUDIODRIVER")) {
		_alsaless_sdl_hack = dlopen("libSDL-1.2.so.0", RTLD_NOW);
		if (!_alsaless_sdl_hack)
			_alsaless_sdl_hack = RTLD_DEFAULT;

		if (_dltrick_handle && _alsaless_sdl_hack
		&& dlsym(_alsaless_sdl_hack, "ALSA_bootstrap")) {
			if (dlsym(_dltrick_handle,"snd_ctl_open")
			|| dlsym(_dltrick_handle,"snd_pcm_open")) {
				if (!audio_driver) audio_driver = "alsa";
			}
		}
	}
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

	save_font();
	shutdown_process |= EXIT_CONSOLEFONT;

	dump_misc_about_text();

	song_initialise();
	cfg_load();

	if (did_classic) {
		status.flags &= ~CLASSIC_MODE;
		if (startup_flags & SF_CLASSIC) status.flags |= CLASSIC_MODE;
	}

	if (!video_driver) {
		video_driver = cfg_video_driver;
		if (!video_driver || !*video_driver) video_driver = 0;
	}
	if (!did_fullscreen) {
		video_fullscreen(cfg_video_fullscreen);
	}

	shutdown_process |= EXIT_SAVECFG;
	display_init();
	palette_apply();
	font_init();
	midi_engine_start();
	song_init_audio(audio_driver);
	song_init_modplug();

	mixer_setup();

	setup_help_text_pointers();
	load_pages();
	main_song_changed_cb();
	
	/* shutdown_process |= 8; -- wtf is this? */

	if (initial_song && !initial_dir)
		initial_dir = get_parent_directory(initial_song);
	if (initial_dir) {
		strncpy(cfg_dir_modules, initial_dir, PATH_MAX);
		cfg_dir_modules[PATH_MAX] = 0;
		free(initial_dir);
	}

	if (startup_flags & SF_FONTEDIT) {
		status.flags |= STARTUP_FONTEDIT;
		set_page(PAGE_FONT_EDIT);
	} else if (initial_song) {
		if (song_load_unchecked(initial_song)) {
			if (startup_flags & SF_PLAY) {
				song_start();
				set_page(PAGE_INFO);
			} else {
				/* set_page(PAGE_LOG); */
				set_page(PAGE_BLANK);
			}
		} else {
			set_page(PAGE_LOG);
		}
		free(initial_song);
	} else {
		set_page(PAGE_ABOUT);
		show_about();
	}

#if !defined(WIN32)
	nice(1);
#endif
	/* poll once */
	midi_engine_poll_ports();
	
	event_loop(); /* never returns */
}
