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
#include "mem.h"

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
		int visible;
	} mouse;

	uint32_t tc_bgr32[256];
	uint32_t pal_opengl[256];
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

/* -------------------------------------------------- */
/* mouse drawing */

static inline void make_mouseline(unsigned int x, unsigned int v, unsigned int y, uint32_t mouseline[80], uint32_t mouseline_mask[80], unsigned int mouse_y)
{
	const struct mouse_cursor *cursor = &cursors[video.mouse.shape];
	uint32_t i;

	uint32_t scenter, swidth, centeroffset;
	uint32_t z, zm;
	uint32_t temp;

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

	scenter = (cursor->center_x / 8) + (cursor->center_x % 8 != 0);
	swidth  = (cursor->width    / 8) + (cursor->width    % 8 != 0);
	centeroffset = cursor->center_x % 8;

	z  = cursor->pointer[y - mouse_y + cursor->center_y];
	zm = cursor->mask[y - mouse_y + cursor->center_y];

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

void video_blitLN(unsigned int bpp, unsigned char *pixels, unsigned int pitch, schism_map_rgb_spec map_rgb, void *map_rgb_data, int width, int height)
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
				vgamem_scan32(iny+1, csp, video.tc_bgr32, mouseline, mouseline_mask);
				dp = esp; esp = csp; csp=dp;
			} else {
				vgamem_scan32(iny, (csp = (uint32_t *)cv32backing), video.tc_bgr32, mouseline, mouseline_mask);
				vgamem_scan32(iny+1, (esp = (csp + NATIVE_SCREEN_WIDTH)), video.tc_bgr32, mouseline, mouseline_mask);
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
void video_blitNN(unsigned int bpp, unsigned char *pixels, unsigned int pitch, uint32_t tpal[256], int width, int height)
{
	// at most 32-bits...
	union {
		uint8_t uc[NATIVE_SCREEN_WIDTH];
		uint16_t us[NATIVE_SCREEN_WIDTH];
		uint32_t ui[NATIVE_SCREEN_WIDTH];
	} pixels_u;

	unsigned int mouse_x, mouse_y;
	video_get_mouse_coordinates(&mouse_x, &mouse_y);

	const int width_div_2 = (width / 2);
	const unsigned int mouseline_x = (mouse_x / 8);
	const unsigned int mouseline_v = (mouse_x % 8);
	const int pad = pitch - (width * bpp);
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];
	int x, y, last_scaled_y;

	for (y = 0; y < height; y++) {
		const int scaled_y = (y * NATIVE_SCREEN_HEIGHT / height);

		// only scan again if we have to or if this the first scan
		if (scaled_y != last_scaled_y || y == 0) {
			make_mouseline(mouseline_x, mouseline_v, scaled_y, mouseline, mouseline_mask, mouse_y);
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

		for (x = 0; x < width; x++) {
			const int scaled_x = (((x * NATIVE_SCREEN_WIDTH) + width_div_2) / width);

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
	unsigned int y, x;
	uint32_t mouseline[80];
	uint32_t mouseline_mask[80];

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

void video_resize(unsigned int width, unsigned int height)
{
	backend->resize(width, height);
}

/* calls back to a function receiving all the colors :) */
void video_colors_iterate(unsigned char palette[16][3], video_colors_callback_spec fun)
{
	/* this handles all of the ACTUAL color stuff, and the callback handles the backend-specific stuff */
	static const unsigned int lastmap[] = { 0, 1, 2, 3, 5 };
	unsigned int i, p;

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
	video.tc_bgr32[i] = rgb[2] |
		(rgb[1] << 8) |
		(rgb[0] << 16) | (0xFF << 24);
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
/* OpenGL crap */

#ifdef SCHISM_WIN32
# define APIENTRY __stdcall
#elif defined(SCHISM_OS2)
# define APIENTRY _System
#else
# define APIENTRY
#endif

typedef unsigned int GLenum;
typedef unsigned char GLboolean;
typedef unsigned int GLbitfield;
typedef void GLvoid;
typedef signed char GLbyte;		/* 1-byte signed */
typedef short GLshort;	/* 2-byte signed */
typedef int GLint;		/* 4-byte signed */
typedef unsigned char GLubyte;	/* 1-byte unsigned */
typedef unsigned short GLushort;	/* 2-byte unsigned */
typedef unsigned int GLuint;		/* 4-byte unsigned */
typedef int GLsizei;	/* 4-byte signed */
typedef float GLfloat;	/* single precision float */
typedef float GLclampf;	/* single precision float in [0,1] */
typedef double GLdouble;	/* double precision float */
typedef double GLclampd;	/* double precision float in [0,1] */

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
#define GL_NO_ERROR 0

#define SCHISM_NVIDIA_PIXELDATARANGE 1

#ifdef SCHISM_NVIDIA_PIXELDATARANGE

#define GL_WRITE_PIXEL_DATA_RANGE_NV      0x8878

static void *(APIENTRY *schism_wglAllocateMemoryNV)(int, float, float, float) = NULL;
static void (APIENTRY *schism_wglFreeMemoryNV)(void *) = NULL;

static void (APIENTRY *schism_glPixelDataRangeNV)(GLenum, GLsizei, GLvoid *) = NULL;
/* static void (APIENTRY *schism_glFlushPixelDataRangeNV)(GLenum) = NULL; */

#endif

static void (APIENTRY *schism_glCallList)(GLuint) = NULL;
static void (APIENTRY *schism_glTexSubImage2D)(GLenum,GLint,GLint,GLint,GLsizei,GLsizei,GLenum,GLenum,const GLvoid *) = NULL;
static void (APIENTRY *schism_glDeleteLists)(GLuint,GLsizei) = NULL;
static void (APIENTRY *schism_glTexParameteri)(GLenum,GLenum,GLint) = NULL;
static void (APIENTRY *schism_glBegin)(GLenum) = NULL;
static void (APIENTRY *schism_glEnd)(void) = NULL;
static void (APIENTRY *schism_glEndList)(void) = NULL;
static void (APIENTRY *schism_glTexCoord2f)(GLfloat,GLfloat) = NULL;
static void (APIENTRY *schism_glVertex2f)(GLfloat,GLfloat) = NULL;
static void (APIENTRY *schism_glNewList)(GLuint, GLenum) = NULL;
static GLboolean (APIENTRY *schism_glIsList)(GLuint) = NULL;
static GLuint (APIENTRY *schism_glGenLists)(GLsizei) = NULL;
static void (APIENTRY *schism_glLoadIdentity)(void) = NULL;
static void (APIENTRY *schism_glMatrixMode)(GLenum) = NULL;
static void (APIENTRY *schism_glEnable)(GLenum) = NULL;
static void (APIENTRY *schism_glDisable)(GLenum) = NULL;
static void (APIENTRY *schism_glBindTexture)(GLenum,GLuint) = NULL;
static void (APIENTRY *schism_glClear)(GLbitfield mask) = NULL;
static void (APIENTRY *schism_glClearColor)(GLclampf,GLclampf,GLclampf,GLclampf) = NULL;
static const GLubyte* (APIENTRY *schism_glGetString)(GLenum) = NULL;
static void (APIENTRY *schism_glTexImage2D)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,GLenum,GLenum,const GLvoid *) = NULL;
static void (APIENTRY *schism_glGetIntegerv)(GLenum, GLint *) = NULL;
static void (APIENTRY *schism_glShadeModel)(GLenum) = NULL;
static void (APIENTRY *schism_glGenTextures)(GLsizei,GLuint*) = NULL;
static void (APIENTRY *schism_glDeleteTextures)(GLsizei, const GLuint *) = NULL;
static void (APIENTRY *schism_glViewport)(GLint,GLint,GLsizei,GLsizei) = NULL;
static void (APIENTRY *schism_glEnableClientState)(GLenum) = NULL;
static GLenum (APIENTRY *schism_glGetError)(void) = NULL;

#define VIDEO_GL_PITCH (NATIVE_SCREEN_WIDTH * 4) /* bruh */

static struct {
	video_opengl_object_unload_spec object_unload;
	video_opengl_set_attribute_spec set_attribute;
	video_opengl_swap_buffers_spec  swap_buffers;

	int init;

	void *framebuffer;
	GLuint texture;
	GLuint displaylist;
	GLint max_texture_size;

#ifdef SCHISM_NVIDIA_PIXELDATARANGE
	int pixel_data_range;
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

	extensions = (const char *)schism_glGetString(GL_EXTENSIONS);
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
	int loaded = 0;

	if (video_gl.init)
		return 0; /* already init */

	memset(&video_gl, 0, sizeof(video_gl));

	if (!extension_supported) extension_supported = opengl_extension_supported_default;

	if (object_load(NULL)
#ifdef SCHISM_WIN32
		|| object_load("opengl.dll") || object_load("opengl32.dll")
#else
		|| object_load("libGL.so") || object_load("GL")
#endif
	) {
		loaded = 1;

#define Z(q) \
		do { \
			schism_##q = function_load(#q); \
			if (!schism_##q) \
				goto fail; \
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

		if (extension_supported("GL_NV_pixel_data_range")) {
			schism_glPixelDataRangeNV = function_load("glPixelDataRangeNV");
			schism_wglAllocateMemoryNV = function_load("wglAllocateMemoryNV");
			schism_wglFreeMemoryNV = function_load("wglFreeMemoryNV");

			video_gl.pixel_data_range = (schism_glPixelDataRangeNV
				&& schism_wglAllocateMemoryNV
				&& schism_wglFreeMemoryNV);
		}

		video_gl.object_unload = object_unload;
		video_gl.set_attribute = set_attribute;
		video_gl.swap_buffers  = swap_buffers;

		video_gl.init = 1;

		return 1;
	}

fail:
	if (loaded && object_unload)
		object_unload();

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

	schism_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, x);
	schism_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, x);
}

int video_opengl_setup(uint32_t w, uint32_t h,
	int (*callback)(uint32_t *px, uint32_t *py, uint32_t *pw, uint32_t *ph))
{
	GLfloat tex_width, tex_height;
	int32_t texsize;
	uint32_t x, y;

	if (!video_gl.init)
		return 0;

	video_gl.set_attribute(VIDEO_GL_DOUBLEBUFFER, 1);
	video_gl.set_attribute(VIDEO_GL_STENCIL_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_DEPTH_SIZE, 32);
	video_gl.set_attribute(VIDEO_GL_ACCUM_RED_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_ACCUM_GREEN_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_ACCUM_BLUE_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_ACCUM_ALPHA_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_SWAP_CONTROL, 0);

	if (!callback(&x, &y, &w, &h))
		return 0; /* ? */

	/* is this really necessary? */
	video_gl.set_attribute(VIDEO_GL_DOUBLEBUFFER, 1);
	video_gl.set_attribute(VIDEO_GL_STENCIL_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_DEPTH_SIZE, 32);
	video_gl.set_attribute(VIDEO_GL_ACCUM_RED_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_ACCUM_GREEN_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_ACCUM_BLUE_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_ACCUM_ALPHA_SIZE, 0);
	video_gl.set_attribute(VIDEO_GL_SWAP_CONTROL, 0);

	schism_glViewport(x, y, w, h);

	if (!video_gl.framebuffer) {
#ifdef SCHISM_NVIDIA_PIXELDATARANGE
		if (video_gl.pixel_data_range) {
			video_gl.framebuffer = schism_wglAllocateMemoryNV(
				NATIVE_SCREEN_WIDTH * NATIVE_SCREEN_HEIGHT * 4,
				0.0, 1.0, 1.0);
			if (video_gl.framebuffer) {
				schism_glPixelDataRangeNV(GL_WRITE_PIXEL_DATA_RANGE_NV,
					NATIVE_SCREEN_WIDTH * NATIVE_SCREEN_HEIGHT * 4,
					video_gl.framebuffer);
				schism_glEnableClientState(GL_WRITE_PIXEL_DATA_RANGE_NV);
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

	schism_glMatrixMode(GL_PROJECTION);
	schism_glDeleteTextures(1, &video_gl.texture);
	schism_glGenTextures(1, &video_gl.texture);
	schism_glBindTexture(GL_TEXTURE_2D, video_gl.texture);
	schism_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
	schism_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);

	video_opengl_reset_interpolation();

	schism_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize,
		0, GL_BGRA_EXT, GL_UNSIGNED_INT_8_8_8_8_REV, 0);
	if (schism_glGetError() != GL_NO_ERROR)
		return 0; /* nope */

	schism_glClearColor(0.0, 0.0, 0.0, 1.0);
	schism_glClear(GL_COLOR_BUFFER_BIT);
	video_gl.swap_buffers();
	schism_glClear(GL_COLOR_BUFFER_BIT);
	schism_glShadeModel(GL_FLAT);
	schism_glDisable(GL_DEPTH_TEST);
	schism_glDisable(GL_LIGHTING);
	schism_glDisable(GL_CULL_FACE);
	schism_glEnable(GL_TEXTURE_2D);
	schism_glMatrixMode(GL_MODELVIEW);
	schism_glLoadIdentity();

	tex_width = ((GLfloat)(NATIVE_SCREEN_WIDTH) / (GLfloat)texsize);
	tex_height = ((GLfloat)(NATIVE_SCREEN_HEIGHT) / (GLfloat)texsize);

	if (schism_glIsList(video_gl.displaylist))
		schism_glDeleteLists(video_gl.displaylist, 1);

	video_gl.displaylist = schism_glGenLists(1);
	schism_glNewList(video_gl.displaylist, GL_COMPILE);
	schism_glBindTexture(GL_TEXTURE_2D, video_gl.texture);
	schism_glBegin(GL_QUADS);

	schism_glTexCoord2f(0, tex_height);
	schism_glVertex2f(-1.0f, -1.0f);
	schism_glTexCoord2f(tex_width, tex_height);
	schism_glVertex2f(1.0f, -1.0f);
	schism_glTexCoord2f(tex_width, 0);
	schism_glVertex2f(1.0f, 1.0f);
	schism_glTexCoord2f(0, 0);
	schism_glVertex2f(-1.0f, 1.0f);

	schism_glEnd();
	schism_glEndList();

	return 1;
}

SCHISM_HOT void video_opengl_blit(void)
{
	if (!video_gl.init || !video_gl.framebuffer)
		return;

	/* we should probably be using RGBA and not BGRA */
	video_blit11(4, video_gl.framebuffer, VIDEO_GL_PITCH, video.tc_bgr32);

	schism_glBindTexture(GL_TEXTURE_2D, video_gl.texture);
	schism_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
		NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT,
		GL_BGRA_EXT,
		GL_UNSIGNED_INT_8_8_8_8_REV,
		video_gl.framebuffer);
	schism_glCallList(video_gl.displaylist);

	/* flip */
	video_gl.swap_buffers();
}

void video_opengl_quit(void)
{
	if (!video_gl.init)
		return;

	if (video_gl.pixel_data_range) {
		schism_wglFreeMemoryNV(video_gl.framebuffer);
	} else {
		free(video_gl.framebuffer);
	}

	video_gl.init = 0;
	if (video_gl.object_unload) {
		video_gl.object_unload();
		video_gl.object_unload = NULL;
	}
}

int video_opengl_used(void)
{
	return video_gl.init;
}

void video_opengl_report(void)
{
	log_append(2,0, " Using OpenGL interface");
#ifdef SCHISM_NVIDIA_PIXELDATARANGE
	if (video_gl.pixel_data_range)
		log_append(2,0, " Using NVidia pixel range extensions");
#endif
}
