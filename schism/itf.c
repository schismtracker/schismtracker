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

/* this is a little ad-hoc; i did some work trying to bring it back into CVS
   LARGELY because I can't remember all the font characters. :)
*/
#include "headers.h"
#include "it.h"
#include "dmoz.h"
#include "page.h"
#include "version.h"
#include "log.h"

#include "sdlmain.h"
#include <string.h>

static const uint8_t itfmap_chars[] = {
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
static const uint8_t helptext_gen[] =
	"Tab         Next box   \xa8 Alt-C  Copy\n"
	"Shift-Tab   Prev. box  \xa8 Alt-P  Paste\n"
	"F2-F4       Switch box \xa8 Alt-M  Mix paste\n"
	"\x18\x19\x1a\x1b        Dump core  \xa8 Alt-Z  Clear\n"
	"Ctrl-S/F10  Save font  \xa8 Alt-H  Flip horiz\n"
	"Ctrl-R/F9   Load font  \xa8 Alt-V  Flip vert\n"
	"Backspace   Reset font \xa8 Alt-I  Invert\n"
	"Ctrl-Bksp   BIOS font  \xa8 Alt-Bk Reset text\n"
	"                       \xa8 0-9    Palette\n"
	"Ctrl-Q      Exit       \xa8  (+10 with shift)\n";

static const uint8_t helptext_editbox[] =
"Space       Plot/clear point\n"
"Ins/Del     Fill/clear horiz.\n"
"...w/Shift  Fill/clear vert.\n"
"\n"
"+/-         Next/prev. char.\n"
"PgUp/PgDn   Next/previous row\n"
"Home/End    Top/bottom corner\n"
"\n" "Shift-\x18\x19\x1a\x1b  Shift character\n"
"[/]         Rotate 90\xf8\n";

static const uint8_t helptext_charmap[] =
"Home/End    First/last char.\n";

static const uint8_t helptext_fontlist[] =
"Home/End    First/last font\n"
"Enter       Load/save file\n"
"Escape      Hide font list\n"
"\n\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a"
"\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\n\n"
"Remember to save as font.cfg\n"
"to change the default font!\n";

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

static int edit_x = 3, edit_y = 3;
static uint8_t current_char = 'A';
static int itfmap_pos = -1;

static enum {
	EDITBOX, CHARMAP, ITFMAP, FONTLIST
} selected_item = EDITBOX;

static enum {
	MODE_OFF, MODE_LOAD, MODE_SAVE
} fontlist_mode = MODE_OFF;

static dmoz_filelist_t flist;
static int top_font = 0, cur_font = 0;


static void fontlist_reposition(void)
{
	if (cur_font < 0)
		cur_font = 0; /* weird! */
	if (cur_font < top_font)
		top_font = cur_font;
	else if (cur_font > top_font + (VISIBLE_FONTS - 1))
		top_font = cur_font - (VISIBLE_FONTS - 1);
}

static int fontgrep(dmoz_file_t *f)
{
	const char *ext;

	if (f->sort_order == -100)
		return 1; /* this is our font.cfg, at the top of the list */
	if (f->type & TYPE_BROWSABLE_MASK)
		return 0; /* we don't care about directories and stuff */
	ext = get_extension(f->base);
	return (strcasecmp(ext, ".itf") == 0 || strcasecmp(ext, ".fnt") == 0);
}

static void load_fontlist(void)
{
	char *font_dir, *p;
	struct stat st = {};

	dmoz_free(&flist, NULL);

	top_font = cur_font = 0;

	font_dir = dmoz_path_concat_len(cfg_dir_dotschism, "fonts", strlen(cfg_dir_dotschism), 5);
	mkdir(font_dir, 0755);
	p = dmoz_path_concat_len(font_dir, "font.cfg", strlen(font_dir), 8);
	dmoz_add_file(&flist, p, str_dup("font.cfg"), &st, -100); /* put it on top */
	if (dmoz_read(font_dir, &flist, NULL, NULL) < 0)
		log_perror(font_dir);
	free(font_dir);
	dmoz_filter_filelist(&flist, fontgrep, &cur_font, NULL);
	while (dmoz_worker());
	fontlist_reposition();

	/* p is freed by dmoz_free */
}



static uint8_t clipboard[8] = { 0 };

#define INCR_WRAPPED(n) (((n) & 0xf0) | (((n) + 1) & 0xf))
#define DECR_WRAPPED(n) (((n) & 0xf0) | (((n) - 1) & 0xf))

/* if this is nonzero, the screen will be redrawn. none of the functions
 * except main should call draw_anything -- set this instead. */
static void draw_frame(const char* name, int x, int y, int inner_width, int inner_height, int active)
{
	int n, c;
	int len = strlen(name);

	if (len > inner_width + 2)
		len = inner_width + 2;
	c = (status.flags & INVERTED_PALETTE) ? 1 : 3;

	draw_box(x, y + 1, x + inner_width + 5,
			  y + inner_height + 6, BOX_THIN | BOX_CORNER | BOX_OUTSET);
	draw_box(x + 1, y + 2, x + inner_width + 4,
			  y + inner_height + 5, BOX_THIN | BOX_INNER | BOX_INSET);

	draw_char(128, x, y, c, 2);
	for (n = 0; n < len + 1; n++)
		draw_char(129, x + n + 1, y, c, 2);
	draw_char(130, x + n, y, c, 2);
	draw_char(131, x, y + 1, c, 2);
	draw_char(137, x + len + 1, y + 1, c, 2);

	switch (active) {
	case 0:                 /* inactive */
		n = 0;
		break;
	case -1:                        /* disabled */
		n = 1;
		break;
	default:                        /* active */
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

	for (i = 0; i < 8; i++) {
		draw_char('1' + i, INNER_X(EDITBOX_X) + i + 1,
				   INNER_Y(EDITBOX_Y) + 2, (i == edit_x ? 3 : 1), 0);
		draw_char('1' + i, INNER_X(EDITBOX_X),
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
				draw_char(c, INNER_X(EDITBOX_X) + 1 + i,
						   INNER_Y(EDITBOX_Y) + 3 + j, 0, 3);
			else
				draw_char(c, INNER_X(EDITBOX_X) + 1 + i,
						   INNER_Y(EDITBOX_Y) + 3 + j, fg, 0);
		}
	}
	draw_char(current_char, INNER_X(EDITBOX_X), INNER_Y(EDITBOX_Y), 5, 0);

	sprintf(buf, "%3d $%02X", current_char, current_char);
	draw_text(buf, INNER_X(EDITBOX_X) + 2, INNER_Y(EDITBOX_Y), 5, 0);
}

static inline void draw_charmap(void)
{
	int n = 256;

	if (selected_item == CHARMAP) {
		while (n) {
			n--;
			draw_char(n, INNER_X(CHARMAP_X) + n % 16, INNER_Y(CHARMAP_Y) + n / 16,
					   (n == current_char ? 0 : 1), (n == current_char ? 3 : 0));
		}
	} else {
		while (n) {
			n--;
			draw_char(n, INNER_X(CHARMAP_X) + n % 16, INNER_Y(CHARMAP_Y) + n / 16,
					   (n == current_char ? 3 : 1), 0);
		}
	}
}

static inline void draw_itfmap(void)
{
	int n, fg, bg;
	uint8_t *ptr;

	if (itfmap_pos < 0 || itfmap_chars[itfmap_pos] != current_char) {
		ptr = (unsigned char *) strchr((char *) itfmap_chars, current_char);
		if (ptr == NULL)
			itfmap_pos = -1;
		else
			itfmap_pos = ptr - itfmap_chars;
	}

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
		draw_char(itfmap_chars[n],
				   INNER_X(ITFMAP_X) + n % 16, INNER_Y(ITFMAP_Y) + n / 16, fg, bg);
	}
}

static inline void draw_fontlist(void)
{
	int x, pos = 0, n = top_font, cfg, cbg;
	dmoz_file_t *f;
	char *ptr;

	if (selected_item == FONTLIST) {
		cfg = 0;
		cbg = 3;
	} else {
		cfg = 3;
		cbg = 0;
	}

	if (top_font < 0) top_font = 0;
	if (n < 0) n = 0;

	while (n < flist.num_files && pos < VISIBLE_FONTS) {
		x = 1;
		f = flist.files[n];
		if (!f) break;
		ptr = f->base;
		if (n == cur_font) {
			draw_char(183, INNER_X(FONTLIST_X), INNER_Y(FONTLIST_Y) + pos, cfg, cbg);
			while (x < 9 && *ptr && (n == 0 || *ptr != '.')) {
				draw_char(*ptr,
						   INNER_X(FONTLIST_X) + x,
						   INNER_Y(FONTLIST_Y) + pos, cfg, cbg);
				x++;
				ptr++;
			}
			while (x < 9) {
				draw_char(0,
						   INNER_X(FONTLIST_X) + x,
						   INNER_Y(FONTLIST_Y) + pos, cfg, cbg);
				x++;
			}
		} else {
			draw_char(173, INNER_X(FONTLIST_X), INNER_Y(FONTLIST_Y) + pos, 2, 0);
			while (x < 9 && *ptr && (n == 0 || *ptr != '.')) {
				draw_char(*ptr,
						   INNER_X(FONTLIST_X) + x, INNER_Y(FONTLIST_Y) + pos, 5, 0);
				x++;
				ptr++;
			}
			while (x < 9) {
				draw_char(0, INNER_X(FONTLIST_X) + x, INNER_Y(FONTLIST_Y) + pos, 5, 0);
				x++;
			}
		}
		n++;
		pos++;
	}
}

static inline void draw_helptext(void)
{
	const uint8_t *ptr = helptext_gen;
	const uint8_t *eol;
	int line;
	int column;

	for (line = INNER_Y(HELPTEXT_Y); *ptr; line++) {
		eol = (unsigned char *) strchr((char *) ptr, '\n');
		if (!eol)
			eol = (unsigned char *) strchr((char *) ptr, '\0');
		for (column = INNER_X(HELPTEXT_X); ptr < eol; ptr++, column++)
			draw_char(*ptr, column, line, 12, 0);
		ptr++;
	}
	for (line = 0; line < 10; line++)
		draw_char(168, INNER_X(HELPTEXT_X) + 43, INNER_Y(HELPTEXT_Y) + line, 12, 0);

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
		eol = (unsigned char *) strchr((char *) ptr, '\n');
		if (!eol)
			eol = (unsigned char *) strchr((char *) ptr, '\0');
		draw_char(168, INNER_X(HELPTEXT_X) + 43, line, 12, 0);
		for (column = INNER_X(HELPTEXT_X) + 45; ptr < eol; ptr++, column++)
			draw_char(*ptr, column, line, 12, 0);
		ptr++;
	}
	draw_text(ver_short_copyright, 77 - strlen(ver_short_copyright), 46, 1, 0);
}

static inline void draw_time(void)
{
	char buf[16];
	sprintf(buf, "%.2d:%.2d:%.2d", status.tmnow.tm_hour, status.tmnow.tm_min, status.tmnow.tm_sec);
	draw_text(buf, 3, 46, 1, 0);
}

extern unsigned int color_set[16];

static void draw_screen(void)
{
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
	default:                        /* Off? (I sure hope so!) */
		break;
	}

	draw_frame("Quick Help", HELPTEXT_X, HELPTEXT_Y, 74, 12, -1);
	draw_helptext();

	draw_time();
}
static void handle_key_editbox(struct key_event * k)
{
	uint8_t tmp[8] = { 0 };
	int ci = current_char << 3;
	int n, bit;
	uint8_t *ptr = font_data + ci;

	switch (k->sym.sym) {
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
			for (n = 0; n < 8; n++, ptr++)
				*ptr = (*ptr >> 7) | (*ptr << 1);
		} else {
			if (--edit_x < 0)
				edit_x = 7;
		}
		break;
	case SDLK_RIGHT:
		if (k->mod & KMOD_SHIFT) {
			for (n = 0; n < 8; n++, ptr++)
				*ptr = (*ptr << 7) | (*ptr >> 1);
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
		current_char++;
		break;
	case SDLK_MINUS:
	case SDLK_UNDERSCORE:
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

	status.flags |= NEED_UPDATE;
}

static void handle_key_charmap(struct key_event * k)
{
	switch (k->sym.sym) {
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
	status.flags |= NEED_UPDATE;
}

static void handle_key_itfmap(struct key_event * k)
{
	switch (k->sym.sym) {
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
	status.flags |= NEED_UPDATE;
}

static void confirm_font_save_ok(void *vf)
{
	char *f = vf;
	if (font_save(f) != 0) {
		fprintf(stderr, "%s\n", SDL_GetError());
		return;
	}
	selected_item = EDITBOX;
}

static void handle_key_fontlist(struct key_event * k)
{
	int new_font = cur_font;

	switch (k->sym.sym) {
	case SDLK_HOME:
		new_font = 0;
		break;
	case SDLK_END:
		new_font = flist.num_files - 1;
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
		if (k->state == KEY_PRESS)
			return;
		switch (fontlist_mode) {
		case MODE_LOAD:
			if (cur_font < flist.num_files
			&& flist.files[cur_font]
			&& font_load(flist.files[cur_font]->base) != 0) {
				fprintf(stderr, "%s\n", SDL_GetError());
				font_reset();
			}
			break;
		case MODE_SAVE:
			if (cur_font < flist.num_files && flist.files[cur_font]) {
				if (strcasecmp(flist.files[cur_font]->base,"font.cfg") != 0) {
					dialog_create(DIALOG_OK_CANCEL, "Overwrite font file?",
						confirm_font_save_ok, NULL, 1, flist.files[cur_font]->base);
					return;
				}
				confirm_font_save_ok(flist.files[cur_font]->base);
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
		new_font = CLAMP(new_font, 0, flist.num_files - 1);
		if (new_font == cur_font)
			return;
		cur_font = new_font;
		fontlist_reposition();
	}
	status.flags |= NEED_UPDATE;
}

/* --------------------------------------------------------------------- */

static void handle_mouse_editbox(struct key_event *k)
{
	int n, ci = current_char << 3, xrel, yrel;
	uint8_t *ptr = font_data + ci;

	xrel = k->x - INNER_X(EDITBOX_X);
	yrel = k->y - INNER_Y(EDITBOX_Y);

	if (xrel > 0 && yrel > 2) {
		edit_x = xrel - 1;
		edit_y = yrel - 3;
		switch (k->mouse_button) {
		case MOUSE_BUTTON_LEFT: /* set */
			ptr[edit_y] |= (128 >> edit_x);
			break;
		case MOUSE_BUTTON_MIDDLE: /* invert */
			if (k->state == KEY_RELEASE)
				return;
			ptr[edit_y] ^= (128 >> edit_x);
			break;
		case MOUSE_BUTTON_RIGHT: /* clear */
			ptr[edit_y] &= ~(128 >> edit_x);
			break;
		}
	} else if (xrel == 0 && yrel == 2) {
		/* clicking at the origin modifies the entire character */
		switch (k->mouse_button) {
		case MOUSE_BUTTON_LEFT: /* set */
			for (n = 0; n < 8; n++)
				ptr[n] = 255;
			break;
		case MOUSE_BUTTON_MIDDLE: /* invert */
			if (k->state == KEY_RELEASE)
				return;
			for (n = 0; n < 8; n++)
				ptr[n] ^= 255;
			break;
		case MOUSE_BUTTON_RIGHT: /* clear */
			for (n = 0; n < 8; n++)
				ptr[n] = 0;
			break;
		}
	} else if (xrel == 0 && yrel > 2) {
		edit_y = yrel - 3;
		switch (k->mouse_button) {
		case MOUSE_BUTTON_LEFT: /* set */
			ptr[edit_y] = 255;
			break;
		case MOUSE_BUTTON_MIDDLE: /* invert */
			if (k->state == KEY_RELEASE)
				return;
			ptr[edit_y] ^= 255;
			break;
		case MOUSE_BUTTON_RIGHT: /* clear */
			ptr[edit_y] = 0;
			break;
		}
	} else if (yrel == 2 && xrel > 0) {
		edit_x = xrel - 1;
		switch (k->mouse_button) {
		case MOUSE_BUTTON_LEFT: /* set */
			for (n = 0; n < 8; n++)
				ptr[n] |= (128 >> edit_x);
			break;
		case MOUSE_BUTTON_MIDDLE: /* invert */
			if (k->state == KEY_RELEASE)
				return;
			for (n = 0; n < 8; n++)
				ptr[n] ^= (128 >> edit_x);
			break;
		case MOUSE_BUTTON_RIGHT: /* clear */
			for (n = 0; n < 8; n++)
				ptr[n] &= ~(128 >> edit_x);
			break;
		}
	}
}

static void handle_mouse_charmap(struct key_event *k)
{
	int xrel = k->x - INNER_X(CHARMAP_X), yrel = k->y - INNER_Y(CHARMAP_Y);
	if (!k->mouse) return;
	current_char = 16 * yrel + xrel;
}

static void handle_mouse_itfmap(struct key_event *k)
{
	int xrel = k->x - INNER_X(ITFMAP_X), yrel = k->y - INNER_Y(ITFMAP_Y);
	if (!k->mouse) return;
	itfmap_pos = 16 * yrel + xrel;
	current_char = itfmap_chars[itfmap_pos];
}

static void handle_mouse(struct key_event * k)
{
	int x = k->x, y = k->y;
	if (POINT_IN_FRAME(x, y, EDITBOX)) {
		selected_item = EDITBOX;
		if (POINT_IN(x, y, EDITBOX))
			handle_mouse_editbox(k);
	} else if (POINT_IN_FRAME(x, y, CHARMAP)) {
		selected_item = CHARMAP;
		if (POINT_IN(x, y, CHARMAP))
			handle_mouse_charmap(k);
	} else if (POINT_IN_FRAME(x, y, ITFMAP)) {
		selected_item = ITFMAP;
		if (POINT_IN(x, y, ITFMAP))
			handle_mouse_itfmap(k);
	} else {
		//printf("stray click\n");
		return;
	}
	status.flags |= NEED_UPDATE;
}


static int fontedit_handle_key(struct key_event * k)
{
	int n, ci = current_char << 3;
	uint8_t *ptr = font_data + ci;

	if (k->mouse == MOUSE_SCROLL_UP || k->mouse == MOUSE_SCROLL_DOWN) {
		/* err... */
		return 0;
	}

	if (k->mouse == MOUSE_CLICK) {
		handle_mouse(k);
		return 1;
	}

	/* kp is special */
	switch (k->orig_sym.sym) {
	case SDLK_KP_0:
		if (k->state == KEY_RELEASE)
			return 1;
		k->sym.sym += 10;
		/* fall through */
	case SDLK_KP_1...SDLK_KP_9:
		if (k->state == KEY_RELEASE)
			return 1;
		n = k->sym.sym - SDLK_KP_1;
		if (k->mod & KMOD_SHIFT)
			n += 10;
		palette_load_preset(n);
		palette_apply();
		status.flags |= NEED_UPDATE;
		return 1;
	default:
		break;
	};

	switch (k->sym.sym) {
	case '0':
		if (k->state == KEY_RELEASE)
			return 1;
		k->sym.sym += 10;
		/* fall through */
	case '1'...'9':
		if (k->state == KEY_RELEASE)
			return 1;
		n = k->sym.sym - '1';
		if (k->mod & KMOD_SHIFT)
			n += 10;
		palette_load_preset(n);
		palette_apply();
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_F2:
		if (k->state == KEY_RELEASE)
			return 1;
		selected_item = EDITBOX;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_F3:
		if (k->state == KEY_RELEASE)
			return 1;
		selected_item = CHARMAP;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_F4:
		if (k->state == KEY_RELEASE)
			return 1;
		selected_item = ITFMAP;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_TAB:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_SHIFT) {
			if (selected_item == 0)
				selected_item = (fontlist_mode == MODE_OFF ? 2 : 3);
			else
				selected_item--;
		} else {
			selected_item = (selected_item + 1) % (fontlist_mode == MODE_OFF ? 3 : 4);
		}
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_c:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_ALT) {
			memcpy(clipboard, ptr, 8);
			return 1;
		}
		break;
	case SDLK_p:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_ALT) {
			memcpy(ptr, clipboard, 8);
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	case SDLK_m:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_CTRL) {
			SDL_ToggleCursor();
			return 1;
		} else if (k->mod & KMOD_ALT) {
			for (n = 0; n < 8; n++)
				ptr[n] |= clipboard[n];
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	case SDLK_z:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_ALT) {
			memset(ptr, 0, 8);
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	case SDLK_h:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_ALT) {
			for (n = 0; n < 8; n++) {
				int r = ptr[n];
				r = ((r >> 1) & 0x55) | ((r << 1) & 0xaa);
				r = ((r >> 2) & 0x33) | ((r << 2) & 0xcc);
				r = ((r >> 4) & 0x0f) | ((r << 4) & 0xf0);
				ptr[n] = r;
			}
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	case SDLK_v:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_ALT) {
			for (n = 0; n < 4; n++) {
				uint8_t r = ptr[n];
				ptr[n] = ptr[7 - n];
				ptr[7 - n] = r;
			}
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;
	case SDLK_i:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_ALT) {
			for (n = 0; n < 8; n++)
				font_data[ci + n] ^= 255;
			status.flags |= NEED_UPDATE;
			return 1;
		}
		break;

		/* ----------------------------------------------------- */

	case SDLK_l:
	case SDLK_r:
		if (k->state == KEY_RELEASE)
			return 1;
		if (!(k->mod & KMOD_CTRL)) break;
		/* fall through */
	case SDLK_F9:
		if (k->state == KEY_RELEASE)
			return 1;
		load_fontlist();
		fontlist_mode = MODE_LOAD;
		selected_item = FONTLIST;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_s:
		if (k->state == KEY_RELEASE)
			return 1;
		if (!(k->mod & KMOD_CTRL)) break;
		/* fall through */
	case SDLK_F10:
		/* a bit weird, but this ensures that font.cfg
		 * is always the default font to save to, but
		 * without the annoyance of moving the cursor
		 * back to it every time f10 is pressed. */
		if (fontlist_mode != MODE_SAVE) {
			cur_font = top_font = 0;
			load_fontlist();
			fontlist_mode = MODE_SAVE;
		}
		selected_item = FONTLIST;
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_BACKSPACE:
		if (k->state == KEY_RELEASE)
			return 1;
		if (k->mod & KMOD_CTRL) {
			font_reset_bios();
		} else if (k->mod & KMOD_ALT) {
			font_reset_char(current_char);
		} else {
			font_reset_upper();
		}
		status.flags |= NEED_UPDATE;
		return 1;
	case SDLK_RETURN:
		return 0;
	case SDLK_q:
		if (k->mod & KMOD_CTRL)
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		break;
	default:
		if (k->state == KEY_RELEASE)
			return 1;
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
	return 1;
}


static struct widget fontedit_widget_hack[1];

static int fontedit_key_hack(struct key_event *k)
{
	switch (k->sym.sym) {
	case SDLK_r: case SDLK_l: case SDLK_s:
	case SDLK_c: case SDLK_p: case SDLK_m:
	case SDLK_z: case SDLK_v: case SDLK_h:
	case SDLK_i: case SDLK_q: case SDLK_w:
	case SDLK_F1...SDLK_F12:
		return fontedit_handle_key(k);
	case SDLK_RETURN:
		if (status.dialog_type & (DIALOG_MENU|DIALOG_BOX)) return 0;
		if (selected_item == FONTLIST) {
			handle_key_fontlist(k);
			return 1;
		}
	default:
		break;
	};
	return 0;
}

static void do_nil(void) {}
void fontedit_load_page(struct page *page)
{
	page->title = "";
	page->draw_full = draw_screen;
	page->total_widgets = 1;
	page->pre_handle_key = fontedit_key_hack;
	page->widgets = fontedit_widget_hack;
	create_other(fontedit_widget_hack, 0, fontedit_handle_key, do_nil);
}
