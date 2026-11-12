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

#include "headers.h"

#include "playlist.h"
#include "song.h"
#include "dmoz.h"
#include "log.h"
#include "mem.h"

/* --------------------------------------------------------------------- */

static char **entries = NULL;   /* str_dup'd file paths */
static int count = 0;
static int alloc = 0;

static int current = -1;        /* now-playing index, -1 = none */
static int active = 0;          /* auto-advance armed */
static unsigned int max_time = 0; /* per-track cap in seconds, 0 = off */

/* --------------------------------------------------------------------- */

void playlist_set_max_time(unsigned int secs)
{
	max_time = secs;
}

int playlist_count(void)
{
	return count;
}

int playlist_current(void)
{
	return current;
}

int playlist_active(void)
{
	return active;
}

const char *playlist_path(int index)
{
	return (index >= 0 && index < count) ? entries[index] : NULL;
}

const char *playlist_basename(int index)
{
	return (index >= 0 && index < count) ? dmoz_path_get_basename(entries[index]) : "";
}

/* --------------------------------------------------------------------- */

void playlist_clear(void)
{
	int n;

	for (n = 0; n < count; n++)
		free(entries[n]);
	free(entries);
	entries = NULL;
	count = alloc = 0;
	current = -1;
	active = 0;
}

int playlist_add(const char *path)
{
	if (!path || !*path)
		return -1;

	if (count >= alloc) {
		alloc = alloc ? alloc * 2 : 16;
		entries = mem_realloc(entries, alloc * sizeof(*entries));
	}
	entries[count] = str_dup(path);
	return count++;
}

void playlist_remove(int index)
{
	int n;

	if (index < 0 || index >= count)
		return;

	free(entries[index]);
	for (n = index; n < count - 1; n++)
		entries[n] = entries[n + 1];
	count--;

	/* keep the now-playing pointer aimed at the same entry */
	if (index < current)
		current--;
	else if (index == current)
		current = -1;
}

void playlist_move(int from, int to)
{
	char *tmp;
	int n;

	if (from < 0 || from >= count || to < 0 || to >= count || from == to)
		return;

	tmp = entries[from];
	if (from < to) {
		for (n = from; n < to; n++)
			entries[n] = entries[n + 1];
	} else {
		for (n = from; n > to; n--)
			entries[n] = entries[n - 1];
	}
	entries[to] = tmp;

	/* track the now-playing entry across the shuffle */
	if (current == from)
		current = to;
	else if (from < current && current <= to)
		current--;
	else if (to <= current && current < from)
		current++;
}

/* --------------------------------------------------------------------- */

/* Clean-stop, then load the first playable entry starting at idx and moving by
   step (+1/-1), skipping any that fail to load. Sets the now-playing index and
   plays it once. If nothing in that direction loads, auto-advance is disarmed. */
static void play_from(int idx, int step)
{
	/* clean handoff: MIDI all-notes-off, OPL/GM reset. This also leaves the song
	   stopped so song_load_unchecked() won't auto-start with the looping
	   song_start() even when "play after load" is enabled. */
	song_stop();

	while (idx >= 0 && idx < count) {
		if (song_load_unchecked(entries[idx])) {
			current = idx;
			song_start_once();
			active = 1;
			/* discard any end-of-song latched while the previous track was
			   being torn down, so the freshly started track isn't skipped */
			song_check_natural_end();
			status_text_flash("Playing (%d/%d) %s", idx + 1, count, playlist_basename(idx));
			return;
		}
		log_appendf(4, "Playlist: skipping unloadable %s", entries[idx]);
		idx += step;
	}

	/* fell off the end with nothing playable */
	active = 0;
	current = -1;
}

void playlist_start(int index)
{
	if (count == 0)
		return;
	play_from(index, +1);
}

void playlist_advance(int step)
{
	if (!active || count == 0)
		return;
	play_from(current + step, step);
}

void playlist_stop(void)
{
	active = 0;
}

void playlist_poll(void)
{
	/* always consume the latch so a stale end-of-song can't trigger a spurious
	   advance the next time the playlist is armed */
	int ended = song_check_natural_end();

	if (!active || count == 0)
		return;

	/* time cap only applies while actually playing, so a user stop (which also
	   leaves the song at MODE_STOPPED) doesn't get force-advanced */
	if (!ended && max_time
	    && song_get_mode() == MODE_PLAYING
	    && song_get_current_time() >= max_time)
		ended = 1;

	if (ended)
		playlist_advance(+1);
}
