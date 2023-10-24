#include <errno.h>
#include <stdio.h>

#include "headers.h"
#include "it.h"
#include "song.h"

#include "rocket/lib/device.h"
#include "rocket/lib/sync.h"

typedef struct schism_rocket_t {
	int	last_set_current_order, last_set_current_row;
} schism_rocket_t;

static struct sync_device	*rocket_dev;
static unsigned			rocket_ms_per_row;
static schism_rocket_t		schism_rocket;

/* returns 0 on success, -errno on error */
/* host and/or port may be NULL for defaults: localhost and SYNC_DEFAULT_PORT */
int schism_rocket_connect(const char *host, const char *port, const char *bpm, const char *rpb)
{
	const char	*_host = "localhost";
	unsigned short	_port = SYNC_DEFAULT_PORT;
	unsigned int	_bpm = 125;
	unsigned int	_rpb = 8;

	if (host)
		_host = host;

	if (port) /* TODO: check for parse errors */
		sscanf(port, "%hu", &_port);

	if (bpm) /* TODO: check for parse errors */
		sscanf(bpm, "%u", &_bpm);

	if (rpb) /* TODO: check for parse errors */
		sscanf(rpb, "%u", &_rpb);

	rocket_ms_per_row = 60000 / (_bpm * _rpb);

	rocket_dev = sync_create_device("schism-rocket");
	if (!rocket_dev)
		return -ENOMEM;

	if (sync_tcp_connect(rocket_dev, _host, _port)) {
		sync_destroy_device(rocket_dev);
		/* XXX: rocket should really give a proper errno back */
		return -ECONNREFUSED;
	}

	return 0;
}

static void rocket_sync_pause(void *ctxt, int flag)
{
	printf("RKT GOT PAUSE flag=%i\n", flag);
	/* flag = 1 to pause, flag = 0 to unpause */
	/* TODO: either add a new song_pause() or modify existing to handle flag */
	/* I'm just open coding it here for now to get things working. */
	song_lock_audio();

	if (!(current_song->flags & SONG_PAUSED) && flag) {
		current_song->flags |= SONG_ENDREACHED;
		/* we don't want to just pause the playback, we want to position the
		 * pattern editor to the current location on pause as well.
		 */
		 set_current_order(song_get_current_order());
		 set_current_pattern(song_get_playing_pattern());
		 set_current_row(song_get_current_row());
	} else if (!flag)
		song_start_at_order(get_current_order(), get_current_row());

	song_unlock_audio();
	main_song_mode_changed_cb();
}

static void rocket_sync_set_row(void *ctxt, int row)
{
	unsigned	ms = row * rocket_ms_per_row;

	printf("RKT GOT SET ROW row=%i ms=%u ms_per_row=%u\n", row, ms, rocket_ms_per_row);

	/* sets the current schism pattern/order/row-in-pattern to Rocket's "row" */

	{ /* this is copied from schism/page.c::_timejump_ok */
		int	no, np, nr;

		song_get_at_time(ms, &no, &nr);
		set_current_order(no);
		np = current_song->orderlist[no];
		if (np < 200) { /* XXX: this is taken from _timejump_ok */
			set_current_pattern(np);
			set_current_row(nr);
		}
	}

	schism_rocket.last_set_current_order = get_current_order();
	schism_rocket.last_set_current_row = get_current_row();
}

static int rocket_sync_is_playing(void *ctxt)
{
	/* Note that in librocket this is only used to determine if we send our
	 * row up to the editor w/SET_ROW when _varying_ in sync_update().
	 *
	 * So if there's a desire to send SET_ROW commands to the Editor even when just
	 * moving around while paused within schism, we kind of always have to return true
	 * here for song-editing SET_ROWs to get sent despite being paused.
	 */
	return 1;
}

static struct sync_cb rocket_sync_cb = {
	rocket_sync_pause,
	rocket_sync_set_row,
	rocket_sync_is_playing,
};

void schism_rocket_update(void)
{
	int	row = rocket_dev->row; /* TODO: it'd be nice if sync_update() supported something like a -1 row for "whatever's in sync_device.row",
					* for when we just need to run the update to service the socket
					*/

	if (!current_song)
		return;

	if (!(current_song->flags & SONG_PAUSED)) {
		/* When playing the song, derive the current ms from samples_played. */
		/* Note this is why I needed to change Schism to prefill samples_played
		 * on play-from-order-row instead of always starting with 0, if that was
		 * going to result in a remotely close ms/Rocket-row.  The old code would
		 * always start from 0 for the Time/samples_played even on F7 where there's
		 * an actual known temporal point for the order+row being played from.
		 */
		row = samples_played / (current_song->mix_frequency / 1000) / rocket_ms_per_row;
	} else {
		int	co, cr;

		co = get_current_order();
		cr = get_current_row();

		/* Only update the rocket row using the editor's cursor position if it
		 * was changed by schism - and not by a received SET_ROW.
		 * The schism pattern data generally has less resolution than RocketEditor's,
		 * so if we just always updated with a cursorpos-to-ms-derived-row, we'd
		 * be basically rounding it to the nearest row schism could represent and
		 * constantly fighting with the RocketEditor's finer-grained movements.
		 */
		if (co != schism_rocket.last_set_current_order ||
		    cr != schism_rocket.last_set_current_row) {
			row = song_get_length_to_ms(co, cr) / rocket_ms_per_row;
			schism_rocket.last_set_current_order = co;
			schism_rocket.last_set_current_row = cr;
		}
	}

	sync_update(rocket_dev, row, &rocket_sync_cb, NULL);
}
