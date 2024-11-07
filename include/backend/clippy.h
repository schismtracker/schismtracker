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

#ifndef SCHISM_BACKEND_CLIPPY_H_
#define SCHISM_BACKEND_CLIPPY_H_

#include "headers.h"

#ifdef SCHISM_SDL2
# define be_clippy_have_selection sdl2_clippy_have_selection
# define be_clippy_have_clipboard sdl2_clippy_have_clipboard

# define be_clippy_set_selection sdl2_clippy_set_selection
# define be_clippy_set_clipboard sdl2_clippy_set_clipboard

# define be_clippy_get_selection sdl2_clippy_get_selection
# define be_clippy_get_clipboard sdl2_clippy_get_clipboard
#elif SCHISM_SDL12
# define be_clippy_have_selection sdl12_clippy_have_selection
# define be_clippy_have_clipboard sdl12_clippy_have_clipboard

# define be_clippy_set_selection sdl12_clippy_set_selection
# define be_clippy_set_clipboard sdl12_clippy_set_clipboard

# define be_clippy_get_selection sdl12_clippy_get_selection
# define be_clippy_get_clipboard sdl12_clippy_get_clipboard
#endif

int sdl12_clippy_have_selection(void);
int sdl12_clippy_have_clipboard(void);
void sdl12_clippy_set_selection(const char *text);
void sdl12_clippy_set_clipboard(const char *text);
char *sdl12_clippy_get_selection(void);
char *sdl12_clippy_get_clipboard(void);

int sdl2_clippy_have_selection(void);
int sdl2_clippy_have_clipboard(void);
void sdl2_clippy_set_selection(const char *text);
void sdl2_clippy_set_clipboard(const char *text);
char *sdl2_clippy_get_selection(void);
char *sdl2_clippy_get_clipboard(void);

#endif /* SCHISM_BACKEND_CLIPPY_H_ */