/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#if HAVE_SYS_KD_H
# include <sys/kd.h>
#endif
#if HAVE_LINUX_FB_H
# include <linux/fb.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>

#include <SDL.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

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

static inline void display_print_video_info(void)
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

static inline void display_print_info(void)
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

	screen = SDL_SetVideoMode(640, get_fb_size(), 8,
				  (SDL_HWPALETTE | SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_ANYFORMAT));
	if (!screen) {
		fprintf(stderr, "%s\n", SDL_GetError());
		exit(1);
	}
	SDL_ShowCursor(0);
	display_print_info();

	SDL_EnableKeyRepeat(125, 0);

	SDL_WM_SetCaption("Schism Tracker v" VERSION, "Schism Tracker");
	SDL_EnableUNICODE(1);
}

/* --------------------------------------------------------------------- */

static inline void handle_active_event(SDL_ActiveEvent * a)
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

static void run_startup_hook(void)
{
	char filename[PATH_MAX + 1];
	const char *home_dir = getenv("HOME") ? : "/";
	
	strncpy(filename, home_dir, PATH_MAX);
	strncat(filename, "/.schism/startup-hook", PATH_MAX);
	filename[PATH_MAX] = 0;
	
	if (access(filename, X_OK) == 0)
		system(filename);
}

static void run_exit_hook(void)
{
	char filename[PATH_MAX + 1];
	const char *home_dir = getenv("HOME") ? : "/";
	
	strncpy(filename, home_dir, PATH_MAX);
	strncat(filename, "/.schism/exit-hook", PATH_MAX);
	filename[PATH_MAX] = 0;
	
	if (access(filename, X_OK) == 0)
		system(filename);
}

/* --------------------------------------------------------------------- */

int main(int argc, char **argv) NORETURN;
int main(int argc, char **argv)
{
	SDL_Event event;

	run_startup_hook();
	atexit(run_exit_hook);
	
	save_font();
	atexit(restore_font);

	song_initialise();
	cfg_load();
	display_init();
	palette_apply();
	font_init();
	song_init_audio();
	song_init_modplug();
	setup_help_text_pointers();
	load_pages();
	main_song_changed_cb();
	song_stop();
	SDL_PauseAudio(0);
	
	atexit(clear_all_cached_waveforms);
	/* TODO: getopt */
	if (argc >= 2) {
		if (song_load(argv[1])) {
			/* set_page(PAGE_LOG); */
			set_page(PAGE_BLANK);
		}
	} else {
		set_page(PAGE_LOAD_MODULE);
	}
	
	for (;;) {
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
		}

		/* see if stuff changed.
		 * this function is a misnomer, but whatever :P */
		playback_update();

		/* redraw the screen if it's needed */
		if (status.flags & NEED_UPDATE) {
			redraw_screen();
			status.flags &= ~NEED_UPDATE;
		}

		/* be nice to the cpu */
		if (status.flags & IS_VISIBLE) {
			SDL_Delay((status.flags & IS_FOCUSED) ? 30 : 200);
		} else {
			SDL_WaitEvent(NULL);
		}
	}
}
