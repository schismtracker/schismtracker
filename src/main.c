/* This is the first thing I *ever* did with SDL. :) */

// experimental and broken, but go ahead anyway :P
//#define USE_EVENT_THREAD

#include "headers.h"

#include <SDL.h>

#ifdef USE_EVENT_THREAD
# include <SDL_thread.h>
#endif

#include <unistd.h>

#include "it.h"
#include "song.h"
#include "page.h"

/* --------------------------------------------------------------------- */
/* globals */

SDL_Surface *screen;

/* --------------------------------------------------------------------- */

static inline void display_print_video_info(void)
{
        const SDL_VideoInfo *info = SDL_GetVideoInfo();

        log_append(2, 0, "Video hardware capabilities");
        log_append(2, 0, "\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81"
                   "\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81"
                   "\x81\x81\x81");

        if (info->video_mem)
                log_appendf(5, " %dk video memory", info->video_mem);
        if (info->hw_available) {
                char caps[80] = " Accelerated blit capabilities:";

                log_append(5, 0, " Hardware surfaces supported");

                if (info->blit_hw)
                        strcat(caps, " hw");
                if (info->blit_hw_CC)
                        strcat(caps, " hw_CC");
                if (info->blit_hw_A)
                        strcat(caps, " hw_A");
                if (info->blit_sw)
                        strcat(caps, " sw");
                if (info->blit_sw_CC)
                        strcat(caps, " sw_CC");
                if (info->blit_sw_A)
                        strcat(caps, " sw_A");
                if (info->blit_fill)
                        strcat(caps, " fill");
                if (*(strchr(caps, ':') + 1) != 0)
                        log_append(5, 1, strdup(caps));
        }

        log_append(5, 0, " Best pixel format:");
        log_appendf(5, "  Bits/pixel = %d", info->vfmt->BitsPerPixel);
        log_appendf(5, "  Bytes/pixel = %d", info->vfmt->BytesPerPixel);

        log_append(0, 0, "");
        log_append(0, 0, "");
}

static inline void display_print_info(void)
{
        char buf[256];

        log_append(2, 0, "Video initialised");
        log_append(2, 0, "\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81\x81"
                   "\x81\x81\x81\x81\x81\x81");
        log_appendf(5, " Using driver '%s'",
                    SDL_VideoDriverName(buf, 256));
        log_appendf(5, " %s surface", ((screen->flags & SDL_HWSURFACE)
                                       ? "Hardware" : "Software"));
        if (screen->flags & SDL_DOUBLEBUF)
                log_append(5, 0, " Double buffered");
        if (screen->flags & SDL_HWACCEL)
                log_append(5, 0, " Accelerated blits");
        if (screen->flags & SDL_RLEACCEL)
                log_append(5, 0, " RLE accelerated");
        if (SDL_MUSTLOCK(screen))
                log_append(4, 0, " Must lock surface");
        log_append(5, 0, " Pixel format:");
        log_appendf(5, "  Bits/pixel = %d", screen->format->BitsPerPixel);
        log_appendf(5, "  Bytes/pixel = %d",
                    screen->format->BytesPerPixel);
        
        log_append(0, 0, "");
        log_append(0, 0, "");
}

static void display_init(void)
{
        /* Apologies for the ugly ifdef in the middle of a statement,
         * but I don't want to write the whole thing twice (and have to
         * deal with the other ugliness of two open brackets but just
         * one close bracket) */
        if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO
#ifndef NDEBUG
                     | SDL_INIT_NOPARACHUTE
#endif
            ) < 0) {
                fprintf(stderr, "%s\n", SDL_GetError());
                exit(1);
        }
        atexit(SDL_Quit);
        if (SDL_GetVideoInfo()->wm_available)
                status.flags |= WM_AVAILABLE;
        display_print_video_info();

        screen = SDL_SetVideoMode(640, getenv("DISPLAY") ? 400 : 768, 8,
                                  (SDL_HWPALETTE | SDL_HWSURFACE |
                                   SDL_DOUBLEBUF | SDL_ANYFORMAT));
        if (!screen) {
                fprintf(stderr, "%s\n", SDL_GetError());
                exit(1);
        }
        SDL_ShowCursor(0);
        display_print_info();

#ifdef USE_EVENT_THREAD
        SDL_EnableKeyRepeat(125, 30);
#else
        SDL_EnableKeyRepeat(125, 0);
#endif

        SDL_WM_SetCaption("Schism Tracker v" VERSION, "Schism Tracker");
        SDL_EnableUNICODE(1);
}

/* --------------------------------------------------------------------- */

static inline void handle_active_event(SDL_ActiveEvent * a)
{
        if (a->state & SDL_APPACTIVE) {
                if (a->gain)
                        status.flags |= IS_VISIBLE;
                else
                        status.flags &= ~IS_VISIBLE;
        }

        if (a->state & SDL_APPINPUTFOCUS) {
                if (a->gain)
                        status.flags |= IS_FOCUSED;
                else
                        status.flags &= ~IS_FOCUSED;
        }
}

/* --------------------------------------------------------------------- */

#ifdef USE_EVENT_THREAD
static int display_thread(void *data) NORETURN;
static int display_thread(void *data)
{
        SDL_mutex *m = data;

        for (;;) {
                SDL_LockMutex(m);

                /* see if stuff changed.
                 * this function is a misnomer, but whatever :P */
                playback_update();

                /* redraw the screen if it's needed */
                if (status.flags & NEED_UPDATE) {
                        redraw_screen();
                        status.flags &= ~NEED_UPDATE;
                }

                SDL_UnlockMutex(m);

                if (status.flags & IS_VISIBLE) {
                        SDL_Delay((status.flags & IS_FOCUSED) ? 30 : 200);
                } else {
                        SDL_WaitEvent(NULL);
                }
        }
}

static int event_thread(void *data) NORETURN;
static int event_thread(void *data)
{
        SDL_mutex *m = data;
        SDL_Event event;

        for (;;) {
                SDL_WaitEvent(&event);

                SDL_LockMutex(m);

                switch (event.type) {
                case SDL_KEYDOWN:
                        handle_key(&(event.key.keysym));
                        status.last_keysym = event.key.keysym.sym;
                        break;
                case SDL_QUIT:
                        show_exit_prompt();
                        break;
                case SDL_ACTIVEEVENT:
                        handle_active_event(&(event.active));
                        break;
                default:
                        break;
                }

                SDL_UnlockMutex(m);
        }
}
#endif /* USE_EVENT_THREAD */

int main(int argc, char **argv) NORETURN;
int main(int argc, char **argv)
{
#ifdef USE_EVENT_THREAD
        SDL_mutex *m;
#else
        SDL_Event event;
#endif

        display_init();
        font_init();
        palette_load_preset(7);
        setup_help_text_pointers();
        load_pages();
        song_initialize(main_song_changed_cb);
        song_stop();
        SDL_PauseAudio(0);
        atexit(clear_all_cached_waveforms);
        /* TODO: getopt */
        if (argc >= 2) {
                set_page(PAGE_LOG);
                song_load(argv[1]);
        } else {
                set_page(PAGE_LOAD_MODULE);
        }

#ifdef USE_EVENT_THREAD
        m = SDL_CreateMutex();
        SDL_CreateThread(display_thread, m);
        event_thread(m);
#else /* ! USE_EVENT_THREAD */
        for (;;) {
                /* handle events */
                while (SDL_PollEvent(&event)) {
                        switch (event.type) {
                        case SDL_KEYDOWN:
                                handle_key(&(event.key.keysym));
                                status.last_keysym = event.key.keysym.sym;
                                break;
                        case SDL_QUIT:
                                show_exit_prompt();
                                break;
                        case SDL_ACTIVEEVENT:
                                handle_active_event(&(event.active));
                                break;
                        default:
                                break;
                        }
                }

                /* see if stuff changed.
                 * this function is a misnomer, but whatever :P */
                playback_update();

                /* redraw the screen if it's needed */
                if (status.flags & NEED_UPDATE) {
                        redraw_screen();
                        status.flags &= ~NEED_UPDATE;
                }

                /* be nice to the cpu */
                if (status.flags & IS_VISIBLE) {
                        SDL_Delay((status.flags & IS_FOCUSED) ? 30 : 200);
                } else {
                        SDL_WaitEvent(NULL);
                }
        }
#endif /* ! USE_EVENT_THREAD */
}
