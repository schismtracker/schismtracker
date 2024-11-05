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
#include "config.h"
#include "sdlmain.h"
#include "video.h"
#include "osdefs.h"
#include "vgamem.h"

#include <errno.h>

#include <inttypes.h>

#ifndef SCHISM_MACOSX
#ifdef SCHISM_WIN32
#include "auto/schismico.h"
#else
#include "auto/schismico_hires.h"
#endif
#endif

/* leeto drawing skills */
struct mouse_cursor {
	unsigned int pointer[18];
	unsigned int mask[18];
	unsigned int height, width;
	unsigned int center_x, center_y; /* which point of the pointer does actually point */
};

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

struct video_cf {
	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Texture *texture;

	int width, height;

	struct {
		unsigned int x, y;
		enum video_mousecursor_shape shape;
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

	uint32_t pal[256];
};

/* don't stomp defaults */
static struct video_cf video = {
	.mouse = {
		.visible = MOUSE_EMULATED,
		.shape = CURSOR_SHAPE_ARROW
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

const char *video_driver_name(void)
{
	return SDL_GetCurrentVideoDriver();
}

void video_report(void)
{
	log_append(2, 0, "Video initialised");
	log_underline(17);

	SDL_DisplayMode display = {0};

	Uint32 format;
	SDL_QueryTexture(video.texture, &format, NULL, NULL, NULL);

	{
		SDL_RendererInfo renderer;
		SDL_GetRendererInfo(video.renderer, &renderer);
		log_appendf(5, " Using driver '%s' with renderer '%s'", SDL_GetCurrentVideoDriver(), renderer.name);
	}
	

	log_appendf(5, " Display format: %"PRIu32" bits/pixel", SDL_BITSPERPIXEL(format));

	if (!SDL_GetCurrentDisplayMode(0, &display) && video.fullscreen)
		log_appendf(5, " Display dimensions: %dx%d", display.w, display.h);

	log_nl();
}

void video_redraw_texture(void)
{
	if (video.texture)
		SDL_DestroyTexture(video.texture);
	video.texture = SDL_CreateTexture(video.renderer, SDL_PIXELFORMAT_RGB888, SDL_TEXTUREACCESS_STREAMING, NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT);
}

void video_redraw_renderer(int hardware)
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
	video_redraw_texture();

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

void video_colors(unsigned char palette[16][3])
{
	static const int lastmap[] = { 0, 1, 2, 3, 5 };
	int i, p;

	/* make our "base" space */
	for (i = 0; i < 16; i++) {
		video.pal[i] = palette[i][2]
			| (palette[i][1] << 8)
			| (palette[i][0] << 16);
	}

	/* make our "gradient" space */
	for (i = 0; i < 128; i++) {
		p = lastmap[(i>>5)];

		/* voodoo magic */
		video.pal[i + 128] =
			   ((int)palette[p][2] + (((int)(palette[p+1][2] - palette[p][2]) * (i & 0x1F)) / 0x20))
			| (((int)palette[p][1] + (((int)(palette[p+1][1] - palette[p][1]) * (i & 0x1F)) / 0x20)) << 8)
			| (((int)palette[p][0] + (((int)(palette[p+1][0] - palette[p][0]) * (i & 0x1F)) / 0x20)) << 16);
	}
}

void video_refresh(void)
{
	vgamem_flip();
	vgamem_clear();
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

static inline void make_mouseline(unsigned int x, unsigned int v, unsigned int y, uint32_t mouseline[80], uint32_t mouseline_mask[80])
{
	struct mouse_cursor *cursor = &cursors[video.mouse.shape];

	memset(mouseline,      0, 80 * sizeof(*mouseline));
	memset(mouseline_mask, 0, 80 * sizeof(*mouseline));

	if (video.mouse.visible != MOUSE_EMULATED
		|| !video_is_focused()
		|| (video.mouse.y >= cursor->center_y && y < video.mouse.y - cursor->center_y)
		|| y < cursor->center_y
		|| y >= video.mouse.y + cursor->height - cursor->center_y) {
		return;
	}

	unsigned int scenter = (cursor->center_x / 8) + (cursor->center_x % 8 != 0);
	unsigned int swidth  = (cursor->width    / 8) + (cursor->width    % 8 != 0);
	unsigned int centeroffset = cursor->center_x % 8;

	unsigned int z  = cursor->pointer[y - video.mouse.y + cursor->center_y];
	unsigned int zm = cursor->mask[y - video.mouse.y + cursor->center_y];

	z <<= 8;
	zm <<= 8;
	if (v < centeroffset) {
		z <<= centeroffset - v;
		zm <<= centeroffset - v;
	} else {
		z >>= v - centeroffset;
		zm >>= v - centeroffset;
	}

	// always fill the cell the mouse coordinates are in
	mouseline[x]      = z  >> (8 * (swidth - scenter + 1)) & 0xFF;
	mouseline_mask[x] = zm >> (8 * (swidth - scenter + 1)) & 0xFF;

	// draw the parts of the cursor sticking out to the left
	unsigned int temp = (cursor->center_x < v) ? 0 : ((cursor->center_x - v) / 8) + ((cursor->center_x - v) % 8 != 0);
	for (int i = 1; i <= temp && x >= i; i++) {
		mouseline[x-i]      = z  >> (8 * (swidth - scenter + 1 + i)) & 0xFF;
		mouseline_mask[x-i] = zm >> (8 * (swidth - scenter + 1 + i)) & 0xFF;
	}

	// and to the right
	temp = swidth - scenter + 1;
	for (int i = 1; (i <= temp) && (x + i < 80); i++) {
		mouseline[x+i]      = z  >> (8 * (swidth - scenter + 1 - i)) & 0xff;
		mouseline_mask[x+i] = zm >> (8 * (swidth - scenter + 1 - i)) & 0xff;
	}
}

static void _blit11(unsigned char *pixels, int pitch, unsigned int *tpal)
{
	const unsigned int mouseline_x = (video.mouse.x / 8);
	const unsigned int mouseline_v = (video.mouse.x % 8);
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];

	for (unsigned int y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline, mouseline_mask);
		vgamem_scan32(y, (uint32_t *)pixels, tpal, mouseline, mouseline_mask);
		pixels += pitch;
	}
}

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
	{
		unsigned char *pixels;
		int pitch;

		SDL_LockTexture(video.texture, NULL, (void **)&pixels, &pitch);
		_blit11(pixels, pitch, video.pal);
		SDL_UnlockTexture(video.texture);
	}
	SDL_RenderCopy(video.renderer, video.texture, NULL, (cfg_video_want_fixed) ? &dstrect : NULL);
	SDL_RenderPresent(video.renderer);
}

int video_mousecursor_visible(void)
{
	return video.mouse.visible;
}

void video_set_mousecursor_shape(enum video_mousecursor_shape shape)
{
	video.mouse.shape = shape;
}

void video_mousecursor(int vis)
{
	const char *state[] = {
		"Mouse disabled",
		"Software mouse cursor enabled",
		"Hardware mouse cursor enabled",
	};

	switch (vis) {
	case MOUSE_CYCLE_STATE:
		vis = (video.mouse.visible + 1) % MOUSE_CYCLE_STATE;
		/* fall through */
	case MOUSE_DISABLED:
	case MOUSE_SYSTEM:
	case MOUSE_EMULATED:
		video.mouse.visible = vis;
		status_text_flash("%s", state[video.mouse.visible]);
		break;
	case MOUSE_RESET_STATE:
		break;
	default:
		video.mouse.visible = MOUSE_EMULATED;
		break;
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
