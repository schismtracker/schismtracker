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

#ifndef SCHISM_PLAYLIST_H_
#define SCHISM_PLAYLIST_H_

#ifdef __cplusplus
extern "C" {
#endif

/* An ordered list of module file paths played in sequence, advancing
   automatically when the current module reaches its natural end (or an optional
   per-track time cap). The queue is edited from the playlist page; playback is
   driven by playlist_poll(), called once per idle pass from event_loop(). */

/* queue editing */
void playlist_clear(void);
int  playlist_add(const char *path);    /* append a copy of path; returns its index, or -1 */
void playlist_remove(int index);
void playlist_move(int from, int to);   /* reorder one entry */
int  playlist_count(void);
const char *playlist_path(int index);     /* full path, or NULL if out of range */
const char *playlist_basename(int index); /* display name, or "" if out of range */

/* now-playing index (-1 if none); whether auto-advance is armed */
int  playlist_current(void);
int  playlist_active(void);

/* playback control */
void playlist_start(int index);   /* clean-stop, load index, play once, arm auto-advance */
void playlist_advance(int step);  /* +1 next / -1 prev; skips unloadable entries */
void playlist_stop(void);         /* disarm auto-advance (does not stop the audio) */

/* called every idle pass; advances on natural-end latch or time cap */
void playlist_poll(void);

/* per-track wall-clock cap in seconds; 0 disables (the default) */
void playlist_set_max_time(unsigned int secs);

#ifdef __cplusplus
}
#endif

#endif /* SCHISM_PLAYLIST_H_ */
