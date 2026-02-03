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

#include "init.h"

#define NATIVE_SCREEN_WIDTH		640
#define NATIVE_SCREEN_HEIGHT	400
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
static void (SDLCALL *sdl2_StartTextInput)(void);

static int (SDLCALL *sdl2_LockSurface)(SDL_Surface * surface);
static void (SDLCALL *sdl2_UnlockSurface)(SDL_Surface * surface);

// Introduced with SDL 2.0.4
#if !SDL_VERSION_ATLEAST(2, 0, 4)
#define SDL_PIXELFORMAT_NV12 (SDL_DEFINE_PIXELFOURCC('N', 'V', '1', '2'))
#define SDL_PIXELFORMAT_NV21 (SDL_DEFINE_PIXELFOURCC('N', 'V', '2', '1'))
#endif

// SDL_SetTextureScaleMode: introduced in SDL 2.0.12
#if !SDL_VERSION_ATLEAST(2, 0, 12)
# define SDL_ScaleModeNearest (0)
# define SDL_ScaleModeLinear  (1)
# define SDL_ScaleModeBest    (2)
typedef int SDL_ScaleMode;
#endif

static int (SDLCALL *sdl2_SetTextureScaleMode)(SDL_Texture *texture, SDL_ScaleMode scaleMode);

// ONLY in SDL 2.0.18 and newer
static void (SDLCALL *sdl2_RenderWindowToLogical)(SDL_Renderer * renderer, int windowX, int windowY, float *logicalX, float *logicalY);

// Only used in SDL >= 2.30.5, because of a bug in prior versions that makes
// SDL_DestroyWindowRenderer completely useless for our use case
//
// See https://github.com/libsdl-org/SDL/issues/10133
static SDL_Surface *(SDLCALL *sdl2_GetWindowSurface)(SDL_Window *window);
static int (SDLCALL *sdl2_UpdateWindowSurface)(SDL_Window * window);
static SDL_bool (SDLCALL *sdl2_HasWindowSurface)(SDL_Window *window);
static int (SDLCALL *sdl2_DestroyWindowSurface)(SDL_Window *window);

static struct {
	/* The actual window that we blit to */
	SDL_Window *window;

	/* This is the magic switch that changes whether we use surfaces,
	 * renderers, or some other unknown magic thing in the future.
	 *
	 * 0 is reserved strictly for "i am not initialized yet do not
	 * touch me please" */
	enum {
		VIDEO_TYPE_UNINITIALIZED = 0,
		VIDEO_TYPE_SURFACE,
		VIDEO_TYPE_RENDERER,
	} type;

	/* Data for each type of thing */
	union {
		struct {
			SDL_Renderer *renderer;
			SDL_Texture *texture;
		} r;
		struct {
			struct {
				uint32_t x, y, w, h;
			} clip;
			SDL_Surface *surface;
		} s;
	} u;

	SDL_PixelFormat *pixel_format; // may be NULL for YUV formats
	uint32_t format; // SDL pixel format identifier (I hate this)
	uint32_t bpp; // BYTES per pixel

	int width, height;

	struct {
		/* TODO: need to save the state of the menu bar or else
		 * these will be wrong if it's toggled while in fullscreen */
		int width, height, x, y;
	} saved;

	int fullscreen;

	unsigned int yuv_tv : 1;

	uint32_t pal[256];
} video = {0};

// Native formats, in order of preference.
static const struct {
	uint32_t format;
	const char *name;
} native_formats[] = {
	// RGB
	// ------------------------------------
	{SDL_PIXELFORMAT_RGB888, "RGB888"},
	{SDL_PIXELFORMAT_ARGB8888, "ARGB8888"},
	{SDL_PIXELFORMAT_RGB24, "RGB24"},
	{SDL_PIXELFORMAT_RGB565, "RGB565"},
	{SDL_PIXELFORMAT_RGB555, "RGB555"},
	{SDL_PIXELFORMAT_ARGB1555, "ARGB1555"},
	{SDL_PIXELFORMAT_RGB444, "RGB444"},
	{SDL_PIXELFORMAT_ARGB4444, "ARGB4444"},
	{SDL_PIXELFORMAT_RGB332, "RGB332"},
	// ------------------------------------

	// YUV
	// ------------------------------------
	{SDL_PIXELFORMAT_IYUV, "IYUV"},
	{SDL_PIXELFORMAT_YV12, "YV12"},
	// {SDL_PIXELFORMAT_UYVY, "UYVY"},
	// {SDL_PIXELFORMAT_YVYU, "YVYU"},
	{SDL_PIXELFORMAT_YUY2, "YUY2"},
	// {SDL_PIXELFORMAT_NV12, "NV12"},
	// {SDL_PIXELFORMAT_NV21, "NV21"},
	// ------------------------------------
};

static int sdl2_video_is_fullscreen(void)
{
	return video.fullscreen;
}

static int sdl2_video_width(void)
{
	return video.width;
}

static int sdl2_video_height(void)
{
	return video.height;
}

static const char *sdl2_video_driver_name(void)
{
	return sdl2_GetCurrentVideoDriver();
}

static void sdl2_video_report(void)
{
	log_appendf(5, " Using driver '%s'", sdl2_GetCurrentVideoDriver());

	switch (video.type) {
	case VIDEO_TYPE_RENDERER: {
		SDL_RendererInfo renderer;
		sdl2_GetRendererInfo(video.u.r.renderer, &renderer);

		log_appendf(5, " %sware%s renderer '%s'",
			(renderer.flags & SDL_RENDERER_SOFTWARE) ? "Soft" : "Hard",
			(renderer.flags & SDL_RENDERER_ACCELERATED) ? "-accelerated" : "",
			renderer.name);
		break;
	}
	case VIDEO_TYPE_SURFACE:
		/* these should always be in software */
		log_appendf(5, " Software video surface");
		if (SDL_MUSTLOCK(video.u.s.surface))
			log_append(4, 0, " Must lock surface");
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		/* ? */
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
		video_yuv_report();
		break;
	default:
		log_appendf(5, " Display format: %" PRIu32 " bits/pixel", SDL_BITSPERPIXEL(video.format));
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

static inline void video_recalculate_fixed_width(void)
{
	/* Aspect ratio correction if it's wanted */
	switch (video.type) {
	case VIDEO_TYPE_RENDERER:
		if (cfg_video_want_fixed)
			sdl2_RenderSetLogicalSize(video.u.r.renderer, cfg_video_want_fixed_width, cfg_video_want_fixed_height);
		break;
	case VIDEO_TYPE_SURFACE:
		video_calculate_clip(video.u.s.surface->w, video.u.s.surface->h,
			&video.u.s.clip.x, &video.u.s.clip.y, &video.u.s.clip.w, &video.u.s.clip.h);
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		SCHISM_UNREACHABLE;
		break;
	}
}

static void video_redraw_texture(void)
{
	if (video.pixel_format)
		sdl2_FreeFormat(video.pixel_format);

	switch (video.type) {
	case VIDEO_TYPE_RENDERER: {
		size_t pref_last = ARRAY_SIZE(native_formats);
		uint32_t format = SDL_PIXELFORMAT_RGB888;
		uint32_t twidth;
		uint32_t theight;

		if (*cfg_video_format) {
			size_t i;

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
		if (!sdl2_GetRendererInfo(video.u.r.renderer, &info)) {
			uint32_t i;
			size_t j;

			for (i = 0; i < info.num_texture_formats; i++)
				for (j = 0; j < ARRAY_SIZE(native_formats); j++)
					if (info.texture_formats[i] == native_formats[j].format && j < pref_last)
						format = native_formats[pref_last = j].format;
		}

got_format:
		/* option? */
		video.yuv_tv = 0;

		twidth = NATIVE_SCREEN_WIDTH;
		theight = NATIVE_SCREEN_HEIGHT;

		switch (format) {
		case SDL_PIXELFORMAT_NV12:
		case SDL_PIXELFORMAT_NV21:
			video.yuv_tv = 0;
			SCHISM_FALLTHROUGH;
		case SDL_PIXELFORMAT_IYUV:
		case SDL_PIXELFORMAT_YV12:
			if (!video.yuv_tv) {
				twidth *= 2;
				theight *= 2;
			}
			break;
		}

		switch (format) {
		case SDL_PIXELFORMAT_NV12:
			video_yuv_setformat(VIDEO_YUV_NV12);
			break;
		case SDL_PIXELFORMAT_NV21:
			video_yuv_setformat(VIDEO_YUV_NV21);
			break;
		case SDL_PIXELFORMAT_IYUV:
			video_yuv_setformat(video.yuv_tv ? VIDEO_YUV_IYUV_TV : VIDEO_YUV_IYUV);
			break;
		case SDL_PIXELFORMAT_YV12:
			video_yuv_setformat(video.yuv_tv ? VIDEO_YUV_YV12_TV : VIDEO_YUV_YV12);
			break;
		}

		video.u.r.texture = sdl2_CreateTexture(video.u.r.renderer, format, SDL_TEXTUREACCESS_STREAMING, twidth, theight);
		video.format = format;
		break;
	}
	case VIDEO_TYPE_SURFACE:
		video.format = video.u.s.surface->format->format;
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		/* ? */
		break;
	}

	video.pixel_format = sdl2_AllocFormat(video.format);

	// ok
	video.bpp = SDL_BYTESPERPIXEL(video.format);

	video_recalculate_fixed_width();
}

static void sdl2_video_set_hardware(int hardware)
{
	int ask_for_no_acceleration = (!hardware && !getenv("SDL_FRAMEBUFFER_ACCELERATION"));

	/* do all the necessary cleanup HERE */
	switch (video.type) {
	case VIDEO_TYPE_RENDERER:
		if (video.u.r.texture)
			sdl2_DestroyTexture(video.u.r.texture);
		if (video.u.r.renderer)
			sdl2_DestroyRenderer(video.u.r.renderer);
		break;
	case VIDEO_TYPE_SURFACE:
		SCHISM_RUNTIME_ASSERT(sdl2_HasWindowSurface(video.window), "Internal video type says surface, but the window doesn't have one");
		sdl2_DestroyWindowSurface(video.window);
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		break;
	}

	/* Check the actual SDL version. If it's lower than we want, just use
	 * the render backend with the software renderer. It does exactly what
	 * we do, just with one more layer of indirection. */
	video.type = (!hardware && sdl2_ver_atleast(2, 30, 5))
		? (VIDEO_TYPE_SURFACE)
		: (VIDEO_TYPE_RENDERER);

	/* There is no way to clear an SDL hint in SDL < 2.24.0, UGH! */
	if (ask_for_no_acceleration)
		setenv("SDL_FRAMEBUFFER_ACCELERATION", "0", 1);

	switch (video.type) {
	case VIDEO_TYPE_SURFACE:
		video.u.s.surface = sdl2_GetWindowSurface(video.window);
		if (video.u.s.surface)
			break;
		// Else fall through to renderer implementation.
		video.type = VIDEO_TYPE_RENDERER;
		SCHISM_FALLTHROUGH;
	case VIDEO_TYPE_RENDERER:
		video.u.r.renderer = sdl2_CreateRenderer(video.window, -1, (hardware) ? SDL_RENDERER_ACCELERATED : SDL_RENDERER_SOFTWARE);
		if (!video.u.r.renderer)
			video.u.r.renderer = sdl2_CreateRenderer(video.window, -1, 0); // welp
		SCHISM_RUNTIME_ASSERT(!!video.u.r.renderer, "Failed to create a renderer!");
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		/* should never ever happen */
		break;
	}

	if (ask_for_no_acceleration)
		unsetenv("SDL_FRAMEBUFFER_ACCELERATION");

	video_redraw_texture();

	video_report();
}

static void sdl2_video_shutdown(void)
{
	/* do all the necessary cleanup HERE */
	switch (video.type) {
	case VIDEO_TYPE_RENDERER:
		if (video.u.r.texture)
			sdl2_DestroyTexture(video.u.r.texture);
		if (video.u.r.renderer)
			sdl2_DestroyRenderer(video.u.r.renderer);
		break;
	case VIDEO_TYPE_SURFACE:
		SCHISM_RUNTIME_ASSERT(sdl2_HasWindowSurface(video.window), "Internal video type says surface, but window doesn't have one");
		sdl2_DestroyWindowSurface(video.window);
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		break;
	}

	sdl2_DestroyWindow(video.window);
}

static void sdl2_video_setup(int quality)
{
	/* hint for later, in case we switch from software -> hardware */
	static const char *names[] = {
		[VIDEO_INTERPOLATION_NEAREST] = "nearest",
		[VIDEO_INTERPOLATION_LINEAR]  = "linear",
		[VIDEO_INTERPOLATION_BEST]    = "best",
	};

	sdl2_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, names[quality]);

	switch (video.type) {
	case VIDEO_TYPE_RENDERER: {
#if defined(SDL2_DYNAMIC_LOAD) || SDL_VERSION_ATLEAST(2, 0, 12)
		static const SDL_ScaleMode modes[] = {
			[VIDEO_INTERPOLATION_NEAREST] = SDL_ScaleModeNearest,
			[VIDEO_INTERPOLATION_LINEAR]  = SDL_ScaleModeLinear,
			[VIDEO_INTERPOLATION_BEST]    = SDL_ScaleModeBest,
		};

		if (!sdl2_SetTextureScaleMode || sdl2_SetTextureScaleMode(video.u.r.texture, modes[quality]))
#endif
		{
			// this is stupid
			if (video.u.r.texture)
				sdl2_DestroyTexture(video.u.r.texture);

			video_redraw_texture();
		}
		break;
	}
	case VIDEO_TYPE_SURFACE:
		// nothing to do
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		break;
	}
}

static int sdl2_video_startup(void)
{
	vgamem_clear();
	vgamem_flip();

	video_setup(cfg_video_interpolation);

	video.width = cfg_video_width;
	video.height = cfg_video_height;
	video.saved.x = video.saved.y = SDL_WINDOWPOS_CENTERED;

	video.window = sdl2_CreateWindow(WINDOW_TITLE, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, video.width, video.height, SDL_WINDOW_RESIZABLE);
	if (!video.window)
		return 0;

	video_fullscreen(cfg_video_fullscreen);
	video_set_hardware(cfg_video_hardware);

	if (video_have_menu() && !video.fullscreen) {
		sdl2_SetWindowSize(video.window, video.width, video.height);
		sdl2_SetWindowPosition(video.window, video.saved.x, video.saved.y);
	}

	/* okay, i think we're ready */
	sdl2_StartTextInput();
	sdl2_ShowCursor(SDL_DISABLE);
	set_icon();

	return 1;
}

static void sdl2_video_fullscreen(int new_fs_flag)
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

static void sdl2_video_resize(uint32_t width, uint32_t height)
{
	video.width = width;
	video.height = height;
	if (video.type == VIDEO_TYPE_SURFACE)
		video.u.s.surface = sdl2_GetWindowSurface(video.window);
	video_recalculate_fixed_width();
}

static void sdl_pal_(unsigned int i, unsigned char rgb[3])
{
	video.pal[i] = sdl2_MapRGB(video.pixel_format, rgb[0], rgb[1], rgb[2]);
}

static void sdl2_video_colors(unsigned char palette[16][3])
{
	switch (video.format) {
	case SDL_PIXELFORMAT_IYUV:
	case SDL_PIXELFORMAT_YV12:
	case SDL_PIXELFORMAT_NV12:
	case SDL_PIXELFORMAT_NV21:
		video_colors_iterate(palette, video_yuv_pal);
		break;
	default:
		video_colors_iterate(palette, sdl_pal_);
		break;
	}
}

static int sdl2_video_is_focused(void)
{
	return !!(sdl2_GetWindowFlags(video.window) & SDL_WINDOW_INPUT_FOCUS);
}

static int sdl2_video_is_visible(void)
{
	return !!(sdl2_GetWindowFlags(video.window) & SDL_WINDOW_SHOWN);
}

static int sdl2_video_is_wm_available(void)
{
	SDL_SysWMinfo info;
	SDL_VERSION(&info.version);

	return !!sdl2_GetWindowWMInfo(video.window, &info);
}

static int sdl2_video_is_hardware(void)
{
	SDL_RendererInfo info;

	switch (video.type) {
	case VIDEO_TYPE_RENDERER:
		sdl2_GetRendererInfo(video.u.r.renderer, &info);
		return !!(info.flags & SDL_RENDERER_ACCELERATED);
	case VIDEO_TYPE_SURFACE:
		// never
		return 0;
	case VIDEO_TYPE_UNINITIALIZED:
		break;
	}

	// WUT?
	return 0;
}

/* -------------------------------------------------------- */

static int sdl2_video_is_screensaver_enabled(void)
{
	return sdl2_IsScreenSaverEnabled();
}

static void sdl2_video_toggle_screensaver(int enabled)
{
	if (enabled) sdl2_EnableScreenSaver();
	else sdl2_DisableScreenSaver();
}

/* ---------------------------------------------------------- */
/* coordinate translation */

static void sdl2_video_translate(uint32_t vx, uint32_t vy, uint32_t *x, uint32_t *y)
{
	uint32_t cx, cy, cw, ch;

	switch (video.type) {
	case VIDEO_TYPE_SURFACE:
		cx = video.u.s.clip.x;
		cy = video.u.s.clip.y;
		cw = video.u.s.clip.w;
		ch = video.u.s.clip.h;
		break;
	case VIDEO_TYPE_RENDERER:
		cx = cy = 0;
		cw = (cfg_video_want_fixed) ? cfg_video_want_fixed_width  : video.width;
		ch = (cfg_video_want_fixed) ? cfg_video_want_fixed_height : video.height;
		break;
	case VIDEO_TYPE_UNINITIALIZED:
	default:
		SCHISM_UNREACHABLE;
		return;
	}

	video_translate_calculate(vx, vy,
		/* clip rect */
		cx, cy, cw, ch,
		/* return pointers */
		x, y);
}

/* -------------------------------------------------- */
/* input grab */

static int sdl2_video_is_input_grabbed(void)
{
	return !!sdl2_GetWindowGrab(video.window);
}

static void sdl2_video_set_input_grabbed(int enabled)
{
	sdl2_SetWindowGrab(video.window, enabled ? SDL_TRUE : SDL_FALSE);
}

/* -------------------------------------------------- */
/* warp mouse position */

static void sdl2_video_warp_mouse(uint32_t x, uint32_t y)
{
	sdl2_WarpMouseInWindow(video.window, x, y);
}

/* -------------------------------------------------- */
/* menu toggling */

static int sdl2_video_have_menu(void)
{
#ifdef SCHISM_WIN32
	return 1;
#else
	return 0;
#endif
}

static void sdl2_video_toggle_menu(SCHISM_UNUSED int on)
{
	if (!sdl2_video_have_menu())
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

SCHISM_HOT static uint32_t sdl2_map_rgb_callback(void *data, uint8_t r, uint8_t g, uint8_t b)
{
	SDL_PixelFormat *format = (SDL_PixelFormat *)data;

	return sdl2_MapRGB(format, r, g, b);
}

SCHISM_HOT static void sdl2_video_blit(void)
{
	unsigned char *pixels;
	int pitch;

	switch (video.type) {
	case VIDEO_TYPE_RENDERER: {
		SDL_Rect dstrect;

		if (cfg_video_want_fixed) {
			dstrect.x = 0;
			dstrect.y = 0;
			dstrect.w = cfg_video_want_fixed_width;
			dstrect.h = cfg_video_want_fixed_height;
		}

		sdl2_RenderClear(video.u.r.renderer);

		sdl2_LockTexture(video.u.r.texture, NULL, (void **)&pixels, &pitch);

		switch (video.format) {
		case SDL_PIXELFORMAT_IYUV:
		case SDL_PIXELFORMAT_YV12:
		case SDL_PIXELFORMAT_NV12:
		case SDL_PIXELFORMAT_NV21:
			video_yuv_blit_sequenced(pixels, pitch);
			break;
		default:
			video_blit11(video.bpp, pixels, pitch, video.pal);
			break;
		}
		sdl2_UnlockTexture(video.u.r.texture);
		sdl2_RenderCopy(video.u.r.renderer, video.u.r.texture, NULL, (cfg_video_want_fixed) ? &dstrect : NULL);
		sdl2_RenderPresent(video.u.r.renderer);
		break;
	}
	case VIDEO_TYPE_SURFACE:
		if (SDL_MUSTLOCK(video.u.s.surface))
			while (sdl2_LockSurface(video.u.s.surface) < 0)
				timer_msleep(10);

		video_blitSC(video.u.s.surface->format->BytesPerPixel,
			video.u.s.surface->pixels,
			video.u.s.surface->pitch,
			video.pal,
			sdl2_map_rgb_callback,
			video.pixel_format,
			video.u.s.clip.x,
			video.u.s.clip.y,
			video.u.s.clip.w,
			video.u.s.clip.h);

		if (SDL_MUSTLOCK(video.u.s.surface))
			sdl2_UnlockSurface(video.u.s.surface);

		sdl2_UpdateWindowSurface(video.window);
		break;
	case VIDEO_TYPE_UNINITIALIZED:
		break;
	}
}

/* ------------------------------------------------- */

static void sdl2_video_mousecursor_changed(void)
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

	switch (info.subsystem) {
#ifdef SDL_VIDEO_DRIVER_WINDOWS
	case SDL_SYSWM_WINDOWS:
		wm_data->subsystem = VIDEO_WM_DATA_SUBSYSTEM_WINDOWS;
		wm_data->data.windows.hwnd = info.info.win.window;
		break;
#endif
#ifdef SDL_VIDEO_DRIVER_X11
	case SDL_SYSWM_X11:
		wm_data->subsystem = VIDEO_WM_DATA_SUBSYSTEM_X11;
		wm_data->data.x11.display = info.info.x11.display;
		wm_data->data.x11.window = info.info.x11.window;
		wm_data->data.x11.lock_func = NULL;
		wm_data->data.x11.unlock_func = NULL;
		break;
#endif
	default:
		return 0;
	}

	return 1;
}

static void sdl2_video_show_cursor(int enabled)
{
	sdl2_ShowCursor(enabled ? SDL_ENABLE : SDL_DISABLE);
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
	SCHISM_SDL2_SYM(GetWindowSurface);
	SCHISM_SDL2_SYM(UpdateWindowSurface);
	SCHISM_SDL2_SYM(LockSurface);
	SCHISM_SDL2_SYM(UnlockSurface);
	SCHISM_SDL2_SYM(StartTextInput);

	return 0;
}

static int sdl_video_load_optional_syms(void)
{
#if SDL2_DYNAMIC_LOAD || SDL_VERSION_ATLEAST(2, 0, 12)
	SCHISM_SDL2_SYM(SetTextureScaleMode);
#endif
#if SDL2_DYNAMIC_LOAD || SDL_VERSION_ATLEAST(2, 0, 18)
	SCHISM_SDL2_SYM(RenderWindowToLogical);
#endif
#if SDL2_DYNAMIC_LOAD || SDL_VERSION_ATLEAST(2, 28, 0)
	SCHISM_SDL2_SYM(HasWindowSurface);
	SCHISM_SDL2_SYM(DestroyWindowSurface);
#endif

	return 0;
}

static int sdl2_video_init(void)
{
	if (!sdl2_init())
		return 0;

	if (sdl2_video_load_syms()) {
		sdl2_quit();
		return 0;
	}

	//  :)
	sdl_video_load_optional_syms();

	/* cruft: have to do this before initializing video */
#ifndef SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR
/* older SDL2 versions don't define this, don't fail the build for it */
#define SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR "SDL_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR"
#endif
	sdl2_SetHint(SDL_HINT_VIDEO_X11_NET_WM_BYPASS_COMPOSITOR, "0");
	sdl2_SetHint("SDL_WINDOWS_NO_CLOSE_ON_ALT_F4", "1"); /* dunno if the hint is defined for old SDL, optional anyway */
	sdl2_SetHint("SDL_WINDOWS_DPI_SCALING", "1");
	sdl2_SetHint("SDL_WINDOWS_DPI_AWARENESS", "permonitorv2");
	sdl2_SetHint("SDL_ENABLE_SCREEN_KEYBOARD", "0");
	sdl2_SetHint("SDL_GRAB_KEYBOARD", "1");

	if (sdl2_InitSubSystem(SDL_INIT_VIDEO) < 0) {
		sdl2_quit();
		return 0;
	}

	if (!events_init(&schism_events_backend_sdl2)) {
		sdl2_QuitSubSystem(SDL_INIT_VIDEO);
		sdl2_quit();
		return 0;
	}

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
	.is_input_grabbed = sdl2_video_is_input_grabbed,
	.set_input_grabbed = sdl2_video_set_input_grabbed,
	.warp_mouse = sdl2_video_warp_mouse,
	.have_menu = sdl2_video_have_menu,
	.toggle_menu = sdl2_video_toggle_menu,
	.blit = sdl2_video_blit,
	.mousecursor_changed = sdl2_video_mousecursor_changed,
	.get_wm_data = sdl2_video_get_wm_data,
	.show_cursor = sdl2_video_show_cursor,
};
