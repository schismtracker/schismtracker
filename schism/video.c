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
#include "bits.h"
#include "config.h"
#include "video.h"
#include "osdefs.h"
#include "vgamem.h"
#include "mem.h"
#include "palettes.h"

#include "backend/video.h"

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
		/* actually an enum... */
		int visible;

		uint32_t x, y;

		/* mouseline */
		uint32_t ml_x, ml_v;
	} mouse;

	/* we should move opengl and yuv stuff out of here.
	 * or maybe we can keep it. I don't know :) */

	uint32_t tc_bgr32[256];
	uint32_t pal_opengl[256];

	struct {
		uint32_t pal_y[256];
		uint32_t pal_u[256];
		uint32_t pal_v[256];
	} yuv;
} video = {
	.mouse = {
		.visible = MOUSE_EMULATED,
		.shape = CURSOR_SHAPE_ARROW,
	},
};

static const schism_video_backend_t *backend = NULL;

/* ----------------------------------------------------------- */

void video_rgb_to_yuv(uint32_t *y, uint32_t *u, uint32_t *v, unsigned char rgb[3])
{
	/* YCbCr. This results in somewhat less saturated colors when
	 * using SDL 1.2 software overlays, but this is the official
	 * translation. */
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

		SCHISM_FALLTHROUGH;
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

/* ------------------------------------------------------------------------ */
/* mouse drawing */

static void make_mouseline(uint32_t y, uint32_t mouseline[80],
	uint32_t mouseline_mask[80])
{
	const struct mouse_cursor *cursor = &cursors[video.mouse.shape];
	uint32_t i;
	/* precalculated mouseline -- done in video_translate_calculate */
	uint32_t x = video.mouse.ml_x;
	uint32_t v = video.mouse.ml_v;

	uint32_t scenter, swidth, centeroffset;
	uint32_t z, zm;
	uint32_t temp;

	memset(mouseline,      0, 80 * sizeof(*mouseline));
	memset(mouseline_mask, 0, 80 * sizeof(*mouseline));

	if (video_mousecursor_visible() != MOUSE_EMULATED
		|| !video_is_focused()
		|| (video.mouse.y >= cursor->center_y && y < video.mouse.y - cursor->center_y)
		|| y < cursor->center_y
		|| y >= video.mouse.y + cursor->height - cursor->center_y) {
		return;
	}

	scenter = (cursor->center_x / 8) + (cursor->center_x % 8 != 0);
	swidth  = (cursor->width    / 8) + (cursor->width    % 8 != 0);
	centeroffset = cursor->center_x % 8;

	z  = cursor->pointer[y - video.mouse.y + cursor->center_y];
	zm = cursor->mask[y - video.mouse.y + cursor->center_y];

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
	temp = (cursor->center_x < v) ? 0 : ((cursor->center_x - v) / 8) + ((cursor->center_x - v) % 8 != 0);
	for (i = 1; i <= temp && x >= i; i++) {
		mouseline[x-i]      = z  >> (8 * (swidth - scenter + 1 + i)) & 0xFF;
		mouseline_mask[x-i] = zm >> (8 * (swidth - scenter + 1 + i)) & 0xFF;
	}

	// and to the right
	temp = swidth - scenter + 1;
	for (i = 1; (i <= temp) && (x + i < 80); i++) {
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

void video_blitLN(uint32_t bpp, unsigned char *pixels, uint32_t pitch, schism_map_rgb_spec map_rgb, void *map_rgb_data, uint32_t width, uint32_t height)
{
	uint32_t cv32backing[NATIVE_SCREEN_WIDTH * 2];
	uint32_t *csp, *esp;
	uint32_t pad;
	int32_t fixedx, fixedy, scalex, scaley;
	uint32_t y, x;
	int32_t iny, lasty;

	csp = cv32backing;
	esp = cv32backing + NATIVE_SCREEN_WIDTH;
	lasty = -2;
	pad = pitch - (width * bpp);
	scalex = (INT2FIXED(NATIVE_SCREEN_WIDTH) - 1) / width;
	scaley = (INT2FIXED(NATIVE_SCREEN_HEIGHT) - 1) / height;
	for (y = 0, fixedy = 0; y < height; y++, fixedy += scaley) {
		iny = FIXED2INT(fixedy);

		if (iny != lasty) {
			uint32_t mouseline[80];
			uint32_t mouseline_mask[80];

			/* we'll downblit the colors later */
			if (iny == lasty + 1) {
				uint32_t *dp;

				/* move up one line */
				make_mouseline(iny+1, mouseline, mouseline_mask);
				vgamem_scan32(iny+1, csp, video.tc_bgr32, mouseline, mouseline_mask);

				/* only need to swap the buffer pointers */
				dp  = esp;
				esp = csp;
				csp = dp;
			} else {
				make_mouseline(iny,   mouseline, mouseline_mask);
				vgamem_scan32(iny,   csp, video.tc_bgr32, mouseline, mouseline_mask);
				make_mouseline(iny+1, mouseline, mouseline_mask);
				vgamem_scan32(iny+1, esp, video.tc_bgr32, mouseline, mouseline_mask);
			}

			lasty = iny;
		}

		for (x = 0, fixedx = 0; x < width; x++, fixedx += scalex) {
			uint32_t ex, ey;
			uint32_t c00, c01, c10, c11;
			uint32_t outr, outg, outb;
			uint32_t t1, t2;

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

			switch (bpp) {
			case 1: *(uint8_t*)pixels = c; break;
			case 2: *(uint16_t*)pixels = c; break;
			case 3:
				// convert 32-bit to 24-bit
#ifdef WORDS_BIGENDIAN
				*pixels++ = ((char *)&c)[1];
				*pixels++ = ((char *)&c)[2];
				*pixels++ = ((char *)&c)[3];
#else
				*pixels++ = ((char *)&c)[0];
				*pixels++ = ((char *)&c)[1];
				*pixels++ = ((char *)&c)[2];
#endif
				break;
			case 4: *(uint32_t*)pixels = c; break;
			default: break;
			}

			pixels += bpp;
		}
		pixels += pad;
	}
}

/* Fast nearest neighbor blitter */
void video_blitNN(uint32_t bpp, unsigned char *pixels, uint32_t pitch, uint32_t tpal[256], uint32_t width, uint32_t height)
{
	// at most 32-bits...
	union {
		uint8_t uc[NATIVE_SCREEN_WIDTH];
		uint16_t us[NATIVE_SCREEN_WIDTH];
		uint32_t ui[NATIVE_SCREEN_WIDTH];
	} pixels_u;
	/* NOTE: we might be able to get away with 24.8 fixed point,
	 * and reuse the stuff from the code above */
	const uint64_t scaley = (((uint64_t)NATIVE_SCREEN_HEIGHT << 32) - 1) / height;
	const uint64_t scalex = (((uint64_t)NATIVE_SCREEN_WIDTH << 32) - 1) / width;

	const uint32_t pad = (pitch - (width * bpp));
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];
	uint32_t y, last_scaled_y = 0 /* shut up gcc */;
	uint64_t fixedy;

	for (y = 0, fixedy = 0; y < height; y++, fixedy += scaley) {
		uint32_t x;
		uint64_t fixedx;

		const uint32_t scaled_y = fixedy >> 32;

		// only scan again if we have to, or if this the first scan
		if (y == 0 || scaled_y != last_scaled_y) {
			make_mouseline(scaled_y, mouseline, mouseline_mask);
			switch (bpp) {
			case 1:
				vgamem_scan8(scaled_y, pixels_u.uc, tpal, mouseline, mouseline_mask);
				break;
			case 2:
				vgamem_scan16(scaled_y, pixels_u.us, tpal, mouseline, mouseline_mask);
				break;
			case 3:
			case 4:
				vgamem_scan32(scaled_y, pixels_u.ui, tpal, mouseline, mouseline_mask);
				break;
			default:
				// should never happen
				break;
			}
		}

		for (x = 0, fixedx = 0; x < width; x++, fixedx += scalex) {
			const uint32_t scaled_x = fixedx >> 32;

			switch (bpp) {
			case 1: *pixels = pixels_u.uc[scaled_x]; break;
			case 2: *(uint16_t *)pixels = pixels_u.us[scaled_x]; break;
			case 3:
				// convert 32-bit to 24-bit
#ifdef WORDS_BIGENDIAN
				*pixels++ = pixels_u.uc[(scaled_x) * 4 + 1];
				*pixels++ = pixels_u.uc[(scaled_x) * 4 + 2];
				*pixels++ = pixels_u.uc[(scaled_x) * 4 + 3];
#else
				*pixels++ = pixels_u.uc[(scaled_x) * 4 + 0];
				*pixels++ = pixels_u.uc[(scaled_x) * 4 + 1];
				*pixels++ = pixels_u.uc[(scaled_x) * 4 + 2];
#endif
				break;
			case 4:  *(uint32_t *)pixels = pixels_u.ui[scaled_x]; break;
			default: break; // should never happen
			}

			pixels += bpp;
		}

		last_scaled_y = scaled_y;

		pixels += pad;
	}
}

void video_blitYY(unsigned char *pixels, uint32_t pitch, uint32_t tpal[256])
{
	// this is here because pixels is write only on SDL2
	uint16_t pixels_r[NATIVE_SCREEN_WIDTH];

	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];
	int y;

	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(y, mouseline, mouseline_mask);

		vgamem_scan16(y, pixels_r, tpal, mouseline, mouseline_mask);
		memcpy(pixels, pixels_r, pitch);
		pixels += pitch;
		memcpy(pixels, pixels_r, pitch);
		pixels += pitch;
	}
}

void video_blitUV(unsigned char *pixels, uint32_t pitch, uint32_t tpal[256])
{
	video_blit11(1, pixels, pitch, tpal);
}

void video_blitTV(unsigned char *pixels, uint32_t pitch, uint32_t tpal[256])
{
	unsigned char cv8backing[NATIVE_SCREEN_WIDTH];
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];
	uint32_t y, x;

	uint32_t len = MIN(pitch, NATIVE_SCREEN_WIDTH);

	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y += 2) {
		make_mouseline(y, mouseline, mouseline_mask);
		vgamem_scan8(y, cv8backing, tpal, mouseline, mouseline_mask);
		for (x = 0; x < len; x += 2)
			pixels[x >> 1] = cv8backing[x+1] | (cv8backing[x] << 4);
		pixels += (pitch >> 1);
	}
}

void video_blit11(uint32_t bpp, unsigned char *pixels, uint32_t pitch, uint32_t tpal[256])
{
	uint32_t cv32backing[NATIVE_SCREEN_WIDTH];
	uint32_t y, x;
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];

	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(y, mouseline, mouseline_mask);
		switch (bpp) {
		case 1:
			vgamem_scan8(y, (uint8_t *)pixels, tpal, mouseline, mouseline_mask);
			break;
		case 2:
			vgamem_scan16(y, (uint16_t *)pixels, tpal, mouseline, mouseline_mask);
			break;
		case 3:
			vgamem_scan32(y, (uint32_t *)cv32backing, tpal, mouseline, mouseline_mask);
			for (x = 0; x < NATIVE_SCREEN_WIDTH; x++) {
#ifdef WORDS_BIGENDIAN
				pixels[(x * 3) + 0] = ((unsigned char *)cv32backing)[(x * 4) + 1];
				pixels[(x * 3) + 1] = ((unsigned char *)cv32backing)[(x * 4) + 2];
				pixels[(x * 3) + 2] = ((unsigned char *)cv32backing)[(x * 4) + 3];
#else
				pixels[(x * 3) + 0] = ((unsigned char *)cv32backing)[(x * 4) + 0];
				pixels[(x * 3) + 1] = ((unsigned char *)cv32backing)[(x * 4) + 1];
				pixels[(x * 3) + 2] = ((unsigned char *)cv32backing)[(x * 4) + 2];
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

/* scaled blit, according to user settings (lots of params here) */
void video_blitSC(uint32_t bpp, unsigned char *pixels, uint32_t pitch, uint32_t pal[256], schism_map_rgb_spec fun, void *fun_data, uint32_t x, uint32_t y, uint32_t w, uint32_t h)
{
	pixels += y * pitch;
	pixels += x * bpp;

	if (w == NATIVE_SCREEN_WIDTH && h == NATIVE_SCREEN_HEIGHT) {
		/* scaling isn't necessary */
		video_blit11(bpp, pixels, pitch, pal);
	} else {
		switch (cfg_video_interpolation) {
		case VIDEO_INTERPOLATION_NEAREST:
			video_blitNN(bpp, pixels, pitch, pal, w, h);
			break;
		case VIDEO_INTERPOLATION_LINEAR:
		case VIDEO_INTERPOLATION_BEST:
			video_blitLN(bpp, pixels, pitch, fun, fun_data, w, h);
			break;
		}
	}
}

// ----------------------------------------------------------------------------------

int video_is_fullscreen(void)
{
	return backend ? backend->is_fullscreen() : 0;
}

int video_width(void)
{
	return backend ? backend->width() : 0;
}

int video_height(void)
{
	return backend ? backend->height() : 0;
}

const char *video_driver_name(void)
{
	return backend ? backend->driver_name() : "";
}

void video_report(void)
{
	log_nl();

	log_append_timestamp(2, "Video initialised");
	log_underline();

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

void video_setup(int interpolation)
{
	cfg_video_interpolation = interpolation;

	backend->setup(interpolation);
}

int video_startup(void)
{
	static const schism_video_backend_t *backends[] = {
		// ordered by preference
#ifdef SCHISM_SDL3
		&schism_video_backend_sdl3,
#endif
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
		if (backend->init() && backend->startup())
			break;

		backend = NULL;
	}

	if (!backend)
		return -1;

	return 0;
}

void video_fullscreen(int new_fs_flag)
{
	backend->fullscreen(new_fs_flag);
}

void video_resize(uint32_t width, uint32_t height)
{
	backend->resize(width, height);
	palette_apply();
	status.flags |= NEED_UPDATE;
}

/* calls back to a function receiving all the colors :) */
void video_colors_iterate(unsigned char palette[16][3], video_colors_callback_spec fun)
{
	/* this handles all of the ACTUAL color stuff, and the callback handles the backend-specific stuff */
	static const unsigned int lastmap[] = { 0, 1, 2, 3, 5 };
	uint32_t i, p;

	/* make our "base" space */
	for (i = 0; i < 16; i++) {
		unsigned char rgb[3];
		for (size_t j = 0; j < ARRAY_SIZE(rgb); j++)
			rgb[j] = palette[i][j];

		fun(i, rgb);
	}

	/* make our "gradient" space; this is used exclusively for the waterfall page (Alt-F12) */
	for (i = 0; i < 128; i++) {
		size_t j;
		unsigned char rgb[3];

		p = lastmap[(i>>5)];

		for (j = 0; j < ARRAY_SIZE(rgb); j++)
			rgb[j] = (int)palette[p][j] + (((int)(palette[p+1][j] - palette[p][j]) * (i & 0x1F)) / 0x20);

		fun(i + 128, rgb);
	}
}

static void bgr32_fun_(unsigned int i, unsigned char rgb[3])
{
	video.tc_bgr32[i]
		= ((uint32_t)rgb[2] << 0)
		| ((uint32_t)rgb[1] << 8)
		| ((uint32_t)rgb[0] << 16)
		| ((uint32_t)0xFF   << 24);
}

static void gl_fun_(unsigned int i, unsigned char rgb[3])
{
	video.pal_opengl[i] = rgb[2]
		| (rgb[1] << 8)
		| (rgb[0] << 16)
		| (0xFF << 24);
}

void video_colors(unsigned char palette[16][3])
{
	video_colors_iterate(palette, bgr32_fun_);
	video_colors_iterate(palette, gl_fun_);

	backend->colors(palette);
}

int video_is_focused(void)
{
	return backend ? backend->is_focused() : 0;
}

int video_is_visible(void)
{
	return backend ? backend->is_visible() : 0;
}

int video_is_wm_available(void)
{
	return backend ? backend->is_wm_available() : 0;
}

int video_is_hardware(void)
{
	return backend ? backend->is_hardware() : 0;
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

/* called back by the video impl
 *
 * TODO it's probably simpler to request the clip rect
 * from the backend and use it, but this may be more
 * versatile */
void video_translate_calculate(uint32_t vx, uint32_t vy,
	/* clip rect */
	uint32_t cx, uint32_t cy, uint32_t cw, uint32_t ch,
	/* return */
	uint32_t *x, uint32_t *y)
{
	/* handle negative mouse coordinates when the mouse is
	 * dragged outside of window bounds */
	if ((int32_t)vx < 0) vx = 0;
	if ((int32_t)vy < 0) vy = 0;

	/* I'm 99% sure this can be done simpler, but hey, whatever. :) */
	vx = MAX(vx, cx);
	vx -= cx;

	vy = MAX(vy, cy);
	vy -= cy;

	vx = MIN(vx, cw);
	vy = MIN(vy, ch);

	if (cw != NATIVE_SCREEN_WIDTH)
		vx = vx * NATIVE_SCREEN_WIDTH / cw;

	if (ch != NATIVE_SCREEN_HEIGHT)
		vy = vy * NATIVE_SCREEN_HEIGHT / ch;

	if (video_mousecursor_visible() && (video.mouse.x != vx || video.mouse.y != vy))
		status.flags |= SOFTWARE_MOUSE_MOVED;

	video.mouse.x = vx;
	video.mouse.y = vy;
	video.mouse.ml_x = (vx / 8);
	video.mouse.ml_v = (vx % 8);

	if (x) *x = vx;
	if (y) *y = vy;
}

void video_translate(uint32_t vx, uint32_t vy, uint32_t *x, uint32_t *y)
{
	backend->translate(vx, vy, x, y);
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

void video_warp_mouse(uint32_t x, uint32_t y)
{
	backend->warp_mouse(x, y);
}

void video_get_mouse_coordinates(uint32_t *x, uint32_t *y)
{
	if (x) *x = video.mouse.x;
	if (y) *y = video.mouse.y;

	//backend->get_mouse_coordinates(x, y);
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
	if (backend) backend->blit();
}

/* ------------------------------------------------------------ */

int video_get_wm_data(video_wm_data_t *wm_data)
{
	return backend->get_wm_data(wm_data);
}

/* ------------------------------------------------------------ */

void video_show_cursor(int enabled)
{
	backend->show_cursor(enabled);
}

/* ------------------------------------------------------------ */

/* takes in a width and height, and calculates clip from it.
 * NOTE this uses global config variables (kind of evil, but most
 * of the video stuff is already evil) */
void video_calculate_clip(uint32_t w, uint32_t h,
	uint32_t *px, uint32_t *py, uint32_t *pw, uint32_t *ph)
{
	if (cfg_video_want_fixed) {
		const double ratio_w = (double)w / (double)cfg_video_want_fixed_width;
		const double ratio_h = (double)h / (double)cfg_video_want_fixed_height;

		if (ratio_w < ratio_h) {
			*pw = w;
			*ph = (double)cfg_video_want_fixed_height * ratio_w;
		} else {
			*ph = h;
			*pw = (double)cfg_video_want_fixed_width * ratio_h;
		}

		*px = (w - *pw) / 2;
		*py = (h - *ph) / 2;
	} else {
		*px = *py = 0;
		*pw = w;
		*ph = h;
	}
}

/* ------------------------------------------------------------------------ */
/* I'm so yuv! */

static uint32_t yuvformat = VIDEO_YUV_NONE;

void video_yuv_setformat(uint32_t format)
{
	yuvformat = format;
}

/* video_colors callback for built-in yuv stuff */
void video_yuv_pal(unsigned int i, unsigned char rgb[3])
{
	uint32_t y, u, v, t;
	video_rgb_to_yuv(&y, &u, &v, rgb);

	/* Swap the u and v values. Makes everything else
	 * just a bit less complicated, since a few of these
	 * formats just swap u/v from others */
	switch (yuvformat) {
	case VIDEO_YUV_YV12:
	case VIDEO_YUV_YV12_TV:
	case VIDEO_YUV_NV21:
		t = v;
		v = u;
		u = t;
		break;
	}

	switch (yuvformat) {
	case VIDEO_YUV_YV12_TV:
	case VIDEO_YUV_IYUV_TV:
		/* halfwidth */
		video.yuv.pal_y[i] = y;
		video.yuv.pal_u[i] = (u >> 4) & 0xF;
		video.yuv.pal_v[i] = (v >> 4) & 0xF;
		break;
	case VIDEO_YUV_YV12:
	case VIDEO_YUV_IYUV:
		video.yuv.pal_y[i] = y | (y << 8);
		video.yuv.pal_u[i] = u;
		video.yuv.pal_v[i] = v;
		break;
	case VIDEO_YUV_NV12:
	case VIDEO_YUV_NV21:
		video.yuv.pal_y[i] = y | (y << 8);
		/* U/V interleaved */
#ifdef WORDS_BIGENDIAN
		video.yuv.pal_u[i] = (u << 8) | v;
#else
		video.yuv.pal_u[i] = (v << 8) | u;
#endif
		break;
	/* packed modes */
	case VIDEO_YUV_YVYU:
		/* y0 v0 y1 u0 */
#ifdef WORDS_BIGENDIAN
		video.yuv.pal_y[i] = u | (y << 8) | (v << 16) | (y << 24);
#else
		video.yuv.pal_y[i] = y | (v << 8) | (y << 16) | (u << 24);
#endif
		break;
	case VIDEO_YUV_UYVY:
		/* u0 y0 v0 y1 */
#ifdef WORDS_BIGENDIAN
		video.yuv.pal_y[i] = y | (v << 8) | (y << 16) | (u << 24);
#else
		video.yuv.pal_y[i] = u | (y << 8) | (v << 16) | (y << 24);
#endif
		break;
	case VIDEO_YUV_YUY2:
		/* y0 u0 y1 v0 */
#ifdef WORDS_BIGENDIAN
		video.yuv.pal_y[i] = v | (y << 8) | (u << 16) | (y << 24);
#else
		video.yuv.pal_y[i] = y | (u << 8) | (y << 16) | (v << 24);
#endif
		break;
	default:
		break; // err
	}
}

/* blit in the current yuv format
 * planes:  planes to blit
 * pitches: pitches for each plane
 * the number of planes depends on the YUV format.
 * for example, IYUV has three planes, in the order Y + U + V.
 * NV12 has two planes, Y and then U/V interleaved. */
void video_yuv_blit(unsigned char *plane0, unsigned char *plane1, unsigned char *plane2,
	uint32_t pitch0, uint32_t pitch1, uint32_t pitch2)
{
	switch (yuvformat) {
	case VIDEO_YUV_IYUV:
	case VIDEO_YUV_YV12:
		video_blitYY(plane0, pitch0, video.yuv.pal_y);
		video_blitUV(plane1, pitch1, video.yuv.pal_u);
		video_blitUV(plane2, pitch2, video.yuv.pal_v);
		break;
	case VIDEO_YUV_IYUV_TV:
	case VIDEO_YUV_YV12_TV:
		video_blitUV(plane0, pitch0, video.yuv.pal_y);
		video_blitTV(plane1, pitch1 << 1, video.yuv.pal_u);
		video_blitTV(plane2, pitch2 << 1, video.yuv.pal_v);
		break;
	case VIDEO_YUV_NV12:
	case VIDEO_YUV_NV21:
		video_blitYY(plane0, pitch0, video.yuv.pal_y);
		video_blit11(2, plane1, pitch1, video.yuv.pal_u);
		break;
	case VIDEO_YUV_YVYU:
	case VIDEO_YUV_UYVY:
	case VIDEO_YUV_YUY2:
		video_blit11(4, plane0, pitch0, video.yuv.pal_y);
		break;
	}
}

/* sequential blit */
void video_yuv_blit_sequenced(unsigned char *pixels, uint32_t pitch)
{
	switch (yuvformat) {
	case VIDEO_YUV_IYUV:
	case VIDEO_YUV_YV12:
		video_yuv_blit(pixels,
			pixels + ((NATIVE_SCREEN_HEIGHT * pitch) << 1),
			pixels + ((NATIVE_SCREEN_HEIGHT * pitch) << 1) + ((NATIVE_SCREEN_HEIGHT * pitch) >> 1),
			pitch, pitch >> 1, pitch >> 1);
		break;
	case VIDEO_YUV_IYUV_TV:
	case VIDEO_YUV_YV12_TV:
		video_yuv_blit(pixels,
			pixels + (NATIVE_SCREEN_HEIGHT * pitch),
			pixels + (NATIVE_SCREEN_HEIGHT * pitch) + ((NATIVE_SCREEN_HEIGHT * pitch) >> 2),
			pitch, pitch >> 1, pitch >> 1);
		break;
	case VIDEO_YUV_NV12:
	case VIDEO_YUV_NV21:
		video_yuv_blit(pixels, pixels + ((NATIVE_SCREEN_HEIGHT * pitch) << 1), NULL, pitch, pitch, 0);
		break;
	case VIDEO_YUV_YVYU:
	case VIDEO_YUV_UYVY:
	case VIDEO_YUV_YUY2:
		video_yuv_blit(pixels, NULL, NULL, pitch, 0, 0);
		break;
	}
}

void video_yuv_report(void)
{
	struct {
		uint32_t num;
		const char *name, *type;
	} yuv_layouts[] = {
		{VIDEO_YUV_IYUV, "IYUV", "planar"},
		{VIDEO_YUV_YV12, "YV12", "planar"},
		{VIDEO_YUV_YV12_TV, "YV12", "planar+tv"},
		{VIDEO_YUV_IYUV_TV, "IYUV", "planar+tv"},
		{VIDEO_YUV_NV21, "NV21", "planar"},
		{VIDEO_YUV_NV12, "NV12", "planar"},
		{VIDEO_YUV_YVYU, "YVYU", "packed"},
		{VIDEO_YUV_UYVY, "UYVY", "packed"},
		{VIDEO_YUV_YUY2, "YUY2", "packed"},
		{0, NULL, NULL},
	}, *layout = yuv_layouts;

	while (yuvformat != layout->num && layout->name != NULL)
		layout++;
	if (layout->name)
		log_appendf(5, " Display format: %s (%s)", layout->name, layout->type);
	else
		log_appendf(5, " Display format: %x", yuvformat);
}

/* ------------------------------------------------------------------------ */
/* OpenGL crap
 *
 * NOTE: OpenGL 3.0 support is extremely limited, since we
 * currently have no proper use for it. Maybe some day it will
 * be implemented. I highly doubt it will. */

#ifdef SCHISM_WIN32
# define APIENTRY __stdcall
#elif defined(SCHISM_OS2)
# define APIENTRY _System
#else
# define APIENTRY
#endif

typedef unsigned char GLboolean;
typedef int8_t GLbyte;
typedef uint8_t GLubyte;
typedef int16_t GLshort;
typedef uint16_t GLushort;
typedef int32_t GLint;
typedef uint32_t GLuint;
typedef int32_t GLfixed; /* 16.16 */
typedef int64_t GLint64;
typedef uint64_t GLuint64;
typedef int32_t GLsizei;
typedef uint32_t GLenum;
typedef intptr_t GLintptr;
typedef intptr_t GLsizeiptr;
typedef /*u?*/intptr_t GLsync;
typedef uint32_t GLbitfield;
typedef uint16_t GLhalf; /* 16-bit floating point ??? */
typedef float GLfloat;
typedef float GLclampf;
typedef double GLdouble;
typedef double GLclampd;
typedef void GLvoid;

/* hm? */
#define GL_BGRA_EXT                       0x80E1
#define GL_UNSIGNED_INT_8_8_8_8_REV       0x8367
#define GL_COLOR_BUFFER_BIT			0x00004000
#define GL_UNSIGNED_BYTE			0x1401
#define GL_RGBA8				0x8058
#define GL_MAX_TEXTURE_SIZE         0x0D33
#define GL_TEXTURE_2D               0x0DE1
#define GL_TEXTURE_WRAP_S           0x2802
#define GL_TEXTURE_WRAP_T           0x2803
#define GL_TEXTURE_MAG_FILTER       0x2800
#define GL_TEXTURE_MIN_FILTER       0x2801
#define GL_NEAREST                  0x2600
#define GL_TRIANGLES				0x0004
#define GL_QUADS				0x0007
#define GL_COMPILE				0x1300
#define GL_MODELVIEW				0x1700
#define GL_CULL_FACE				0x0B44
#define GL_LIGHTING				0x0B50
#define GL_DEPTH_TEST				0x0B71
#define GL_FLAT					0x1D00
#define GL_LINEAR				0x2601
#define GL_PROJECTION				0x1701
#define GL_CLAMP				0x2900
#define GL_EXTENSIONS				0x1F03
#define GL_INVALID_ENUM 0x0500
#define GL_VERSION				0x1F02
#define GL_MAJOR_VERSION                  0x821B
#define GL_MINOR_VERSION                  0x821C
#define GL_NUM_EXTENSIONS 0x821D
#define GL_NO_ERROR 0

#define SCHISM_NVIDIA_PIXELDATARANGE 1

/* Use pixel-buffer-objects to asynchronously transfer
 * to the GPU. This is in essence a more modern take on
 * the NVidia pixel-data-range extension. */
#define SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT 1

#ifdef SCHISM_NVIDIA_PIXELDATARANGE
# define GL_WRITE_PIXEL_DATA_RANGE_NV      0x8878
#endif

#ifdef SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT
# define GL_PIXEL_UNPACK_BUFFER 0x88EC
#define GL_WRITE_ONLY                     0x88B9
#define GL_STREAM_DRAW                    0x88E0
#endif

#define VIDEO_GL_PITCH (NATIVE_SCREEN_WIDTH * 4) /* bruh */

/* Dynamically loaded OpenGL functions */
struct video_opengl_funcs {
	/* TODO put these in a header so they don't get out of sync */
	void (APIENTRY *glCallList)(GLuint);
	void (APIENTRY *glTexSubImage2D)(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const GLvoid *);
	void (APIENTRY *glDeleteLists)(GLuint,GLsizei);
	void (APIENTRY *glTexParameteri)(GLenum,GLenum,GLint);
	void (APIENTRY *glBegin)(GLenum);
	void (APIENTRY *glEnd)(void);
	void (APIENTRY *glEndList)(void);
	void (APIENTRY *glTexCoord2f)(GLfloat,GLfloat);
	void (APIENTRY *glVertex2f)(GLfloat,GLfloat);
	void (APIENTRY *glNewList)(GLuint, GLenum);
	GLboolean (APIENTRY *glIsList)(GLuint);
	GLuint (APIENTRY *glGenLists)(GLsizei);
	void (APIENTRY *glLoadIdentity)(void);
	void (APIENTRY *glMatrixMode)(GLenum);
	void (APIENTRY *glEnable)(GLenum);
	void (APIENTRY *glDisable)(GLenum);
	void (APIENTRY *glBindTexture)(GLenum,GLuint);
	void (APIENTRY *glClear)(GLbitfield mask);
	void (APIENTRY *glClearColor)(GLclampf,GLclampf,GLclampf,GLclampf);
	const GLubyte* (APIENTRY *glGetString)(GLenum);
	void (APIENTRY *glTexImage2D)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid *);
	void (APIENTRY *glGetIntegerv)(GLenum, GLint *);
	void (APIENTRY *glShadeModel)(GLenum);
	void (APIENTRY *glGenTextures)(GLsizei,GLuint*);
	void (APIENTRY *glDeleteTextures)(GLsizei, const GLuint *);
	void (APIENTRY *glViewport)(GLint,GLint,GLsizei,GLsizei);
	void (APIENTRY *glEnableClientState)(GLenum);
	GLenum (APIENTRY *glGetError)(void);

#ifdef SCHISM_NVIDIA_PIXELDATARANGE
	/* I think these are Windows-specific? */
	void *(APIENTRY *wglAllocateMemoryNV)(int, float, float, float);
	void (APIENTRY *wglFreeMemoryNV)(void *);

	void (APIENTRY *glPixelDataRangeNV)(GLenum, GLsizei, GLvoid *);
	/* void (APIENTRY *glFlushPixelDataRangeNV)(GLenum); */
#endif

#ifdef SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT
	void (APIENTRY *glGenBuffers)(GLsizei n, GLuint *buffers);
	void (APIENTRY *glBindBuffer)(GLenum target, GLuint buffer);
	void (APIENTRY *glBufferData)(GLenum,GLsizeiptr,const void *,GLenum);
	void *(APIENTRY *glMapBuffer)(GLenum,GLenum);
	GLboolean (APIENTRY *glUnmapBuffer)(GLenum);
#endif
};

static int video_opengl_reload_funcs(struct video_opengl_funcs *f,
	video_opengl_function_load_spec function_load)
{
	/* Probably not really necessary */
	memset(f, 0, sizeof(*f));

#define Z(q) \
	do { \
		f->q = function_load(#q); \
		if (!f->q) \
			return -1; \
	} while (0)

	Z(glCallList);
	Z(glTexSubImage2D);
	Z(glDeleteLists);
	Z(glTexParameteri);
	Z(glBegin);
	Z(glEnd);
	Z(glEndList);
	Z(glTexCoord2f);
	Z(glVertex2f);
	Z(glNewList);
	Z(glIsList);
	Z(glGenLists);
	Z(glLoadIdentity);
	Z(glMatrixMode);
	Z(glEnable);
	Z(glDisable);
	Z(glBindTexture);
	Z(glClear);
	Z(glClearColor);
	Z(glGetString);
	Z(glTexImage2D);
	Z(glGetIntegerv);
	Z(glShadeModel);
	Z(glGenTextures);
	Z(glDeleteTextures);
	Z(glViewport);
	Z(glEnableClientState);
	Z(glGetError);

#undef Z

		/* Optional OpenGL extensions */
#define Z(q) \
	f->q = function_load(#q)

#ifdef SCHISM_NVIDIA_PIXELDATARANGE
	Z(glPixelDataRangeNV);
	Z(wglAllocateMemoryNV);
	Z(wglFreeMemoryNV);
#endif

#ifdef SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT
	Z(glGenBuffers);
	Z(glBindBuffer);
	Z(glBufferData);
	Z(glMapBuffer);
	Z(glUnmapBuffer);
#endif

#undef Z

	return 0;
}

/* ------------------------------------------------------------------------ */

static struct {
	video_opengl_object_unload_spec object_unload;
	video_opengl_set_attribute_spec set_attribute;
	video_opengl_swap_buffers_spec  swap_buffers;
	video_opengl_extension_supported_spec extension_supported;
	video_opengl_function_load_spec function_load;

	/* function table */
	struct video_opengl_funcs f;

	int init;

	void *framebuffer;
	GLuint texture;
	GLuint displaylist;
	GLint max_texture_size;

#ifdef SCHISM_NVIDIA_PIXELDATARANGE
	unsigned int pixel_data_range : 1;
#endif
#ifdef SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT
	GLuint pbo;

	unsigned int pixel_buffer_object : 1;
#endif
} video_gl = {0};

static int32_t int32_log2(int32_t val) {
	int32_t l = 0;
	while ((val >>= 1) != 0)
		l++;
	return l;
}

static int opengl_extension_supported_default(const char *extension)
{
	const char *start, *extensions;
	const char *where, *terminator;

	/* Extension names should not have spaces. */
	where = strchr(extension, ' ');
	if (where || *extension == '\0')
		return 0;

	extensions = (const char *)video_gl.f.glGetString(GL_EXTENSIONS);
	if (!extensions)
		return 0;

	/* It takes a bit of care to be fool-proof about parsing the
	 * OpenGL extensions string. Don't be fooled by sub-strings,
	 * etc. */
	start = extensions;

	for (;;) {
		where = strstr(start, extension);
		if (!where)
			break;

		terminator = where + strlen(extension);
		if (where == start || *(where - 1) == ' ')
			if (*terminator == ' ' || *terminator == '\0')
				return 1;

		start = terminator;
	}

	return 0;
}

int video_opengl_init(video_opengl_object_load_spec object_load,
	video_opengl_function_load_spec function_load,
	video_opengl_extension_supported_spec extension_supported,
	video_opengl_object_unload_spec object_unload,
	video_opengl_set_attribute_spec set_attribute,
	video_opengl_swap_buffers_spec swap_buffers)
{
	if (video_gl.init)
		return 0; /* already init */

	memset(&video_gl, 0, sizeof(video_gl));

	if (extension_supported) {
		video_gl.extension_supported = extension_supported;
	} else {
		video_gl.extension_supported = opengl_extension_supported_default;
	}

	if (object_load(NULL)
#ifdef SCHISM_WIN32
		|| object_load("opengl.dll") || object_load("opengl32.dll")
#else
		|| object_load("libGL.so") || object_load("GL")
#endif
	) {
		video_gl.object_unload = object_unload;
		video_gl.set_attribute = set_attribute;
		video_gl.swap_buffers  = swap_buffers;
		video_gl.function_load = function_load;

		video_gl.init = 1;

		return 1;
	}

	return 0;
}

void video_opengl_reset_interpolation(void)
{
	uint32_t x;

	switch (cfg_video_interpolation) {
	case VIDEO_INTERPOLATION_NEAREST:
		x = GL_NEAREST;
		break;
	case VIDEO_INTERPOLATION_LINEAR:
	case VIDEO_INTERPOLATION_BEST:
	default:
		x = GL_LINEAR;
		break;
	}

	video_gl.f.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, x);
	video_gl.f.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, x);
}

static void opengl_set_attributes(void)
{
	/*
	SDL 1.2 code used to call this, but evidently it
	breaks everything, especially on Windows. We just
	get a black screen. Not sure why.

	video_gl.set_attribute(VIDEO_GL_DEPTH_SIZE, 32);
	*/

	/* This is for vsync I think... */
	video_gl.set_attribute(VIDEO_GL_SWAP_CONTROL, 0);
}

int video_opengl_setup(uint32_t w, uint32_t h,
	int (*callback)(uint32_t *px, uint32_t *py, uint32_t *pw, uint32_t *ph))
{
	GLfloat tex_width, tex_height;
	GLsizei texsize;
	uint32_t x, y;

#define GL_ASSERT(X) \
do { \
	GLenum err; \
\
	X; \
\
	err = video_gl.f.glGetError(); \
	if (err != GL_NO_ERROR) { \
		log_appendf(4, "OpenGL error: `" #X "`: %d", (int)err); \
		return 0; \
	} \
} while (0)

	if (!video_gl.init)
		return 0;

	opengl_set_attributes();

	if (!callback(&x, &y, &w, &h))
		return 0; /* ? */

	if (video_opengl_reload_funcs(&video_gl.f, video_gl.function_load) < 0)
		return 0;

#ifdef SCHISM_NVIDIA_PIXELDATARANGE
	/* check if nvidia pixel data crap is supported */
	if (video_gl.extension_supported("GL_NV_pixel_data_range")) {
		video_gl.pixel_data_range = (video_gl.f.glPixelDataRangeNV
			&& video_gl.f.wglAllocateMemoryNV
			&& video_gl.f.wglFreeMemoryNV);
	}
#endif

#ifdef SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT
	/* ARB and EXT are functionally equivalent */
	if (video_gl.extension_supported("GL_ARB_pixel_buffer_object")
			|| video_gl.extension_supported("GL_EXT_pixel_buffer_object")) {
		video_gl.pixel_buffer_object = (video_gl.f.glGenBuffers
			&& video_gl.f.glBindBuffer
			&& video_gl.f.glBufferData
			&& video_gl.f.glMapBuffer
			&& video_gl.f.glUnmapBuffer);
	}
#endif

	GL_ASSERT(video_gl.f.glViewport(x, y, w, h));

	texsize = INT32_C(2) << int32_log2(NATIVE_SCREEN_WIDTH);

#ifdef SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT
	if (video_gl.pixel_buffer_object) {
		/* ehhhhhh... */
		video_gl.f.glGenBuffers(1, &video_gl.pbo);
		if (video_gl.f.glGetError()) video_gl.pixel_buffer_object = 0;
		video_gl.f.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, video_gl.pbo);
		if (video_gl.f.glGetError()) video_gl.pixel_buffer_object = 0;
		video_gl.f.glBufferData(GL_PIXEL_UNPACK_BUFFER, NATIVE_SCREEN_WIDTH * NATIVE_SCREEN_HEIGHT * 4, NULL, GL_STREAM_DRAW);
		if (video_gl.f.glGetError()) video_gl.pixel_buffer_object = 0;
		video_gl.f.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
		if (video_gl.f.glGetError()) video_gl.pixel_buffer_object = 0;
	}
#endif

	if (!video_gl.pixel_buffer_object && !video_gl.framebuffer) {
#ifdef SCHISM_NVIDIA_PIXELDATARANGE
		if (video_gl.pixel_data_range) {
			video_gl.framebuffer = video_gl.f.wglAllocateMemoryNV(
				NATIVE_SCREEN_WIDTH * NATIVE_SCREEN_HEIGHT * 4,
				0.0, 1.0, 1.0);
			if (video_gl.framebuffer) {
				video_gl.f.glPixelDataRangeNV(GL_WRITE_PIXEL_DATA_RANGE_NV,
					NATIVE_SCREEN_WIDTH * NATIVE_SCREEN_HEIGHT * 4,
					video_gl.framebuffer);
				video_gl.f.glEnableClientState(GL_WRITE_PIXEL_DATA_RANGE_NV);
			} else {
				video_gl.pixel_data_range = 0;
			}
		}
#endif

		if (!video_gl.framebuffer)
			video_gl.framebuffer = mem_alloc(NATIVE_SCREEN_WIDTH
				* NATIVE_SCREEN_HEIGHT * 4);

		if (!video_gl.framebuffer)
			return 0; /* okay */
	}

	GL_ASSERT(video_gl.f.glMatrixMode(GL_PROJECTION));
	GL_ASSERT(video_gl.f.glDeleteTextures(1, &video_gl.texture));
	GL_ASSERT(video_gl.f.glGenTextures(1, &video_gl.texture));
	GL_ASSERT(video_gl.f.glBindTexture(GL_TEXTURE_2D, video_gl.texture));
	GL_ASSERT(video_gl.f.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP));
	GL_ASSERT(video_gl.f.glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP));

	video_opengl_reset_interpolation();

	GL_ASSERT(video_gl.f.glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize,
		0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0));

	GL_ASSERT(video_gl.f.glClearColor(0.0, 0.0, 0.0, 1.0));
	GL_ASSERT(video_gl.f.glClear(GL_COLOR_BUFFER_BIT));
	video_gl.swap_buffers();
	GL_ASSERT(video_gl.f.glClear(GL_COLOR_BUFFER_BIT));
	GL_ASSERT(video_gl.f.glShadeModel(GL_FLAT));
	GL_ASSERT(video_gl.f.glDisable(GL_DEPTH_TEST));
	GL_ASSERT(video_gl.f.glDisable(GL_LIGHTING));
	GL_ASSERT(video_gl.f.glDisable(GL_CULL_FACE));
	GL_ASSERT(video_gl.f.glEnable(GL_TEXTURE_2D));
	GL_ASSERT(video_gl.f.glMatrixMode(GL_MODELVIEW));
	GL_ASSERT(video_gl.f.glLoadIdentity());

	tex_width = ((GLfloat)(NATIVE_SCREEN_WIDTH) / (GLfloat)texsize);
	tex_height = ((GLfloat)(NATIVE_SCREEN_HEIGHT) / (GLfloat)texsize);

	if (video_gl.f.glIsList(video_gl.displaylist))
		video_gl.f.glDeleteLists(video_gl.displaylist, 1);

	GL_ASSERT(video_gl.displaylist = video_gl.f.glGenLists(1));
	video_gl.f.glNewList(video_gl.displaylist, GL_COMPILE);
	video_gl.f.glBindTexture(GL_TEXTURE_2D, video_gl.texture);

	/* use quads */
	video_gl.f.glBegin(GL_QUADS);

	video_gl.f.glTexCoord2f(0, tex_height);
	video_gl.f.glVertex2f(-1.0f, -1.0f);
	video_gl.f.glTexCoord2f(tex_width, tex_height);
	video_gl.f.glVertex2f(1.0f, -1.0f);
	video_gl.f.glTexCoord2f(tex_width, 0);
	video_gl.f.glVertex2f(1.0f, 1.0f);
	video_gl.f.glTexCoord2f(0, 0);
	video_gl.f.glVertex2f(-1.0f, 1.0f);

	video_gl.f.glEnd();
	video_gl.f.glEndList();

	return 1;
}

SCHISM_HOT void video_opengl_blit(void)
{
	if (!video_gl.init)
		return;

#ifdef SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT
	if (video_gl.pixel_buffer_object) {
		void *fb;

		/* Bind the buffer -- important! */
		video_gl.f.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, video_gl.pbo);

		fb = video_gl.f.glMapBuffer(GL_PIXEL_UNPACK_BUFFER, GL_WRITE_ONLY);
		video_blit11(4, fb, VIDEO_GL_PITCH, video.tc_bgr32);
		video_gl.f.glUnmapBuffer(GL_PIXEL_UNPACK_BUFFER);

		video_gl.f.glBindTexture(GL_TEXTURE_2D, video_gl.texture);
		video_gl.f.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT,
			GL_BGRA_EXT,
			GL_UNSIGNED_INT_8_8_8_8_REV, 0);

		/* Unbind -- equally as important */
		video_gl.f.glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
	} else
#endif
	{
		if (!video_gl.framebuffer)
			return;

		/* we should probably be using RGBA and not BGRA */
		video_blit11(4, video_gl.framebuffer, VIDEO_GL_PITCH, video.tc_bgr32);

		video_gl.f.glBindTexture(GL_TEXTURE_2D, video_gl.texture);
		video_gl.f.glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT,
			GL_BGRA_EXT,
			GL_UNSIGNED_INT_8_8_8_8_REV,
			video_gl.framebuffer);
	}

	video_gl.f.glCallList(video_gl.displaylist);

	/* flip */
	video_gl.swap_buffers();
}

void video_opengl_quit(void)
{
	if (!video_gl.init)
		return;

#ifdef SCHISM_NVIDIA_PIXELDATARANGE
	if (video_gl.pixel_data_range) {
		video_gl.f.wglFreeMemoryNV(video_gl.framebuffer);
	} else
#endif
	{
		free(video_gl.framebuffer);
	}

	video_gl.init = 0;
	if (video_gl.object_unload) {
		video_gl.object_unload();
		video_gl.object_unload = NULL;
	}

	/* zero it out */
	memset(&video_gl, 0, sizeof(video_gl));
}

void video_opengl_report(void)
{
	log_appendf(5, " OpenGL interface");
#ifdef SCHISM_NVIDIA_PIXELDATARANGE
	if (video_gl.pixel_data_range)
		log_append(5,0, " NVidia pixel range extensions available");
#endif
#ifdef SCHISM_GL_EXT_PIXEL_BUFFER_OBJECT
	if (video_gl.pixel_buffer_object)
		log_append(5,0, " Pixel buffer object extensions available");
#endif
}
