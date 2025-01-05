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

#ifndef SCHISM_TIMER_H_
#define SCHISM_TIMER_H_

#include "headers.h"

typedef uint64_t schism_ticks_t;

// Get the amount of milliseconds since timer_init() was called
schism_ticks_t timer_ticks(void);

// Same as the above but in microseconds
schism_ticks_t timer_ticks_us(void);

// Legacy macro, backends should account for missing bits in the timer
// by e.g. keeping a global state. See the SDL 1.2 backend for an example
// of this.
#define timer_ticks_passed(a, b) ((a) >= (b))

// Sleep for `usec` microseconds. (This may have much much less precision
// than anticipated!)
void timer_usleep(uint64_t usec);

// Old functions that are simply macros now.
#define timer_msleep(ms) timer_usleep((ms) * 1000)
#define timer_delay(ms)  timer_msleep(ms)

// Run a function after `ms` milliseconds
void timer_oneshot(uint32_t ms, void (*callback)(void *param), void *param);

// init/quit
int timer_init(void);
void timer_quit(void);

#endif /* SCHISM_TIMER_H_ */
