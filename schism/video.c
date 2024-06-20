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
#define NATIVE_SCREEN_WIDTH		640
#define NATIVE_SCREEN_HEIGHT	400
#define WINDOW_TITLE			"Schism Tracker"

#include "headers.h"
#include "it.h"
#include "sdlmain.h"
#include "video.h"
#include "osdefs.h"

/* for memcpy */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#ifndef SCHISM_MACOSX
#ifdef SCHISM_WIN32
#include "auto/schismico.h"
#else
#include "auto/schismico_hires.h"
#endif
#endif

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
	unsigned char *framebuf;

	int width, height;

	struct {
		unsigned int x, y;
		int visible;
	} mouse;

#ifdef SCHISM_WIN32
	struct {
		/* TODO: need to save the state of the menu bar or else
		 * these will be wrong if it's toggled while in fullscreen */
		int width, height;

		int x, y;
	} saved;
#endif

	int fullscreen;

	unsigned int pal[256];

	unsigned int tc_bgr32[256];
};

/* don't stomp defaults */
static struct video_cf video = {
	.mouse = {
		.visible = MOUSE_EMULATED
	}
};

int video_is_fullscreen(void)
{
	return video.fullscreen;
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
	SDL_GetWindowSize(video.window, &video.width, &video.height);
}

const char * video_driver_name(void)
{
	return SDL_GetCurrentVideoDriver();
}

void video_report(void)
{
	SDL_DisplayMode display = {0};

	Uint32 format;
	SDL_QueryTexture(video.texture, &format, NULL, NULL, NULL);

	log_appendf(5, " Using driver '%s'", SDL_GetCurrentVideoDriver());

	log_appendf(5, " Display format: %d bits/pixel", SDL_BITSPERPIXEL(format));

	if (!SDL_GetCurrentDisplayMode(0, &display) && video.fullscreen)
		log_appendf(5, " Display dimensions: %dx%d", display.w, display.h);
}

void video_redraw_texture(void)
{
	SDL_DestroyTexture(video.texture);
	video.texture = SDL_CreateTexture(video.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
}

void video_shutdown(void)
{
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
	/* FIXME: is this really necessary? */
	SDL_SetWindowTitle(video.window, WINDOW_TITLE);
#ifndef SCHISM_MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
#ifdef SCHISM_WIN32
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
	/* positive new_fs_flag == set, negative == toggle */
	video.fullscreen = (new_fs_flag >= 0) ? !!new_fs_flag : !video.fullscreen;

	if (video.fullscreen) {
#ifdef SCHISM_WIN32
		SDL_GetWindowSize(video.window, &video.saved.width, &video.saved.height);
		SDL_GetWindowPosition(video.window, &video.saved.x, &video.saved.y);
#endif
		SDL_SetWindowFullscreen(video.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
#ifdef SCHISM_WIN32
		win32_toggle_menu(video.window);
#endif
	} else {
		SDL_SetWindowFullscreen(video.window, 0);
#ifdef SCHISM_WIN32
		/* the menu must be toggled first here */
		win32_toggle_menu(video.window);
		SDL_SetWindowSize(video.window, video.saved.width, video.saved.height);
		SDL_SetWindowPosition(video.window, video.saved.x, video.saved.y);
#endif
		set_icon(); /* XXX is this necessary */
	}
}

void video_startup(void)
{
	vgamem_clear();
	vgamem_flip();

	video_setup(cfg_video_interpolation);

#ifndef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
/* older SDL2 versions don't define this, don't fail the build for it */
#define SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR "SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR"
#endif
	SDL_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

	video.width = cfg_video_width;
	video.height = cfg_video_height;
#ifdef SCHISM_WIN32
	video.saved.x = video.saved.y = SDL_WINDOWPOS_CENTERED;
#endif

	video.window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, video.width, video.height, SDL_WINDOW_RESIZABLE);
	video.renderer = SDL_CreateRenderer(video.window, -1, 0);
	video.texture = SDL_CreateTexture(video.renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
	video.framebuf = calloc(NATIVE_SCREEN_WIDTH * NATIVE_SCREEN_HEIGHT, sizeof(uint32_t));

	/* Aspect ratio correction if it's wanted */
	if (cfg_video_want_fixed)
		SDL_RenderSetLogicalSize(video.renderer, cfg_video_want_fixed_width, cfg_video_want_fixed_height);

	video_fullscreen(cfg_video_fullscreen);
#ifdef SCHISM_WIN32
	if (!video.fullscreen) {
		SDL_SetWindowSize(video.window, video.width, video.height);
		SDL_SetWindowPosition(video.window, video.saved.x, video.saved.y);
	}
#endif

	/* okay, i think we're ready */
	SDL_ShowCursor(SDL_DISABLE);
	set_icon();
}

void video_resize(unsigned int width, unsigned int height)
{
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
	const int lastmap[] = { 0,1,2,3,5 };
	int rgb[3], i, j, p;

	/* make our "base" space */
	for (i = 0; i < 16; i++) {
		rgb[0]=palette[i][0];
		rgb[1]=palette[i][1];
		rgb[2]=palette[i][2];
		_gl_pal(i, rgb);
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
		_gl_pal(i, rgb);
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
	unsigned int z;

	memset(mouseline, 0, 80*sizeof(unsigned int));
	if (video.mouse.visible != MOUSE_EMULATED
		|| !(status.flags & IS_FOCUSED)
		|| y < video.mouse.y
		|| y >= video.mouse.y+MOUSE_HEIGHT) {
		return;
	}

	z = _mouse_pointer[ y - video.mouse.y ];
	mouseline[x] = z >> v;
	if (x < 79) mouseline[x+1] = (z << (8-v)) & 0xff;
}

static void _blit11(unsigned char *pixels, unsigned int pitch, unsigned int *tpal)
{
	unsigned int mouseline_x = (video.mouse.x / 8);
	unsigned int mouseline_v = (video.mouse.x % 8);
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
	SDL_Rect dstrect = {
		.x = 0,
		.y = 0,
		.w = cfg_video_want_fixed_width,
		.h = cfg_video_want_fixed_height
	};

	unsigned char *pixels = video.framebuf;
	unsigned int pitch = NATIVE_SCREEN_WIDTH * sizeof(Uint32);

	_blit11(pixels, pitch, video.pal);

	SDL_RenderClear(video.renderer);
	SDL_UpdateTexture(video.texture, NULL, pixels, pitch);
	SDL_RenderCopy(video.renderer, video.texture, NULL, (cfg_video_want_fixed) ? &dstrect : NULL);
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

void video_translate(int vx, int vy, unsigned int *x, unsigned int *y)
{
	if (video.mouse.visible && (video.mouse.x != vx || video.mouse.y != vy))
		status.flags |= SOFTWARE_MOUSE_MOVED;

	vx *= NATIVE_SCREEN_WIDTH;
	vy *= NATIVE_SCREEN_HEIGHT;
	vx /= (cfg_video_want_fixed) ? cfg_video_want_fixed_width  : video.width;
	vy /= (cfg_video_want_fixed) ? cfg_video_want_fixed_height : video.height;

	vx = CLAMP(vx, 0, NATIVE_SCREEN_WIDTH - 1);
	vy = CLAMP(vy, 0, NATIVE_SCREEN_HEIGHT - 1);

	*x = video.mouse.x = vx;
	*y = video.mouse.y = vy;
}

void video_get_logical_coordinates(int x, int y, int *trans_x, int *trans_y)
{
	if (!cfg_video_want_fixed) {
		*trans_x = x;
		*trans_y = y;
	} else {
		float xx, yy;
#if SDL_VERSION_ATLEAST(2, 0, 18)
		SDL_RenderWindowToLogical(video.renderer, x, y, &xx, &yy);
#else
		/* Alternative for older SDL versions. MIGHT work with high DPI */
		float scale_x = 1, scale_y = 1;

		SDL_RenderGetScale(video.renderer, &scale_x, &scale_y);

		xx = x - (video.width / 2) - (((float)cfg_video_want_fixed_width * scale_x) / 2);
		yy = y - (video.height / 2) - (((float)cfg_video_want_fixed_height * scale_y) / 2);

		xx /= (float)video.width * cfg_video_want_fixed_width;
		yy /= (float)video.height * cfg_video_want_fixed_height;
#endif
		*trans_x = (int)xx;
		*trans_y = (int)yy;
	}
}

SDL_Window * video_window(void)
{
	return video.window;
}
