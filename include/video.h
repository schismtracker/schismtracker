/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
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
#ifndef __video_h
#define __video_h

/* the vgamem implementation lives in draw-char.c
it needs access to the fonts, and it shrank recently :)
*/
void vgamem_clear(void);

struct vgamem_overlay {
	unsigned int x1, y1, x2, y2; /* in character cells... */
	unsigned int pitch, size;	/* in bytes */
	unsigned int chars, base;
	int width, height; /* in pixels; signed to avoid bugs elsewhere */
};

void vgamem_lock(void);
void vgamem_unlock(void);
void vgamem_flip(void);
void vgamem_font_reserve(struct vgamem_overlay *n);
void vgamem_fill_reserve(struct vgamem_overlay *n, int fg, int bg);
void vgamem_font_putpixel(struct vgamem_overlay *n, int x, int y);
void vgamem_font_clearpixel(struct vgamem_overlay *n, int x, int y);
void vgamem_font_drawline(struct vgamem_overlay *n, int xs, int ys, int xe, int ye);

void vgamem_scan32(unsigned int y,unsigned int *out,unsigned int tc[16]);
void vgamem_scan16(unsigned int y,unsigned short *out,unsigned int tc[16]);
void vgamem_scan8(unsigned int y,unsigned char *out,unsigned int tc[16]);

/* video output routines */
const char *video_driver_name(void);

void video_mode(int mode);
void video_init(const char *id);
void video_colors(unsigned char palette[16][3]);
void video_resize(unsigned int width, unsigned int height);
void video_fullscreen(int tri);
void video_translate(unsigned int vx, unsigned int vy,
			unsigned int *x, unsigned int *y);
void video_blit(void);
void video_mousecursor(int z);

int video_is_fullscreen(void);

#endif
