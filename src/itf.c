/*
 * ITFedit - an Impulse / Schism Tracker font file editor
 * copyright (c) 2003-2005 chisel <someguy@here.is> <http://here.is/someguy/>
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

/* Don't look. You're better off not knowing the hideous details. :) */

#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

/* FIXME | Including the IT header here is overkill.
 * FIXME | All the stuff that isn't specific to IT really should be moved
 * FIXME | to a separate header. */
#include "it.h"

#include <sys/stat.h>
#include <sys/types.h>
#include <sys/ioctl.h>
#if HAVE_SYS_KD_H
# include <sys/kd.h>
#endif
#if HAVE_LINUX_FB_H
# include <linux/fb.h>
#endif

#include <SDL.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

/* FIXME: do this somewhere else */
#ifndef HAVE_VERSIONSORT
# define versionsort alphasort
#endif

/* --------------------------------------------------------------------- */
/* globals */

SDL_Surface *screen;

/* this is in config.c for schism, but itf is compiled without config.c
 * (for obvious reasons) */
char cfg_font[NAME_MAX + 1] = "font.cfg";

/* d'oh! the only reason this needs to be defined here at all is because the
box drawing uses (status.flags & INVERTED_PALETTE), and it's rather excessive
to define a separate variable just for that one flag. (in schism, that is)
this whole itf editor is a bunch of hacks, anyway, so i'm not going to lose
any sleep over this... */
struct tracker_status status;

/* --------------------------------------------------------------------- */
/* statics & local constants
note: x/y are for the top left corner of the frame, but w/h define the size of its *contents* */

#define EDITBOX_X 0
#define EDITBOX_Y 0
#define EDITBOX_W 9
#define EDITBOX_H 11

#define CHARMAP_X 17
#define CHARMAP_Y 0
#define CHARMAP_W 16
#define CHARMAP_H 16

#define ITFMAP_X 41
#define ITFMAP_Y 0
#define ITFMAP_W 16
#define ITFMAP_H 15

#define FONTLIST_X 65
#define FONTLIST_Y 0
#define VISIBLE_FONTS 22 /* this should be called FONTLIST_H... */

#define HELPTEXT_X 0
#define HELPTEXT_Y 31

/* don't randomly mess with these for obvious reasons */
#define INNER_X(x) ((x) + 3)
#define INNER_Y(y) ((y) + 4)

#define FRAME_RIGHT 3
#define FRAME_BOTTOM 3

#define WITHIN(n,l,u) ((n) >= (l) && (n) < (u))
#define POINT_IN(x,y,item) \
	(WITHIN((x), INNER_X(item##_X), INNER_X(item##_X) + item##_W) \
	&& WITHIN((y), INNER_Y(item##_Y), INNER_Y(item##_Y) + item##_H))
#define POINT_IN_FRAME(x,y,item) \
	(WITHIN((x), item##_X, INNER_X(item##_X) + item##_W + FRAME_RIGHT) \
	&& WITHIN((y), item##_Y, INNER_Y(item##_Y) + item##_H + FRAME_BOTTOM))

/* --------------------------------------------------------------------- */

static const byte itfmap_chars[] = {
	128, 129, 130, ' ', 128, 129, 141, ' ', 142, 143, 144, ' ', 168, 'C', '-', '0',
	131, ' ', 132, ' ', 131, ' ', 132, ' ', 145, ' ', 146, ' ', 168, 'D', '-', '1',
	133, 134, 135, ' ', 140, 134, 135, ' ', 147, 148, 149, ' ', 168, 'E', '-', '2',
	' ', ' ', ' ', ' ', ' ', 139, 134, 138, 153, 148, 152, ' ', 168, 'F', '-', '3',
	174, ' ', ' ', ' ', 155, 132, ' ', 131, 146, ' ', 145, ' ', 168, 'G', '-', '4',
	175, ' ', ' ', ' ', 156, 137, 129, 136, 151, 143, 150, ' ', 168, 'A', '-', '5',
	176, ' ', ' ', ' ', 157, ' ', 184, 184, 191, '6', '4', 192, 168, 'B', '-', '6',
	176, 177, ' ', ' ', 158, 163, 250, 250, 250, 250, 250, ' ', 168, 'C', '#', '7',
	176, 178, ' ', ' ', 159, 164, ' ', ' ', ' ', 185, 186, ' ', 168, 'D', '#', '8',
	176, 179, 180, ' ', 160, 165, ' ', ' ', ' ', 189, 190, ' ', 168, 'E', '#', '9',
	176, 179, 181, ' ', 161, 166, ' ', ' ', ' ', 187, 188, ' ', 168, 'F', '#', '1',
	176, 179, 182, ' ', 162, 167, 126, 126, 126, ' ', ' ', ' ', 168, 'G', '#', '2',
	154, 154, 154, 154, ' ', ' ', 205, 205, 205, ' ', 183, ' ', 168, 'A', '#', '3',
	169, 170, 171, 172, ' ', ' ', '^', '^', '^', ' ', 173, ' ', 168, 'B', '#', '4',
	193, 194, 195, 196, 197, 198, 199, 200, 201, ' ', ' ', ' ', ' ', ' ', ' ', ' ',
};
/* *INDENT-OFF* */

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
	"Ins/Del     Fill/clear horiz.\n"
	"...w/Shift  Fill/clear vert.\n"
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
/* stuff SDL should already be doing but isn't */
/* copied from main.c -- this should probably be moved elsewhere */

#if HAVE_SYS_KD_H
static byte console_font[512 * 32];
static int font_saved = 0;
static void save_font(void)
{
	int t = open("/dev/tty", O_RDONLY);
	if (t < 0)
		return;
	if (ioctl(t, GIO_FONT, &console_font) >= 0)
		font_saved = 1;
	close(t);
}
static void restore_font(void)
{
	int t;
	
	if (!font_saved)
		return;
	t = open("/dev/tty", O_RDONLY);
	if (t < 0)
		return;
	if (ioctl(t, PIO_FONT, &console_font) < 0)
		perror("set font");
	close(t);
}
#else
static void save_font(void)
{
}
static void restore_font(void)
{
}
#endif

static inline int get_fb_size(void)
{
	int r = 400;
#if HAVE_LINUX_FB_H
	struct fb_var_screeninfo s;
	int fb;

	if (getenv("DISPLAY") == NULL && (fb = open("/dev/fb0", O_RDONLY)) > -1) {
		if (ioctl(fb, FBIOGET_VSCREENINFO, &s) < 0)
			perror("ioctl FBIOGET_VSCREENINFO");
		else
			r = s.yres;
		close(fb);
	}
#endif
	return r;
}

/* --------------------------------------------------------------------- */

static void draw_frame(const byte * name, int x, int y, int inner_width, int inner_height, int active)
{
	int n, c;
	int len = strlen(name);

	if (len > inner_width + 2)
		len = inner_width + 2;
	c = (status.flags & INVERTED_PALETTE) ? 1 : 3;

	SDL_LockSurface(screen);

	draw_box_unlocked(x, y + 1, x + inner_width + 5,
			  y + inner_height + 6, BOX_THIN | BOX_CORNER | BOX_OUTSET);
	draw_box_unlocked(x + 1, y + 2, x + inner_width + 4,
			  y + inner_height + 5, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_char_unlocked(128, x, y, c, 2);
	for (n = 0; n < len + 1; n++)
		draw_char_unlocked(129, x + n + 1, y, c, 2);
	draw_char_unlocked(130, x + n, y, c, 2);
	draw_char_unlocked(131, x, y + 1, c, 2);
	draw_char_unlocked(137, x + len + 1, y + 1, c, 2);

	SDL_UnlockSurface(screen);

	switch (active) {
	case 0:			/* inactive */
		n = 0;
		break;
	case -1:			/* disabled */
		n = 1;
		break;
	default:			/* active */
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
				   INNER_Y(EDITBOX_Y) + 2, (i == edit_x ? 3 : 1), 0);
		draw_char_unlocked('1' + i, INNER_X(EDITBOX_X),
				   INNER_Y(EDITBOX_Y) + i + 3, (i == edit_y ? 3 : 1), 0);

		for (j = 0; j < 8; j++) {
			if (font_data[ci + j] & (128 >> i)) {
				c = 15;
				fg = 6;
			} else {
				c = 173;
				fg = 1;
			}
			if (selected_item == EDITBOX && i == edit_x && j == edit_y)
				draw_char_unlocked(c, INNER_X(EDITBOX_X) + 1 + i,
						   INNER_Y(EDITBOX_Y) + 3 + j, 0, 3);
			else
				draw_char_unlocked(c, INNER_X(EDITBOX_X) + 1 + i,
						   INNER_Y(EDITBOX_Y) + 3 + j, fg, 0);
		}
	}
	draw_char_unlocked(current_char, INNER_X(EDITBOX_X), INNER_Y(EDITBOX_Y), 5, 0);
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
			draw_char_unlocked(n, INNER_X(CHARMAP_X) + n % 16, INNER_Y(CHARMAP_Y) + n / 16,
					   (n == current_char ? 0 : 1), (n == current_char ? 3 : 0));
		}
	} else {
		while (n) {
			n--;
			draw_char_unlocked(n, INNER_X(CHARMAP_X) + n % 16, INNER_Y(CHARMAP_Y) + n / 16,
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
				   INNER_X(ITFMAP_X) + n % 16, INNER_Y(ITFMAP_Y) + n / 16, fg, bg);
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
			draw_char_unlocked(183, INNER_X(FONTLIST_X), INNER_Y(FONTLIST_Y) + pos, cfg, cbg);
			while (x < 9 && *ptr && (n == 0 || *ptr != '.')) {
				draw_char_unlocked(*ptr,
						   INNER_X(FONTLIST_X) + x,
						   INNER_Y(FONTLIST_Y) + pos, cfg, cbg);
				x++;
				ptr++;
			}
			while (x < 9) {
				draw_char_unlocked(0,
						   INNER_X(FONTLIST_X) + x,
						   INNER_Y(FONTLIST_Y) + pos, cfg, cbg);
				x++;
			}
		} else {
			draw_char_unlocked(173, INNER_X(FONTLIST_X), INNER_Y(FONTLIST_Y) + pos, 2, 0);
			while (x < 9 && *ptr && (n == 0 || *ptr != '.')) {
				draw_char_unlocked(*ptr,
						   INNER_X(FONTLIST_X) + x, INNER_Y(FONTLIST_Y) + pos, 5, 0);
				x++;
				ptr++;
			}
			while (x < 9) {
				draw_char_unlocked(0, INNER_X(FONTLIST_X) + x, INNER_Y(FONTLIST_Y) + pos, 5, 0);
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
		for (column = INNER_X(HELPTEXT_X); ptr < eol; ptr++, column++)
			draw_char_unlocked(*ptr, column, line, 12, 0);
		ptr++;
	}
	for (line = 0; line < 10; line++)
		draw_char_unlocked(168, INNER_X(HELPTEXT_X) + 43, INNER_Y(HELPTEXT_Y) + line, 12, 0);

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
		draw_char_unlocked(168, INNER_X(HELPTEXT_X) + 43, line, 12, 0);
		for (column = INNER_X(HELPTEXT_X) + 45; ptr < eol; ptr++, column++)
			draw_char_unlocked(*ptr, column, line, 12, 0);
		ptr++;
	}
	draw_text_unlocked("(c) 2003-2005 chisel", 57, 46, 1, 0);

	SDL_UnlockSurface(screen);
}

static inline void draw_time(void)
{
	char buf[16];
	time_t timep = 0;
	struct tm local;

	time(&timep);
	localtime_r(&timep, &local);
	sprintf(buf, "%.2d:%.2d:%.2d", local.tm_hour, local.tm_min, local.tm_sec);
	draw_text(buf, 3, 46, 1, 0);
}

static void draw_screen(void)
{
	draw_fill_rect(NULL, 0);

	draw_frame("Edit Box", EDITBOX_X, EDITBOX_Y, 9, 11, !!(selected_item == EDITBOX));
	draw_editbox();

	draw_frame("Current Font", CHARMAP_X, CHARMAP_Y, 16, 16, !!(selected_item == CHARMAP));
	draw_charmap();

	draw_frame("Preview", ITFMAP_X, ITFMAP_Y, 16, 15, !!(selected_item == ITFMAP));
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
	default:			/* Off? (I sure hope so!) */
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
		if (k->mod & KMOD_SHIFT) {
			for (n = 0; n < 8; n++)
				ptr[n] |= (128 >> edit_x);
		} else {
			ptr[edit_y] = 255;
		}
		break;
	case SDLK_DELETE:
		if (k->mod & KMOD_SHIFT) {
			for (n = 0; n < 8; n++)
				ptr[n] &= ~(128 >> edit_x);
		} else {
			ptr[edit_y] = 0;
		}
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
			/* fontlist_mode = MODE_OFF; */
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
		palette_apply();
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
		palette_apply();
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
				selected_item = (fontlist_mode == MODE_OFF ? 2 : 3);
			else
				selected_item--;
		} else {
			selected_item = (selected_item + 1) % (fontlist_mode == MODE_OFF ? 3 : 4);
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
		if (k->mod & KMOD_CTRL) {
			SDL_ToggleCursor();
			return;
		} else if (k->mod & (KMOD_ALT | KMOD_META)) {
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

static void handle_mouse_editbox(SDL_MouseButtonEvent *m)
{
	int n, ci = current_char << 3, xrel, yrel;
	byte *ptr = font_data + ci;
	
	if (m->button == SDL_BUTTON_WHEELUP) {
		current_char--;
		return;
	} else if (m->button == SDL_BUTTON_WHEELDOWN) {
		current_char++;
		return;
	}
	
	xrel = (m->x >> 3) - INNER_X(EDITBOX_X);
	yrel = (m->y >> 3) - INNER_Y(EDITBOX_Y);
	
	if (xrel > 0 && yrel > 2) {
		edit_x = xrel - 1;
		edit_y = yrel - 3;
		switch (m->button) {
		case SDL_BUTTON_LEFT: /* set */
			ptr[edit_y] |= (128 >> edit_x);
			break;
		case SDL_BUTTON_MIDDLE: /* invert */
			ptr[edit_y] ^= (128 >> edit_x);
			break;
		case SDL_BUTTON_RIGHT: /* clear */
			ptr[edit_y] &= ~(128 >> edit_x);
			break;
		}
	} else if (xrel == 0 && yrel == 2) {
		/* clicking at the origin modifies the entire character */
		switch (m->button) {
		case SDL_BUTTON_LEFT: /* set */
			for (n = 0; n < 8; n++)
				ptr[n] = 255;
			break;
		case SDL_BUTTON_MIDDLE: /* invert */
			for (n = 0; n < 8; n++)
				ptr[n] ^= 255;
			break;
		case SDL_BUTTON_RIGHT: /* clear */
			for (n = 0; n < 8; n++)
				ptr[n] = 0;
			break;
		}
	} else if (xrel == 0 && yrel > 2) {
		edit_y = yrel - 3;
		switch (m->button) {
		case SDL_BUTTON_LEFT: /* set */
			ptr[edit_y] = 255;
			break;
		case SDL_BUTTON_MIDDLE: /* invert */
			ptr[edit_y] ^= 255;
			break;
		case SDL_BUTTON_RIGHT: /* clear */
			ptr[edit_y] = 0;
			break;
		}
	} else if (yrel == 2 && xrel > 0) {
		edit_x = xrel - 1;
		switch (m->button) {
		case SDL_BUTTON_LEFT: /* set */
			for (n = 0; n < 8; n++)
				ptr[n] |= (128 >> edit_x);
			break;
		case SDL_BUTTON_MIDDLE: /* invert */
			for (n = 0; n < 8; n++)
				ptr[n] ^= (128 >> edit_x);
			break;
		case SDL_BUTTON_RIGHT: /* clear */
			for (n = 0; n < 8; n++)
				ptr[n] &= ~(128 >> edit_x);
			break;
		}
	}
}

static void handle_mouse_charmap(SDL_MouseButtonEvent *m)
{
	int xrel = (m->x >> 3) - INNER_X(CHARMAP_X), yrel = (m->y >> 3) - INNER_Y(CHARMAP_Y);
	if (m->button == SDL_BUTTON_LEFT) {
		current_char = 16 * yrel + xrel;
	}
}

static void handle_mouse_itfmap(SDL_MouseButtonEvent *m)
{
	int xrel = (m->x >> 3) - INNER_X(ITFMAP_X), yrel = (m->y >> 3) - INNER_Y(ITFMAP_Y);
	if (m->button == SDL_BUTTON_LEFT) {
		itfmap_pos = 16 * yrel + xrel;
		current_char = itfmap_chars[itfmap_pos];
	}
}

/*
static void handle_mouse_fontlist(SDL_MouseButtonEvent *m)
{
	int xrel = (m->x >> 3) - INNER_X(FONTLIST_X), yrel = (m->y >> 3) - INNER_Y(FONTLIST_Y);
	printf("fontlist + (%d, %d)\n", xrel, yrel);
}
*/

static void handle_mouse(SDL_MouseButtonEvent * m)
{
	int x = m->x >> 3, y = m->y >> 3;
	//printf("handle_mouse (%d,%d) which=%d button=%d\n", x, y, m->which, m->button);
	
	if (POINT_IN_FRAME(x, y, EDITBOX)) {
		selected_item = EDITBOX;
		if (POINT_IN(x, y, EDITBOX))
			handle_mouse_editbox(m);
	} else if (POINT_IN_FRAME(x, y, CHARMAP)) {
		selected_item = CHARMAP;
		if (POINT_IN(x, y, CHARMAP))
			handle_mouse_charmap(m);
	} else if (POINT_IN_FRAME(x, y, ITFMAP)) {
		selected_item = ITFMAP;
		if (POINT_IN(x, y, ITFMAP))
			handle_mouse_itfmap(m);
	} else {
		//printf("stray click\n");
		return;
	}
	need_redraw = 1;
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
	char font_dir[PATH_MAX + 1];
	int n;

	strncpy(font_dir, getenv("HOME") ? : "/", PATH_MAX);
	strncat(font_dir, "/.schism/fonts", PATH_MAX);
	font_dir[PATH_MAX] = 0;

	/* FIXME: some systems don't have scandir */
	n = scandir(font_dir, &names, dirent_select, versionsort);

	if (n < 0) {
		perror(font_dir);
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

	screen = SDL_SetVideoMode(640, get_fb_size(), 8,
				  (SDL_HWPALETTE | SDL_HWSURFACE | SDL_DOUBLEBUF | SDL_ANYFORMAT));
	if (!screen) {
		fprintf(stderr, "%s\n", SDL_GetError());
		exit(1);
	}
	//SDL_ShowCursor(0);
	SDL_EnableKeyRepeat(125, 10);
	SDL_WM_SetCaption("ITFedit", "ITFedit");
}

int main(int argc, char **argv) NORETURN;
int main(UNUSED int argc, UNUSED char **argv)
{
	SDL_Event event;
	Uint32 second = -1, new_second;
	
	save_font();
	atexit(restore_font);
	
	font_init();
	load_fontlist();

	display_init();
	palette_load_preset(7);
	palette_apply();

	for (;;) {
		while (SDL_PollEvent(&event)) {
			switch (event.type) {
			case SDL_KEYDOWN:
				handle_key(&(event.key.keysym));
				break;
			case SDL_MOUSEBUTTONDOWN:
				handle_mouse(&(event.button));
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
