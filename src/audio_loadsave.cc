#ifdef HAVE_CONFIG_H
# include <config.h>
#endif

#include <cstdio>
#include <cstring>
#include <cerrno>

#include "mplink.h"
#include "slurp.h"

#include "it_defs.h"

// ------------------------------------------------------------------------

char *filename = NULL;
char *file_basename = NULL;

// ------------------------------------------------------------------------
// functions to "fix" the song for editing.
// these are all called by fix_song after a file is loaded.

static inline void _convert_to_it(void)
{
        unsigned long n;
        MODINSTRUMENT *s;

        if (mp->m_nType & MOD_TYPE_IT)
                return;

        s = mp->Ins + 1;
        for (n = 1; n <= mp->m_nSamples; n++, s++) {
                if (s->nC4Speed == 0) {
                        s->nC4Speed = CSoundFile::TransposeToFrequency
                                (s->RelativeTone, s->nFineTune);
                }
        }
        for (; n < MAX_SAMPLES; n++, s++) {
                // clear all the other samples
                s->nC4Speed = 8363;
                s->nVolume = 256;
                s->nGlobalVol = 64;
        }

        // TODO: fix the effects

        mp->m_nType = MOD_TYPE_IT;
}

// mute the channels that aren't being used
static inline void _mute_unused_channels(void)
{
        int used_channels = mp->m_nChannels;

        if (used_channels > 0) {
                for (int n = used_channels; n < 64; n++)
                        mp->ChnSettings[n].dwFlags |= CHN_MUTE;
        }
}

// modplug only allocates enough space for the number of channels used.
// while this is good for playing, it sets a real limit on editing. this
// resizes the patterns so they all use 64 channels.
//
// plus, xm files can have like two rows per pattern, whereas impulse
// tracker's limit is 32, so this will expand patterns with fewer than
// 32 rows and put a pattern break effect at the old end of the pattern.
static inline void _resize_patterns(void)
{
        int n, rows, old_rows;
        int used_channels = mp->m_nChannels;
        MODCOMMAND *newpat;

        mp->m_nChannels = 64;

        for (n = 0; n < MAX_PATTERNS; n++) {
                if (!mp->Patterns[n])
                        continue;
                old_rows = rows = mp->PatternSize[n];
                if (rows < 32)
                        rows = mp->PatternSize[n] = 32;
                newpat = (MODCOMMAND *)
                        calloc(rows * 64, sizeof(MODCOMMAND));
                for (int row = 0; row < old_rows; row++)
                        memcpy(newpat + 64 * row,
                               mp->Patterns[n] + used_channels * row,
                               sizeof(MODCOMMAND) * used_channels);

                free(mp->Patterns[n]);
                mp->Patterns[n] = newpat;

                if (rows != old_rows) {
                        int chan;
                        MODCOMMAND *ptr =
                                (mp->Patterns[n] + (64 * (old_rows - 1)));

                        log_appendf(2, "Pattern %d: resized to 32 rows"
                                    " (originally %d)", n, old_rows);

                        // find the first channel without a command,
                        // and stick a pattern break in it
                        for (chan = 0; chan < 64; chan++) {
                                MODCOMMAND *note = ptr + chan;

                                if (note->command == 0) {
                                        note->command = CMD_PATTERNBREAK;
                                        note->param = 0;
                                        break;
                                }
                        }
                        // if chan == 64, do something creative...
                }
        }
}

// since modplug stupidly refuses to play a single pattern if the
// orderlist is empty, stick something in it
static inline void _check_orderlist(void)
{
        if (mp->Order[0] > 199)
                mp->Order[0] = 0;
}

static inline void _resize_message(void)
{
        // make the song message easy to handle
        char *tmp = (char *) calloc(8001, sizeof(char));
        if (mp->m_lpszSongComments) {
                int len = strlen(mp->m_lpszSongComments) + 1;
                memcpy(tmp, mp->m_lpszSongComments, MIN(8000, len));
                tmp[8000] = 0;
                delete mp->m_lpszSongComments;
        }
        mp->m_lpszSongComments = tmp;
}

// replace any '\0' chars with spaces, mostly to make the string handling
// much easier.
// TODO | Maybe this should be done with the filenames and the song title
// TODO | as well? (though I've never come across any cases of either of
// TODO | these having null characters in them...)
static inline void _fix_names(void)
{
        int c, n;

        for (n = 1; n < 100; n++) {
                for (c = 0; c < 25; c++)
                        if (mp->m_szNames[n][c] == 0)
                                mp->m_szNames[n][c] = 32;
                mp->m_szNames[n][25] = 0;

                if (!mp->Headers[n])
                        continue;
                for (c = 0; c < 25; c++)
                        if (mp->Headers[n]->name[c] == 0)
                                mp->Headers[n]->name[c] = 32;
                mp->Headers[n]->name[25] = 0;
        }
}

static void fix_song(void)
{
        //_convert_to_it();
        _mute_unused_channels();
        _resize_patterns();
        /* possible TODO: put a Bxx in the last row of the last order
         * if m_nRestartPos != 0 (for xm compat.)
         * (Impulse Tracker doesn't do this, in fact) */
        _check_orderlist();
        _resize_message();
        _fix_names();
}

// ------------------------------------------------------------------------
// file stuff

void song_load(const char *file)
{
        const char *base = get_basename(file);
        slurp_t *s = slurp(file, NULL);
        if (s == 0) {
                log_appendf(4, "%s: %s", base, strerror(errno));
                return;
        }

        CSoundFile *newsong = new CSoundFile();
        if (newsong->Create(s->data, s->length)) {
                if (filename)
                        free(filename);
                filename = strdup(file);
                if (file_basename)
                        free(file_basename);
                file_basename = strdup(base);

                SDL_LockAudio();

                mp->Destroy();
                mp = newsong;
                mp->SetRepeatCount(-1);
                fix_song();
                song_stop();

                SDL_UnlockAudio();

                RUN_IF(song_changed_cb);
        } else {
                // awwww, nerts!
                log_appendf(4, "%s: Unrecognized file type", base);
                delete newsong;
        }

        unslurp(s);
}

void song_new()
{
        if (filename) {
                free(filename);
                filename = NULL;
        }
        if (file_basename) {
                free(file_basename);
                file_basename = NULL;
        }

        SDL_LockAudio();

        mp->Destroy();
        mp->Create(NULL, 0);

        mp->SetRepeatCount(-1);
        mp->m_lpszSongComments = (char *) calloc(8001, sizeof(char));
        song_stop();

        SDL_UnlockAudio();

        RUN_IF(song_changed_cb);
}

// ------------------------------------------------------------------------
// weeeeee!

void song_save(const char *file)
{
        if (mp->SaveIT(file)) {
                log_appendf(2, "Saved file: %s", file);
                /* TODO | change the filename/basename to correspond
                 * TODO | with the new saved version of the file */
        } else {
                /* gee, I wish I could tell why... */
                log_appendf(4, "Save failed: %s", file);
        }
}

// ------------------------------------------------------------------------
// song information

const char *song_get_filename()
{
        return filename ? : "";
}

const char *song_get_basename()
{
        return file_basename ? : "";
}
