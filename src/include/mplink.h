#ifndef _MPLINK_H
# define _MPLINK_H

#include "it.h"
#include "song.h"

#include "stdafx.h"
#include "sndfile.h"

extern CSoundFile *mp;

extern char *filename;  /* the full path (as given to song_load) */
extern char *file_basename;     /* everything after the last slash */

/* milliseconds = (samples * 1000) / frequency */
extern unsigned long samples_played;

extern unsigned long max_channels_used;

/* called whenever the song is *actually* changed */
extern void (*song_changed_cb) (void);

#endif /* ! _MPLINK_H */
