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
#define NATIVE_SCREEN_WIDTH             640
#define NATIVE_SCREEN_HEIGHT            400
#define WINDOW_TITLE			"Schism Tracker"

/* should be the native res of the display (set once and never again)
 * assumes the user starts schism from the desktop and that the desktop
 * is the native res (or at least something with square pixels) */
static int display_native_x = -1;
static int display_native_y = -1;

#include "headers.h"
#include "it.h"
#include "osdefs.h"

/* bugs
 * ... in sdl. not in this file :)
 *
 *  - take special care to call SDL_SetVideoMode _exactly_ once
 *    when on a console (video.desktop.fb_hacks)
 *
 */

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
#if HAVE_SIGNAL_H
#include <signal.h>
#endif

/* for memcpy */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "sdlmain.h"

#include <unistd.h>
#include <fcntl.h>

#include "video.h"

#ifndef MACOSX
#ifdef WIN32
#include "auto/schismico.h"
#else
#include "auto/schismico_hires.h"
#endif
#endif

extern int macosx_did_finderlaunch;

/* leeto drawing skills */
#define MOUSE_HEIGHT    14
static const unsigned int _mouse_pointer[] = {
	/* x....... */  0x80,
	/* xx...... */  0xc0,
	/* xxx..... */  0xe0,
	/* xxxx.... */  0xf0,
	/* xxxxx... */  0xf8,
	/* xxxxxx.. */  0xfc,
	/* xxxxxxx. */  0xfe,
	/* xxxxxxxx */  0xff,
	/* xxxxxxx. */  0xfe,
	/* xxxxx... */  0xf8,
	/* x...xx.. */  0x8c,
	/* ....xx.. */  0x0c,
	/* .....xx. */  0x06,
	/* .....xx. */  0x06,

0,0
};

struct video_cf {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_DisplayMode display;
	unsigned char *framebuf;
	unsigned int width;
	unsigned int height;
	int x;
	int y;
	int width_height_defined;

	struct {
		unsigned int width;
		unsigned int height;
	} prev;

	struct {
		unsigned int x;
		unsigned int y;
		int visible;
	} mouse;

	struct {
		unsigned int width;
		unsigned int height;
		int fullscreen;
	} fullscreen;

	unsigned int pal[256];

	unsigned int tc_bgr32[256];
};
static struct video_cf video;

int video_is_fullscreen(void)
{
	return video.fullscreen.fullscreen;
}

int video_width(void)
{
	return video.width;
}

int video_height(void)
{
	return video.height;
}

void video_update(void)
{
	SDL_GetWindowPosition(video.window, &video.x, &video.y);
}

const char * video_driver_name(void)
{
	return "SDL2";
}

void video_report(void)
{
	Uint32 format;
	SDL_QueryTexture(video.texture, &format, NULL, NULL, NULL);

	log_appendf(5, " Using driver '%s'", SDL_GetCurrentVideoDriver());

	log_appendf(5, " Display format: %d bits/pixel", SDL_BITSPERPIXEL(format));
	if (video.fullscreen.fullscreen) {
		log_appendf(5, " Display dimensions: %dx%d", video.display.w, video.display.h);
	}
}

void video_redraw_texture(void)
{
	SDL_DestroyTexture(video.texture);
	video.texture = SDL_CreateTexture(video.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
}

void video_shutdown(void)
{
	SDL_GetWindowPosition(video.window, &video.x, &video.y);
	SDL_DestroyRenderer(video.renderer);
	SDL_DestroyWindow(video.window);
	SDL_DestroyTexture(video.texture);
}

void video_setup(const char* quality)
{
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, quality);
}

static void set_icon(void)
{
	SDL_SetWindowTitle(video.window, WINDOW_TITLE);
#ifndef MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
#ifdef WIN32
/* win32 icons must be 32x32 according to SDL 1.2 doc */
	SDL_Surface *icon = xpmdata(_schism_icon_xpm);
#else
	SDL_Surface *icon = xpmdata(_schism_icon_xpm_hires);
#endif
	SDL_SetWindowIcon(video.window, icon);
	SDL_FreeSurface(icon);
#endif
}

void video_fullscreen(int new_fs_flag)
{
	/**
	 * new_fs_flag has three values:
	 * >0 being fullscreen,
	 *  0 being windowed, and
	 * <0 meaning to switch.
	**/
	if (new_fs_flag > 0) {
		video.fullscreen.fullscreen = 1;
	} else if (new_fs_flag < 0){
		if (video.fullscreen.fullscreen > 0) {
			video.fullscreen.fullscreen = 0;
		} else {
			video.fullscreen.fullscreen = 1;
		}
	} else {
		video.fullscreen.fullscreen = 0;
	}
	if (video.fullscreen.fullscreen) {
		SDL_SetWindowSize(video.window, video.display.w, video.display.h);
		video_resize(video.display.w, video.display.h);
		SDL_SetWindowFullscreen(video.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		SDL_SetWindowResizable(video.window, SDL_FALSE);
	} else {
		SDL_SetWindowFullscreen(video.window, 0);
		video_resize(video.fullscreen.width, video.fullscreen.height);
		SDL_SetWindowSize(video.window, video.fullscreen.width, video.fullscreen.height);
		SDL_SetWindowResizable(video.window, SDL_TRUE);
		set_icon();
	}
	video.fullscreen.width = video.prev.width;
	video.fullscreen.height = video.prev.height;
}

void video_startup(void)
{
	vgamem_clear();
	vgamem_flip();

	if (!SDL_GetHint(SDL_HINT_RENDER_SCALE_QUALITY))
		SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, cfg_video_interpolation);

	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

	if (!video.width_height_defined) {
		video.x = SDL_WINDOWPOS_UNDEFINED;
		video.y = SDL_WINDOWPOS_UNDEFINED;
		video.fullscreen.width = video.prev.width = video.width = cfg_video_width;
		video.fullscreen.height = video.prev.height = video.height = cfg_video_height;
	}

	SDL_CreateWindowAndRenderer(video.width, video.height, SDL_WINDOW_RESIZABLE, &video.window, &video.renderer);
	video_resize(video.width, video.height);
	video.texture = SDL_CreateTexture(video.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
	video.framebuf = calloc(NATIVE_SCREEN_WIDTH * NATIVE_SCREEN_HEIGHT, sizeof(Uint32));

	SDL_SetWindowPosition(video.window, video.x, video.y);
	SDL_GetCurrentDisplayMode(0, &video.display);
	video_fullscreen(cfg_video_fullscreen);

	/* okay, i think we're ready */
	SDL_ShowCursor(SDL_DISABLE);
	set_icon();
	video.width_height_defined = 1;
}

void video_resize(unsigned int width, unsigned int height)
{
	/* Aspect ratio correction if it's wanted */
	if (cfg_video_want_fixed)
		SDL_RenderSetLogicalSize(video.renderer,
					 NATIVE_SCREEN_WIDTH * 5, NATIVE_SCREEN_HEIGHT * 6); // 4:3
	else 
		SDL_RenderSetLogicalSize(video.renderer, width, height);

	video.prev.width = video.width;
	video.prev.height = video.height;
	video.width = width;
	video.height = height;
	status.flags |= (NEED_UPDATE);
}

static void _bgr32_pal(int i, int rgb[3])
{
	video.tc_bgr32[i] = rgb[2] |
			(rgb[1] << 8) |
			(rgb[0] << 16) | (255 << 24);
}
static void _gl_pal(int i, int rgb[3])
{
	video.pal[i] = rgb[2] |
			(rgb[1] << 8) |
			(rgb[0] << 16) | (255 << 24);
}

void video_colors(unsigned char palette[16][3])
{
	void (*fun)(int i,int rgb[3]);
	const int lastmap[] = { 0,1,2,3,5 };
	int rgb[3], i, j, p;

	fun = _gl_pal;

	/* make our "base" space */
	for (i = 0; i < 16; i++) {
		rgb[0]=palette[i][0];
		rgb[1]=palette[i][1];
		rgb[2]=palette[i][2];
		fun(i, rgb);
		_bgr32_pal(i, rgb);
	}
	/* make our "gradient" space */
	for (i = 128; i < 256; i++) {
		j = i - 128;
		p = lastmap[(j>>5)];
		rgb[0] = (int)palette[p][0] +
			(((int)(palette[p+1][0] - palette[p][0]) * (j&31)) /32);
		rgb[1] = (int)palette[p][1] +
			(((int)(palette[p+1][1] - palette[p][1]) * (j&31)) /32);
		rgb[2] = (int)palette[p][2] +
			(((int)(palette[p+1][2] - palette[p][2]) * (j&31)) /32);
		fun(i, rgb);
		_bgr32_pal(i, rgb);
	}
}

void video_refresh(void)
{
	vgamem_flip();
	vgamem_clear();
}

static inline void make_mouseline(unsigned int x, unsigned int v, unsigned int y, unsigned int mouseline[80])
{
	int y_int =	(video.mouse.y >= 5368708) ? 0 :
			(video.mouse.y >= 400)     ? 399 : (int)video.mouse.y;
	unsigned int z;

	memset(mouseline, 0, 80*sizeof(unsigned int));
	if (video.mouse.visible != MOUSE_EMULATED
	    || !(status.flags & IS_FOCUSED)
	    || y < y_int
	    || y >= y_int+MOUSE_HEIGHT) {
		return;
	}

	z = _mouse_pointer[ y - y_int ];
	mouseline[x] = z >> v;
	if (x < 79) mouseline[x+1] = (z << (8-v)) & 0xff;
}

static void _blit11(unsigned char *pixels, unsigned int pitch, unsigned int *tpal)
{
	/* emulates how SDL 1.2 handled it :p */
	/* effectively handles underflows/overflows */
	unsigned int mouseline_x =	(video.mouse.x >= 3355443) ? 0  :
					(video.mouse.x >= 640)     ? 79 : (video.mouse.x / 8);
	unsigned int mouseline_v =	(video.mouse.x >= 3355443) ? 0  :
					(video.mouse.x >= 640)     ? 7  : (video.mouse.x % 8);
	unsigned int mouseline[80];
	unsigned char *pdata;
	unsigned int x, y;
	int pitch24;

	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline);
		vgamem_scan32(y, (unsigned int *)pixels, tpal, mouseline);
		pixels += pitch;
	}
}

void video_blit(void)
{
	unsigned char *pixels = video.framebuf;
	unsigned int pitch = NATIVE_SCREEN_WIDTH * sizeof(Uint32);

	_blit11(pixels, pitch, video.pal);

	SDL_RenderClear(video.renderer);
	SDL_UpdateTexture(video.texture, NULL, pixels, pitch);
	SDL_RenderCopy(video.renderer, video.texture, NULL, NULL);
	SDL_RenderPresent(video.renderer);
}

int video_mousecursor_visible(void)
{
	return video.mouse.visible;
}

void video_mousecursor(int vis)
{
	const char *state[] = {
		"Mouse disabled",
		"Software mouse cursor enabled",
		"Hardware mouse cursor enabled",
	};

	if (status.flags & NO_MOUSE) {
		// disable it no matter what
		video.mouse.visible = MOUSE_DISABLED;
		//SDL_ShowCursor(0);
		return;
	}

	switch (vis) {
	case MOUSE_CYCLE_STATE:
		vis = (video.mouse.visible + 1) % MOUSE_CYCLE_STATE;
		/* fall through */
	case MOUSE_DISABLED:
	case MOUSE_SYSTEM:
	case MOUSE_EMULATED:
		video.mouse.visible = vis;
		status_text_flash("%s", state[video.mouse.visible]);
	case MOUSE_RESET_STATE:
		break;
	default:
		video.mouse.visible = MOUSE_EMULATED;
	}

	SDL_ShowCursor(video.mouse.visible == MOUSE_SYSTEM);

	// Totally turn off mouse event sending when the mouse is disabled
	int evstate = video.mouse.visible == MOUSE_DISABLED ? SDL_DISABLE : SDL_ENABLE;
	if (evstate != SDL_EventState(SDL_MOUSEMOTION, SDL_QUERY)) {
		SDL_EventState(SDL_MOUSEMOTION, evstate);
		SDL_EventState(SDL_MOUSEBUTTONDOWN, evstate);
		SDL_EventState(SDL_MOUSEBUTTONUP, evstate);
	}
}

void video_translate(unsigned int vx, unsigned int vy, unsigned int *x, unsigned int *y)
{
	if (video.mouse.visible && (video.mouse.x != vx || video.mouse.y != vy))
		status.flags |= SOFTWARE_MOUSE_MOVED;

	vx *= NATIVE_SCREEN_WIDTH;
	vy *= NATIVE_SCREEN_HEIGHT;
	vx /= (video.width - (video.width - video.prev.width));
	vy /= (video.height - (video.height - video.prev.height));

	video.mouse.x = vx;
	video.mouse.y = vy;
	if (x)
		*x = vx;

	if (y)
		*y = vy;
}

SDL_Window * video_window(void)
{
	return video.window;
}
