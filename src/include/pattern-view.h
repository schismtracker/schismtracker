#ifndef _PATTERN_VIEW_H
#define _PATTERN_VIEW_H

/* NOTE: these functions need to be called with the screen LOCKED */

typedef void (*draw_channel_header_func) (int chan, int x, int y, int fg);
typedef void (*draw_note_func) (int x, int y, song_note * note,
                                int cursor_pos, int fg, int bg);

#define PATTERN_VIEW(n) \
        void draw_channel_header_##n(int chan, int x, int y, int fg); \
        void draw_note_##n(int x, int y, song_note * note, \
                           int cursor_pos, int fg, int bg);

PATTERN_VIEW(13);
PATTERN_VIEW(10);
PATTERN_VIEW(7);
PATTERN_VIEW(6);
PATTERN_VIEW(3);
PATTERN_VIEW(2);
PATTERN_VIEW(1);

#undef PATTERN_VIEW

#endif /* ! _PATTERN_VIEW_H */
