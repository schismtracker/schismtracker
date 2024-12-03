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
#include "video.h"
#include "osdefs.h"
#include "vgamem.h"

#include <errno.h>
#include <inttypes.h>

#include "backend/video.h"

#include "init.h"
#include <SDL_syswm.h>

#ifndef SCHISM_MACOSX
#include "auto/schismico_hires.h"
#endif

static int (SDLCALL *sdl2_InitSubSystem)(Uint32 flags) = NULL;
static void (SDLCALL *sdl2_QuitSubSystem)(Uint32 flags) = NULL;

static const char *(SDLCALL *sdl2_GetCurrentVideoDriver)(void);
static int (SDLCALL *sdl2_GetCurrentDisplayMode)(int displayIndex, SDL_DisplayMode * mode);
static int (SDLCALL *sdl2_GetRendererInfo)(SDL_Renderer * renderer, SDL_RendererInfo * info);
static int (SDLCALL *sdl2_ShowCursor)(int toggle);
static SDL_bool (SDLCALL *sdl2_GetWindowWMInfo)(SDL_Window * window, SDL_SysWMinfo * info);
static Uint32 (SDLCALL *sdl2_GetWindowFlags)(SDL_Window * window);
static Uint32 (SDLCALL *sdl2_MapRGB)(const SDL_PixelFormat * format, Uint8 r, Uint8 g, Uint8 b);
static void (SDLCALL *sdl2_SetWindowPosition)(SDL_Window * window, int x, int y);
static void (SDLCALL *sdl2_SetWindowSize)(SDL_Window * window, int w, int h);
static int (SDLCALL *sdl2_SetWindowFullscreen)(SDL_Window * window, Uint32 flags);
static void (SDLCALL *sdl2_GetWindowPosition)(SDL_Window * window, int *x, int *y);
static SDL_Window * (SDLCALL *sdl2_CreateWindow)(const char *title, int x, int y, int w, int h, Uint32 flags);
static SDL_Renderer * (SDLCALL *sdl2_CreateRenderer)(SDL_Window * window, int index, Uint32 flags);
static SDL_Texture * (SDLCALL *sdl2_CreateTexture)(SDL_Renderer * renderer, Uint32 format, int access, int w, int h);
static SDL_PixelFormat * (SDLCALL *sdl2_AllocFormat)(Uint32 pixel_format);
static void (SDLCALL *sdl2_FreeFormat)(SDL_PixelFormat *format);
static void (SDLCALL *sdl2_DestroyTexture)(SDL_Texture * texture);
static void (SDLCALL *sdl2_DestroyRenderer)(SDL_Renderer * renderer);
static void (SDLCALL *sdl2_DestroyWindow)(SDL_Window * window);
static Uint8 (SDLCALL *sdl2_EventState)(Uint32 type, int state);
static SDL_bool (SDLCALL *sdl2_IsScreenSaverEnabled)(void);
static void (SDLCALL *sdl2_EnableScreenSaver)(void);
static void (SDLCALL *sdl2_DisableScreenSaver)(void);
static void (SDLCALL *sdl2_RenderGetScale)(SDL_Renderer * renderer, float *scaleX, float *scaleY);
static void (SDLCALL *sdl2_SetWindowGrab)(SDL_Window * window, SDL_bool grabbed);
static void (SDLCALL *sdl2_WarpMouseInWindow)(SDL_Window * window, int x, int y);
static Uint32 (SDLCALL *sdl2_GetWindowFlags)(SDL_Window * window);
static void (SDLCALL *sdl2_GetWindowSize)(SDL_Window * window, int *w, int *h);
static void (SDLCALL *sdl2_SetWindowSize)(SDL_Window * window, int w, int h);
static int (SDLCALL *sdl2_RenderClear)(SDL_Renderer * renderer);
static int (SDLCALL *sdl2_LockTexture)(SDL_Texture * texture, const SDL_Rect * rect, void **pixels, int *pitch);
static void (SDLCALL *sdl2_UnlockTexture)(SDL_Texture * texture);
static int (SDLCALL *sdl2_RenderCopy)(SDL_Renderer * renderer, SDL_Texture * texture, const SDL_Rect * srcrect, const SDL_Rect * dstrect);
static void (SDLCALL *sdl2_RenderPresent)(SDL_Renderer * renderer);
static void (SDLCALL *sdl2_SetWindowTitle)(SDL_Window * window, const char *title);
static SDL_Surface* (SDLCALL *sdl2_CreateRGBSurfaceFrom)(void *pixels, int width, int height, int depth, int pitch, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);
static void (SDLCALL *sdl2_SetWindowIcon)(SDL_Window * window, SDL_Surface * icon);
static void (SDLCALL *sdl2_FreeSurface)(SDL_Surface * surface);
static SDL_bool (SDLCALL *sdl2_SetHint)(const char *name, const char *value);
static int (SDLCALL *sdl2_RenderSetLogicalSize)(SDL_Renderer * renderer, int w, int h);
static SDL_bool (SDLCALL *sdl2_GetWindowGrab)(SDL_Window * window);

// ONLY in SDL 2.0.18
static void (SDLCALL *sdl2_RenderWindowToLogical)(SDL_Renderer * renderer, int windowX, int windowY, float *logicalX, float *logicalY);


#if !SDL_VERSION_ATLEAST(2, 0, 4)
#define SDL_PIXELFORMAT_NV12 (SDL_DEFINE_PIXELFOURCC('N', 'V', '1', '2'))
#define SDL_PIXELFORMAT_NV21 (SDL_DEFINE_PIXELFOURCC('N', 'V', '2', '1'))
#endif

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

static int sdl2_video_is_fullscreen(void)
{
	return video.fullscreen;
}

int sdl2_video_width(void)
{
	return video.width;
}

int sdl2_video_height(void)
{
	return video.height;
}

const char *sdl2_video_driver_name(void)
{
	return sdl2_GetCurrentVideoDriver();
}

void sdl2_video_report(void)
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

	{
		SDL_RendererInfo renderer;
		sdl2_GetRendererInfo(video.renderer, &renderer);
		log_appendf(5, " Using driver '%s'", sdl2_GetCurrentVideoDriver());
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
		if (!sdl2_GetCurrentDisplayMode(0, &display) && video.fullscreen)
			log_appendf(5, " Display dimensions: %dx%d", display.w, display.h);
	}
}

static void set_icon(void)
{
	sdl2_SetWindowTitle(video.window, WINDOW_TITLE);
#ifndef SCHISM_MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
	{
		uint32_t *pixels;
		int width, height;
		if (!xpmdata(_schism_icon_xpm_hires, &pixels, &width, &height)) {
			SDL_Surface *icon = sdl2_CreateRGBSurfaceFrom(pixels, width, height, 32, width * sizeof(uint32_t), 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000);
			if (icon) {
				sdl2_SetWindowIcon(video.window, icon);
				sdl2_FreeSurface(icon);
			}
			free(pixels);
		}
	}
#endif
}

static void video_redraw_texture(void)
{
	int i, j, pref_last = ARRAY_SIZE(native_formats);
	uint32_t format = SDL_PIXELFORMAT_RGB888;

	if (video.texture)
		sdl2_DestroyTexture(video.texture);

	if (video.pixel_format)
		sdl2_FreeFormat(video.pixel_format);

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
	if (!sdl2_GetRendererInfo(video.renderer, &info)) {
		for (i = 0; i < info.num_texture_formats; i++)
			for (j = 0; j < ARRAY_SIZE(native_formats); j++)
				if (info.texture_formats[i] == native_formats[j].format && j < pref_last)
					format = native_formats[pref_last = j].format;
	}

got_format:
	video.texture = sdl2_CreateTexture(video.renderer, format, SDL_TEXTUREACCESS_STREAMING, NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
	video.pixel_format = sdl2_AllocFormat(format);
	video.format = format;

	// find the bytes per pixel
	switch (video.format) {
	// irrelevant
	case SDL_PIXELFORMAT_YV12:
	case SDL_PIXELFORMAT_IYUV: break;

	default: video.bpp = video.pixel_format->BytesPerPixel; break;
	}
}

void sdl2_video_set_hardware(int hardware)
{
	sdl2_DestroyTexture(video.texture);

	sdl2_DestroyRenderer(video.renderer);

	video.renderer = sdl2_CreateRenderer(video.window, -1, hardware ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);
	if (!video.renderer)
		video.renderer = sdl2_CreateRenderer(video.window, -1, 0); // welp

	video_redraw_texture();

	video_report();
}

void sdl2_video_shutdown(void)
{
	sdl2_DestroyTexture(video.texture);
	sdl2_DestroyRenderer(video.renderer);
	sdl2_DestroyWindow(video.window);
}

void sdl2_video_setup(const char *quality)
{
	strncpy(cfg_video_interpolation, quality, 7);
	sdl2_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, quality);
	video_redraw_texture();
}

void sdl2_video_startup(void)
{
	vgamem_clear();
	vgamem_flip();

	video_setup(cfg_video_interpolation);

#ifndef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
/* older SDL2 versions don't define this, don't fail the build for it */
#define SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR "SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR"
#endif
	sdl2_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");

	video.width = cfg_video_width;
	video.height = cfg_video_height;
	video.saved.x = video.saved.y = SDL_WINDOWPOS_CENTERED;

	video.window = sdl2_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, video.width, video.height, SDL_WINDOW_RESIZABLE);
	video_set_hardware(cfg_video_hardware);

	/* Aspect ratio correction if it's wanted */
	if (cfg_video_want_fixed)
		sdl2_RenderSetLogicalSize(video.renderer, cfg_video_want_fixed_width, cfg_video_want_fixed_height);

	video_fullscreen(cfg_video_fullscreen);
	if (video_have_menu() && !video.fullscreen) {
		sdl2_SetWindowSize(video.window, video.width, video.height);
		sdl2_SetWindowPosition(video.window, video.saved.x, video.saved.y);
	}

	/* okay, i think we're ready */
	sdl2_ShowCursor(SDL_DISABLE);
	set_icon();
}

void sdl2_video_fullscreen(int new_fs_flag)
{
	const int have_menu = video_have_menu();
	/* positive new_fs_flag == set, negative == toggle */
	video.fullscreen = (new_fs_flag >= 0) ? !!new_fs_flag : !video.fullscreen;

	if (video.fullscreen) {
		if (have_menu) {
			sdl2_GetWindowSize(video.window, &video.saved.width, &video.saved.height);
			sdl2_GetWindowPosition(video.window, &video.saved.x, &video.saved.y);
		}
		sdl2_SetWindowFullscreen(video.window, SDL_WINDOW_FULLSCREEN_DESKTOP);
		if (have_menu)
			video_toggle_menu(0);
	} else {
		sdl2_SetWindowFullscreen(video.window, 0);
		if (have_menu) {
			/* the menu must be toggled first here */
			video_toggle_menu(1);
			sdl2_SetWindowSize(video.window, video.saved.width, video.saved.height);
			sdl2_SetWindowPosition(video.window, video.saved.x, video.saved.y);
		}
		set_icon(); /* XXX is this necessary */
	}
}

void sdl2_video_resize(unsigned int width, unsigned int height)
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
	video.pal[i] = sdl2_MapRGB(video.pixel_format, rgb[0], rgb[1], rgb[2]);
}

void sdl2_video_colors(unsigned char palette[16][3])
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

int sdl2_video_is_focused(void)
{
	return !!(sdl2_GetWindowFlags(video.window) & SDL_WINDOW_INPUT_FOCUS);
}

int sdl2_video_is_visible(void)
{
	return !!(sdl2_GetWindowFlags(video.window) & SDL_WINDOW_SHOWN);
}

int sdl2_video_is_wm_available(void)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	return !!sdl2_GetWindowWMInfo(video.window, &info);
}

int sdl2_video_is_hardware(void)
{
	SDL_RendererInfo info;
	sdl2_GetRendererInfo(video.renderer, &info);
	return !!(info.flags & SDL_RENDERER_ACCELERATED);
}

/* -------------------------------------------------------- */
/* mousecursor */

void sdl2_video_show_system_cursor(int show)
{
	sdl2_ShowCursor(show);
}

/* -------------------------------------------------------- */

int sdl2_video_is_screensaver_enabled(void)
{
	return sdl2_IsScreenSaverEnabled();
}

void sdl2_video_toggle_screensaver(int enabled)
{
	if (enabled) sdl2_EnableScreenSaver();
	else sdl2_DisableScreenSaver();
}

/* ---------------------------------------------------------- */
/* coordinate translation */

void sdl2_video_translate(int vx, int vy, unsigned int *x, unsigned int *y)
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

void sdl2_video_get_logical_coordinates(int x, int y, int *trans_x, int *trans_y)
{
	if (!cfg_video_want_fixed) {
		*trans_x = x;
		*trans_y = y;
	} else {
		float xx, yy;
#if SDL_DYNAMIC_LOAD || SDL_VERSION_ATLEAST(2, 0, 18)
		if (sdl2_RenderWindowToLogical)
			sdl2_RenderWindowToLogical(video.renderer, x, y, &xx, &yy);
#endif

		/* Alternative for older SDL versions. MIGHT work with high DPI */
		float scale_x = 1, scale_y = 1;

		sdl2_RenderGetScale(video.renderer, &scale_x, &scale_y);

		xx = x - (video.width / 2) - (((float)cfg_video_want_fixed_width * scale_x) / 2);
		yy = y - (video.height / 2) - (((float)cfg_video_want_fixed_height * scale_y) / 2);

		xx /= (float)video.width * cfg_video_want_fixed_width;
		yy /= (float)video.height * cfg_video_want_fixed_height;

		*trans_x = (int)xx;
		*trans_y = (int)yy;
	}
}

/* -------------------------------------------------- */
/* input grab */

int sdl2_video_is_input_grabbed(void)
{
	return !!sdl2_GetWindowGrab(video.window);
}

void sdl2_video_set_input_grabbed(int enabled)
{
	sdl2_SetWindowGrab(video.window, enabled ? SDL_TRUE : SDL_FALSE);
}

/* -------------------------------------------------- */
/* warp mouse position */

void sdl2_video_warp_mouse(unsigned int x, unsigned int y)
{
	sdl2_WarpMouseInWindow(video.window, x, y);
}

void sdl2_video_get_mouse_coordinates(unsigned int *x, unsigned int *y)
{
	*x = video.mouse.x;
	*y = video.mouse.y;
}

/* -------------------------------------------------- */
/* menu toggling */

int sdl2_video_have_menu(void)
{
#ifdef SCHISM_WIN32
	return 1;
#else
	return 0;
#endif
}

void sdl2_video_toggle_menu(int on)
{
	if (!video_have_menu())
		return;

	const int flags = sdl2_GetWindowFlags(video.window);
	int width, height;

	const int cache_size = !(flags & SDL_WINDOW_MAXIMIZED);
	if (cache_size)
		sdl2_GetWindowSize(video.window, &width, &height);

#ifdef SCHISM_WIN32
	/* Get the HWND */
	SDL_SysWMinfo wm_info;
	SDL_VERSION(&wm_info.version);
	if (!sdl2_GetWindowWMInfo(video.window, &wm_info))
		return;

	win32_toggle_menu(wm_info.info.win.window, on);
#else
	/* ... */
#endif

	if (cache_size)
		sdl2_SetWindowSize(video.window, width, height);
}

/* ------------------------------------------------------------ */

void sdl2_video_blit(void)
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

	sdl2_RenderClear(video.renderer);

	// regular format blitter
	unsigned char *pixels;
	int pitch;

	sdl2_LockTexture(video.texture, NULL, (void **)&pixels, &pitch);

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
	sdl2_UnlockTexture(video.texture);
	sdl2_RenderCopy(video.renderer, video.texture, NULL, (cfg_video_want_fixed) ? &dstrect : NULL);
	sdl2_RenderPresent(video.renderer);
}

/* ------------------------------------------------- */

void sdl2_video_mousecursor_changed(void)
{
	const int vis = video_mousecursor_visible();
	sdl2_ShowCursor(vis == MOUSE_SYSTEM);

	// Totally turn off mouse event sending when the mouse is disabled
	int evstate = (vis == MOUSE_DISABLED) ? SDL_DISABLE : SDL_ENABLE;
	if (evstate != sdl2_EventState(SDL_MOUSEMOTION, SDL_QUERY)) {
		sdl2_EventState(SDL_MOUSEMOTION, evstate);
		sdl2_EventState(SDL_MOUSEBUTTONDOWN, evstate);
		sdl2_EventState(SDL_MOUSEBUTTONUP, evstate);
	}
}

//////////////////////////////////////////////////////////////////////////////

static int sdl2_video_get_wm_data(video_wm_data_t *wm_data)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	if (!sdl2_GetWindowWMInfo(video.window, &info))
		return 0;

#if defined(SDL_VIDEO_DRIVER_WINDOWS)
	if (info.subsystem == SDL_SYSWM_WINDOWS) {
		wm_data->subsystem = VIDEO_WM_DATA_SUBSYSTEM_WINDOWS;
		wm_data->data.windows.hwnd = info.info.win.window;
	}
#endif

	return 1;
}

//////////////////////////////////////////////////////////////////////////////

static int sdl2_video_load_syms(void)
{
	SCHISM_SDL2_SYM(InitSubSystem);
	SCHISM_SDL2_SYM(QuitSubSystem);

	SCHISM_SDL2_SYM(GetCurrentVideoDriver);
	SCHISM_SDL2_SYM(GetCurrentDisplayMode);
	SCHISM_SDL2_SYM(GetRendererInfo);
	SCHISM_SDL2_SYM(ShowCursor);
	SCHISM_SDL2_SYM(GetWindowWMInfo);
	SCHISM_SDL2_SYM(GetWindowFlags);
	SCHISM_SDL2_SYM(MapRGB);
	SCHISM_SDL2_SYM(SetWindowPosition);
	SCHISM_SDL2_SYM(SetWindowSize);
	SCHISM_SDL2_SYM(SetWindowFullscreen);
	SCHISM_SDL2_SYM(GetWindowPosition);
	SCHISM_SDL2_SYM(CreateWindow);
	SCHISM_SDL2_SYM(CreateRenderer);
	SCHISM_SDL2_SYM(CreateTexture);
	SCHISM_SDL2_SYM(AllocFormat);
	SCHISM_SDL2_SYM(FreeFormat);
	SCHISM_SDL2_SYM(DestroyTexture);
	SCHISM_SDL2_SYM(DestroyRenderer);
	SCHISM_SDL2_SYM(DestroyWindow);
	SCHISM_SDL2_SYM(EventState);
	SCHISM_SDL2_SYM(IsScreenSaverEnabled);
	SCHISM_SDL2_SYM(EnableScreenSaver);
	SCHISM_SDL2_SYM(DisableScreenSaver);
	SCHISM_SDL2_SYM(RenderGetScale);
	SCHISM_SDL2_SYM(SetWindowGrab);
	SCHISM_SDL2_SYM(WarpMouseInWindow);
	SCHISM_SDL2_SYM(GetWindowFlags);
	SCHISM_SDL2_SYM(GetWindowSize);
	SCHISM_SDL2_SYM(SetWindowSize);
	SCHISM_SDL2_SYM(RenderClear);
	SCHISM_SDL2_SYM(LockTexture);
	SCHISM_SDL2_SYM(UnlockTexture);
	SCHISM_SDL2_SYM(RenderCopy);
	SCHISM_SDL2_SYM(RenderPresent);
	SCHISM_SDL2_SYM(SetWindowTitle);
	SCHISM_SDL2_SYM(CreateRGBSurfaceFrom);
	SCHISM_SDL2_SYM(SetWindowIcon);
	SCHISM_SDL2_SYM(FreeSurface);
	SCHISM_SDL2_SYM(SetHint);
	SCHISM_SDL2_SYM(RenderSetLogicalSize);
	SCHISM_SDL2_SYM(GetWindowGrab);

	return 0;
}

static int sdl2_0_18_video_load_syms(void)
{
#if SDL_DYNAMIC_LOAD || SDL_VERSION_ATLEAST(2, 0, 18)
	SCHISM_SDL2_SYM(RenderWindowToLogical);

	return 0;
#else
	return -1;
#endif
}

static int sdl2_video_init(void)
{
	if (!sdl2_init())
		return 0;

	if (sdl2_video_load_syms())
		return 0;

	//  :) 
	sdl2_0_18_video_load_syms();

	if (sdl2_InitSubSystem(SDL_INIT_VIDEO) < 0)
		return 0;

	return 1;
}

static void sdl2_video_quit(void)
{
	sdl2_QuitSubSystem(SDL_INIT_VIDEO);

	sdl2_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_video_backend_t schism_video_backend_sdl2 = {
	.init = sdl2_video_init,
	.quit = sdl2_video_quit,

	.startup = sdl2_video_startup,
	.shutdown = sdl2_video_shutdown,

	.is_fullscreen = sdl2_video_is_fullscreen,
	.width = sdl2_video_width,
	.height = sdl2_video_height,
	.driver_name = sdl2_video_driver_name,
	.report = sdl2_video_report,
	.set_hardware = sdl2_video_set_hardware,
	.setup = sdl2_video_setup,
	.fullscreen = sdl2_video_fullscreen,
	.resize = sdl2_video_resize,
	.colors = sdl2_video_colors,
	.is_focused = sdl2_video_is_focused,
	.is_visible = sdl2_video_is_visible,
	.is_wm_available = sdl2_video_is_wm_available,
	.is_hardware = sdl2_video_is_hardware,
	.is_screensaver_enabled = sdl2_video_is_screensaver_enabled,
	.toggle_screensaver = sdl2_video_toggle_screensaver,
	.translate = sdl2_video_translate,
	.get_logical_coordinates = sdl2_video_get_logical_coordinates,
	.is_input_grabbed = sdl2_video_is_input_grabbed,
	.set_input_grabbed = sdl2_video_set_input_grabbed,
	.warp_mouse = sdl2_video_warp_mouse,
	.get_mouse_coordinates = sdl2_video_get_mouse_coordinates,
	.have_menu = sdl2_video_have_menu,
	.toggle_menu = sdl2_video_toggle_menu,
	.blit = sdl2_video_blit,
	.mousecursor_changed = sdl2_video_mousecursor_changed,
	.get_wm_data = sdl2_video_get_wm_data,
};
