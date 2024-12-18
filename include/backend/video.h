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

#ifndef SCHISM_BACKEND_VIDEO_H_
#define SCHISM_BACKEND_VIDEO_H_

typedef struct {
	// extremely unorganized, sorry
	int (*init)(void);
	void (*quit)(void);

	int (*is_fullscreen)(void);
	int (*width)(void);
	int (*height)(void);
	const char *(*driver_name)(void);
	void (*report)(void);
	void (*set_hardware)(int hardware);
	void (*shutdown)(void);
	void (*setup)(const char *quality);
	void (*startup)(void);
	void (*fullscreen)(int new_fs_flag);
	void (*resize)(unsigned int width, unsigned int height);
	void (*colors)(unsigned char palette[16][3]);
	int (*is_focused)(void);
	int (*is_visible)(void);
	int (*is_wm_available)(void);
	int (*is_hardware)(void);
	void (*show_system_cursor)(int show);
	int (*is_screensaver_enabled)(void);
	void (*toggle_screensaver)(int enabled);
	void (*translate)(unsigned int vx, unsigned int vy, unsigned int *x, unsigned int *y);
	void (*get_logical_coordinates)(int x, int y, int *trans_x, int *trans_y);
	int (*is_input_grabbed)(void);
	void (*set_input_grabbed)(int enabled);
	void (*warp_mouse)(unsigned int x, unsigned int y);
	void (*get_mouse_coordinates)(unsigned int *x, unsigned int *y);
	int (*have_menu)(void);
	void (*toggle_menu)(int on);
	void (*blit)(void);
	void (*mousecursor_changed)(void);
	int (*get_wm_data)(video_wm_data_t *wm_data);
} schism_video_backend_t;

#ifdef SCHISM_SDL12
extern const schism_video_backend_t schism_video_backend_sdl12;
#endif
#ifdef SCHISM_SDL2
extern const schism_video_backend_t schism_video_backend_sdl2;
#endif

#endif /* SCHISM_BACKEND_VIDEO_H_ */
