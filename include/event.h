/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
 * copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
 * copyright (c) 2009 Storlek & Mrs. Brisby
 * copyright (c) 2010-2011 Storlek
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
#ifndef _schismevent_h
#define _schismevent_h

#include "sdlmain.h"

#define SCHISM_EVENT_UPDATE_IPMIDI      SDL_USEREVENT+0
#define SCHISM_EVENT_MIDI               SDL_USEREVENT+1
#define SCHISM_EVENT_PLAYBACK           SDL_USEREVENT+2
#define SCHISM_EVENT_NATIVE             SDL_USEREVENT+3
#define SCHISM_EVENT_PASTE              SDL_USEREVENT+4

#define SCHISM_EVENT_MIDI_NOTE          1
#define SCHISM_EVENT_MIDI_CONTROLLER    2
#define SCHISM_EVENT_MIDI_PROGRAM       3
#define SCHISM_EVENT_MIDI_AFTERTOUCH    4
#define SCHISM_EVENT_MIDI_PITCHBEND     5
#define SCHISM_EVENT_MIDI_TICK          6
#define SCHISM_EVENT_MIDI_SYSEX         7
#define SCHISM_EVENT_MIDI_SYSTEM        8

#define SCHISM_EVENT_NATIVE_OPEN        1
#define SCHISM_EVENT_NATIVE_SCRIPT      16

#endif
