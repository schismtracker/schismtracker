#define NEED_TIME
#include "headers.h"

#include <SDL.h>
#include <stdarg.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */

static char *status_text = NULL;
static struct timeval text_timeout;

/* --------------------------------------------------------------------- */

void status_text_flash(const char *format, ...)
{
        va_list ap;

        if (gettimeofday(&text_timeout, NULL) < 0) {
                perror("gettimeofday");
                return;
        }

        /* the message expires in one second */
        text_timeout.tv_sec++;

        if (status_text)
                free(status_text);

        va_start(ap, format);
        vasprintf(&status_text, format, ap);
        va_end(ap);

        status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static inline void draw_song_playing_status(void)
{
        int pos = 2;
        char buf[16];
        int pattern = song_get_current_pattern();

        SDL_LockSurface(screen);

        pos += draw_text_unlocked("Playing, Order: ", 2, 9, 0, 2);
        pos += draw_text_unlocked(numtostr_n
                                  (song_get_current_order(), buf), pos, 9,
                                  3, 2);
        draw_char_unlocked('/', pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr_n(song_get_num_orders(), buf),
                                  pos, 9, 3, 2);
        pos += draw_text_unlocked(", Pattern: ", pos, 9, 0, 2);
        pos += draw_text_unlocked(numtostr_n(pattern, buf), pos, 9, 3, 2);
        pos += draw_text_unlocked(", Row: ", pos, 9, 0, 2);
        pos += draw_text_unlocked(numtostr_n(song_get_current_row(), buf),
                                  pos, 9, 3, 2);
        draw_char_unlocked('/', pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr_n
                                  (song_get_pattern(pattern, NULL), buf),
                                  pos, 9, 3, 2);
        draw_char_unlocked(',', pos, 9, 0, 2);
        pos++;
        draw_char_unlocked(0, pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr_n
                                  (song_get_playing_channels(), buf), pos,
                                  9, 3, 2);
        SDL_UnlockSurface(screen);

        if (draw_text_len(" Channels", 62 - pos, pos, 9, 0, 2) < 9)
                draw_char(16, 61, 9, 1, 2);
}

static inline void draw_pattern_playing_status(void)
{
        int pos = 2;
        char buf[16];
        int pattern = song_get_current_pattern();

        SDL_LockSurface(screen);

        pos += draw_text_unlocked("Playing, Pattern: ", 2, 9, 0, 2);
        pos += draw_text_unlocked(numtostr_n(pattern, buf), pos, 9, 3, 2);
        pos += draw_text_unlocked(", Row: ", pos, 9, 0, 2);
        pos += draw_text_unlocked(numtostr_n(song_get_current_row(), buf),
                                  pos, 9, 3, 2);
        draw_char_unlocked('/', pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr_n
                                  (song_get_pattern(pattern, NULL), buf),
                                  pos, 9, 3, 2);
        draw_char_unlocked(',', pos, 9, 0, 2);
        pos++;
        draw_char_unlocked(0, pos, 9, 0, 2);
        pos++;
        pos += draw_text_unlocked(numtostr_n
                                  (song_get_playing_channels(), buf), pos,
                                  9, 3, 2);
        SDL_UnlockSurface(screen);

        if (draw_text_len(" Channels", 62 - pos, pos, 9, 0, 2) < 9)
                draw_char(16, 61, 9, 1, 2);
}

void status_text_redraw(void)
{
        struct timeval tv;

        gettimeofday(&tv, NULL);

        /* if there's a message set, and it's expired, clear it */
        if (status_text && timercmp(&tv, &text_timeout, >)) {
                free(status_text);
                status_text = NULL;
        }

        if (status_text) {
                draw_text_len(status_text, 60, 2, 9, 0, 2);
        } else {
                switch (song_get_mode()) {
                case MODE_PLAYING:
                        draw_song_playing_status();
                        break;
                case MODE_PATTERN_LOOP:
                        draw_pattern_playing_status();
                        break;
                default:       /* MODE_STOPPED */
                        draw_fill_chars(2, 9, 62, 9, 2);
                        break;
                }
        }
}
