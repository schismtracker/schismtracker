/* Don't look. You're better off not knowing the hideous details. :) */

#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include <SDL.h>
#include <stdarg.h>

/* FIXME | Including the IT header here is overkill.
 * FIXME | All the stuff that isn't specific to IT really should be moved
 * FIXME | to a separate header. */
#include "it.h"

/* --------------------------------------------------------------------- */
/* globals */

SDL_Surface *screen;

/* --------------------------------------------------------------------- */
/* statics & local constants */

#define EDITBOX_X 0
#define EDITBOX_Y 0

#define CHARMAP_X 17
#define CHARMAP_Y 0

#define ITFMAP_X 41
#define ITFMAP_Y 0

#define FONTLIST_X 65
#define FONTLIST_Y 0
#define VISIBLE_FONTS 22

#define HELPTEXT_X 0
#define HELPTEXT_Y 31

/* don't randomly mess with these for obvious reasons */
#define INNER_X(x) ((x) + 3)
#define INNER_Y(y) ((y) + 4)

/* --------------------------------------------------------------------- */

/* *INDENT-OFF* */

static const byte itfmap_chars[240] =
        "\x80\x81\x82 \x80\x81\x8d \x8e\x8f\x90 \xa8\x43-0"
        "\x83 \x84 \x83 \x84 \x91 \x92 \xa8\x44-1"
        "\x85\x86\x87 \x8c\x86\x87 \x93\x94\x95 \xa8\x45-2"
        "     \x8b\x86\x8a\x99\x94\x98 \xa8\x46-3"
        "\xae   \x9b\x84 \x83\x92 \x91 \xa8G-4"
        "\xaf   \x9c\x89\x81\x88\x97\x8f\x96 \xa8\x41-5"
        "\xb0   \x9d \xb8\xb8\xbf\x36\x34\xc0\xa8\x42-6"
        "\xb0\xb1  \x9e\xa3\xfa\xfa\xfa\xfa\xfa \xa8\x43#7"
        "\xb0\xb2  \x9f\xa4   \xb9\xba \xa8\x44#8"
        "\xb0\xb3\xb4 \xa0\xa5   \xbd\xbe \xa8\x45#9"
        "\xb0\xb3\xb5 \xa1\xa6   \xbb\xbc \xa8\x46#1"
        "\xb0\xb3\xb6 \xa2\xa7~~~   \xa8G#2"
        "\x9a\x9a\x9a\x9a  \xcd\xcd\xcd \xb7 \xa8\x41#3"
        "\xa9\xaa\xab\xac  ^^^ \xad \xa8\x42#4"
        "\xc1\xc2\xc3\xc4\xc5\xc6\xc7\xc8\xc9       ";

/* Maybe I should put each column in a separate variable or something. */
static const byte helptext_gen[] =
        "Tab         Next box   \xa8 Alt-C  Copy\n"
        "Shift-Tab   Prev. box  \xa8 Alt-P  Paste\n"
        "F2-F4       Switch box \xa8 Alt-M  Mix paste\n"
        "\x18\x19\x1a\x1b        Dump core  \xa8 Alt-Z  Clear\n"
        "Ctrl-S/F9   Save font  \xa8 Alt-H  Flip horiz\n"
        "Ctrl-R/F10  Load font  \xa8 Alt-V  Flip vert\n"
        "Backspace   Reset font \xa8 Alt-I  Invert\n"
        "Ctrl-Bksp   BIOS font  \xa8\n"
        "Alt-Enter   Fullscreen \xa8 0-9    Palette\n"
        "Ctrl-Q      Exit       \xa8  (+10 with shift)\n";

static const byte helptext_editbox[] =
        "Space       Plot/clear point\n"
        "Ins         Fill horiz. line\n"
        "Del         Clear horiz. line\n"
        "\n"
        "+/-         Next/prev. char.\n"
        "PgUp/PgDn   Next/previous row\n"
        "Home/End    Top/bottom corner\n"
        "\n" "Shift-\x18\x19\x1a\x1b  Shift character\n"
        "[/]         Rotate 90\xf8\n";

static const byte helptext_charmap[] =
        "Home/End    First/last char.\n";

static const byte helptext_fontlist[] =
        "Home/End    First/last font\n"
        "Enter       Load/save file\n"
        "Escape      Hide font list\n"
        "\n\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a"
        "\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\n\n"
        "Remember to save as font.cfg\n"
        "to change the default font!\n";

/* *INDENT-ON* */

/* --------------------------------------------------------------------- */

static int edit_x = 3, edit_y = 3;
static byte current_char = 'A';
static int itfmap_pos = -1;

static enum {
        EDITBOX, CHARMAP, ITFMAP, FONTLIST
} selected_item = EDITBOX;

static enum {
        MODE_OFF, MODE_LOAD, MODE_SAVE
} fontlist_mode = MODE_OFF;

static char **fontlist = NULL;
int num_fonts = 0;
int top_font = 0, cur_font = 0;

static byte clipboard[8] = { 0 };

#define INCR_WRAPPED(n) (((n) & 0xf0) | (((n) + 1) & 0xf))
#define DECR_WRAPPED(n) (((n) & 0xf0) | (((n) - 1) & 0xf))

/* if this is nonzero, the screen will be redrawn. none of the functions
 * except main should call draw_anything -- set this instead. */
int need_redraw = 0;

/* --------------------------------------------------------------------- */
/* hmm... I didn't really think about this when I came up with the idea of
 * using the IT log instead of stdout/stderr for everything :) */

void log_append(UNUSED int color, int must_free, const char *text)
{
        printf("%s\n", text);
        if (must_free) {
                free((void *) text);
        }
}

void log_appendf(int color, const char *format, ...)
{
        char *ptr;
        va_list ap;

        va_start(ap, format);
        vasprintf(&ptr, format, ap);
        va_end(ap);

        log_append(color, 1, ptr);
}

/* --------------------------------------------------------------------- */

static void draw_frame(const byte * name, int x, int y, int inner_width,
                       int inner_height, int active)
{
        int n;
        int len = strlen(name);

        if (len > inner_width + 2)
                len = inner_width + 2;

        SDL_LockSurface(screen);

        draw_box_unlocked(x, y + 1, x + inner_width + 5,
                          y + inner_height + 6,
                          BOX_THIN | BOX_CORNER | BOX_OUTSET);
        draw_box_unlocked(x + 1, y + 2, x + inner_width + 4,
                          y + inner_height + 5,
                          BOX_THIN | BOX_INNER | BOX_INSET);

        draw_char_unlocked(128, x, y, 3, 2);
        for (n = 0; n < len + 1; n++)
                draw_char_unlocked(129, x + n + 1, y, 3, 2);
        draw_char_unlocked(130, x + n, y, 3, 2);
        draw_char_unlocked(131, x, y + 1, 3, 2);
        draw_char_unlocked(137, x + len + 1, y + 1, 3, 2);

        SDL_UnlockSurface(screen);

        switch (active) {
        case 0:        /* inactive */
                n = 0;
                break;
        case -1:       /* disabled */
                n = 1;
                break;
        default:       /* active */
                n = 3;
                break;
        }
        draw_text_len(name, len, x + 1, y + 1, n, 2);
}

/* --------------------------------------------------------------------- */

static inline void draw_editbox(void)
{
        int c;
        char buf[12];
        int ci = current_char << 3, i, j, fg;

        SDL_LockSurface(screen);
        for (i = 0; i < 8; i++) {
                draw_char_unlocked('1' + i, INNER_X(EDITBOX_X) + i + 1,
                                   INNER_Y(EDITBOX_Y) + 2,
                                   (i == edit_x ? 3 : 1), 0);
                draw_char_unlocked('1' + i, INNER_X(EDITBOX_X),
                                   INNER_Y(EDITBOX_Y) + i + 3,
                                   (i == edit_y ? 3 : 1), 0);

                for (j = 0; j < 8; j++) {
                        if (font_data[ci + j] & (128 >> i)) {
                                c = 15;
                                fg = 6;
                        } else {
                                c = 173;
                                fg = 1;
                        }
                        if (selected_item == EDITBOX && i == edit_x
                            && j == edit_y)
                                draw_char_unlocked(c,
                                                   INNER_X(EDITBOX_X) + 1 +
                                                   i,
                                                   INNER_Y(EDITBOX_Y) + 3 +
                                                   j, 0, 3);
                        else
                                draw_char_unlocked(c,
                                                   INNER_X(EDITBOX_X) + 1 +
                                                   i,
                                                   INNER_Y(EDITBOX_Y) + 3 +
                                                   j, fg, 0);
                }
        }
        draw_char_unlocked(current_char, INNER_X(EDITBOX_X),
                           INNER_Y(EDITBOX_Y), 5, 0);
        SDL_UnlockSurface(screen);

        sprintf(buf, "%3d $%02X", current_char, current_char);
        draw_text(buf, INNER_X(EDITBOX_X) + 2, INNER_Y(EDITBOX_Y), 5, 0);
}

static inline void draw_charmap(void)
{
        int n = 256;

        SDL_LockSurface(screen);
        if (selected_item == CHARMAP) {
                while (n) {
                        n--;
                        draw_char_unlocked(n, INNER_X(CHARMAP_X) + n % 16,
                                           INNER_Y(CHARMAP_Y) + n / 16,
                                           (n == current_char ? 0 : 1),
                                           (n == current_char ? 3 : 0));
                }
        } else {
                while (n) {
                        n--;
                        draw_char_unlocked(n, INNER_X(CHARMAP_X) + n % 16,
                                           INNER_Y(CHARMAP_Y) + n / 16,
                                           (n == current_char ? 3 : 1), 0);
                }
        }
        SDL_UnlockSurface(screen);
}

static inline void draw_itfmap(void)
{
        int n, fg, bg;
        byte *ptr;

        if (itfmap_pos < 0 || itfmap_chars[itfmap_pos] != current_char) {
                ptr = strchr(itfmap_chars, current_char);
                if (ptr == NULL)
                        itfmap_pos = -1;
                else
                        itfmap_pos = ptr - itfmap_chars;
        }

        SDL_LockSurface(screen);
        for (n = 0; n < 240; n++) {
                fg = 1;
                bg = 0;
                if (n == itfmap_pos) {
                        if (selected_item == ITFMAP) {
                                fg = 0;
                                bg = 3;
                        } else {
                                fg = 3;
                        }
                }
                draw_char_unlocked(itfmap_chars[n],
                                   INNER_X(ITFMAP_X) + n % 16,
                                   INNER_Y(ITFMAP_Y) + n / 16, fg, bg);
        }
        SDL_UnlockSurface(screen);
}

static inline void draw_fontlist(void)
{
        int x, pos = 0, n = top_font, cfg, cbg;
        char *ptr;

        if (selected_item == FONTLIST) {
                cfg = 0;
                cbg = 3;
        } else {
                cfg = 3;
                cbg = 0;
        }

        SDL_LockSurface(screen);
        while (n < num_fonts && pos < VISIBLE_FONTS) {
                x = 1;
                ptr = fontlist[n];
                if (n == cur_font) {
                        draw_char_unlocked(183, INNER_X(FONTLIST_X),
                                           INNER_Y(FONTLIST_Y) + pos, cfg,
                                           cbg);
                        while (x < 9 && *ptr && (n == 0 || *ptr != '.')) {
                                draw_char_unlocked(*ptr,
                                                   INNER_X(FONTLIST_X) + x,
                                                   INNER_Y(FONTLIST_Y) +
                                                   pos, cfg, cbg);
                                x++;
                                ptr++;
                        }
                        while (x < 9) {
                                draw_char_unlocked(0,
                                                   INNER_X(FONTLIST_X) + x,
                                                   INNER_Y(FONTLIST_Y) +
                                                   pos, cfg, cbg);
                                x++;
                        }
                } else {
                        draw_char_unlocked(173, INNER_X(FONTLIST_X),
                                           INNER_Y(FONTLIST_Y) + pos, 2,
                                           0);
                        while (x < 9 && *ptr && (n == 0 || *ptr != '.')) {
                                draw_char_unlocked(*ptr,
                                                   INNER_X(FONTLIST_X) + x,
                                                   INNER_Y(FONTLIST_Y) +
                                                   pos, 5, 0);
                                x++;
                                ptr++;
                        }
                        while (x < 9) {
                                draw_char_unlocked(0,
                                                   INNER_X(FONTLIST_X) + x,
                                                   INNER_Y(FONTLIST_Y) +
                                                   pos, 5, 0);
                                x++;
                        }
                }
                n++;
                pos++;
        }
        SDL_UnlockSurface(screen);
}

static inline void draw_helptext(void)
{
        const byte *ptr = helptext_gen;
        const byte *eol;
        int line;
        int column;

        SDL_LockSurface(screen);

        for (line = INNER_Y(HELPTEXT_Y); *ptr; line++) {
                eol = strchr(ptr, '\n');
                if (!eol)
                        eol = strchr(ptr, '\0');
                for (column = INNER_X(HELPTEXT_X); ptr < eol;
                     ptr++, column++)
                        draw_char_unlocked(*ptr, column, line, 12, 0);
                ptr++;
        }
        for (line = 0; line < 10; line++)
                draw_char_unlocked(168, INNER_X(HELPTEXT_X) + 43,
                                   INNER_Y(HELPTEXT_Y) + line, 12, 0);

        /* context sensitive stuff... oooh :) */
        switch (selected_item) {
        case EDITBOX:
                ptr = helptext_editbox;
                break;
        case CHARMAP:
        case ITFMAP:
                ptr = helptext_charmap;
                break;
        case FONTLIST:
                ptr = helptext_fontlist;
                break;
        }
        for (line = INNER_Y(HELPTEXT_Y); *ptr; line++) {
                eol = strchr(ptr, '\n');
                if (!eol)
                        eol = strchr(ptr, '\0');
                draw_char_unlocked(168, INNER_X(HELPTEXT_X) + 43, line, 12,
                                   0);
                for (column = INNER_X(HELPTEXT_X) + 45; ptr < eol;
                     ptr++, column++)
                        draw_char_unlocked(*ptr, column, line, 12, 0);
                ptr++;
        }
        draw_text_unlocked("(c) 2003 chisel", 62, 46, 1, 0);

        SDL_UnlockSurface(screen);
}

static inline void draw_time(void)
{
        char buf[16];
        time_t timep = 0;
        struct tm local;

        time(&timep);
        localtime_r(&timep, &local);
        sprintf(buf, "%.2d:%.2d:%.2d", local.tm_hour, local.tm_min,
                local.tm_sec);
        draw_text(buf, 3, 46, 1, 0);
}

static void draw_screen(void)
{
        draw_fill_rect(NULL, 0);

        draw_frame("Edit Box", EDITBOX_X, EDITBOX_Y, 9, 11,
                   !!(selected_item == EDITBOX));
        draw_editbox();

        draw_frame("Current Font", CHARMAP_X, CHARMAP_Y, 16, 16,
                   !!(selected_item == CHARMAP));
        draw_charmap();

        draw_frame("Preview", ITFMAP_X, ITFMAP_Y, 16, 15,
                   !!(selected_item == ITFMAP));
        draw_itfmap();

        switch (fontlist_mode) {
        case MODE_LOAD:
                draw_frame("Load/Browse", FONTLIST_X, FONTLIST_Y, 9,
                           VISIBLE_FONTS, !!(selected_item == FONTLIST));
                draw_fontlist();
                break;
        case MODE_SAVE:
                draw_frame("Save As...", FONTLIST_X, FONTLIST_Y, 9,
                           VISIBLE_FONTS, !!(selected_item == FONTLIST));
                draw_fontlist();
                break;
        default:       /* Off? (I sure hope so!) */
                break;
        }

        draw_frame("Quick Help", HELPTEXT_X, HELPTEXT_Y, 74, 12, -1);
        draw_helptext();

        draw_time();

        SDL_Flip(screen);
}

/* --------------------------------------------------------------------- */

static void handle_key_editbox(SDL_keysym * k)
{
        byte tmp[8] = { 0 };
        int ci = current_char << 3;
        int n, bit;
        byte *ptr = font_data + ci;

        switch (k->sym) {
        case SDLK_UP:
                if (k->mod & KMOD_SHIFT) {
                        int s = ptr[0];
                        for (n = 0; n < 7; n++)
                                ptr[n] = ptr[n + 1];
                        ptr[7] = s;
                } else {
                        if (--edit_y < 0)
                                edit_y = 7;
                }
                break;
        case SDLK_DOWN:
                if (k->mod & KMOD_SHIFT) {
                        int s = ptr[7];
                        for (n = 7; n; n--)
                                ptr[n] = ptr[n - 1];
                        ptr[0] = s;
                } else {
                        edit_y = (edit_y + 1) % 8;
                }
                break;
        case SDLK_LEFT:
                if (k->mod & KMOD_SHIFT) {
#if defined(__i386__) && defined(__GNUC__) && __GNUC__ >= 2
                        /* *INDENT-OFF* */
                        asm volatile("rolb  (%0); rolb 1(%0);"
                                     "rolb 2(%0); rolb 3(%0);"
                                     "rolb 4(%0); rolb 5(%0);"
                                     "rolb 6(%0); rolb 7(%0)"
                                     : "=r" (ptr) : "r" (ptr));
                        /* *INDENT-ON* */
#else
                        for (n = 0; n < 8; n++, ptr++)
                                *ptr = (*ptr >> 7) | (*ptr << 1);
#endif
                } else {
                        if (--edit_x < 0)
                                edit_x = 7;
                }
                break;
        case SDLK_RIGHT:
                if (k->mod & KMOD_SHIFT) {
#if defined(__i386__) && defined(__GNUC__) && __GNUC__ >= 2
                        /* *INDENT-OFF* */
                        asm volatile("rorb  (%0); rorb 1(%0);"
                                     "rorb 2(%0); rorb 3(%0);"
                                     "rorb 4(%0); rorb 5(%0);"
                                     "rorb 6(%0); rorb 7(%0)"
                                     : "=r" (ptr) : "r" (ptr));
                        /* *INDENT-ON* */
#else
                        for (n = 0; n < 8; n++, ptr++)
                                *ptr = (*ptr << 7) | (*ptr >> 1);
#endif
                } else {
                        edit_x = (edit_x + 1) % 8;
                }
                break;
        case SDLK_HOME:
                edit_x = edit_y = 0;
                break;
        case SDLK_END:
                edit_x = edit_y = 7;
                break;
        case SDLK_SPACE:
                ptr[edit_y] ^= (128 >> edit_x);
                break;
        case SDLK_INSERT:
                ptr[edit_y] = 255;
                break;
        case SDLK_DELETE:
                ptr[edit_y] = 0;
                break;
        case SDLK_LEFTBRACKET:
                for (n = 0; n < 8; n++)
                        for (bit = 0; bit < 8; bit++)
                                if (ptr[n] & (1 << bit))
                                        tmp[bit] |= 1 << (7 - n);
                memcpy(ptr, tmp, 8);
                break;
        case SDLK_RIGHTBRACKET:
                for (n = 0; n < 8; n++)
                        for (bit = 0; bit < 8; bit++)
                                if (ptr[n] & (1 << bit))
                                        tmp[7 - bit] |= 1 << n;
                memcpy(ptr, tmp, 8);
                break;
        case SDLK_PLUS:
        case SDLK_EQUALS:
        case SDLK_KP_PLUS:
                current_char++;
                break;
        case SDLK_MINUS:
        case SDLK_UNDERSCORE:
        case SDLK_KP_MINUS:
                current_char--;
                break;
        case SDLK_PAGEUP:
                current_char -= 16;
                break;
        case SDLK_PAGEDOWN:
                current_char += 16;
                break;
        default:
                return;
        }

        need_redraw = 1;
}

static void handle_key_charmap(SDL_keysym * k)
{
        switch (k->sym) {
        case SDLK_UP:
                current_char -= 16;
                break;
        case SDLK_DOWN:
                current_char += 16;
                break;
        case SDLK_LEFT:
                current_char = DECR_WRAPPED(current_char);
                break;
        case SDLK_RIGHT:
                current_char = INCR_WRAPPED(current_char);
                break;
        case SDLK_HOME:
                current_char = 0;
                break;
        case SDLK_END:
                current_char = 255;
                break;
        default:
                return;
        }
        need_redraw = 1;
}

static void handle_key_itfmap(SDL_keysym * k)
{
        switch (k->sym) {
        case SDLK_UP:
                if (itfmap_pos < 0) {
                        itfmap_pos = 224;
                } else {
                        itfmap_pos -= 16;
                        if (itfmap_pos < 0)
                                itfmap_pos += 240;
                }
                current_char = itfmap_chars[itfmap_pos];
                break;
        case SDLK_DOWN:
                if (itfmap_pos < 0)
                        itfmap_pos = 16;
                else
                        itfmap_pos = (itfmap_pos + 16) % 240;
                current_char = itfmap_chars[itfmap_pos];
                break;
        case SDLK_LEFT:
                if (itfmap_pos < 0)
                        itfmap_pos = 15;
                else
                        itfmap_pos = DECR_WRAPPED(itfmap_pos);
                current_char = itfmap_chars[itfmap_pos];
                break;
        case SDLK_RIGHT:
                if (itfmap_pos < 0)
                        itfmap_pos = 0;
                else
                        itfmap_pos = INCR_WRAPPED(itfmap_pos);
                current_char = itfmap_chars[itfmap_pos];
                break;
        case SDLK_HOME:
                current_char = itfmap_chars[0];
                itfmap_pos = 0;
                break;
        case SDLK_END:
                current_char = itfmap_chars[239];
                itfmap_pos = 239;
                break;
        default:
                return;
        }
        need_redraw = 1;
}

static void fontlist_reposition(void)
{
        if (cur_font < top_font)
                top_font = cur_font;
        else if (cur_font > top_font + (VISIBLE_FONTS - 1))
                top_font = cur_font - (VISIBLE_FONTS - 1);
}

static void handle_key_fontlist(SDL_keysym * k)
{
        int new_font = cur_font;

        switch (k->sym) {
        case SDLK_HOME:
                new_font = 0;
                break;
        case SDLK_END:
                new_font = num_fonts - 1;
                break;
        case SDLK_UP:
                new_font--;
                break;
        case SDLK_DOWN:
                new_font++;
                break;
        case SDLK_PAGEUP:
                new_font -= VISIBLE_FONTS;
                break;
        case SDLK_PAGEDOWN:
                new_font += VISIBLE_FONTS;
                break;
        case SDLK_ESCAPE:
                selected_item = EDITBOX;
                fontlist_mode = MODE_OFF;
                break;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                switch (fontlist_mode) {
                case MODE_LOAD:
                        if (font_load(fontlist[cur_font]) != 0) {
                                fprintf(stderr, "%s\n", SDL_GetError());
                                font_reset();
                        }
                        break;
                case MODE_SAVE:
                        /* TODO: if cur_font != 0 (which is font.cfg),
                         * ask before overwriting it */
                        if (font_save(fontlist[cur_font]) != 0) {
                                fprintf(stderr, "%s\n", SDL_GetError());
                                return;
                        }
                        selected_item = EDITBOX;
                        //fontlist_mode = MODE_OFF;
                        break;
                default:
                        /* should never happen */
                        return;
                }
                break;
        default:
                return;
        }

        if (new_font != cur_font) {
                new_font = CLAMP(new_font, 0, num_fonts - 1);
                if (new_font == cur_font)
                        return;
                cur_font = new_font;
                fontlist_reposition();
        }
        need_redraw = 1;
}

void handle_key(SDL_keysym * k)
{
        int n, ci = current_char << 3;
        byte *ptr = font_data + ci;

        switch (k->sym) {
        case SDLK_KP0:
                k->sym += 10;
                /* fall through */
        case SDLK_KP1...SDLK_KP9:
                n = k->sym - SDLK_KP1;
                if (k->mod & KMOD_SHIFT)
                        n += 10;
                palette_load_preset(n);
                need_redraw = 1;
                return;
        case '0':
                k->sym += 10;
                /* fall through */
        case '1'...'9':
                n = k->sym - '1';
                if (k->mod & KMOD_SHIFT)
                        n += 10;
                palette_load_preset(n);
                need_redraw = 1;
                return;
        case SDLK_F2:
                selected_item = EDITBOX;
                need_redraw = 1;
                return;
        case SDLK_F3:
                selected_item = CHARMAP;
                need_redraw = 1;
                return;
        case SDLK_F4:
                selected_item = ITFMAP;
                need_redraw = 1;
                return;
        case SDLK_TAB:
                if (k->mod & KMOD_SHIFT) {
                        if (selected_item == 0)
                                selected_item =
                                        (fontlist_mode ==
                                         MODE_OFF ? 2 : 3);
                        else
                                selected_item--;
                } else {
                        selected_item =
                                (selected_item + 1) % (fontlist_mode ==
                                                       MODE_OFF ? 3 : 4);
                }
                need_redraw = 1;
                return;
        case SDLK_c:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        memcpy(clipboard, ptr, 8);
                        return;
                }
                break;
        case SDLK_p:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        memcpy(ptr, clipboard, 8);
                        need_redraw = 1;
                        return;
                }
                break;
        case SDLK_m:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        for (n = 0; n < 8; n++)
                                ptr[n] |= clipboard[n];
                        need_redraw = 1;
                        return;
                }
                break;
        case SDLK_z:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        memset(ptr, 0, 8);
                        need_redraw = 1;
                        return;
                }
                break;
        case SDLK_h:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        for (n = 0; n < 8; n++) {
                                int r = ptr[n];
                                r = ((r >> 1) & 0x55) | ((r << 1) & 0xaa);
                                r = ((r >> 2) & 0x33) | ((r << 2) & 0xcc);
                                r = ((r >> 4) & 0x0f) | ((r << 4) & 0xf0);
                                ptr[n] = r;
                        }
                        need_redraw = 1;
                        return;
                }
                break;
        case SDLK_v:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        for (n = 0; n < 4; n++) {
                                byte r = ptr[n];
                                ptr[n] = ptr[7 - n];
                                ptr[7 - n] = r;
                        }
                        need_redraw = 1;
                        return;
                }
                break;
        case SDLK_i:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        for (n = 0; n < 8; n++)
                                font_data[ci + n] ^= 255;
                        need_redraw = 1;
                        return;
                }
                break;

                /* ----------------------------------------------------- */

        case SDLK_r:
                /* what a nifty hack :) */
                if (k->mod & KMOD_CTRL) {
        case SDLK_F9:
                        fontlist_mode = MODE_LOAD;
                        selected_item = FONTLIST;
                        need_redraw = 1;
                        return;
                }
                break;
        case SDLK_s:
                if (k->mod & KMOD_CTRL) {
        case SDLK_F10:
                        /* a bit weird, but this ensures that font.cfg
                         * is always the default font to save to, but
                         * without the annoyance of moving the cursor
                         * back to it every time f10 is pressed. */
                        if (fontlist_mode != MODE_SAVE) {
                                cur_font = top_font = 0;
                                fontlist_mode = MODE_SAVE;
                        }
                        selected_item = FONTLIST;
                        need_redraw = 1;
                        return;
                }
                break;
        case SDLK_BACKSPACE:
                if (k->mod & KMOD_CTRL)
                        font_reset_bios();
                else
                        font_reset_upper();
                need_redraw = 1;
                return;
        case SDLK_RETURN:
        case SDLK_KP_ENTER:
                if (k->mod & (KMOD_ALT | KMOD_META)) {
                        SDL_WM_ToggleFullScreen(screen);
                        return;
                }
                break;
        case SDLK_q:
                if (k->mod & KMOD_CTRL)
                        exit(0);
                break;
        default:
                break;
        }

        switch (selected_item) {
        case EDITBOX:
                handle_key_editbox(k);
                break;
        case CHARMAP:
                handle_key_charmap(k);
                break;
        case ITFMAP:
                handle_key_itfmap(k);
                break;
        case FONTLIST:
                handle_key_fontlist(k);
                break;
        default:
                break;
        }
}

/* --------------------------------------------------------------------- */

static int dirent_select(const struct dirent *ent)
{
        char *ptr;

        if (ent->d_name[0] == '.')
                return 0;
        ptr = strrchr(ent->d_name, '.');
        if (ptr == NULL)
                return 0;
        if (strcasecmp(ptr, ".itf") == 0) {
                return 1;
        }
        return 0;
}

static void load_fontlist(void)
{
        struct dirent **names;

        /* FIXME: some systems don't have scandir */
        int n = scandir(".", &names, dirent_select, versionsort);

        if (n < 0) {
                perror("scandir");
                names = NULL;
                n = 0;
        }
        num_fonts = n + 1;
        fontlist = calloc(n + 2, sizeof(char *));
        while (n) {
                n--;
                fontlist[n + 1] = strdup(names[n]->d_name);
                free(names[n]);
        }
        free(names);
        fontlist[0] = strdup("font.cfg");
}

/* --------------------------------------------------------------------- */

static inline void display_init(void)
{
        if (SDL_Init(SDL_INIT_VIDEO) < 0) {
                fprintf(stderr, "%s\n", SDL_GetError());
                exit(1);
        }
        atexit(SDL_Quit);

        screen = SDL_SetVideoMode(640, getenv("DISPLAY") ? 400 : 768, 8,
                                  (SDL_HWPALETTE | SDL_HWSURFACE |
                                   SDL_DOUBLEBUF | SDL_ANYFORMAT));
        if (!screen) {
                fprintf(stderr, "%s\n", SDL_GetError());
                exit(1);
        }
        SDL_ShowCursor(0);
        SDL_EnableKeyRepeat(125, 10);
        SDL_WM_SetCaption("ITFedit", "ITFedit");
}

int main(int argc, char **argv) NORETURN;
int main(UNUSED int argc, UNUSED char **argv)
{
        SDL_Event event;
        Uint32 second = -1, new_second;
        char buf[PATH_MAX];

        /* try to find the font directory */
        if (chdir("../extra/fonts") == 0 || chdir("extra/fonts") == 0) {
                printf("found font dir: %s\n", getcwd(buf, sizeof(buf)));
        }

        font_init();
        load_fontlist();

        display_init();
        palette_load_preset(7);

        for (;;) {
                while (SDL_PollEvent(&event)) {
                        switch (event.type) {
                        case SDL_KEYDOWN:
                                handle_key(&(event.key.keysym));
                                break;
                        case SDL_QUIT:
                                exit(0);
                        }
                }

                /* fix this */
                new_second = SDL_GetTicks() / 1000;
                if (new_second != second) {
                        second = new_second;
                        need_redraw = 1;
                }

                if (need_redraw) {
                        draw_screen();
                        need_redraw = 0;
                }

                SDL_Delay(20);
        }
}
