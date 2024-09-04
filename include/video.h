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
#ifndef SCHISM_VIDEO_H_
#define SCHISM_VIDEO_H_

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
void video_translate(int vx, int vy, unsigned int *x, unsigned int *y);
void video_blit(void);

/* cursor-specific stuff */
enum video_mousecursor_shape {
	CURSOR_SHAPE_ARROW,
	CURSOR_SHAPE_CROSSHAIR,
};

void video_mousecursor(int z); /* takes in the MOUSE_* enum from it.h (why is it there?) */
int video_mousecursor_visible(void);
void video_set_mousecursor_shape(enum video_mousecursor_shape shape);

/* getters, will sometimes poll SDL */

int video_is_fullscreen(void);
int video_is_wm_available(void);
int video_is_focused(void);
int video_is_visible(void);
int video_width(void);
int video_height(void);
SDL_Window *video_window(void);

void video_get_logical_coordinates(int x, int y, int *trans_x, int *trans_y);

SDL_Surface *xpmdata(const char *xpmdata[]);

#endif /* SCHISM_VIDEO_H_ */
