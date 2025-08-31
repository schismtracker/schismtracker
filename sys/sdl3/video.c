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

#define WINDOW_TITLE			"Schism Tracker"

#include "headers.h"

#include "it.h"
#include "charset.h"
#include "bits.h"
#include "config.h"
#include "video.h"
#include "osdefs.h"
#include "vgamem.h"
#include "events.h"

#include "backend/video.h"

#include "init.h"

#ifndef SCHISM_MACOSX
#include "auto/schismico_hires.h"
#endif

static bool (SDLCALL *sdl3_InitSubSystem)(SDL_InitFlags flags) = NULL;
static void (SDLCALL *sdl3_QuitSubSystem)(SDL_InitFlags flags) = NULL;

static const char *(SDLCALL *sdl3_GetCurrentVideoDriver)(void);
static SDL_DisplayID (SDLCALL *sdl3_GetDisplayForWindow)(SDL_Window *window);
static const SDL_DisplayMode *(SDLCALL *sdl3_GetCurrentDisplayMode)(SDL_DisplayID id);
static const char * (SDLCALL *sdl3_GetRendererName)(SDL_Renderer * renderer);
static bool (SDLCALL *sdl3_ShowCursor)(void);
static bool (SDLCALL *sdl3_HideCursor)(void);
static SDL_WindowFlags (SDLCALL *sdl3_GetWindowFlags)(SDL_Window * window);
static Uint32 (SDLCALL *sdl3_MapRGB)(const SDL_PixelFormatDetails *format, const SDL_Palette *palette, Uint8 r, Uint8 g, Uint8 b);
static bool (SDLCALL *sdl3_SetWindowPosition)(SDL_Window * window, int x, int y);
static bool (SDLCALL *sdl3_SetWindowSize)(SDL_Window * window, int w, int h);
static bool (SDLCALL *sdl3_SetWindowFullscreen)(SDL_Window * window, bool fullscreen);
static bool (SDLCALL *sdl3_GetWindowPosition)(SDL_Window * window, int *x, int *y);
static SDL_Window * (SDLCALL *sdl3_CreateWindow)(const char *title, int w, int h, SDL_WindowFlags flags);
static SDL_Renderer * (SDLCALL *sdl3_CreateRenderer)(SDL_Window *window, const char *name);
static SDL_Texture * (SDLCALL *sdl3_CreateTexture)(SDL_Renderer * renderer, SDL_PixelFormat format, SDL_TextureAccess access, int w, int h);
static const SDL_PixelFormatDetails * (SDLCALL *sdl3_GetPixelFormatDetails)(Uint32 pixel_format);
static void (SDLCALL *sdl3_DestroyTexture)(SDL_Texture * texture);
static void (SDLCALL *sdl3_DestroyRenderer)(SDL_Renderer * renderer);
static void (SDLCALL *sdl3_DestroyWindow)(SDL_Window * window);
static bool (SDLCALL *sdl3_ScreenSaverEnabled)(void);
static bool (SDLCALL *sdl3_EnableScreenSaver)(void);
static bool (SDLCALL *sdl3_DisableScreenSaver)(void);
static bool (SDLCALL *sdl3_GetRenderScale)(SDL_Renderer * renderer, float *scaleX, float *scaleY);
static void (SDLCALL *sdl3_WarpMouseInWindow)(SDL_Window * window, float x, float y);
static bool (SDLCALL *sdl3_GetWindowSize)(SDL_Window * window, int *w, int *h);
static bool (SDLCALL *sdl3_SetWindowSize)(SDL_Window * window, int w, int h);
static bool (SDLCALL *sdl3_RenderClear)(SDL_Renderer * renderer);
static bool (SDLCALL *sdl3_LockTexture)(SDL_Texture * texture, const SDL_Rect * rect, void **pixels, int *pitch);
static void (SDLCALL *sdl3_UnlockTexture)(SDL_Texture * texture);
static bool (SDLCALL *sdl3_RenderTexture)(SDL_Renderer * renderer, SDL_Texture * texture, const SDL_FRect * srcrect, const SDL_FRect * dstrect);
static bool (SDLCALL *sdl3_RenderPresent)(SDL_Renderer * renderer);
static bool (SDLCALL *sdl3_SetWindowTitle)(SDL_Window * window, const char *title);

static bool (SDLCALL *sdl3_SetWindowIcon)(SDL_Window * window, SDL_Surface * icon);
static void (SDLCALL *sdl3_DestroySurface)(SDL_Surface * surface);
static bool (SDLCALL *sdl3_SetHint)(const char *name, const char *value);

/* RENDERER */
static bool (SDLCALL *sdl3_SetRenderLogicalPresentation)(SDL_Renderer *renderer, int w, int h, SDL_RendererLogicalPresentation mode);

/* WINDOW GRAB */
static bool (SDLCALL *sdl3_GetWindowMouseGrab)(SDL_Window * window);
static bool (SDLCALL *sdl3_GetWindowKeyboardGrab)(SDL_Window * window);
static bool (SDLCALL *sdl3_SetWindowMouseGrab)(SDL_Window * window, bool grabbed);
static bool (SDLCALL *sdl3_SetWindowKeyboardGrab)(SDL_Window * window, bool grabbed);

static bool (SDLCALL *sdl3_EventEnabled)(Uint32 type);
static void (SDLCALL *sdl3_SetEventEnabled)(Uint32 type, bool enabled);

static bool (SDLCALL *sdl3_RenderCoordinatesFromWindow)(SDL_Renderer * renderer, float windowX, float windowY, float *logicalX, float *logicalY);

static bool (SDLCALL *sdl3_SetTextureScaleMode)(SDL_Texture * texture, SDL_ScaleMode scaleMode);

static SDL_PropertiesID (SDLCALL *sdl3_GetRendererProperties)(SDL_Renderer *renderer);
static SDL_PropertiesID (SDLCALL *sdl3_GetWindowProperties)(SDL_Window *window);
static void *(SDLCALL *sdl3_GetPointerProperty)(SDL_PropertiesID props, const char *name, void *default_value);
static Sint64 (SDLCALL *sdl3_GetNumberProperty)(SDL_PropertiesID props, const char *name, Sint64 default_value);

/* WINDOW FRAMEBUFFER */
static bool (SDLCALL *sdl3_WindowHasSurface)(SDL_Window *window);
static SDL_Surface *(SDLCALL *sdl3_GetWindowSurface)(SDL_Window *window);
static bool (SDLCALL *sdl3_DestroyWindowSurface)(SDL_Window *window);
static bool (SDLCALL *sdl3_UpdateWindowSurface)(SDL_Window *window);
static bool (SDLCALL *sdl3_LockSurface)(SDL_Surface *surface);
static void (SDLCALL *sdl3_UnlockSurface)(SDL_Surface *surface);

static bool (SDLCALL *sdl3_StartTextInput)(SDL_Window *window);

// this is used in multiple places here.
static int sdl3_video_get_wm_data(video_wm_data_t *wm_data);

static SDL_Surface *(SDLCALL *sdl3_CreateSurfaceFrom)(int width, int height, SDL_PixelFormat format, void *pixels, int pitch);
static SDL_PixelFormat (SDLCALL *sdl3_GetPixelFormatForMasks)(int bpp, Uint32 Rmask, Uint32 Gmask, Uint32 Bmask, Uint32 Amask);

static void sdl3_video_setup(int quality);

static struct {
	SDL_Window *window;

	enum {
		VIDEO_TYPE_UNINITIALIZED = 0,
		VIDEO_TYPE_RENDERER = 1,
		VIDEO_TYPE_SURFACE = 2,
	} type;

	/* renderer */
	union {
		struct {
			SDL_Renderer *renderer;
			SDL_Texture *texture;
		} r;
		struct {
			SDL_Surface *surface;
			struct {
				int x, y, w, h;
			} clip;
		} s;
	} u; /* "type data" */

	const SDL_PixelFormatDetails *pixel_format; // may be NULL
	SDL_PixelFormat format;
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
	{SDL_PIXELFORMAT_XRGB8888, "RGB888"},
	{SDL_PIXELFORMAT_ARGB8888, "ARGB8888"},
	{SDL_PIXELFORMAT_RGB24, "RGB24"},
	{SDL_PIXELFORMAT_RGB565, "RGB565"},
	{SDL_PIXELFORMAT_XRGB1555, "RGB555"},
	{SDL_PIXELFORMAT_ARGB1555, "ARGB1555"},
	{SDL_PIXELFORMAT_XRGB4444, "RGB444"},
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

static int sdl3_video_is_fullscreen(void)
{
	return video.fullscreen;
}

static int sdl3_video_width(void)
{
	return video.width;
}

static int sdl3_video_height(void)
{
	return video.height;
}

static const char *sdl3_video_driver_name(void)
{
	return sdl3_GetCurrentVideoDriver();
}

static void sdl3_video_report(void)
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

	log_appendf(5, " Using driver '%s'", sdl3_GetCurrentVideoDriver());

	switch (video.type) {
	case VIDEO_TYPE_RENDERER: {
		const char *name = sdl3_GetRendererName(video.u.r.renderer);

		log_appendf(5, " %s renderer '%s'",
			(!strcmp(name, SDL_SOFTWARE_RENDERER)) ? "Software" : "Hardware-accelerated",
			name);
		break;
	}
	case VIDEO_TYPE_SURFACE:
		/* hm */
		log_appendf(5, " Software video surface");
		if (SDL_MUSTLOCK(video.u.s.surface))
			log_append(4, 0, " Must lock surface");
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		SCHISM_UNREACHABLE;
		break;
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

	if (video.fullscreen) {
		const SDL_DisplayID id = sdl3_GetDisplayForWindow(video.window);
		if (id) {
			const SDL_DisplayMode *display = sdl3_GetCurrentDisplayMode(id);
			if (display)
				log_appendf(5, " Display dimensions: %dx%d", display->w, display->h);
		}
	}
}

static void set_icon(void)
{
	sdl3_SetWindowTitle(video.window, WINDOW_TITLE);
#ifndef SCHISM_MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
	{
		uint32_t *pixels;
		int width, height;
		if (!xpmdata(_schism_icon_xpm_hires, &pixels, &width, &height)) {
			SDL_Surface *icon = sdl3_CreateSurfaceFrom(width, height, sdl3_GetPixelFormatForMasks(32, 0x00FF0000, 0x0000FF00, 0x000000FF, 0xFF000000), pixels, width * sizeof(uint32_t));
			if (icon) {
				sdl3_SetWindowIcon(video.window, icon);
				sdl3_DestroySurface(icon);
			}
			free(pixels);
		}
	}
#endif
}

static inline void video_recalculate_fixed_width(void)
{
	/* Aspect ratio correction if it's wanted */
	if (cfg_video_want_fixed) {
		switch (video.type) {
		case VIDEO_TYPE_RENDERER:
			sdl3_SetRenderLogicalPresentation(video.u.r.renderer,
				cfg_video_want_fixed_width, cfg_video_want_fixed_height,
				SDL_LOGICAL_PRESENTATION_LETTERBOX);
			break;
		case VIDEO_TYPE_SURFACE: {
			const double ratio_w = (double)video.width  / (double)cfg_video_want_fixed_width;
			const double ratio_h = (double)video.height / (double)cfg_video_want_fixed_height;

			if (ratio_w < ratio_h) {
				video.u.s.clip.w = video.width;
				video.u.s.clip.h = (double)cfg_video_want_fixed_height * ratio_w;
			} else {
				video.u.s.clip.h = video.height;
				video.u.s.clip.w = (double)cfg_video_want_fixed_width  * ratio_h;
			}

			video.u.s.clip.x = (video.width  - video.u.s.clip.w) / 2;
			video.u.s.clip.y = (video.height - video.u.s.clip.h) / 2;
			break;
		}
		case VIDEO_TYPE_UNINITIALIZED:
			/* WUT */
			SCHISM_UNREACHABLE;
			break;
		}
	} else if (video.type == VIDEO_TYPE_SURFACE) {
		video.u.s.clip.x = video.u.s.clip.y = 0;
		video.u.s.clip.w = video.width;
		video.u.s.clip.h = video.height;
	}
}

static void sdl3_video_redraw_texture(void)
{
	switch (video.type) {
	case VIDEO_TYPE_RENDERER: {
		size_t pref_last = ARRAY_SIZE(native_formats);
		uint32_t format = SDL_PIXELFORMAT_XRGB8888;

		if (*cfg_video_format) {
			for (size_t i = 0; i < ARRAY_SIZE(native_formats); i++) {
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
		SDL_PropertiesID rprop = sdl3_GetRendererProperties(video.u.r.renderer);
		if (rprop) {
			const SDL_PixelFormat *formats = sdl3_GetPointerProperty(rprop, SDL_PROP_RENDERER_TEXTURE_FORMATS_POINTER, NULL);
			if (formats) {
				for (uint32_t i = 0; formats[i] != SDL_PIXELFORMAT_UNKNOWN; i++)
					for (size_t j = 0; j < ARRAY_SIZE(native_formats); j++)
						if (formats[i] == native_formats[j].format && j < pref_last)
							format = native_formats[pref_last = j].format;
			}
		}

got_format:
		video.u.r.texture = sdl3_CreateTexture(video.u.r.renderer, format, SDL_TEXTUREACCESS_STREAMING, NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
		video.format = format;
		break;
	}
	case VIDEO_TYPE_SURFACE:
		video.format = video.u.s.surface->format;
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		/* ? */
		break;
	}

	video.pixel_format = sdl3_GetPixelFormatDetails(video.format);

	// ok
	video.bpp = SDL_BYTESPERPIXEL(video.format);

	sdl3_video_setup(cfg_video_interpolation); // ew

	video_recalculate_fixed_width();
}

static void sdl3_video_set_hardware(int hardware)
{
	/* do all the necessary cleanup HERE */
	switch (video.type) {
	case VIDEO_TYPE_RENDERER:
		if (video.u.r.texture)
			sdl3_DestroyTexture(video.u.r.texture);
		if (video.u.r.renderer)
			sdl3_DestroyRenderer(video.u.r.renderer);
		break;
	case VIDEO_TYPE_SURFACE:
		SCHISM_RUNTIME_ASSERT(sdl3_WindowHasSurface(video.window),
			"Internal video type says surface, but the window doesn't have one");
		sdl3_DestroyWindowSurface(video.window);
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		break;
	}

	/* If we're going to be using a software renderer anyway, then
	 * just request a surface. Our built-in blit is plenty fast. */
	video.type = (hardware) ? VIDEO_TYPE_RENDERER : VIDEO_TYPE_SURFACE;

	switch (video.type) {
	case VIDEO_TYPE_SURFACE:
		video.u.s.surface = sdl3_GetWindowSurface(video.window);
		if (video.u.s.surface)
			break;
		video.type = VIDEO_TYPE_RENDERER;
		SCHISM_FALLTHROUGH;
	case VIDEO_TYPE_RENDERER:
		video.u.r.renderer = sdl3_CreateRenderer(video.window,
			(hardware) ? (const char *)NULL : SDL_SOFTWARE_RENDERER);
		SCHISM_RUNTIME_ASSERT(!!video.u.r.renderer,
			"Failed to create a renderer!");
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		/* wut? */
		break;
	}

	// hope that all worked!

	sdl3_video_redraw_texture();

	video_report();
}

static void sdl3_video_shutdown(void)
{
	/* do all the necessary cleanup HERE */
	switch (video.type) {
	case VIDEO_TYPE_RENDERER:
		if (video.u.r.texture)
			sdl3_DestroyTexture(video.u.r.texture);
		if (video.u.r.renderer)
			sdl3_DestroyRenderer(video.u.r.renderer);
		break;
	case VIDEO_TYPE_SURFACE:
		SCHISM_RUNTIME_ASSERT(sdl3_WindowHasSurface(video.window),
			"Internal video type says surface, but window doesn't have one");
		sdl3_DestroyWindowSurface(video.window);
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		break;
	}

	sdl3_DestroyWindow(video.window);
}

static void sdl3_video_setup(int interpolation)
{
	static const SDL_ScaleMode modes[] = {
		[VIDEO_INTERPOLATION_NEAREST] = SDL_SCALEMODE_NEAREST,
		[VIDEO_INTERPOLATION_LINEAR]  = SDL_SCALEMODE_LINEAR,
		[VIDEO_INTERPOLATION_BEST]    = SDL_SCALEMODE_LINEAR,
	};

	if (video.type == VIDEO_TYPE_RENDERER)
		sdl3_SetTextureScaleMode(video.u.r.texture, modes[interpolation]);
}

static int sdl3_video_startup(void)
{
	vgamem_clear();
	vgamem_flip();

	sdl3_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
	sdl3_SetHint("SDL_WINDOWS_DPI_AWARENESS", "unaware");
	sdl3_SetHint(SDL_HINT_WINDOWS_CLOSE_ON_ALT_F4, "0");

	video.width = cfg_video_width;
	video.height = cfg_video_height;
	video.saved.x = video.saved.y = SDL_WINDOWPOS_CENTERED;

	video.window = sdl3_CreateWindow(WINDOW_TITLE, video.width, video.height, SDL_WINDOW_RESIZABLE);
	if (!video.window)
		return 0;

	video_fullscreen(cfg_video_fullscreen);
	video_set_hardware(cfg_video_hardware);

	if (video_have_menu() && !video.fullscreen) {
		sdl3_SetWindowSize(video.window, video.width, video.height);
		sdl3_SetWindowPosition(video.window, video.saved.x, video.saved.y);
	}

	/* okay, i think we're ready */
	sdl3_HideCursor();
	sdl3_StartTextInput(video.window);
	set_icon();

	return 1;
}

static void sdl3_video_fullscreen(int new_fs_flag)
{
	const int have_menu = video_have_menu();
	/* positive new_fs_flag == set, negative == toggle */
	video.fullscreen = (new_fs_flag >= 0) ? !!new_fs_flag : !video.fullscreen;

	if (video.fullscreen) {
		if (have_menu) {
			sdl3_GetWindowSize(video.window, &video.saved.width, &video.saved.height);
			sdl3_GetWindowPosition(video.window, &video.saved.x, &video.saved.y);
		}
		sdl3_SetWindowFullscreen(video.window, 1);
		if (have_menu)
			video_toggle_menu(0);
	} else {
		sdl3_SetWindowFullscreen(video.window, 0);
		if (have_menu) {
			/* the menu must be toggled first here */
			video_toggle_menu(1);
			sdl3_SetWindowSize(video.window, video.saved.width, video.saved.height);
			sdl3_SetWindowPosition(video.window, video.saved.x, video.saved.y);
		}
		set_icon(); /* XXX is this necessary */
	}
}

static void sdl3_video_resize(unsigned int width, unsigned int height)
{
	video.width = width;
	video.height = height;
	video_recalculate_fixed_width();
	if (video.type == VIDEO_TYPE_SURFACE)
		video.u.s.surface = sdl3_GetWindowSurface(video.window);
	status.flags |= (NEED_UPDATE);
}

static void yuv_pal_(unsigned int i, unsigned char rgb[3])
{
	uint32_t y, u, v;
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

static void sdl_pal_(unsigned int i, unsigned char rgb[3])
{
	video.pal[i] = sdl3_MapRGB(video.pixel_format, NULL, rgb[0], rgb[1], rgb[2]);
}

static void sdl3_video_colors(unsigned char palette[16][3])
{
	switch (video.format) {
	case SDL_PIXELFORMAT_IYUV:
	case SDL_PIXELFORMAT_YV12:
		video_colors_iterate(palette, yuv_pal_);
		break;
	default:
		video_colors_iterate(palette, sdl_pal_);
		break;
	}
}

static int sdl3_video_is_focused(void)
{
	return !!(sdl3_GetWindowFlags(video.window) & SDL_WINDOW_INPUT_FOCUS);
}

static int sdl3_video_is_visible(void)
{
	return !(sdl3_GetWindowFlags(video.window) & SDL_WINDOW_HIDDEN);
}

static int sdl3_video_is_wm_available(void)
{
	// Ok
	return 1;
}

static int sdl3_video_is_hardware(void)
{
	switch (video.type) {
	case VIDEO_TYPE_UNINITIALIZED:
	case VIDEO_TYPE_SURFACE:
		return 0;
	case VIDEO_TYPE_RENDERER:
		return strcmp(sdl3_GetRendererName(video.u.r.renderer),
			SDL_SOFTWARE_RENDERER);
	}

	/* should never happen */
	return 0;
}

/* -------------------------------------------------------- */

static int sdl3_video_is_screensaver_enabled(void)
{
	return sdl3_ScreenSaverEnabled();
}

static void sdl3_video_toggle_screensaver(int enabled)
{
	/* this API reeks */
	if (enabled) sdl3_EnableScreenSaver();
	else sdl3_DisableScreenSaver();
}

/* ------------------------------------------------------------------------ */
/* coordinate translation */

static void sdl3_video_translate(unsigned int vx, unsigned int vy,
	unsigned int *x, unsigned int *y)
{
	if (video_mousecursor_visible()
		&& (video.mouse.x != vx || video.mouse.y != vy))
		status.flags |= SOFTWARE_MOUSE_MOVED;

	vx *= NATIVE_SCREEN_WIDTH;
	vy *= NATIVE_SCREEN_HEIGHT;
	vx /= (cfg_video_want_fixed) ? cfg_video_want_fixed_width  : video.width;
	vy /= (cfg_video_want_fixed) ? cfg_video_want_fixed_height : video.height;

	vx = MIN(vx, NATIVE_SCREEN_WIDTH - 1);
	vy = MIN(vy, NATIVE_SCREEN_HEIGHT - 1);

	video.mouse.x = vx;
	video.mouse.y = vy;
	if (x) *x = vx;
	if (y) *y = vy;
}

static void sdl3_video_get_logical_coordinates(int x, int y, int *trans_x,
	int *trans_y)
{
	if (cfg_video_want_fixed && video.type == VIDEO_TYPE_RENDERER) {
		float xf, yf;

		sdl3_RenderCoordinatesFromWindow(video.u.r.renderer, x, y, &xf, &yf);

		*trans_x = (int)xf;
		*trans_y = (int)yf;
	} else {
		*trans_x = x;
		*trans_y = y;
	}
}

/* ------------------------------------------------------------------------ */
/* input grab */

static int sdl3_video_is_input_grabbed(void)
{
	return sdl3_GetWindowMouseGrab(video.window)
		&& sdl3_GetWindowKeyboardGrab(video.window);
}

static void sdl3_video_set_input_grabbed(int enabled)
{
	sdl3_SetWindowMouseGrab(video.window, (bool)enabled);
	sdl3_SetWindowKeyboardGrab(video.window, (bool)enabled);
}

/* ------------------------------------------------------------------------ */
/* warp mouse position */

static void sdl3_video_warp_mouse(unsigned int x, unsigned int y)
{
	sdl3_WarpMouseInWindow(video.window, x, y);
}

static void sdl3_video_get_mouse_coordinates(unsigned int *x, unsigned int *y)
{
	*x = video.mouse.x;
	*y = video.mouse.y;
}

/* -------------------------------------------------- */
/* menu toggling */

static int sdl3_video_have_menu(void)
{
#ifdef SCHISM_WIN32
	return 1;
#else
	return 0;
#endif
}

static void sdl3_video_toggle_menu(SCHISM_UNUSED int on)
{
	if (!sdl3_video_have_menu())
		return;

	const int flags = sdl3_GetWindowFlags(video.window);
	int width, height;

	const int cache_size = !(flags & SDL_WINDOW_MAXIMIZED);
	if (cache_size)
		sdl3_GetWindowSize(video.window, &width, &height);

#ifdef SCHISM_WIN32
	/* Get the HWND */
	video_wm_data_t wm_data;
	if (!sdl3_video_get_wm_data(&wm_data))
		return;

	win32_toggle_menu(wm_data.data.windows.hwnd, on);
#else
	/* ... */
#endif

	if (cache_size)
		sdl3_SetWindowSize(video.window, width, height);
}

/* ------------------------------------------------------------------------ */

SCHISM_HOT static uint32_t sdl3_map_rgb_callback(void *data, uint8_t r,
	uint8_t g, uint8_t b)
{
	const SDL_PixelFormatDetails *format = data;

	return sdl3_MapRGB(format, NULL, r, g, b);
}

SCHISM_HOT static void sdl3_video_blit(void)
{
	switch (video.type) {
	case VIDEO_TYPE_RENDERER: {
		unsigned char *pixels;
		int pitch;
		SDL_FRect dstrect;

		if (cfg_video_want_fixed) {
			dstrect.x = 0;
			dstrect.y = 0;
			dstrect.w = cfg_video_want_fixed_width;
			dstrect.h = cfg_video_want_fixed_height;
		}

		sdl3_RenderClear(video.u.r.renderer);

		sdl3_LockTexture(video.u.r.texture, NULL, (void **)&pixels, &pitch);

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
		sdl3_UnlockTexture(video.u.r.texture);
		sdl3_RenderTexture(video.u.r.renderer, video.u.r.texture, NULL, (cfg_video_want_fixed) ? &dstrect : NULL);
		sdl3_RenderPresent(video.u.r.renderer);
		break;
	}
	case VIDEO_TYPE_SURFACE:
		if (SDL_MUSTLOCK(video.u.s.surface))
			while (!sdl3_LockSurface(video.u.s.surface))
				timer_msleep(10);

		video_blitSC(SDL_BYTESPERPIXEL(video.format),
			video.u.s.surface->pixels,
			video.u.s.surface->pitch,
			video.pal,
			sdl3_map_rgb_callback,
			(void *)video.pixel_format /* stupid cast */,
			video.u.s.clip.x,
			video.u.s.clip.y,
			video.u.s.clip.w,
			video.u.s.clip.h);

		if (SDL_MUSTLOCK(video.u.s.surface))
			sdl3_UnlockSurface(video.u.s.surface);

		sdl3_UpdateWindowSurface(video.window);
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		break;
	}
}

/* ------------------------------------------------- */

static void sdl3_video_show_cursor(int enabled)
{
	if (enabled) sdl3_ShowCursor();
	else sdl3_HideCursor();
}

static void sdl3_video_mousecursor_changed(void)
{
	const int vis = video_mousecursor_visible();

	sdl3_video_show_cursor(vis == MOUSE_SYSTEM);

	// Totally turn off mouse event sending when the mouse is disabled
	bool evstate = !(vis == MOUSE_DISABLED);
	if (evstate != sdl3_EventEnabled(SDL_EVENT_MOUSE_MOTION)) {
		sdl3_SetEventEnabled(SDL_EVENT_MOUSE_MOTION, evstate);
		sdl3_SetEventEnabled(SDL_EVENT_MOUSE_BUTTON_DOWN, evstate);
		sdl3_SetEventEnabled(SDL_EVENT_MOUSE_BUTTON_UP, evstate);
	}
}

//////////////////////////////////////////////////////////////////////////////

static int sdl3_video_get_wm_data(video_wm_data_t *wm_data)
{
	const char *driver;
	SDL_PropertiesID wprops;

	if (!wm_data)
		return 0;

	wprops = sdl3_GetWindowProperties(video.window);
	if (!wprops)
		return 0;

	driver = sdl3_GetCurrentVideoDriver();

	if (!strcmp(driver, "windows")) {
		wm_data->subsystem = VIDEO_WM_DATA_SUBSYSTEM_WINDOWS;
		wm_data->data.windows.hwnd = sdl3_GetPointerProperty(wprops, SDL_PROP_WINDOW_WIN32_HWND_POINTER, NULL);

		return !!wm_data->data.windows.hwnd;
	} else if (!strcmp(driver, "x11")) {
		wm_data->subsystem = VIDEO_WM_DATA_SUBSYSTEM_X11;
		wm_data->data.x11.display = sdl3_GetPointerProperty(wprops, SDL_PROP_WINDOW_X11_DISPLAY_POINTER, NULL);
		wm_data->data.x11.window = sdl3_GetNumberProperty(wprops, SDL_PROP_WINDOW_X11_WINDOW_NUMBER, 0);
		wm_data->data.x11.lock_func = NULL;
		wm_data->data.x11.unlock_func = NULL;

		return (wm_data->data.x11.display && wm_data->data.x11.window);
	}

	// maybe the real WM data was the friends we made along the way
	return 0;
}

//////////////////////////////////////////////////////////////////////////////

static int sdl3_video_load_syms(void)
{
	SCHISM_SDL3_SYM(InitSubSystem);
	SCHISM_SDL3_SYM(QuitSubSystem);

	SCHISM_SDL3_SYM(GetCurrentVideoDriver);
	SCHISM_SDL3_SYM(GetCurrentDisplayMode);
	SCHISM_SDL3_SYM(GetRendererName);
	SCHISM_SDL3_SYM(ShowCursor);
	SCHISM_SDL3_SYM(HideCursor);
	SCHISM_SDL3_SYM(GetWindowFlags);
	SCHISM_SDL3_SYM(MapRGB);
	SCHISM_SDL3_SYM(SetWindowPosition);
	SCHISM_SDL3_SYM(SetWindowSize);
	SCHISM_SDL3_SYM(SetWindowFullscreen);
	SCHISM_SDL3_SYM(GetWindowPosition);
	SCHISM_SDL3_SYM(CreateWindow);
	SCHISM_SDL3_SYM(CreateRenderer);
	SCHISM_SDL3_SYM(CreateTexture);
	SCHISM_SDL3_SYM(GetPixelFormatDetails);
	SCHISM_SDL3_SYM(DestroyTexture);
	SCHISM_SDL3_SYM(DestroyRenderer);
	SCHISM_SDL3_SYM(DestroyWindow);
	SCHISM_SDL3_SYM(EventEnabled);
	SCHISM_SDL3_SYM(SetEventEnabled);
	SCHISM_SDL3_SYM(ScreenSaverEnabled);
	SCHISM_SDL3_SYM(EnableScreenSaver);
	SCHISM_SDL3_SYM(DisableScreenSaver);
	SCHISM_SDL3_SYM(GetRenderScale);
	SCHISM_SDL3_SYM(WarpMouseInWindow);
	SCHISM_SDL3_SYM(GetWindowSize);
	SCHISM_SDL3_SYM(SetWindowSize);
	SCHISM_SDL3_SYM(RenderClear);
	SCHISM_SDL3_SYM(LockTexture);
	SCHISM_SDL3_SYM(UnlockTexture);
	SCHISM_SDL3_SYM(RenderTexture);
	SCHISM_SDL3_SYM(RenderPresent);
	SCHISM_SDL3_SYM(SetWindowTitle);
	SCHISM_SDL3_SYM(CreateSurfaceFrom);
	SCHISM_SDL3_SYM(GetPixelFormatForMasks);
	SCHISM_SDL3_SYM(SetWindowIcon);
	SCHISM_SDL3_SYM(DestroySurface);
	SCHISM_SDL3_SYM(SetHint);
	SCHISM_SDL3_SYM(SetRenderLogicalPresentation);
	SCHISM_SDL3_SYM(RenderCoordinatesFromWindow);
	SCHISM_SDL3_SYM(SetTextureScaleMode);
	SCHISM_SDL3_SYM(GetRendererProperties);
	SCHISM_SDL3_SYM(GetWindowProperties);
	SCHISM_SDL3_SYM(GetPointerProperty);
	SCHISM_SDL3_SYM(GetNumberProperty);
	SCHISM_SDL3_SYM(StartTextInput);

	SCHISM_SDL3_SYM(GetDisplayForWindow);

	SCHISM_SDL3_SYM(SetWindowKeyboardGrab);
	SCHISM_SDL3_SYM(GetWindowKeyboardGrab);
	SCHISM_SDL3_SYM(SetWindowMouseGrab);
	SCHISM_SDL3_SYM(GetWindowMouseGrab);

	SCHISM_SDL3_SYM(WindowHasSurface);
	SCHISM_SDL3_SYM(GetWindowSurface);
	SCHISM_SDL3_SYM(DestroyWindowSurface);
	SCHISM_SDL3_SYM(UpdateWindowSurface);
	SCHISM_SDL3_SYM(LockSurface);
	SCHISM_SDL3_SYM(UnlockSurface);

	return 0;
}

static int sdl3_video_init(void)
{
	if (!sdl3_init())
		return 0;

	if (sdl3_video_load_syms() || !sdl3_InitSubSystem(SDL_INIT_VIDEO)) {
		sdl3_quit();
		return 0;
	}

	if (!events_init(&schism_events_backend_sdl3)) {
		sdl3_QuitSubSystem(SDL_INIT_VIDEO);
		sdl3_quit();
		return 0;
	}

	return 1;
}

static void sdl3_video_quit(void)
{
	sdl3_QuitSubSystem(SDL_INIT_VIDEO);

	sdl3_quit();
}

//////////////////////////////////////////////////////////////////////////////

const schism_video_backend_t schism_video_backend_sdl3 = {
	.init = sdl3_video_init,
	.quit = sdl3_video_quit,

	.startup = sdl3_video_startup,
	.shutdown = sdl3_video_shutdown,

	.is_fullscreen = sdl3_video_is_fullscreen,
	.width = sdl3_video_width,
	.height = sdl3_video_height,
	.driver_name = sdl3_video_driver_name,
	.report = sdl3_video_report,
	.set_hardware = sdl3_video_set_hardware,
	.setup = sdl3_video_setup,
	.fullscreen = sdl3_video_fullscreen,
	.resize = sdl3_video_resize,
	.colors = sdl3_video_colors,
	.is_focused = sdl3_video_is_focused,
	.is_visible = sdl3_video_is_visible,
	.is_wm_available = sdl3_video_is_wm_available,
	.is_hardware = sdl3_video_is_hardware,
	.is_screensaver_enabled = sdl3_video_is_screensaver_enabled,
	.toggle_screensaver = sdl3_video_toggle_screensaver,
	.translate = sdl3_video_translate,
	.get_logical_coordinates = sdl3_video_get_logical_coordinates,
	.is_input_grabbed = sdl3_video_is_input_grabbed,
	.set_input_grabbed = sdl3_video_set_input_grabbed,
	.warp_mouse = sdl3_video_warp_mouse,
	.get_mouse_coordinates = sdl3_video_get_mouse_coordinates,
	.have_menu = sdl3_video_have_menu,
	.toggle_menu = sdl3_video_toggle_menu,
	.blit = sdl3_video_blit,
	.mousecursor_changed = sdl3_video_mousecursor_changed,
	.get_wm_data = sdl3_video_get_wm_data,
	.show_cursor = sdl3_video_show_cursor,
};
