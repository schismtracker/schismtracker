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
#include "charset.h"
#include "bswap.h"
#include "config.h"
#include "sdlmain.h"
#include "video.h"
#include "osdefs.h"
#include "vgamem.h"

#include <errno.h>

#include <inttypes.h>

#ifndef SCHISM_MACOSX
#include "auto/schismico_hires.h"
#endif

/* leeto drawing skills */
struct mouse_cursor {
	uint32_t pointer[18];
	uint32_t mask[18];
	uint32_t height, width;
	uint32_t center_x, center_y; /* which point of the pointer does actually point */
};

#if !SDL_VERSION_ATLEAST(2, 0, 4)
#define SDL_PIXELFORMAT_NV12 (SDL_DEFINE_PIXELFOURCC('N', 'V', '1', '2'))
#define SDL_PIXELFORMAT_NV21 (SDL_DEFINE_PIXELFOURCC('N', 'V', '2', '1'))
#endif

/* ex. cursors[CURSOR_SHAPE_ARROW] */
static struct mouse_cursor cursors[] = {
	[CURSOR_SHAPE_ARROW] = {
		.pointer = { /* / -|-------------> */
			0x0000,  /* | ................ */
			0x4000,  /* - .x.............. */
			0x6000,  /* | .xx............. */
			0x7000,  /* | .xxx............ */
			0x7800,  /* | .xxxx........... */
			0x7c00,  /* | .xxxxx.......... */
			0x7e00,  /* | .xxxxxx......... */
			0x7f00,  /* | .xxxxxxx........ */
			0x7f80,  /* | .xxxxxxxx....... */
			0x7f00,  /* | .xxxxxxx........ */
			0x7c00,  /* | .xxxxx.......... */
			0x4600,  /* | .x...xx......... */
			0x0600,  /* | .....xx......... */
			0x0300,  /* | ......xx........ */
			0x0300,  /* | ......xx........ */
			0x0000,  /* v ................ */
			0,0
		},
		.mask = {    /* / -|-------------> */
			0xc000,  /* | xx.............. */
			0xe000,  /* - xxx............. */
			0xf000,  /* | xxxx............ */
			0xf800,  /* | xxxxx........... */
			0xfc00,  /* | xxxxxx.......... */
			0xfe00,  /* | xxxxxxx......... */
			0xff00,  /* | xxxxxxxx........ */
			0xff80,  /* | xxxxxxxxx....... */
			0xffc0,  /* | xxxxxxxxxx...... */
			0xff80,  /* | xxxxxxxxx....... */
			0xfe00,  /* | xxxxxxx......... */
			0xff00,  /* | xxxxxxxx........ */
			0x4f00,  /* | .x..xxxx........ */
			0x0780,  /* | .....xxxx....... */
			0x0780,  /* | .....xxxx....... */
			0x0300,  /* v ......xx........ */
			0,0
		},
		.height = 16,
		.width = 10,
		.center_x = 1,
		.center_y = 1,
	},
	[CURSOR_SHAPE_CROSSHAIR] = {
		.pointer = {  /* / ---|---> */
			0x00,     /* | ........ */
			0x10,     /* | ...x.... */
			0x7c,     /* - .xxxxx.. */
			0x10,     /* | ...x.... */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* v ........ */
			0,0
		},
		.mask = {     /* / ---|---> */
			0x10,     /* | ...x.... */
			0x7c,     /* | .xxxxx.. */
			0xfe,     /* - xxxxxxx. */
			0x7c,     /* | .xxxxx.. */
			0x10,     /* | ...x.... */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* | ........ */
			0x00,     /* v ........ */
			0,0
		},
		.height = 5,
		.width = 7,
		.center_x = 3,
		.center_y = 2,
	},
};

static struct {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;
	SDL_PixelFormat *pixel_format; // may be NULL
	uint32_t format;
	uint32_t bpp; // BYTES per pixel

	int width, height;

	struct {
		unsigned int x, y;
	} mouse;

	struct {
		/* TODO: need to save the state of the menu bar or else
		 * these will be wrong if it's toggled while in fullscreen */
		int width, height;

		int x, y;
	} saved;

	int fullscreen;

	struct {
		uint32_t pal_y[256];
		uint32_t pal_u[256];
		uint32_t pal_v[256];
	} yuv;

	uint32_t pal[256];
} video = {0};

// Native formats, in order of preference.
static const struct {
	uint32_t format;
	const char *name;
} native_formats[] = {
	// RGB
	// ----------------
	{SDL_PIXELFORMAT_RGB888, "RGB888"},
	{SDL_PIXELFORMAT_ARGB8888, "ARGB8888"},
	// {SDL_PIXELFORMAT_RGB24, "RGB24"},
	{SDL_PIXELFORMAT_RGB565, "RGB565"},
	{SDL_PIXELFORMAT_RGB555, "RGB555"},
	{SDL_PIXELFORMAT_ARGB1555, "ARGB1555"},
	{SDL_PIXELFORMAT_RGB444, "RGB444"},
	{SDL_PIXELFORMAT_ARGB4444, "ARGB4444"},
	{SDL_PIXELFORMAT_RGB332, "RGB332"},
	// ----------------

	// YUV
	// ----------------
	{SDL_PIXELFORMAT_IYUV, "IYUV"},
	{SDL_PIXELFORMAT_YV12, "YV12"},
	// {SDL_PIXELFORMAT_UYVY, "UYVY"},
	// {SDL_PIXELFORMAT_YVYU, "YVYU"},
	// {SDL_PIXELFORMAT_YUY2, "YUY2"},
	// {SDL_PIXELFORMAT_NV12, "NV12"},
	// {SDL_PIXELFORMAT_NV21, "NV21"},
	// ----------------
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

const char *video_driver_name(void)
{
	return SDL_GetCurrentVideoDriver();
}

void video_report(void)
{
	struct {
		uint32_t num;
		const char *name, *type;
	} yuv_layouts[] = {
		{SDL_PIXELFORMAT_YV12, "YV12", "planar+tv"},
		{SDL_PIXELFORMAT_IYUV, "IYUV", "planar+tv"},
		{SDL_PIXELFORMAT_YVYU, "YVYU", "packed"},
		{SDL_PIXELFORMAT_UYVY, "UYVY", "packed"},
		{SDL_PIXELFORMAT_YUY2, "YUY2", "packed"},
		{SDL_PIXELFORMAT_NV12, "NV12", "planar"},
		{SDL_PIXELFORMAT_NV21, "NV21", "planar"},
		{0, NULL, NULL},
	}, *layout = yuv_layouts;

	log_append(2, 0, "Video initialised");
	log_underline(17);

	{
		SDL_RendererInfo renderer;
		SDL_GetRendererInfo(video.renderer, &renderer);
		log_appendf(5, " Using driver '%s'", SDL_GetCurrentVideoDriver());
		log_appendf(5, " %sware%s renderer '%s'",
			(renderer.flags & SDL_RENDERER_SOFTWARE) ? "Soft" : "Hard",
			(renderer.flags & SDL_RENDERER_ACCELERATED) ? "-accelerated" : "",
			renderer.name);
	}

	switch (video.format) {
	case SDL_PIXELFORMAT_IYUV:
	case SDL_PIXELFORMAT_YV12:
	case SDL_PIXELFORMAT_YVYU:
	case SDL_PIXELFORMAT_UYVY:
	case SDL_PIXELFORMAT_YUY2:
	case SDL_PIXELFORMAT_NV12:
	case SDL_PIXELFORMAT_NV21:
		while (video.format != layout->num && layout->name != NULL)
			layout++;

		if (layout->name)
			log_appendf(5, " Display format: %s (%s)", layout->name, layout->type);
		else
			log_appendf(5, " Display format: %" PRIx32, video.format);
		break;
	default:
		log_appendf(5, " Display format: %"PRIu32" bits/pixel", SDL_BITSPERPIXEL(video.format));
		break;
	}

	{
		SDL_DisplayMode display;
		if (!SDL_GetCurrentDisplayMode(0, &display) && video.fullscreen)
			log_appendf(5, " Display dimensions: %dx%d", display.w, display.h);
	}

	log_nl();
}

static void set_icon(void)
{
	SDL_SetWindowTitle(video.window, WINDOW_TITLE);
#ifndef SCHISM_MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
	SDL_Surface *icon = xpmdata(_schism_icon_xpm_hires);
	SDL_SetWindowIcon(video.window, icon);
	SDL_FreeSurface(icon);
#endif
}

static void video_redraw_texture(void)
{
	int i, j, pref_last = ARRAY_SIZE(native_formats);
	uint32_t format = SDL_PIXELFORMAT_RGB888;

	if (video.texture)
		SDL_DestroyTexture(video.texture);

	if (video.pixel_format)
		SDL_FreeFormat(video.pixel_format);

	if (*cfg_video_format) {
		for (i = 0; i < ARRAY_SIZE(native_formats); i++) {
			if (!charset_strcasecmp(cfg_video_format, CHARSET_UTF8, native_formats[i].name, CHARSET_UTF8)) {
				format = native_formats[i].format;
				goto got_format;
			}
		}
	}

	// We want to find the best format we can natively
	// output to. If we can't, then we fall back to
	// SDL_PIXELFORMAT_RGB888 and let SDL deal with the
	// conversion.
	SDL_RendererInfo info;
	SDL_GetRendererInfo(video.renderer, &info);
	for (i = 0; i < info.num_texture_formats; i++)
		for (j = 0; j < ARRAY_SIZE(native_formats); j++)
			if (info.texture_formats[i] == native_formats[j].format && j < pref_last)
				format = native_formats[pref_last = j].format;

got_format:
	video.texture = SDL_CreateTexture(video.renderer, format, SDL_TEXTUREACCESS_STREAMING, NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
	video.pixel_format = SDL_AllocFormat(format);
	video.format = format;

	// find the bytes per pixel
	switch (video.format) {
	// irrelevant
	case SDL_PIXELFORMAT_YV12:
	case SDL_PIXELFORMAT_IYUV: break;

	default: video.bpp = video.pixel_format->BytesPerPixel; break;
	}
}

void video_set_hardware(int hardware)
{
	SDL_DestroyTexture(video.texture);

	SDL_DestroyRenderer(video.renderer);

	video.renderer = SDL_CreateRenderer(video.window, -1, hardware ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);
	if (!video.renderer)
		video.renderer = SDL_CreateRenderer(video.window, -1, 0); // welp

	video_redraw_texture();

	video_report();
}

void video_shutdown(void)
{
	SDL_DestroyTexture(video.texture);
	SDL_DestroyRenderer(video.renderer);
	SDL_DestroyWindow(video.window);
}

void video_setup(const char *quality)
{
	strncpy(cfg_video_interpolation, quality, 7);
	SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, quality);
	video_redraw_texture();
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
	video.saved.x = video.saved.y = SDL_WINDOWPOS_CENTERED;

	video.window = SDL_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, video.width, video.height, SDL_WINDOW_RESIZABLE);
	video_set_hardware(cfg_video_hardware);

	/* Aspect ratio correction if it's wanted */
	if (cfg_video_want_fixed)
		SDL_RenderSetLogicalSize(video.renderer, cfg_video_want_fixed_width, cfg_video_want_fixed_height);

	video_fullscreen(cfg_video_fullscreen);
	if (video_have_menu() && !video.fullscreen) {
		SDL_SetWindowSize(video.window, video.width, video.height);
		SDL_SetWindowPosition(video.window, video.saved.x, video.saved.y);
	}

	/* okay, i think we're ready */
	SDL_ShowCursor(SDL_DISABLE);
	set_icon();
}

void video_fullscreen(int new_fs_flag)
{
	const int have_menu = video_have_menu();
	/* positive new_fs_flag == set, negative == toggle */
	video.fullscreen = (new_fs_flag >= 0) ? !!new_fs_flag : !video.fullscreen;

	if (video.fullscreen) {
		if (have_menu) {
			SDL_GetWindowSize(video.window, &video.saved.width, &video.saved.height);
			SDL_GetWindowPosition(video.window, &video.saved.x, &video.saved.y);
		}
		SDL_SetWindowFullscreen(video.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		if (have_menu)
			video_toggle_menu(0);
	} else {
		SDL_SetWindowFullscreen(video.window, 0);
		if (have_menu) {
			/* the menu must be toggled first here */
			video_toggle_menu(1);
			SDL_SetWindowSize(video.window, video.saved.width, video.saved.height);
			SDL_SetWindowPosition(video.window, video.saved.x, video.saved.y);
		}
		set_icon(); /* XXX is this necessary */
	}
}

void video_resize(unsigned int width, unsigned int height)
{
	video.width = width;
	video.height = height;
	status.flags |= (NEED_UPDATE);
}

static void yuv_pal_(int i, unsigned char rgb[3])
{
	unsigned int y, u, v;
	video_rgb_to_yuv(&y, &u, &v, rgb);

	switch (video.format) {
	case SDL_PIXELFORMAT_IYUV:
	case SDL_PIXELFORMAT_YV12:
		video.yuv.pal_y[i] = y;
		video.yuv.pal_u[i] = (u >> 4) & 0xF;
		video.yuv.pal_v[i] = (v >> 4) & 0xF;
		break;
	default:
		break; // err
	}
}

static void sdl_pal_(int i, unsigned char rgb[3])
{
	video.pal[i] = SDL_MapRGB(video.pixel_format, rgb[0], rgb[1], rgb[2]);
}

void video_colors(unsigned char palette[16][3])
{
	static const int lastmap[] = { 0, 1, 2, 3, 5 };
	int i, p;
	void (*fun)(int i, unsigned char rgb[3]);

	switch (video.format) {
	case SDL_PIXELFORMAT_IYUV:
	case SDL_PIXELFORMAT_YV12:
		fun = yuv_pal_;
		break;
	default:
		fun = sdl_pal_;
		break;
	}

	/* make our "base" space */
	for (i = 0; i < 16; i++)
		fun(i, (unsigned char []){palette[i][0], palette[i][1], palette[i][2]});

	/* make our "gradient" space */
	for (i = 0; i < 128; i++) {
		p = lastmap[(i>>5)];

		fun(i + 128, (unsigned char []){
			(int)palette[p][0] + (((int)(palette[p+1][0] - palette[p][0]) * (i & 0x1F)) / 0x20),
			(int)palette[p][1] + (((int)(palette[p+1][1] - palette[p][1]) * (i & 0x1F)) / 0x20),
			(int)palette[p][2] + (((int)(palette[p+1][2] - palette[p][2]) * (i & 0x1F)) / 0x20),
		});
	}
}

int video_is_focused(void)
{
	return !!(SDL_GetWindowFlags(video.window) & SDL_WINDOW_INPUT_FOCUS);
}

int video_is_visible(void)
{
	return !!(SDL_GetWindowFlags(video.window) & SDL_WINDOW_SHOWN);
}

int video_is_wm_available(void)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	return !!SDL_GetWindowWMInfo(video.window, &info);
}

int video_is_hardware(void)
{
	SDL_RendererInfo info;
	SDL_GetRendererInfo(video.renderer, &info);
	return !!(info.flags & SDL_RENDERER_ACCELERATED);
}

/* -------------------------------------------------------- */
/* mousecursor */

void video_show_system_cursor(int show)
{
	SDL_ShowCursor(show);
}

void video_toggle_mouse_events(int enabled)
{
	int evstate = enabled ? SDL_ENABLE : SDL_DISABLE;
	if (evstate != SDL_EventState(SDL_MOUSEMOTION, SDL_QUERY)) {
		SDL_EventState(SDL_MOUSEMOTION, evstate);
		SDL_EventState(SDL_MOUSEBUTTONDOWN, evstate);
		SDL_EventState(SDL_MOUSEBUTTONUP, evstate);
	}
}

/* -------------------------------------------------------- */

int video_is_screensaver_enabled(void)
{
	return SDL_IsScreenSaverEnabled();
}

void video_toggle_screensaver(int enabled)
{
	if (enabled) SDL_EnableScreenSaver();
	else SDL_DisableScreenSaver();
}

/* ---------------------------------------------------------- */
/* coordinate translation */

void video_translate(int vx, int vy, unsigned int *x, unsigned int *y)
{
	if (video_mousecursor_visible() && (video.mouse.x != vx || video.mouse.y != vy))
		status.flags |= SOFTWARE_MOUSE_MOVED;

	vx *= NATIVE_SCREEN_WIDTH;
	vy *= NATIVE_SCREEN_HEIGHT;
	vx /= (cfg_video_want_fixed) ? cfg_video_want_fixed_width  : video.width;
	vy /= (cfg_video_want_fixed) ? cfg_video_want_fixed_height : video.height;

	vx = CLAMP(vx, 0, NATIVE_SCREEN_WIDTH - 1);
	vy = CLAMP(vy, 0, NATIVE_SCREEN_HEIGHT - 1);

	video.mouse.x = vx;
	video.mouse.y = vy;
	if (x) *x = vx;
	if (y) *y = vy;
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

/* -------------------------------------------------- */
/* input grab */

int video_is_input_grabbed(void)
{
	return !!SDL_GetWindowGrab(video.window);
}

void video_set_input_grabbed(int enabled)
{
	SDL_SetWindowGrab(video.window, enabled ? SDL_TRUE : SDL_FALSE);
}

/* -------------------------------------------------- */
/* warp mouse position */

void video_warp_mouse(unsigned int x, unsigned int y)
{
	SDL_WarpMouseInWindow(video.window, x, y);
}

void video_get_mouse_coordinates(unsigned int *x, unsigned int *y)
{
	*x = video.mouse.x;
	*y = video.mouse.y;
}

/* -------------------------------------------------- */
/* menu toggling */

int video_have_menu(void)
{
#ifdef SCHISM_WIN32
	return 1;
#else
	return 0;
#endif
}

void video_toggle_menu(int on)
{
	if (!video_have_menu())
		return;

	const int flags = SDL_GetWindowFlags(video.window);
	int width, height;

	const int cache_size = !(flags & SDL_WINDOW_MAXIMIZED);
	if (cache_size)
		SDL_GetWindowSize(video.window, &width, &height);

#ifdef SCHISM_WIN32
	/* Get the HWND */
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (!SDL_GetWindowWMInfo(video.window, &wm_info))
		return;

	win32_toggle_menu(wm_info.info.win.window, on);
#else
	/* ... */
#endif

	if (cache_size)
		SDL_SetWindowSize(video.window, width, height);
}

/* ------------------------------------------------------------ */

void video_blit(void)
{
	SDL_Rect dstrect;

	if (cfg_video_want_fixed) {
		dstrect = (SDL_Rect){
			.x = 0,
			.y = 0,
			.w = cfg_video_want_fixed_width,
			.h = cfg_video_want_fixed_height,
		};
	}

	SDL_RenderClear(video.renderer);

	// regular format blitter
	unsigned char *pixels;
	int pitch;

	SDL_LockTexture(video.texture, NULL, (void **)&pixels, &pitch);

	switch (video.format) {
	case SDL_PIXELFORMAT_IYUV: {
		video_blitUV(pixels, pitch, video.yuv.pal_y);
		pixels += (NATIVE_SCREEN_HEIGHT * pitch);
		video_blitTV(pixels, pitch, video.yuv.pal_u);
		pixels += (NATIVE_SCREEN_HEIGHT * pitch) / 4;
		video_blitTV(pixels, pitch, video.yuv.pal_v);
		break;
	}
	case SDL_PIXELFORMAT_YV12: {
		video_blitUV(pixels, pitch, video.yuv.pal_y);
		pixels += (NATIVE_SCREEN_HEIGHT * pitch);
		video_blitTV(pixels, pitch, video.yuv.pal_v);
		pixels += (NATIVE_SCREEN_HEIGHT * pitch) / 4;
		video_blitTV(pixels, pitch, video.yuv.pal_u);
		break;
	}
	default: {
		video_blit11(video.bpp, pixels, pitch, video.pal);
		break;
	}
	}
	SDL_UnlockTexture(video.texture);
	SDL_RenderCopy(video.renderer, video.texture, NULL, (cfg_video_want_fixed) ? &dstrect : NULL);
	SDL_RenderPresent(video.renderer);
}

/* ------------------------------------------------- */

int video_gl_bilinear(void)
{
	// no-op
	return cfg_video_gl_bilinear;
}