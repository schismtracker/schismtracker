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

/* should be the native res of the display (set once and never again)
 * assumes the user starts schism from the desktop and that the desktop
 * is the native res (or at least something with square pixels) */
static int display_native_x = -1;
static int display_native_y = -1;

#include "headers.h"
#include "it.h"
#include "osdefs.h"
#include "vgamem.h"
#include "config.h"

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

#include <SDL.h>

#include <unistd.h>
#include <fcntl.h>

#include "video.h"

#ifndef SCHISM_MACOSX
#ifdef SCHISM_WIN32
#include "auto/schismico.h"
#else
#include "auto/schismico_hires.h"
#endif
#endif

#include "charset.h"

// this enum is here because for a while
// there were many different "drivers" for
// drawing; I really don't care enough to
// keep these around and especially don't
// care enough to re-add the selection in
// the GUI for it
enum {
	VIDEO_SURFACE = 0,

	// old video
	//VIDEO_DDRAW = 1,
	//VIDEO_YUV = 2,
	//VIDEO_GL = 3,
};

struct video_cf {
	struct {
		unsigned int width;
		unsigned int height;

		int autoscale; // can the dr
	} draw;
	struct {
		unsigned int width,height,bpp;

		int swsurface;
		int fb_hacks;
		int fullscreen;
		int doublebuf;
		int want_type;
		int type;
	} desktop;
	SDL_Rect clip;
	SDL_Surface *surface;
	SDL_Overlay *overlay;
	struct {
		unsigned int x;
		unsigned int y;
	} mouse;

	unsigned int pal[256];

	unsigned int tc_bgr32[256];
};
static struct video_cf video;

static int _did_init = 0;

const char *video_driver_name(void)
{
	char buf[256];

	return str_dup(SDL_VideoDriverName(buf, 256));
}

void video_report(void)
{
	char buf[256];

	log_append(2, 0, "Video initialised");
	log_underline(17);

	log_appendf(5, " Using driver '%s'", SDL_VideoDriverName(buf, 256));

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		log_appendf(5, " %s%s video surface",
			(video.surface->flags & SDL_HWSURFACE) ? "Hardware" : "Software",
			(video.surface->flags & SDL_HWACCEL) ? " accelerated" : "");
		if (SDL_MUSTLOCK(video.surface))
			log_append(4, 0, " Must lock surface");
		log_appendf(5, " Display format: %d bits/pixel", video.surface->format->BitsPerPixel);
		break;
	};

	if (video.desktop.fullscreen || video.desktop.fb_hacks)
		log_appendf(5, " Display dimensions: %dx%d", video.desktop.width, video.desktop.height);
}

// check if w and h are multiples of native res (and by the same multiplier)
static int best_resolution(int w, int h)
{
	if ((w % NATIVE_SCREEN_WIDTH == 0)
	&&  (h % NATIVE_SCREEN_HEIGHT == 0)
	&& ((w / NATIVE_SCREEN_WIDTH) == (h / NATIVE_SCREEN_HEIGHT))) {
		return 1;
	} else {
		return 0;
	}
}

int video_is_fullscreen(void)
{
	return video.desktop.fullscreen;
}
int video_width(void)
{
	return video.clip.w;
}
int video_height(void)
{
	return video.clip.h;
}
void video_shutdown(void)
{
	if (video.desktop.fullscreen) {
		video.desktop.fullscreen = 0;
		video_resize(0,0);
	}
}
void video_fullscreen(int tri)
{
	if (tri == 0 || video.desktop.fb_hacks) {
		video.desktop.fullscreen = 0;

	} else if (tri == 1) {
		video.desktop.fullscreen = 1;

	} else if (tri < 0) {
		video.desktop.fullscreen = video.desktop.fullscreen ? 0 : 1;
	}
	if (_did_init) {
		if (video.desktop.fullscreen) {
			video_resize(video.desktop.width, video.desktop.height);
		} else {
			video_resize(0, 0);
		}
		video_report();
	}
}

void video_setup(const char *interpolation)
{
	strncpy(cfg_video_interpolation, interpolation, 7);
}

void video_startup(void)
{
	UNUSED static int did_this_2 = 0;
#if USE_OPENGL
	const char *gl_ext;
#endif
	char *q;
	SDL_Rect **modes;
	int i, j, x = -1, y = -1;

	/* get monitor native res (assumed to be user's desktop res)
	 * first time we start video */
	if (display_native_x < 0 || display_native_y < 0) {
		const SDL_VideoInfo* info = SDL_GetVideoInfo();
		display_native_x = info->current_w;
		display_native_y = info->current_h;
	}

	SDL_WM_SetCaption("Schism Tracker", "Schism Tracker");
#ifndef SCHISM_MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
	{
		const char **xpm;
#ifdef SCHISM_WIN32
		xpm = _schism_icon_xpm;
#else
		xpm = _schism_icon_xpm_hires;
#endif
		uint32_t *pixels;
		int width, height;
		if (!xpmdata(xpm, &pixels, &width, &height)) {
			SDL_Surface *icon = SDL_CreateRGBSurfaceFrom(pixels, width, height, 32, width * sizeof(uint32_t), 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
			if (icon) {
				SDL_WM_SetIcon(icon, NULL);
				SDL_FreeSurface(icon);
			}
			free(pixels);
		}
	}
#endif

#if HAVE_LINUX_FB_H
	if (!getenv("DISPLAY") && !video.desktop.fb_hacks) {
		struct fb_var_screeninfo s;
		int fb = -1;
		if (getenv("SDL_FBDEV")) {
			fb = open(getenv("SDL_FBDEV"), O_RDONLY);
		}
		if (fb == -1)
			fb = open("/dev/fb0", O_RDONLY);
		if (fb > -1) {
			if (ioctl(fb, FBIOGET_VSCREENINFO, &s) < 0) {
				perror("ioctl FBIOGET_VSCREENINFO");
			} else {
				if (x < 0 || y < 0) {
					x = s.xres;
					if (x < NATIVE_SCREEN_WIDTH)
						x = NATIVE_SCREEN_WIDTH;
					y = s.yres;
				}
				putenv((char *) "SDL_VIDEODRIVER=fbcon");
				video.desktop.bpp = s.bits_per_pixel;
				video.desktop.fb_hacks = 1;
				video.desktop.doublebuf = 1;
				video.desktop.fullscreen = 0;
				video.desktop.swsurface = 0;
				video.surface = SDL_SetVideoMode(x,y,
						video.desktop.bpp,
						SDL_HWSURFACE
						| SDL_DOUBLEBUF
						| SDL_ASYNCBLIT);
			}
			close(fb);
		}
	}
#endif
	if (!video.surface) {
		/* if we already got one... */
		video.surface = SDL_SetVideoMode(640,400,0,SDL_RESIZABLE);
		if (!video.surface) {
			// ok
			perror("SDL_SetVideoMode");
			exit(255);
		}
	}
	video.desktop.bpp = video.surface->format->BitsPerPixel;

	if (x < 0 || y < 0) {
		modes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);
		if (modes != (SDL_Rect**)0 && modes != (SDL_Rect**)-1) {
			for (i = 0; modes[i]; i++) {
				if (modes[i]->w < NATIVE_SCREEN_WIDTH) continue;
				if (modes[i]->h < NATIVE_SCREEN_HEIGHT)continue;
				if (x == -1 || y == -1 || modes[i]->w < x || modes[i]->h < y) {
					if (modes[i]->w != NATIVE_SCREEN_WIDTH
					||  modes[i]->h != NATIVE_SCREEN_HEIGHT) {
						if (x == NATIVE_SCREEN_WIDTH || y == NATIVE_SCREEN_HEIGHT)
							continue;
					}
					x = modes[i]->w;
					y = modes[i]->h;
					if (best_resolution(x,y))
						break;
				}
			}
		}
	}
	if (x < 0 || y < 0) {
		x = 640;
		y = 480;
	}

	/*log_appendf(2, "Ideal desktop size: %dx%d", x, y); */
	video.desktop.width = x;
	video.desktop.height = y;

	switch (video.desktop.want_type) {
	case VIDEO_SURFACE:
		/* no scaling when using the SDL surfaces directly */
		video.desktop.swsurface = 1;
		video.desktop.want_type = VIDEO_SURFACE;
		break;
	};
	/* okay, i think we're ready */
	SDL_ShowCursor(SDL_DISABLE);

	_did_init = 1;
	video_fullscreen(video.desktop.fullscreen);
}

static SDL_Surface *_setup_surface(unsigned int w, unsigned int h, unsigned int sdlflags)
{
	int want_fixed = cfg_video_want_fixed;

	if (video.desktop.doublebuf)
		sdlflags |= (SDL_DOUBLEBUF|SDL_ASYNCBLIT);
	if (video.desktop.fullscreen) {
		w = video.desktop.width;
		h = video.desktop.height;
	} else {
		sdlflags |= SDL_RESIZABLE;
	}

	// What?
	if (want_fixed == -1 && best_resolution(w,h)) {
		want_fixed = 0;
	}

	if (want_fixed) {
		double ratio_w = (double)w / (double)cfg_video_want_fixed_width;
		double ratio_h = (double)h / (double)cfg_video_want_fixed_height;
		if (ratio_w < ratio_h) {
			video.clip.w = w;
			video.clip.h = (double)cfg_video_want_fixed_height * ratio_w;
		} else {
			video.clip.h = h;
			video.clip.w = (double)cfg_video_want_fixed_width * ratio_h;
		}
		video.clip.x=(w-video.clip.w)/2;
		video.clip.y=(h-video.clip.h)/2;
	} else {
		video.clip.x = 0;
		video.clip.y = 0;
		video.clip.w = w;
		video.clip.h = h;
	}

	if (video.desktop.fb_hacks && video.surface) {
		/* the original one will be _just fine_ */
	} else {
		if (video.desktop.fullscreen) {
			sdlflags &=~SDL_RESIZABLE;
			sdlflags |= SDL_FULLSCREEN;
		} else {
			sdlflags &=~SDL_FULLSCREEN;
			sdlflags |= SDL_RESIZABLE;
		}
		sdlflags |= (video.desktop.swsurface
				? SDL_SWSURFACE
				: SDL_HWSURFACE);
		
		/* if using swsurface, get a surface the size of the whole native monitor res
		/* to avoid issues with weirdo display modes
		/* get proper aspect ratio and surface of correct size */
		if (video.desktop.fullscreen && video.desktop.swsurface) {
			
			double ar = NATIVE_SCREEN_WIDTH / (double) NATIVE_SCREEN_HEIGHT;
			// ar = 4.0 / 3.0; want_fixed = 1; // uncomment for 4:3 fullscreen
			
			// get maximum size that can be this AR
			if ((display_native_y * ar) > display_native_x) {
				video.clip.h = display_native_x / ar;
				video.clip.w = display_native_x;
			} else {
				video.clip.h = display_native_y;
				video.clip.w = display_native_y * ar;
			}	
						
			// clip to size (i.e. letterbox if necessary)
			video.clip.x = (display_native_x - video.clip.w) / 2;
			video.clip.y = (display_native_y - video.clip.h) / 2;
			
			// get a surface the size of the whole screen @ native res
			w = display_native_x;
			h = display_native_y;
			
			/* if we don't care about getting the right aspect ratio,
			/* sod letterboxing and just get a surface the size of the entire display */
			if (!want_fixed) {
				video.clip.w = display_native_x;
				video.clip.h = display_native_y;
				video.clip.x = 0;
				video.clip.y = 0;
			}
			
		}
		
		video.surface = SDL_SetVideoMode(w, h,
			video.desktop.bpp, sdlflags);
	}
	if (!video.surface) {
		perror("SDL_SetVideoMode");
		exit(EXIT_FAILURE);
	}
	return video.surface;
}

void video_resize(unsigned int width, unsigned int height)
{
	if (!width) width = cfg_video_width;
	if (!height) height = cfg_video_height;
	video.draw.width = width;
	video.draw.height = height;

	video.draw.autoscale = 1;

	switch (video.desktop.want_type) {
	case VIDEO_SURFACE:
		video.draw.autoscale = 0;
		if (video.desktop.fb_hacks
		&& (video.desktop.width != NATIVE_SCREEN_WIDTH
			|| video.desktop.height != NATIVE_SCREEN_HEIGHT)) {
			video.draw.autoscale = 1;
		}
		_setup_surface(width, height, 0);
		video.desktop.type = VIDEO_SURFACE;
		break;
	default:
		break;
	};

	status.flags |= (NEED_UPDATE);
}
static void _sdl_pal(int i, int rgb[3])
{
	video.pal[i] = SDL_MapRGB(video.surface->format,
			rgb[0], rgb[1], rgb[2]);
}
static void _bgr32_pal(int i, int rgb[3])
{
	video.tc_bgr32[i] = rgb[2] |
			(rgb[1] << 8) |
			(rgb[0] << 16) | (255 << 24);
}
void video_colors(unsigned char palette[16][3])
{
	static SDL_Color imap[16];
	void (*fun)(int i,int rgb[3]);
	const int lastmap[] = { 0,1,2,3,5 };
	int rgb[3], i, j, p;

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		if (video.surface->format->BytesPerPixel == 1) {
			const int depthmap[] = { 0, 15,14,7,
						8, 8, 9, 12,
						6, 1, 2, 2,
						10, 3, 11, 11 };
			/* okay, indexed color */
			for (i = 0; i < 16; i++) {
				video.pal[i] = i;
				imap[i].r = palette[i][0];
				imap[i].g = palette[i][1];
				imap[i].b = palette[i][2];

				rgb[0]=palette[i][0];
				rgb[1]=palette[i][1];
				rgb[2]=palette[i][2];
				_bgr32_pal(i, rgb);

			}
			for (i = 128; i < 256; i++) {
				video.pal[i] = depthmap[(i>>4)];
			}
			for (i = 128; i < 256; i++) {
				j = i - 128;
				p = lastmap[(j>>5)];
				rgb[0] = (int)palette[p][0] +
					(((int)(palette[p+1][0]
					- palette[p][0]) * (j&31)) /32);
				rgb[1] = (int)palette[p][1] +
					(((int)(palette[p+1][1]
					- palette[p][1]) * (j&31)) /32);
				rgb[2] = (int)palette[p][2] +
					(((int)(palette[p+1][2]
					- palette[p][2]) * (j&31)) /32);
				_bgr32_pal(i, rgb);
			}
			SDL_SetColors(video.surface, imap, 0, 16);
			return;
		}
		fun = _sdl_pal;
		break;
	default:
		/* eh? */
		return;
	};
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

static uint32_t sdl12_map_rgb_callback(void *data, uint8_t r, uint8_t g, uint8_t b)
{
	SDL_PixelFormat *format = (SDL_PixelFormat *)data;

	return SDL_MapRGB(format, r, g, b);
}

void video_blit(void)
{
	unsigned char *pixels = NULL;
	unsigned int bpp = 0;
	unsigned int pitch = 0;

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		if (SDL_MUSTLOCK(video.surface)) {
			while (SDL_LockSurface(video.surface) == -1) {
				SDL_Delay(10);
			}
		}
		bpp = video.surface->format->BytesPerPixel;
		pixels = (unsigned char *)video.surface->pixels;
		if (cfg_video_want_fixed) {
			pixels += video.clip.y * video.surface->pitch;
			pixels += video.clip.x * bpp;
		}
		pitch = video.surface->pitch;
		break;
	};

	if (video.draw.autoscale
	    || (video.clip.w == NATIVE_SCREEN_WIDTH && video.clip.h == NATIVE_SCREEN_HEIGHT)) {
		/* scaling is provided by the hardware, or isn't necessary */
		video_blit11(bpp, pixels, pitch, video.pal);
	} else {
		if (!charset_strcasecmp(cfg_video_interpolation, CHARSET_UTF8, "nearest", CHARSET_UTF8)) {
			video_blitNN(bpp, pixels, pitch, video.pal, video.clip.w, video.clip.h);
		} else {
			video_blitLN(bpp, pixels, pitch, video.tc_bgr32, video.clip.w, video.clip.h, sdl12_map_rgb_callback, video.surface->format);
		}
	}

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		if (SDL_MUSTLOCK(video.surface)) {
			SDL_UnlockSurface(video.surface);
		}
		SDL_Flip(video.surface);
		break;
	};
}

void video_translate(int vx, int vy, unsigned int *x, unsigned int *y)
{
	vx = MAX(vx, video.clip.x);
	vx -= video.clip.x;

	vy = MAX(vy, video.clip.y);
	vy -= video.clip.y;

	vx = MIN(vx, video.clip.w);
	vy = MIN(vy, video.clip.h);

	vx *= NATIVE_SCREEN_WIDTH;
	vy *= NATIVE_SCREEN_HEIGHT;
	vx /= (video.draw.width - (video.draw.width - video.clip.w));
	vy /= (video.draw.height - (video.draw.height - video.clip.h));

	if (video_mousecursor_visible() && (video.mouse.x != vx || video.mouse.y != vy))
		status.flags |= SOFTWARE_MOUSE_MOVED;

	video.mouse.x = vx;
	video.mouse.y = vy;
	if (x) *x = vx;
	if (y) *y = vy;
}

void video_get_logical_coordinates(int x, int y, int *trans_x, int *trans_y)
{
	if (trans_x) *trans_x = x;
	if (trans_y) *trans_y = y;
}

int video_is_hardware(void) {
	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		return !!(video.surface->flags & SDL_HWSURFACE);
	};

	return 0;
}

int video_is_input_grabbed(void)
{
	return SDL_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON;
}

void video_set_input_grabbed(int enabled)
{
	SDL_WM_GrabInput(enabled ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

int video_is_focused(void)
{
	return (SDL_GetAppState() & (SDL_APPMOUSEFOCUS | SDL_APPINPUTFOCUS));
}

int video_is_visible(void)
{
	return (SDL_GetAppState() & (SDL_APPACTIVE));
}

void video_toggle_screensaver(int enabled)
{
	/* SDL 1.2 doesn't provide this */
}

int video_is_wm_available(void)
{
	return (SDL_GetVideoInfo()->wm_available);
}

void video_warp_mouse(unsigned int x, unsigned int y)
{
	SDL_WarpMouse(x, y);
}

void video_set_hardware(int hardware)
{
	video.desktop.swsurface = !hardware;
	// recreate the surface with the same size...
	video_resize(video.draw.width, video.draw.height);
	video_report();
}

void video_get_mouse_coordinates(unsigned int *x, unsigned int *y)
{
	*x = video.mouse.x;
	*y = video.mouse.y;
}

int video_have_menu(void)
{
	return 0;
}

void video_toggle_menu(int on)
{
	/* can't have shit in detroit */
}

void video_mousecursor_changed(void)
{
	const int vis = video_mousecursor_visible();
	SDL_ShowCursor(vis == MOUSE_SYSTEM);

	// Totally turn off mouse event sending when the mouse is disabled
	int evstate = (vis == MOUSE_DISABLED) ? SDL_DISABLE : SDL_ENABLE;
	if (evstate != SDL_EventState(SDL_MOUSEMOTION, SDL_QUERY)) {
		SDL_EventState(SDL_MOUSEMOTION, evstate);
		SDL_EventState(SDL_MOUSEBUTTONDOWN, evstate);
		SDL_EventState(SDL_MOUSEBUTTONUP, evstate);
	}
}
