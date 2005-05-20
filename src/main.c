/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

#include "headers.h"

#include "it.h"
#include "song.h"
#include "page.h"
#include "dmoz.h"
#include "frag-opt.h"

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

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#if !defined(WIN32) && !defined(__amigaos4__)
# define ENABLE_HOOKS 1
#endif

/* --------------------------------------------------------------------- */
/* globals */

SDL_Surface *screen;

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

static inline int get_fb_size(void)
{
	int r = 400;
#if HAVE_LINUX_FB_H
	struct fb_var_screeninfo s;
	int fb;

	if (getenv("DISPLAY") == NULL && (fb = open("/dev/fb0", O_RDONLY)) > -1) {
		if (ioctl(fb, FBIOGET_VSCREENINFO, &s) < 0)
			perror("ioctl FBIOGET_VSCREENINFO");
		else
			r = s.yres;
		close(fb);
	}
#endif
	return r;
}

/* --------------------------------------------------------------------- */

static void display_print_video_info(void)
{
	const SDL_VideoInfo *info = SDL_GetVideoInfo();

	log_append(2, 0, "Video hardware capabilities");
	log_append(2, 0, "\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81"
		   "\x81\x81\x81\x81\x81\x81\x81");
	
	if (info->video_mem)
		log_appendf(5, " %dk video memory", info->video_mem);
	if (info->hw_available) {
		char caps[80] = " Accelerated blit capabilities:";

		log_append(5, 0, " Hardware surfaces supported");

		if (info->blit_hw)
			strcat(caps, " hw");
		if (info->blit_hw_CC)
			strcat(caps, " hw_CC");
		if (info->blit_hw_A)
			strcat(caps, " hw_A");
		if (info->blit_sw)
			strcat(caps, " sw");
		if (info->blit_sw_CC)
			strcat(caps, " sw_CC");
		if (info->blit_sw_A)
			strcat(caps, " sw_A");
		if (info->blit_fill)
			strcat(caps, " fill");
		if (*(strchr(caps, ':') + 1) != 0)
			log_append(5, 1, strdup(caps));
	}

	log_append(5, 0, " Best pixel format:");
	log_appendf(5, "  Bits/pixel = %d", info->vfmt->BitsPerPixel);
	log_appendf(5, "  Bytes/pixel = %d", info->vfmt->BytesPerPixel);

	log_append(0, 0, "");
	log_append(0, 0, "");
}

static void display_print_info(void)
{
	char buf[256];

	log_append(2, 0, "Video initialised");
	log_append(2, 0, "\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81");
	log_appendf(5, " Using driver '%s'", SDL_VideoDriverName(buf, 256));
	log_appendf(5, " %s surface", ((screen->flags & SDL_HWSURFACE) ? "Hardware" : "Software"));
	if (screen->flags & SDL_DOUBLEBUF)
		log_append(5, 0, " Double buffered");
	if (screen->flags & SDL_HWACCEL)
		log_append(5, 0, " Accelerated blits");
	if (screen->flags & SDL_RLEACCEL)
		log_append(5, 0, " RLE accelerated");
	if (SDL_MUSTLOCK(screen))
		log_append(4, 0, " Must lock surface");
	log_append(5, 0, " Pixel format:");
	log_appendf(5, "  Bits/pixel = %d", screen->format->BitsPerPixel);
	log_appendf(5, "  Bytes/pixel = %d", screen->format->BytesPerPixel);

	log_append(0, 0, "");
	log_append(0, 0, "");
}

/* If we're not not debugging, don't not dump core. (Have I ever mentioned
 * that NDEBUG is poorly named -- or that identifiers for settings in the
 * negative form are a bad idea?) */
#if defined(NDEBUG)
# define SDL_INIT_FLAGS SDL_INIT_AUDIO | SDL_INIT_VIDEO
#else
# define SDL_INIT_FLAGS SDL_INIT_AUDIO | SDL_INIT_VIDEO | SDL_INIT_NOPARACHUTE
#endif
static unsigned long sdl_videomode_flags = SDL_HWPALETTE | SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_ANYFORMAT;

static void display_init(void)
{
	if (SDL_Init(SDL_INIT_FLAGS) < 0) {
		fprintf(stderr, "%s\n", SDL_GetError());
		exit(1);
	}
	atexit(SDL_Quit);
	if (SDL_GetVideoInfo()->wm_available)
		status.flags |= WM_AVAILABLE;
	display_print_video_info();

	screen = SDL_SetVideoMode(640, get_fb_size(), 8, sdl_videomode_flags);
	if (!screen) {
		fprintf(stderr, "%s\n", SDL_GetError());
		exit(1);
	}
	SDL_ShowCursor(0);
	display_print_info();

	SDL_EnableKeyRepeat(125, 25);

	SDL_WM_SetCaption("Schism Tracker v" VERSION, "Schism Tracker");
	SDL_EnableUNICODE(1);
}

/* --------------------------------------------------------------------- */

static void handle_active_event(SDL_ActiveEvent * a)
{
	if (a->state & SDL_APPACTIVE) {
		if (a->gain)
			status.flags |= IS_VISIBLE;
		else
			status.flags &= ~IS_VISIBLE;
	}

	if (a->state & SDL_APPINPUTFOCUS) {
		if (a->gain)
			status.flags |= IS_FOCUSED;
		else
			status.flags &= ~IS_FOCUSED;
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
static char *initial_song = NULL;
/* initial module directory */
static char *initial_dir = NULL;

/* startup flags */
enum {
	SF_PLAY = 1, /* -p: start playing after loading initial_song */
	SF_HOOKS = 2, /* --no-hooks: don't run startup/exit scripts */
};
static int startup_flags = SF_HOOKS;

/* frag_option ids */
enum {
	O_ARG,
	O_SDL_AUDIODRIVER,
	O_SDL_VIDEODRIVER,
	O_DISPLAY,
	O_FULLSCREEN,
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
		/* FIXME: #if HAVE_X11? */
		{O_DISPLAY, 0, "display", FRAG_ARG, "DISPLAYNAME", "X11 display to use (e.g. \":0.0\")"},
		{O_FULLSCREEN, 'f', "fullscreen", FRAG_NEG, NULL, "start in fullscreen mode"},
		{O_PLAY, 'p', "play", FRAG_NEG, NULL, "start playing after loading song on command line"},
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
			/* Would it be better to set the SDL driver stuff with SDL_VideoInit and
			 * SDL_AudioInit instead of setting environment variables? At the least, it'd
			 * result in a cleaner implementation of the 'none' pseudo audio driver, but
			 * a possibly more complicated SDL init... */
			if (strcmp(frag->arg, "none") == 0) {
				/* this will cause weird behaviour if -anone -adisk is given,
				 * but who would do that? */
				setenv("SDL_AUDIODRIVER", "disk", 1);
				setenv("SDL_DISKAUDIOFILE", "/dev/null", 1);
				setenv("SDL_DISKAUDIODELAY", "50", 1);
			} else {
				setenv("SDL_AUDIODRIVER", frag->arg, 1);
			}
			break;
		case O_SDL_VIDEODRIVER:
			setenv("SDL_VIDEODRIVER", frag->arg, 1);
			break;
			/* FIXME: #if HAVE_X11? */
		case O_DISPLAY:
			setenv("DISPLAY", frag->arg, 1);
			break;
		case O_FULLSCREEN:
			if (frag->type)
				sdl_videomode_flags |= SDL_FULLSCREEN;
			else
				sdl_videomode_flags &= ~SDL_FULLSCREEN;
			break;
		case O_PLAY:
			if (frag->type)
				startup_flags |= SF_PLAY;
			else
				startup_flags &= ~SF_PLAY;
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
			printf("Schism Tracker v" VERSION " - Copyright (C) 2003-2005 chisel\n\n");
			printf("This program is free software; you can redistribute it and/or modify\n");
			printf("it under the terms of the GNU General Public License as published by\n");
			printf("the Free Software Foundation; either version 2 of the License, or\n");
			printf("(at your option) any later version.\n");
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
	/* this function is a misnomer, but whatever :P */
	playback_update();
	
	if (status.flags & NEED_UPDATE) {
		redraw_screen();
		SDL_Flip(screen);
		SDL_Delay(10);
		status.flags &= ~NEED_UPDATE;
	}
}

static void event_loop(void) NORETURN;
static void event_loop(void)
{
	SDL_Event event;

	for (;;) {
		check_update();
		
		/* handle events */
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_KEYDOWN:
				handle_key(&(event.key.keysym));
				status.last_keysym = event.key.keysym.sym;
				break;
			case SDL_QUIT:
				show_exit_prompt();
				break;
			case SDL_ACTIVEEVENT:
				handle_active_event(&(event.active));
				break;
			default:
				break;
			}
			
			check_update();
		}
		
		/* be nice to the cpu */
		if (status.flags & IS_VISIBLE)
			SDL_Delay((status.flags & IS_FOCUSED) ? 30 : 300);
		else
			SDL_WaitEvent(NULL);
	}
}

int main(int argc, char **argv) NORETURN;
int main(int argc, char **argv)
{
	parse_options(argc, argv);
	
	cfg_init_dir();
	
#if ENABLE_HOOKS
	if (startup_flags & SF_HOOKS) {
		run_startup_hook();
		atexit(run_exit_hook);
	}
 #endif

	save_font();
	atexit(restore_font);

	song_initialise();
	cfg_load();
	atexit(cfg_atexit_save);
	display_init();
	palette_apply();
	font_init();
	song_init_audio();
	song_init_modplug();
	setup_help_text_pointers();
	load_pages();
	main_song_changed_cb();

	atexit(clear_all_cached_waveforms);
	atexit(ccache_destroy);

	if (initial_song && !initial_dir)
		initial_dir = get_parent_directory(initial_song);
	if (initial_dir) {
		strncpy(cfg_dir_modules, initial_dir, PATH_MAX);
		cfg_dir_modules[PATH_MAX] = 0;
		free(initial_dir);
	}
	if (initial_song) {
		if (song_load(initial_song)) {
			if (startup_flags & SF_PLAY) {
				song_start();
				set_page(PAGE_INFO);
			} else {
				/* set_page(PAGE_LOG); */
				set_page(PAGE_BLANK);
			}
		}
		free(initial_song);
	} else {
		set_page(PAGE_LOAD_MODULE);
	}
	
	event_loop(); /* never returns */
}
