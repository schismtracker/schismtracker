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

#include "headers.h"
#include "osdefs.h"
#include "config.h"
#include "events.h"
#include "mem.h"
#include "video.h"
#include "it.h"

#include "backend/video.h"

#include "init.h"

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
# include <fcntl.h>
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

#include <SDL.h>
#include <SDL_syswm.h>
#ifdef SCHISM_MACOS
# include <SDL_main.h>
#endif

#include "video.h"

#ifndef SCHISM_MACOSX
#ifdef SCHISM_WIN32
#include "auto/schismico.h"
#else
#include "auto/schismico_hires.h"
#endif
#endif

#include "charset.h"

#ifdef SCHISM_MACOS
# include <Quickdraw.h>
#endif

enum {
	VIDEO_SURFACE = 0,

	// removed...
	//VIDEO_DDRAW = 1,
	VIDEO_YUV = 2,
	VIDEO_OPENGL = 3,
};

static struct video_cf {
	int type; /* enum above */

	SDL_Surface *surface;

	SDL_Overlay *overlay;

	/* actual window/screen coordinates */
	struct {
		uint32_t width, height;
	} draw;

	/* desktop info */
	struct {
		uint32_t width, height, bpp;

		/* a buncha booleans */
		unsigned int swsurface : 1; /* this is stupid and needs to go away */
#ifdef HAVE_LINUX_FB_H
		unsigned int fb_hacks : 1;
#endif
		unsigned int fullscreen : 1;
		unsigned int doublebuf : 1;

		uint32_t yuvformat; /* VIDEO_YUV_* */
	} desktop;

	struct {
		uint32_t x, y, w, h;
	} clip;

	SDL_Color imap[256];

	uint32_t pal[256];
} video;

static int (SDLCALL *sdl12_InitSubSystem)(Uint32 flags);
static void (SDLCALL *sdl12_QuitSubSystem)(Uint32 flags);

static char *(SDLCALL *sdl12_VideoDriverName)(char *namebuf, int maxlen);
static const SDL_VideoInfo *(SDLCALL *sdl12_GetVideoInfo)(void);
static void (SDLCALL *sdl12_WM_SetCaption)(const char *title, const char *icon);
static SDL_Surface *(SDLCALL *sdl12_SetVideoMode)(int width, int height, int bpp, Uint32 flags);
static SDL_Rect **(SDLCALL *sdl12_ListModes)(SDL_PixelFormat *format, Uint32 flags);
static int (SDLCALL *sdl12_ShowCursor)(int toggle);
static Uint32 (SDLCALL *sdl12_MapRGB)(const SDL_PixelFormat *fmt, Uint8 r, Uint8 g, Uint8 b);
static int (SDLCALL *sdl12_SetColors)(SDL_Surface *surface, SDL_Color *colors, int firstcolor, int ncolors);
static int (SDLCALL *sdl12_LockSurface)(SDL_Surface *surface);
static void (SDLCALL *sdl12_Delay)(Uint32 ms);
static void (SDLCALL *sdl12_UnlockSurface)(SDL_Surface *surface);
static int (SDLCALL *sdl12_Flip)(SDL_Surface *screen);
static SDL_GrabMode (SDLCALL *sdl12_WM_GrabInput)(SDL_GrabMode mode);
static Uint8 (SDLCALL *sdl12_GetAppState)(void);
static void (SDLCALL *sdl12_WarpMouse)(Uint16 x, Uint16 y);
static Uint8 (SDLCALL *sdl12_EventState)(Uint8 type, int state);
static void (SDLCALL *sdl12_FreeSurface)(SDL_Surface *surface);
static void (SDLCALL *sdl12_WM_SetIcon)(SDL_Surface *icon, Uint8 *mask);
static SDL_Surface *(SDLCALL *sdl12_CreateRGBSurfaceFrom)(void *pixels, int width, int height, int depth, int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
static int (SDLCALL *sdl12_EnableUNICODE)(int enable);
static int (SDLCALL *sdl12_GetWMInfo)(SDL_SysWMinfo *);
#ifdef SCHISM_MACOS
static void (SDLCALL *sdl12_InitQuickDraw)(struct QDGlobals *the_qd);
#endif
static int (SDLCALL *sdl12_VideoModeOK)(int width, int height, int bpp, Uint32 flags);

static void (SDLCALL *sdl12_FreeYUVOverlay)(SDL_Overlay *overlay);
static int (SDLCALL *sdl12_DisplayYUVOverlay)(SDL_Overlay *overlay, SDL_Rect *dstrect);
static SDL_Overlay *(SDLCALL *sdl12_CreateYUVOverlay)(int width, int height, Uint32 format, SDL_Surface *display);
static int (SDLCALL *sdl12_LockYUVOverlay)(SDL_Overlay *overlay);
static void (SDLCALL *sdl12_UnlockYUVOverlay)(SDL_Overlay *overlay);

static const char *sdl12_video_driver_name(void)
{
	static char buf[256];

	sdl12_VideoDriverName(buf, 256);

	return buf;
}

static void sdl12_video_report(void)
{
	log_appendf(5, " Using driver '%s'", sdl12_video_driver_name());

	switch (video.type) {
	case VIDEO_OPENGL:
		video_opengl_report();
		log_appendf(5, " Display format: %d bits/pixel", (int)video.surface->format->BitsPerPixel);
		break;
	case VIDEO_YUV:
		log_appendf(5, " %s-accelerated video overlay",
			video.overlay->hw_overlay ? "Hardware" : "Non");
		video_yuv_report();
		break;
	case VIDEO_SURFACE:
		log_appendf(5, " %s%s video surface",
			(video.surface->flags & SDL_HWSURFACE) ? "Hardware" : "Software",
			(video.surface->flags & SDL_HWACCEL) ? " accelerated" : "");
		if (SDL_MUSTLOCK(video.surface))
			log_append(4, 0, " Must lock surface");
		log_appendf(5, " Display format: %d bits/pixel", (int)video.surface->format->BitsPerPixel);
		break;
	}

	if (video.desktop.fullscreen
#ifdef HAVE_LINUX_FB_H
		|| video.desktop.fb_hacks
#endif
	) {
		log_appendf(5, " Display dimensions: %dx%d", video.desktop.width, video.desktop.height);
	}
}

// check if w and h are multiples of native res (and by the same multiplier)
static inline SCHISM_ALWAYS_INLINE int best_resolution(int w, int h)
{
	return (w % NATIVE_SCREEN_WIDTH == 0)
		&& (h % NATIVE_SCREEN_HEIGHT == 0)
		&& ((w / NATIVE_SCREEN_WIDTH) == (h / NATIVE_SCREEN_HEIGHT));
}

static int sdl12_video_is_fullscreen(void)
{
	return video.desktop.fullscreen;
}

static int sdl12_video_width(void)
{
	return video.clip.w;
}

static int sdl12_video_height(void)
{
	return video.clip.h;
}

static void sdl12_video_shutdown(void)
{
	if (video.desktop.fullscreen) {
		video.desktop.fullscreen = 0;
		video_resize(0,0);
	}
}

static void sdl12_video_fullscreen(int tri)
{
	if (tri == 0
#ifdef HAVE_LINUX_FB_H
		|| video.desktop.fb_hacks
#endif
	) {
		video.desktop.fullscreen = 0;
	} else if (tri == 1) {
		video.desktop.fullscreen = 1;
	} else if (tri < 0) {
		video.desktop.fullscreen = !video.desktop.fullscreen;
	}

	if (video.desktop.fullscreen) {
		video_resize(video.desktop.width, video.desktop.height);
		video_toggle_menu(0);
	} else {
		video_toggle_menu(1);
		video_resize(0, 0);
	}
	video_report();
}

static void sdl12_video_setup(int interpolation)
{
	switch (video.type) {
	case VIDEO_OPENGL:
		video_opengl_reset_interpolation();
		break;
	case VIDEO_YUV:
		/* Rebuild the surface w/ a different type if
		 * interpolation != nearest */
		if (interpolation != VIDEO_INTERPOLATION_NEAREST)
			video_resize(video.draw.width, video.draw.height);
		break;
	}
}

/* defined lower in this file */
static int sdl12_opengl_object_load(const char *path);
static void sdl12_opengl_object_unload(void);
static void *sdl12_opengl_function_load(const char *function);
static int sdl12_opengl_set_attribute(int attr, int value);
static void sdl12_opengl_swap_buffers(void);
static int sdl12_opengl_setup_callback(void);

static int sdl12_video_startup(void)
{
	SDL_Rect **modes;
	int32_t i, x = -1, y = -1;
	const SDL_VideoInfo* info;
	int center_enabled;

	sdl12_WM_SetCaption("Schism Tracker", "Schism Tracker");
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
			SDL_Surface *icon = sdl12_CreateRGBSurfaceFrom(pixels, width, height, 32, width * sizeof(uint32_t), 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
			if (icon) {
				sdl12_WM_SetIcon(icon, NULL);
				sdl12_FreeSurface(icon);
			}
			free(pixels);
		}
	}
#endif

	/* center the window on startup by default.
	 * this is what the SDL 2 backend does, and it's annoying to
	 * have the window pop up in the top left every time. */
	center_enabled = 0;
	if (!getenv("SDL_VIDEO_WINDOW_POS")) {
		sdl12_putenv("SDL_VIDEO_WINDOW_POS=center");
		center_enabled = 1;
	}

	video.desktop.bpp = 0;
	video.desktop.yuvformat = VIDEO_YUV_NONE;

	if (*cfg_video_format) {
		if (!strcmp(cfg_video_format, "RGB888") || !strcmp(cfg_video_format, "ARGB8888")) {
			video.desktop.bpp = 32;
		} else if (!strcmp(cfg_video_format, "RGB24")) {
			video.desktop.bpp = 24; /* ehhh */
		} else if (!strcmp(cfg_video_format, "RGB565")) {
			video.desktop.bpp = 16;
		} else if (!strcmp(cfg_video_format, "RGB555") || !strcmp(cfg_video_format, "ARGB1555")) {
			video.desktop.bpp = 15;
		} else if (!strcmp(cfg_video_format, "RGB332")) {
			video.desktop.bpp = 8;
		} else if (!strcmp(cfg_video_format, "IYUV")) {
			video.desktop.yuvformat = VIDEO_YUV_IYUV;
		} else if (!strcmp(cfg_video_format, "YV12")) {
			video.desktop.yuvformat = VIDEO_YUV_YV12;
		} else if (!strcmp(cfg_video_format, "UYVY")) {
			video.desktop.yuvformat = VIDEO_YUV_UYVY;
		} else if (!strcmp(cfg_video_format, "YVYU")) {
			video.desktop.yuvformat = VIDEO_YUV_YVYU;
		} else if (!strcmp(cfg_video_format, "YUY2")) {
			video.desktop.yuvformat = VIDEO_YUV_YUY2;
		}
	}

	/* Do this before any call to SDL_SetVideoMode!!
	 *
	 * This effectively makes our fullscreen resolution the same as
	 * the regular desktop resolution, which is the sane behavior.
	 * Especially considering we aren't doing anything very taxing
	 * graphically.
	 *
	 * If we can't, fine, we'll fall back to the old SDL_ListModes
	 * crap, and just use 640x480 (lol ok) if all else fails. */
	info = sdl12_GetVideoInfo();
	if (info) {
		x = info->current_w;
		y = info->current_h;

		/* Grab current bpp without extra hassle */
		if (info->vfmt && !video.desktop.bpp)
			video.desktop.bpp = info->vfmt->BitsPerPixel;
	}

	/* old cruft to get the "best" fullscreen resolution; only done
	 * if SDL_GetVideoInfo fails. */
	if (x < 0 || y < 0) {
		modes = sdl12_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);
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

	/* log_appendf(2, "Ideal desktop size: %dx%d", x, y); */
	video.desktop.width = x;
	video.desktop.height = y;

	video_opengl_init(sdl12_opengl_object_load,
		sdl12_opengl_function_load, NULL, NULL,
		sdl12_opengl_set_attribute, sdl12_opengl_swap_buffers);

	/* no scaling when using the SDL surfaces directly */
	video.desktop.swsurface = !cfg_video_hardware;
	video.desktop.fullscreen = cfg_video_fullscreen;

#ifdef HAVE_LINUX_FB_H
	if (!getenv("DISPLAY") && !video.desktop.fb_hacks) {
		struct fb_var_screeninfo s;

		int fb = -1;
		if (getenv("SDL_FBDEV"))
			fb = open(getenv("SDL_FBDEV"), O_RDONLY);

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
				sdl12_putenv("SDL_VIDEODRIVER=fbcon");
				video.desktop.bpp = s.bits_per_pixel;
				video.desktop.fb_hacks = 1;
				video.desktop.doublebuf = 1;
				video.desktop.fullscreen = 0;
				video.desktop.swsurface = 0;
				video.surface = sdl12_SetVideoMode(x,y,
						video.desktop.bpp,
						SDL_HWSURFACE
						| SDL_DOUBLEBUF
						| SDL_ASYNCBLIT);
			}
			close(fb);
		}
	}
#endif

	if (!video.desktop.bpp)
		video.desktop.bpp = sdl12_VideoModeOK(640, 400, 0, SDL_RESIZABLE);

	if (!video.desktop.bpp) {
		/* Ugh, just set the video mode */
		video.surface = sdl12_SetVideoMode(640, 400, 0, SDL_RESIZABLE);
		if (video.surface) {
			video.desktop.bpp = video.surface->format->BitsPerPixel;
		} /* else we will probably fail anyway... */
	}

	/* Uh oh... just set a reasonable default ??? */
	if (!video.desktop.bpp)
		video.desktop.bpp = 16;

	/* Build the surface! */
	video_fullscreen(video.desktop.fullscreen);

	if (!video.surface)
		return 0;

	/* We have to unset this variable, because otherwise
	 * SDL will re-center the window every time it's
	 * resized. */
	if (center_enabled)
		sdl12_putenv("SDL_VIDEO_WINDOW_POS=");

#ifdef SCHISM_WIN32
	/* We want to edit the window style so it accepts drag & drop */
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (sdl12_GetWMInfo(&wm_info)) {
		LONG_PTR xx = GetWindowLongPtrA(wm_info.window, GWL_EXSTYLE);
		SetWindowLongPtrA(wm_info.window, GWL_EXSTYLE, xx | WS_EX_ACCEPTFILES);
	}
#endif

	/* okay, i think we're ready */
	//sdl12_ShowCursor(SDL_DISABLE);

	return 1;
}

static SDL_Surface *setup_surface_(uint32_t w, uint32_t h, uint32_t sdlflags)
{
	if (video.desktop.doublebuf)
		sdlflags |= (SDL_DOUBLEBUF|SDL_ASYNCBLIT);

	if (video.desktop.fullscreen) {
		w = video.desktop.width;
		h = video.desktop.height;
	} else {
		sdlflags |= SDL_RESIZABLE;
	}

#ifdef HAVE_LINUX_FB_H
	if (video.desktop.fb_hacks && video.surface) {
		/* the original one will be _just fine_ */
	} else
#endif
	{
		if (video.desktop.fullscreen) {
			sdlflags &= ~(SDL_RESIZABLE);
			sdlflags |= (SDL_FULLSCREEN);
		} else {
			sdlflags &= ~(SDL_FULLSCREEN);
			sdlflags |= (SDL_RESIZABLE);
		}

		sdlflags |= (video.desktop.swsurface
				? SDL_SWSURFACE
				: SDL_HWSURFACE);

		video.surface = sdl12_SetVideoMode(w, h, video.desktop.bpp, sdlflags);
	}

	if (!video.surface)
		fprintf(stderr, "SDL_SetVideoMode: %s\n", sdl12_get_error());

	/* use the actual w/h of the surface.
	 * this fixes weird hi-dpi issues under SDL2 and SDL3,
	 * I suppose it won't hurt here. */
	video_calculate_clip(video.surface->w, video.surface->h,
		&video.clip.x, &video.clip.y, &video.clip.w, &video.clip.h);

	return video.surface;
}

static int sdl12_video_opengl_setup_callback(uint32_t *x, uint32_t *y,
	uint32_t *w, uint32_t *h)
{
	int r;

	r = !!setup_surface_(*w, *h, SDL_OPENGL);
	if (video.surface->format->BitsPerPixel < 15)
		return 0; /* automatic fail */

	*x = video.clip.x;
	*y = video.clip.y;
	*w = video.clip.w;
	*h = video.clip.h;

	return r;
}

static void sdl12_video_resize(uint32_t width, uint32_t height)
{
	if (!width) width = cfg_video_width;
	if (!height) height = cfg_video_height;
	video.draw.width = width;
	video.draw.height = height;

	if (cfg_video_hardware
		&& video_opengl_setup(width, height, sdl12_video_opengl_setup_callback)) {
		video.type = VIDEO_OPENGL;
		return;
		/* We get a nasty little black flicker here, ugh */
	} else if ((cfg_video_hardware && (cfg_video_interpolation == VIDEO_INTERPOLATION_NEAREST)) || video.desktop.yuvformat != VIDEO_YUV_NONE) {
		if (video.overlay) {
			sdl12_FreeYUVOverlay(video.overlay);
			video.overlay = NULL;
		}
		if (setup_surface_(width, height, 0)) {
			uint32_t yuvfmt = (video.desktop.yuvformat != VIDEO_YUV_NONE) 
				? video.desktop.yuvformat
				: VIDEO_YUV_IYUV;

			switch (yuvfmt) {
			case VIDEO_YUV_YV12:
				video.overlay = sdl12_CreateYUVOverlay
					(2 * NATIVE_SCREEN_WIDTH,
					 2 * NATIVE_SCREEN_HEIGHT,
					 SDL_YV12_OVERLAY, video.surface);
				break;
			case VIDEO_YUV_IYUV:
				video.overlay = sdl12_CreateYUVOverlay
					(2 * NATIVE_SCREEN_WIDTH,
					 2 * NATIVE_SCREEN_HEIGHT,
					 SDL_IYUV_OVERLAY, video.surface);
				break;
			case VIDEO_YUV_YV12_TV:
				video.overlay = sdl12_CreateYUVOverlay
					(NATIVE_SCREEN_WIDTH,
					 NATIVE_SCREEN_HEIGHT,
					 SDL_YV12_OVERLAY, video.surface);
				break;
			case VIDEO_YUV_IYUV_TV:
				video.overlay = sdl12_CreateYUVOverlay
					(NATIVE_SCREEN_WIDTH,
					 NATIVE_SCREEN_HEIGHT,
					 SDL_IYUV_OVERLAY, video.surface);
				break;
			case VIDEO_YUV_UYVY:
				video.overlay = sdl12_CreateYUVOverlay
					(2 * NATIVE_SCREEN_WIDTH,
					 NATIVE_SCREEN_HEIGHT,
					 SDL_UYVY_OVERLAY, video.surface);
				break;
			case VIDEO_YUV_YVYU:
				video.overlay = sdl12_CreateYUVOverlay
					(2 * NATIVE_SCREEN_WIDTH,
					 NATIVE_SCREEN_HEIGHT,
					 SDL_YVYU_OVERLAY, video.surface);
				break;
			case VIDEO_YUV_YUY2:
				video.overlay = sdl12_CreateYUVOverlay
					(2 * NATIVE_SCREEN_WIDTH,
					 NATIVE_SCREEN_HEIGHT,
					 SDL_YUY2_OVERLAY, video.surface);
				break;
			}

			if (video.overlay && (video.overlay->planes == 3 || video.overlay->planes == 1) &&
				 ((video.desktop.yuvformat != VIDEO_YUV_NONE) || video.overlay->hw_overlay)) {
				video.type = VIDEO_YUV;
				video_yuv_setformat(yuvfmt);
				return;
			}
		}
	}

	setup_surface_(width, height, 0);
	video.type = VIDEO_SURFACE;
}

/* for indexed color */
static void sdl8bit_pal_(unsigned int i, unsigned char rgb[3])
{
	video.pal[i] = i;

	/* copy RGB values into the color map */
	video.imap[i].r = rgb[0];
	video.imap[i].g = rgb[1];
	video.imap[i].b = rgb[2];
}

static void sdl_pal_(unsigned int i, unsigned char rgb[3])
{
	video.pal[i] = sdl12_MapRGB(video.surface->format,
			rgb[0], rgb[1], rgb[2]);
}

static void sdl12_video_colors(unsigned char palette[16][3])
{
	if (video.type == VIDEO_YUV) {
		video_colors_iterate(palette, video_yuv_pal);
	} else if (video.surface->format->BytesPerPixel == 1) {
		/* ;) */
		video_colors_iterate(palette, sdl8bit_pal_);

		sdl12_SetColors(video.surface, video.imap, 0, 256);
	} else {
		video_colors_iterate(palette, sdl_pal_);
	}
}

static uint32_t sdl12_map_rgb_callback(void *data, uint8_t r, uint8_t g, uint8_t b)
{
	SDL_PixelFormat *format = (SDL_PixelFormat *)data;

	switch (format->BytesPerPixel) {
	case 4:
		/* inline MapRGB */
		return 0xFF000000 | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
	case 3:
		/* inline MapRGB */
		return ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
	case 2:
		/* inline MapRGB if possible */
		if (format->palette) {
			/* err... */
			break;
		} else if (format->Gloss == 2) {
			/* RGB565 */
			return (((uint32_t)r << 8) & 0xF800) |
				(((uint32_t)g << 3) & 0x07E0) |
				((uint32_t)b >> 3);
		} else {
			/* RGB555 */
			return 0x8000 |
				((r << 7) & 0x7C00) |
				((g << 2) & 0x03E0) |
				(b >> 3);
		}
	}

	/* we're using palettes (really damn slow) */
	return sdl12_MapRGB(format, r, g, b);
}

SCHISM_HOT static void sdl12_video_blit(void)
{
	switch (video.type) {
	case VIDEO_OPENGL:
		/* TODO handle opengl errors; need to fallback to surface if all else fails */
		video_opengl_blit();
		break;
	case VIDEO_YUV: {
		SDL_Rect clip;

		clip.x = video.clip.x;
		clip.y = video.clip.y;
		clip.w = video.clip.w;
		clip.h = video.clip.h;

		while (sdl12_LockYUVOverlay(video.overlay) < 0)
			sdl12_Delay(10);

		sdl12_LockYUVOverlay(video.overlay);
		video_yuv_blit(video.overlay->pixels[0], video.overlay->pixels[1], video.overlay->pixels[2],
			video.overlay->pitches[0], video.overlay->pitches[1], video.overlay->pitches[2]);
		sdl12_UnlockYUVOverlay(video.overlay);
		sdl12_DisplayYUVOverlay(video.overlay, &clip);
		break;
	}
	case VIDEO_SURFACE:
		if (SDL_MUSTLOCK(video.surface))
			while (sdl12_LockSurface(video.surface) < 0)
				sdl12_Delay(10);

#ifdef HAVE_LINUX_FB_H
		if (video.desktop.fb_hacks) {
			video_blit11(video.surface->format->BytesPerPixel,
				video.surface->pixels,
				video.surface->pitch,
				video.pal);
		} else
#endif
		{
			video_blitSC(video.surface->format->BytesPerPixel,
				video.surface->pixels,
				video.surface->pitch,
				video.pal,
				sdl12_map_rgb_callback,
				video.surface->format,
				video.clip.x,
				video.clip.y,
				video.clip.w,
				video.clip.h);
		}

		if (SDL_MUSTLOCK(video.surface))
			sdl12_UnlockSurface(video.surface);

		sdl12_Flip(video.surface);
		break;
	}
}

static void sdl12_video_translate(uint32_t vx, uint32_t vy, uint32_t *x, uint32_t *y)
{
	/* callback~! */

	video_translate_calculate(vx, vy,
		/* clip rect */
		video.clip.x, video.clip.y, video.clip.w, video.clip.h,
		/* return pointers */
		x, y);
}

static int sdl12_video_is_hardware(void)
{
	if (video.type == VIDEO_YUV && video.overlay)
		return video.overlay->hw_overlay;

	return !!(video.surface->flags & (SDL_HWSURFACE|SDL_OPENGL));
}

static int sdl12_video_is_input_grabbed(void)
{
	return sdl12_WM_GrabInput(SDL_GRAB_QUERY) == SDL_GRAB_ON;
}

static void sdl12_video_set_input_grabbed(SCHISM_UNUSED int enabled)
{
	sdl12_WM_GrabInput(enabled ? SDL_GRAB_ON : SDL_GRAB_OFF);
}

static int sdl12_video_is_focused(void)
{
	return (sdl12_GetAppState() & (SDL_APPMOUSEFOCUS | SDL_APPINPUTFOCUS));
}

static int sdl12_video_is_visible(void)
{
	return (sdl12_GetAppState() & (SDL_APPACTIVE));
}

static int sdl12_video_is_screensaver_enabled(void)
{
	// assume its enabled
	return 1;
}

static void sdl12_video_toggle_screensaver(SCHISM_UNUSED int enabled)
{
	/* SDL 1.2 doesn't provide this */
}

static int sdl12_video_is_wm_available(void)
{
	return (sdl12_GetVideoInfo()->wm_available);
}

static void sdl12_video_warp_mouse(uint32_t x, uint32_t y)
{
	sdl12_WarpMouse(x, y);
}

static void sdl12_video_set_hardware(int hardware)
{
	cfg_video_hardware = !!hardware;
	// recreate the surface with the same size...
	video_resize(video.draw.width, video.draw.height);
	video_report();
}

static int sdl12_video_have_menu(void)
{
#ifdef SCHISM_WIN32
	return 1;
#else
	return 0;
#endif
}

static void sdl12_video_toggle_menu(SCHISM_UNUSED int on)
{
	/* ... */
	if (!video_have_menu())
		return;

	int width, height;

	int cache_size = 0;

#ifdef SCHISM_WIN32
	/* Get the HWND */
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (!sdl12_GetWMInfo(&wm_info))
		return;

	WINDOWPLACEMENT placement;
	placement.length = sizeof(placement);

	GetWindowPlacement(wm_info.window, &placement);

	cache_size = (placement.showCmd == SW_MAXIMIZE);
#endif
	if (cache_size) {
		width = video.draw.width;
		height = video.draw.height;
	}
#ifdef SCHISM_WIN32
	win32_toggle_menu(wm_info.window, on);
#endif

#ifdef SCHISM_WIN32
	/* weird magic that makes menu bars work right when
	 * using OpenGL. Tested this with messing around
	 * a lot between fullscreen and menu bar and
	 * it seems to work right */
	if ((video.type == VIDEO_OPENGL)
			&& !(video.surface->flags & SDL_FULLSCREEN)
			&& !cache_size) {
		int h;
		RECT w, c;

		GetWindowRect(wm_info.window, &w);
		GetClientRect(wm_info.window, &c);

		h = GetSystemMetrics(SM_CYMENU);
		if (!cfg_video_want_menu_bar)
			h = -h;

		SetWindowPos(wm_info.window, NULL, 0, 0, w.right - w.left, (w.bottom - w.top) + h, SWP_NOMOVE | SWP_NOZORDER);
	}
#endif

	if (cache_size) {
		video_resize(width, height);
	}
}

static void sdl12_video_mousecursor_changed(void)
{
	const int vis = video_mousecursor_visible();
	int evstate;

	/* FIXME: On Windows, this only seems to work after releasing
	 * mouse focus, and then giving back mouse focus. */
	sdl12_ShowCursor((vis == MOUSE_SYSTEM) ? SDL_ENABLE : SDL_DISABLE);

	// Totally turn off mouse event sending when the mouse is disabled
	evstate = (vis == MOUSE_DISABLED) ? SDL_DISABLE : SDL_ENABLE;
	if (evstate != sdl12_EventState(SDL_MOUSEMOTION, SDL_QUERY)) {
		sdl12_EventState(SDL_MOUSEMOTION, evstate);
		sdl12_EventState(SDL_MOUSEBUTTONDOWN, evstate);
		sdl12_EventState(SDL_MOUSEBUTTONUP, evstate);
	}
}

/* ------------------------------------------------------------------------ */

static int (SDLCALL *sdl12_GL_LoadLibrary)(const char *path) = NULL;
static int (SDLCALL *sdl12_GL_SetAttribute)(SDL_GLattr attr, int value) = NULL;
static void *(SDLCALL *sdl12_GL_GetProcAddress)(const char* proc) = NULL;
static void (SDLCALL *sdl12_GL_SwapBuffers)(void) = NULL;

static int sdl12_opengl_object_load(const char *path)
{
	return !sdl12_GL_LoadLibrary(path);
}

static void *sdl12_opengl_function_load(const char *function)
{
	return sdl12_GL_GetProcAddress(function);
}

static int sdl12_opengl_set_attribute(int attr, int value)
{
	SDL_GLattr sdlattr;

	switch (attr) {
	/*
	case VIDEO_GL_RED_SIZE: sdlattr = SDL_GL_RED_SIZE; break;
	case VIDEO_GL_GREEN_SIZE: sdlattr = SDL_GL_GREEN_SIZE; break;
	case VIDEO_GL_BLUE_SIZE: sdlattr = SDL_GL_BLUE_SIZE; break;
	case VIDEO_GL_ALPHA_SIZE: sdlattr = SDL_GL_ALPHA_SIZE; break;
	case VIDEO_GL_BUFFER_SIZE: sdlattr = SDL_GL_BUFFER_SIZE; break;
	*/
	case VIDEO_GL_DOUBLEBUFFER: sdlattr = SDL_GL_DOUBLEBUFFER; break;
	case VIDEO_GL_DEPTH_SIZE: sdlattr = SDL_GL_DEPTH_SIZE; break;
	case VIDEO_GL_STENCIL_SIZE: sdlattr = SDL_GL_STENCIL_SIZE; break;
	case VIDEO_GL_ACCUM_RED_SIZE: sdlattr = SDL_GL_ACCUM_RED_SIZE; break;
	case VIDEO_GL_ACCUM_GREEN_SIZE: sdlattr = SDL_GL_ACCUM_GREEN_SIZE; break;
	case VIDEO_GL_ACCUM_BLUE_SIZE: sdlattr = SDL_GL_ACCUM_BLUE_SIZE; break;
	case VIDEO_GL_ACCUM_ALPHA_SIZE: sdlattr = SDL_GL_ACCUM_ALPHA_SIZE; break;
#if SDL_VERSION_ATLEAST(1, 2, 11)
	case VIDEO_GL_SWAP_CONTROL: sdlattr = SDL_GL_SWAP_CONTROL; break;
#endif
	default: return -1;
	}

	return sdl12_GL_SetAttribute(sdlattr, value);
}

static void sdl12_opengl_swap_buffers(void)
{
	sdl12_GL_SwapBuffers();
}

/* ------------------------------------------------------------------------ */

static int sdl12_video_get_wm_data(video_wm_data_t *wm_data)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	if (!sdl12_GetWMInfo(&info))
		return 0;

#ifdef SCHISM_WIN32
	wm_data->subsystem = VIDEO_WM_DATA_SUBSYSTEM_WINDOWS;
	wm_data->data.windows.hwnd = info.window;
	// don't care about other values for now
#else
# ifdef SDL_VIDEO_DRIVER_X11
	if (info.subsystem == SDL_SYSWM_X11) {
		wm_data->subsystem = VIDEO_WM_DATA_SUBSYSTEM_X11;
		wm_data->data.x11.display = info.info.x11.display;
		wm_data->data.x11.window = info.info.x11.window;
		wm_data->data.x11.lock_func = info.info.x11.lock_func;
		wm_data->data.x11.unlock_func = info.info.x11.unlock_func;
	}
# endif
#endif

	return 1;
}

/* ------------------------------------------------------------------------ */

static void sdl12_video_show_cursor(int enabled)
{
	sdl12_ShowCursor(enabled ? SDL_ENABLE : SDL_DISABLE);
}

/* ------------------------------------------------------------------------ */

static int sdl12_video_load_syms(void)
{
	SCHISM_SDL12_SYM(InitSubSystem);
	SCHISM_SDL12_SYM(QuitSubSystem);

	SCHISM_SDL12_SYM(VideoDriverName);
	SCHISM_SDL12_SYM(GetVideoInfo);
	SCHISM_SDL12_SYM(WM_SetCaption);
	SCHISM_SDL12_SYM(SetVideoMode);
	SCHISM_SDL12_SYM(ListModes);
	SCHISM_SDL12_SYM(ShowCursor);
	SCHISM_SDL12_SYM(MapRGB);
	SCHISM_SDL12_SYM(SetColors);
	SCHISM_SDL12_SYM(LockSurface);
	SCHISM_SDL12_SYM(UnlockSurface);
	SCHISM_SDL12_SYM(Delay);
	SCHISM_SDL12_SYM(Flip);
	SCHISM_SDL12_SYM(WM_GrabInput);
	SCHISM_SDL12_SYM(WM_SetIcon);
	SCHISM_SDL12_SYM(GetAppState);
	SCHISM_SDL12_SYM(WarpMouse);
	SCHISM_SDL12_SYM(EventState);
	SCHISM_SDL12_SYM(FreeSurface);
	SCHISM_SDL12_SYM(CreateRGBSurfaceFrom);
	SCHISM_SDL12_SYM(EnableUNICODE);
	SCHISM_SDL12_SYM(GetWMInfo);
#ifdef SCHISM_MACOS
	SCHISM_SDL12_SYM(InitQuickDraw);
#endif

	SCHISM_SDL12_SYM(GL_LoadLibrary);
	SCHISM_SDL12_SYM(GL_SetAttribute);
	SCHISM_SDL12_SYM(GL_GetProcAddress);
	SCHISM_SDL12_SYM(GL_SwapBuffers);

	SCHISM_SDL12_SYM(VideoModeOK);

	SCHISM_SDL12_SYM(CreateYUVOverlay);
	SCHISM_SDL12_SYM(FreeYUVOverlay);
	SCHISM_SDL12_SYM(DisplayYUVOverlay);
	SCHISM_SDL12_SYM(LockYUVOverlay);
	SCHISM_SDL12_SYM(UnlockYUVOverlay);

	return 0;
}

static int sdl12_video_init(void)
{
	if (!sdl12_init())
		return 0;

	if (sdl12_video_load_syms()) {
		sdl12_quit();
		return 0;
	}

	if (sdl12_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		sdl12_quit();
		return 0;
	}

#ifdef SCHISM_MACOS
	sdl12_InitQuickDraw(&qd);
#endif

	sdl12_EnableUNICODE(1);

	if (!events_init(&schism_events_backend_sdl12)) {
		sdl12_QuitSubSystem(SDL_INIT_VIDEO);
		sdl12_quit();
		return 0;
	}

	return 1;
}

static void sdl12_video_quit(void)
{
	video_opengl_quit();

	sdl12_QuitSubSystem(SDL_INIT_VIDEO);

	sdl12_quit();
}

/* ------------------------------------------------------------------------ */

const schism_video_backend_t schism_video_backend_sdl12 = {
	.init = sdl12_video_init,
	.quit = sdl12_video_quit,

	.startup = sdl12_video_startup,
	.shutdown = sdl12_video_shutdown,

	.is_fullscreen = sdl12_video_is_fullscreen,
	.width = sdl12_video_width,
	.height = sdl12_video_height,
	.driver_name = sdl12_video_driver_name,
	.report = sdl12_video_report,
	.set_hardware = sdl12_video_set_hardware,
	.setup = sdl12_video_setup,
	.fullscreen = sdl12_video_fullscreen,
	.resize = sdl12_video_resize,
	.colors = sdl12_video_colors,
	.is_focused = sdl12_video_is_focused,
	.is_visible = sdl12_video_is_visible,
	.is_wm_available = sdl12_video_is_wm_available,
	.is_hardware = sdl12_video_is_hardware,
	.is_screensaver_enabled = sdl12_video_is_screensaver_enabled,
	.toggle_screensaver = sdl12_video_toggle_screensaver,
	.translate = sdl12_video_translate,
	.is_input_grabbed = sdl12_video_is_input_grabbed,
	.set_input_grabbed = sdl12_video_set_input_grabbed,
	.warp_mouse = sdl12_video_warp_mouse,
	.have_menu = sdl12_video_have_menu,
	.toggle_menu = sdl12_video_toggle_menu,
	.blit = sdl12_video_blit,
	.mousecursor_changed = sdl12_video_mousecursor_changed,
	.get_wm_data = sdl12_video_get_wm_data,
	.show_cursor = sdl12_video_show_cursor,
};
