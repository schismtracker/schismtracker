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

#ifndef SCHISM_BACKEND_INIT_H
#define SCHISM_BACKEND_INIT_H

#ifdef SCHISM_SDL2
# define be_init sdl2_init
# define be_quit sdl2_quit
#elif SCHISM_SDL12
# define be_init sdl12_init
# define be_quit sdl12_quit
#endif

int sdl2_init(void);
void sdl2_quit(void);

// these are in the events file...
int sdl2_controller_init(void);
int sdl2_controller_quit(void);

int sdl12_init(void);
void sdl12_quit(void);

#endif /* SCHISM_BACKEND_INIT_H */