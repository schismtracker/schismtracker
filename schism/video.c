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

#include "backend/video.h"

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
	struct {
		enum video_mousecursor_shape shape;
		int visible;
	} mouse;
} video = {
	.mouse = {
		.visible = MOUSE_EMULATED,
		.shape = CURSOR_SHAPE_ARROW,
	},
};

static const schism_video_backend_t *backend = NULL;

/* ----------------------------------------------------------- */

void video_rgb_to_yuv(unsigned int *y, unsigned int *u, unsigned int *v, unsigned char rgb[3])
{
	// YCbCr
	*y =  0.257 * rgb[0] + 0.504 * rgb[1] + 0.098 * rgb[2] +  16;
	*u = -0.148 * rgb[0] - 0.291 * rgb[1] + 0.439 * rgb[2] + 128;
	*v =  0.439 * rgb[0] - 0.368 * rgb[1] - 0.071 * rgb[2] + 128;
}

void video_refresh(void)
{
	vgamem_flip();
	vgamem_clear();
}

/* -------------------------------------------------- */

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

	backend->mousecursor_changed();
}

/* -------------------------------------------------- */
/* mouse drawing */

static inline void make_mouseline(unsigned int x, unsigned int v, unsigned int y, uint32_t mouseline[80], uint32_t mouseline_mask[80], unsigned int mouse_y)
{
	struct mouse_cursor *cursor = &cursors[video.mouse.shape];

	memset(mouseline,      0, 80 * sizeof(*mouseline));
	memset(mouseline_mask, 0, 80 * sizeof(*mouseline));

	video_mousecursor_visible();

	if (video_mousecursor_visible() != MOUSE_EMULATED
		|| !video_is_focused()
		|| (mouse_y >= cursor->center_y && y < mouse_y - cursor->center_y)
		|| y < cursor->center_y
		|| y >= mouse_y + cursor->height - cursor->center_y) {
		return;
	}

	unsigned int scenter = (cursor->center_x / 8) + (cursor->center_x % 8 != 0);
	unsigned int swidth  = (cursor->width    / 8) + (cursor->width    % 8 != 0);
	unsigned int centeroffset = cursor->center_x % 8;

	unsigned int z  = cursor->pointer[y - mouse_y + cursor->center_y];
	unsigned int zm = cursor->mask[y - mouse_y + cursor->center_y];

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

/* --------------------------------------------------------------- */
/* blitters */

/* Video with linear interpolation. */
#define FIXED_BITS 8
#define FIXED_MASK ((1 << FIXED_BITS) - 1)
#define ONE_HALF_FIXED (1 << (FIXED_BITS - 1))
#define INT2FIXED(x) ((x) << FIXED_BITS)
#define FIXED2INT(x) ((x) >> FIXED_BITS)
#define FRAC(x) ((x) & FIXED_MASK)

void video_blitLN(unsigned int bpp, unsigned char *pixels, unsigned int pitch, uint32_t pal[256], int width, int height, schism_map_rgb_func_t map_rgb, void *map_rgb_data)
{
	unsigned char cv32backing[NATIVE_SCREEN_WIDTH * 8];

	uint32_t *csp, *esp, *dp;
	unsigned int c00, c01, c10, c11;
	unsigned int outr, outg, outb;
	unsigned int pad;
	int fixedx, fixedy, scalex, scaley;
	unsigned int y, x,ey,ex,t1,t2;
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];
	unsigned int mouseline_x, mouseline_v;
	int iny, lasty;

	unsigned int mouse_x, mouse_y;
	video_get_mouse_coordinates(&mouse_x, &mouse_y);

	mouseline_x = (mouse_x / 8);
	mouseline_v = (mouse_x % 8);

	csp = (uint32_t *)cv32backing;
	esp = csp + NATIVE_SCREEN_WIDTH;
	lasty = -2;
	iny = 0;
	pad = pitch - (width * bpp);
	scalex = INT2FIXED(NATIVE_SCREEN_WIDTH-1) / width;
	scaley = INT2FIXED(NATIVE_SCREEN_HEIGHT-1) / height;
	for (y = 0, fixedy = 0; (y < height); y++, fixedy += scaley) {
		iny = FIXED2INT(fixedy);
		if (iny != lasty) {
			make_mouseline(mouseline_x, mouseline_v, iny, mouseline, mouseline_mask, mouse_y);

			/* we'll downblit the colors later */
			if (iny == lasty + 1) {
				/* move up one line */
				vgamem_scan32(iny+1, csp, pal, mouseline, mouseline_mask);
				dp = esp; esp = csp; csp=dp;
			} else {
				vgamem_scan32(iny, (csp = (uint32_t *)cv32backing), pal, mouseline, mouseline_mask);
				vgamem_scan32(iny+1, (esp = (csp + NATIVE_SCREEN_WIDTH)), pal, mouseline, mouseline_mask);
			}
			lasty = iny;
		}
		for (x = 0, fixedx = 0; x < width; x++, fixedx += scalex) {
			ex = FRAC(fixedx);
			ey = FRAC(fixedy);

			c00 = csp[FIXED2INT(fixedx)];
			c01 = csp[FIXED2INT(fixedx) + 1];
			c10 = esp[FIXED2INT(fixedx)];
			c11 = esp[FIXED2INT(fixedx) + 1];

#if FIXED_BITS <= 8
			/* When there are enough bits between blue and
			 * red, do the RB channels together
			 * See http://www.virtualdub.org/blog/pivot/entry.php?id=117
			 * for a quick explanation */
#define REDBLUE(Q) ((Q) & 0x00FF00FF)
#define GREEN(Q) ((Q) & 0x0000FF00)
			t1 = REDBLUE((((REDBLUE(c01)-REDBLUE(c00))*ex) >> FIXED_BITS)+REDBLUE(c00));
			t2 = REDBLUE((((REDBLUE(c11)-REDBLUE(c10))*ex) >> FIXED_BITS)+REDBLUE(c10));
			outb = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

			t1 = GREEN((((GREEN(c01)-GREEN(c00))*ex) >> FIXED_BITS)+GREEN(c00));
			t2 = GREEN((((GREEN(c11)-GREEN(c10))*ex) >> FIXED_BITS)+GREEN(c10));
			outg = (((((t2-t1)*ey) >> FIXED_BITS) + t1) >> 8) & 0xFF;

			outr = (outb >> 16) & 0xFF;
			outb &= 0xFF;
#undef REDBLUE
#undef GREEN
#else
#define BLUE(Q) (Q & 255)
#define GREEN(Q) ((Q >> 8) & 255)
#define RED(Q) ((Q >> 16) & 255)
			t1 = ((((BLUE(c01)-BLUE(c00))*ex) >> FIXED_BITS)+BLUE(c00)) & 0xFF;
			t2 = ((((BLUE(c11)-BLUE(c10))*ex) >> FIXED_BITS)+BLUE(c10)) & 0xFF;
			outb = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

			t1 = ((((GREEN(c01)-GREEN(c00))*ex) >> FIXED_BITS)+GREEN(c00)) & 0xFF;
			t2 = ((((GREEN(c11)-GREEN(c10))*ex) >> FIXED_BITS)+GREEN(c10)) & 0xFF;
			outg = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

			t1 = ((((RED(c01)-RED(c00))*ex) >> FIXED_BITS)+RED(c00)) & 0xFF;
			t2 = ((((RED(c11)-RED(c10))*ex) >> FIXED_BITS)+RED(c10)) & 0xFF;
			outr = ((((t2-t1)*ey) >> FIXED_BITS) + t1);
#undef RED
#undef GREEN
#undef BLUE
#endif

			uint32_t c = map_rgb(map_rgb_data, outr, outg, outb);

			/* write the output pixel */
#if WORDS_BIGENDIAN
			memcpy(pixels, ((unsigned char *)&c) + (4 - bpp), bpp);
#else
			memcpy(pixels, &c, bpp);
#endif
			pixels += bpp;
		}
		pixels += pad;
	}
}

/* Nearest neighbor blitter */
void video_blitNN(unsigned int bpp, unsigned char *pixels, unsigned int pitch, uint32_t tpal[256], int width, int height)
{
	// at most 32-bits...
	unsigned char pixels_u[NATIVE_SCREEN_WIDTH * 4];

	unsigned int mouse_x, mouse_y;
	video_get_mouse_coordinates(&mouse_x, &mouse_y);

	unsigned int mouseline_x = (mouse_x / 8);
	unsigned int mouseline_v = (mouse_x % 8);
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];
	int x, a, y, last_scaled_y;
	int pad = pitch - (width * bpp);

	for (y = 0; y < height; y++) {
		int scaled_y = (y * NATIVE_SCREEN_HEIGHT / height);

		// only scan again if we have to or if this the first scan
		if (scaled_y != last_scaled_y || y == 0) {
			make_mouseline(mouseline_x, mouseline_v, scaled_y, mouseline, mouseline_mask, mouse_y);
			switch (bpp) {
			case 1:
				vgamem_scan8(scaled_y, (uint8_t *)pixels_u, tpal, mouseline, mouseline_mask);
				break;
			case 2:
				vgamem_scan16(scaled_y, (uint16_t *)pixels_u, tpal, mouseline, mouseline_mask);
				break;
			case 3:
				vgamem_scan32(scaled_y, (uint32_t *)pixels_u, tpal, mouseline, mouseline_mask);
				for (x = 0; x < NATIVE_SCREEN_WIDTH; x++) {
					/* move these into place */
#if WORDS_BIGENDIAN
					memmove(pixels_u + (x * 3), (pixels_u + (x * 4)) + 1, 3);
#else
					memmove(pixels_u + (x * 3), pixels_u + (x * 4), 3);
#endif
				}
				break;
			case 4:
				vgamem_scan32(scaled_y, (uint32_t *)pixels_u, tpal, mouseline, mouseline_mask);
				break;
			default:
				// should never happen
				break;
			}
		}

		for (x = 0; x < width; x++) {
			memcpy(pixels, pixels_u + ((x * NATIVE_SCREEN_WIDTH / width) * bpp), bpp);

			pixels += bpp;
		}

		last_scaled_y = scaled_y;

		pixels += pad;
	}
}

void video_blitYY(unsigned char *pixels, unsigned int pitch, uint32_t tpal[256])
{
	// this is here because pixels is write only on SDL2
	uint16_t pixels_r[NATIVE_SCREEN_WIDTH];

	unsigned int mouse_x, mouse_y;
	video_get_mouse_coordinates(&mouse_x, &mouse_y);

	unsigned int mouseline_x = (mouse_x / 8);
	unsigned int mouseline_v = (mouse_x % 8);
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];
	int y;

	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline, mouseline_mask, mouse_y);

		vgamem_scan16(y, pixels_r, tpal, mouseline, mouseline_mask);
		memcpy(pixels, pixels_r, pitch);
		pixels += pitch;
		memcpy(pixels, pixels_r, pitch);
		pixels += pitch;
	}
}

void video_blitUV(unsigned char *pixels, unsigned int pitch, uint32_t tpal[256])
{
	unsigned int mouse_x, mouse_y;
	video_get_mouse_coordinates(&mouse_x, &mouse_y);

	const unsigned int mouseline_x = (mouse_x / 8);
	const unsigned int mouseline_v = (mouse_x % 8);
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];

	int y;
	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline, mouseline_mask, mouse_y);
		vgamem_scan8(y, pixels, tpal, mouseline, mouseline_mask);
		pixels += pitch;
	}
}

void video_blitTV(unsigned char *pixels, unsigned int pitch, uint32_t tpal[256])
{
	unsigned int mouse_x, mouse_y;
	video_get_mouse_coordinates(&mouse_x, &mouse_y);

	const unsigned int mouseline_x = (mouse_x / 8);
	const unsigned int mouseline_v = (mouse_x % 8);
	unsigned char cv8backing[NATIVE_SCREEN_WIDTH];
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];
	int y, x;

	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y += 2) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline, mouseline_mask, mouse_y);
		vgamem_scan8(y, cv8backing, tpal, mouseline, mouseline_mask);
		for (x = 0; x < pitch; x += 2)
			*pixels++ = cv8backing[x+1] | (cv8backing[x] << 4);
	}
}

void video_blit11(unsigned int bpp, unsigned char *pixels, unsigned int pitch, uint32_t tpal[256])
{
	uint32_t cv32backing[NATIVE_SCREEN_WIDTH];

	unsigned int mouse_x, mouse_y;
	video_get_mouse_coordinates(&mouse_x, &mouse_y);

	const unsigned int mouseline_x = (mouse_x / 8);
	const unsigned int mouseline_v = (mouse_x % 8);
	unsigned int y, x, a;
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];

	/* wtf */
	if (bpp == 3) {
		int pitch24 = pitch - (NATIVE_SCREEN_WIDTH * 3);
		if (pitch24 < 0)
			return; /* eh? */

		pitch = pitch24;
	}

	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline, mouseline_mask, mouse_y);
		switch (bpp) {
		case 1:
			vgamem_scan8(y, (uint8_t *)pixels, tpal, mouseline, mouseline_mask);
			break;
		case 2:
			vgamem_scan16(y, (uint16_t *)pixels, tpal, mouseline, mouseline_mask);
			break;
		case 3:
			vgamem_scan32(y, (uint32_t *)cv32backing, tpal, mouseline, mouseline_mask);
			for (x = 0, a = 0; x < pitch && a < NATIVE_SCREEN_WIDTH; x += 3, a += 4) {
#if WORDS_BIGENDIAN
				memcpy(pixels + x, cv32backing + a + 1, 3);
#else
				memcpy(pixels + x, cv32backing + a, 3);
#endif
			}
			break;
		case 4:
			vgamem_scan32(y, (uint32_t *)pixels, tpal, mouseline, mouseline_mask);
			break;
		default:
			// should never happen
			break;
		}

		pixels += pitch;
	}
}

// ----------------------------------------------------------------------------------

int video_is_fullscreen(void)
{
	return backend->is_fullscreen();
}

int video_width(void)
{
	return backend->width();
}

int video_height(void)
{
	return backend->height();
}

const char *video_driver_name(void)
{
	return backend->driver_name();
}

void video_report(void)
{
	log_nl();

	log_append(2, 0, "Video initialised");
	log_underline(17);

	backend->report();
}

void video_set_hardware(int hardware)
{
	backend->set_hardware(hardware);
}

void video_shutdown(void)
{
	if (backend) {
		backend->shutdown();
		backend->quit();
		backend = NULL;
	}
}

void video_setup(const char *quality)
{
	backend->setup(quality);
}

int video_startup(void)
{
	static const schism_video_backend_t *backends[] = {
		// ordered by preference
#ifdef SCHISM_SDL2
		&schism_video_backend_sdl2,
#endif
#ifdef SCHISM_SDL12
		&schism_video_backend_sdl12,
#endif
		NULL,
	};

	int i;

	for (i = 0; backends[i]; i++) {
		backend = backends[i];
		if (backend->init())
			break;

		backend = NULL;
	}

	if (!backend)
		return -1;

	// ok, now we can call the backend
	backend->startup();

	return 0;
}

void video_fullscreen(int new_fs_flag)
{
	backend->fullscreen(new_fs_flag);
}

void video_resize(unsigned int width, unsigned int height)
{
	backend->resize(width, height);
}

void video_colors(unsigned char palette[16][3])
{
	backend->colors(palette);
}

int video_is_focused(void)
{
	return backend->is_focused();
}

int video_is_visible(void)
{
	return backend->is_visible();
}

int video_is_wm_available(void)
{
	return backend->is_wm_available();
}

int video_is_hardware(void)
{
	return backend->is_hardware();
}

/* -------------------------------------------------------- */

int video_is_screensaver_enabled(void)
{
	return backend->is_screensaver_enabled();
}

void video_toggle_screensaver(int enabled)
{
	backend->toggle_screensaver(enabled);
}

/* ---------------------------------------------------------- */
/* coordinate translation */

void video_translate(unsigned int vx, unsigned int vy, unsigned int *x, unsigned int *y)
{
	backend->translate(vx, vy, x, y);
}

void video_get_logical_coordinates(int x, int y, int *trans_x, int *trans_y)
{
	backend->get_logical_coordinates(x, y, trans_x, trans_y);
}

/* -------------------------------------------------- */
/* input grab */

int video_is_input_grabbed(void)
{
	return backend->is_input_grabbed();
}

void video_set_input_grabbed(int enabled)
{
	backend->set_input_grabbed(enabled);
}

/* -------------------------------------------------- */
/* warp mouse position */

void video_warp_mouse(unsigned int x, unsigned int y)
{
	backend->warp_mouse(x, y);
}

void video_get_mouse_coordinates(unsigned int *x, unsigned int *y)
{
	backend->get_mouse_coordinates(x, y);
}

/* -------------------------------------------------- */
/* menu toggling */

int video_have_menu(void)
{
	return backend->have_menu();
}

void video_toggle_menu(int on)
{
	backend->toggle_menu(on);
}

/* ------------------------------------------------------------ */

void video_blit(void)
{
	backend->blit();
}

/* ------------------------------------------------------------ */

int video_get_wm_data(video_wm_data_t *wm_data)
{
	return backend->get_wm_data(wm_data);
}
