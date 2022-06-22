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
#ifndef __video_h
#define __video_h

/* the vgamem implementation lives in draw-char.c
it needs access to the fonts, and it shrank recently :)
*/
void vgamem_clear(void);

struct vgamem_overlay {
	unsigned int x1, y1, x2, y2; /* in character cells... */

	unsigned char *q;               /* points inside ovl */
	unsigned int skip;

	int width, height; /* in pixels; signed to avoid bugs elsewhere */
};

void vgamem_flip(void);

void vgamem_ovl_alloc(struct vgamem_overlay *n);
void vgamem_ovl_apply(struct vgamem_overlay *n);

void vgamem_ovl_clear(struct vgamem_overlay *n, int color);
void vgamem_ovl_drawpixel(struct vgamem_overlay *n, int x, int y, int color);
void vgamem_ovl_drawline(struct vgamem_overlay *n, int xs, int ys, int xe, int ye, int color);


void vgamem_scan32(unsigned int y,unsigned int *out,unsigned int tc[16], unsigned int mouse_line[80]);
void vgamem_scan16(unsigned int y,unsigned short *out,unsigned int tc[16], unsigned int mouse_line[80]);
void vgamem_scan8(unsigned int y,unsigned char *out,unsigned int tc[16], unsigned int mouse_line[80]);

/* video output routines */
const char *video_driver_name(void);

void video_warp_mouse(unsigned int x, unsigned int y);
void video_redraw_texture(void);
void video_setup(const char *quality);
void video_startup(void);
void video_shutdown(void);
void video_report(void);
void video_refresh(void);
void video_update(void);
void video_colors(unsigned char palette[16][3]);
void video_resize(unsigned int width, unsigned int height);
void video_fullscreen(int new_fs_flag);
void video_translate(unsigned int vx, unsigned int vy,
			unsigned int *x, unsigned int *y);
void video_blit(void);
void video_mousecursor(int z);
int video_mousecursor_visible(void);

int video_is_fullscreen(void);
int video_width(void);
int video_height(void);
typedef struct SDL_Window SDL_Window;
SDL_Window * video_window(void);

typedef struct SDL_Surface SDL_Surface;
SDL_Surface *xpmdata(const char *xpmdata[]);

#if USE_X11
unsigned int xv_yuvlayout(void);
#endif

#define VIDEO_YUV_UYVY          0x59565955
#define VIDEO_YUV_YUY2          0x32595559
#define VIDEO_YUV_YV12          0x32315659
#define VIDEO_YUV_IYUV          0x56555949
#define VIDEO_YUV_YVYU          0x55595659
#define VIDEO_YUV_YV12_TV       (VIDEO_YUV_YV12 ^ 0xFFFFFFFF)
#define VIDEO_YUV_IYUV_TV       (VIDEO_YUV_IYUV ^ 0xFFFFFFFF)
#define VIDEO_YUV_RGBA          0x41424752
#define VIDEO_YUV_RGBT          0x54424752
#define VIDEO_YUV_RGB565        0x32424752
#define VIDEO_YUV_RGB24         0x0
#define VIDEO_YUV_RGB32         0x3
#define VIDEO_YUV_NONE          0xFFFFFFFF

#endif
