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

#ifndef SCHISM_BACKEND_AUDIO_H_
#define SCHISM_BACKEND_AUDIO_H_

typedef struct {
	int freq; // sample rate
	uint8_t bits; // 8 or 16, always system byte order
	uint8_t channels; // channels
	uint16_t samples; // buffer size
	void (*callback)(uint8_t *stream, int len);
} schism_audio_spec_t;

/* An opaque structure that each backend uses for its own data */
typedef struct schism_audio_device schism_audio_device_t;

#ifdef SCHISM_SDL2
# define be_audio_driver_count sdl2_audio_driver_count
# define be_audio_driver_name sdl2_audio_driver_name

# define be_audio_device_count sdl2_audio_device_count
# define be_audio_device_name sdl2_audio_device_name

# define be_audio_init sdl2_audio_init
# define be_audio_quit sdl2_audio_quit

# define be_audio_open_device sdl2_audio_open_device

# define be_audio_close_device sdl2_audio_close_device
# define be_audio_lock_device sdl2_audio_lock_device
# define be_audio_unlock_device sdl2_audio_unlock_device
# define be_audio_pause_device sdl2_audio_pause_device
#elif defined(SCHISM_SDL12)
# define be_audio_driver_count sdl12_audio_driver_count
# define be_audio_driver_name sdl12_audio_driver_name

# define be_audio_device_count sdl12_audio_device_count
# define be_audio_device_name sdl12_audio_device_name

# define be_audio_init sdl12_audio_init
# define be_audio_quit sdl12_audio_quit

# define be_audio_open_device sdl12_audio_open_device

# define be_audio_close_device sdl12_audio_close_device
# define be_audio_lock_device sdl12_audio_lock_device
# define be_audio_unlock_device sdl12_audio_unlock_device
# define be_audio_pause_device sdl12_audio_pause_device
#endif

/* ---------------------------------------------------------- */
/* drivers */

int sdl2_audio_driver_count();
const char *sdl2_audio_driver_name(int i);

int sdl12_audio_driver_count();
const char *sdl12_audio_driver_name(int i);

/* --------------------------------------------------------------- */
/* devices */

int sdl2_audio_device_count(void);
const char *sdl2_audio_device_name(int i);

int sdl12_audio_device_count(void);
const char *sdl12_audio_device_name(int i);

/* ---------------------------------------------------------- */
/* REAL audio init */

int sdl2_audio_init(const char *driver);
void sdl2_audio_quit(void);

schism_audio_device_t *sdl2_audio_open_device(const char *name, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained);

void sdl2_audio_close_device(schism_audio_device_t *dev);
void sdl2_audio_lock_device(schism_audio_device_t *dev);
void sdl2_audio_unlock_device(schism_audio_device_t *dev);
void sdl2_audio_pause_device(schism_audio_device_t *dev, int paused);

int sdl12_audio_init(const char *driver);
void sdl12_audio_quit(void);

schism_audio_device_t *sdl12_audio_open_device(const char *name, const schism_audio_spec_t *desired, schism_audio_spec_t *obtained);

void sdl12_audio_close_device(schism_audio_device_t *dev);
void sdl12_audio_lock_device(schism_audio_device_t *dev);
void sdl12_audio_unlock_device(schism_audio_device_t *dev);
void sdl12_audio_pause_device(schism_audio_device_t *dev, int paused);

#endif /* SCHISM_BACKEND_AUDIO_H_ */