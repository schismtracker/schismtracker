/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
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

#ifndef MIXER_H
#define MIXER_H

#define VOLUME_MAX 100

/* Hmm. I've found that some systems actually don't support the idea of a
 * "master" volume, so this became necessary. I suppose PCM is really the
 * more useful setting to change anyway. */
#if 0
# define SCHISM_MIXER_CONTROL SOUND_MIXER_VOLUME
#else
# define SCHISM_MIXER_CONTROL SOUND_MIXER_PCM
#endif

void mixer_read_volume(int *left, int *right);
void mixer_write_volume(int left, int right);

#endif /* ! MIXER_H */
