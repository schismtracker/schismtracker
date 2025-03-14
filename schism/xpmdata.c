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

#include "video.h" /* for declaration of xpmdata */
#include "util.h"

#include <ctype.h>

/*
** This came from SDL_image's IMG_xpm.c
*
    SDL_image:  An example image loading library for use with SDL
    Copyright (C) 1999-2004 Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    slouken@libsdl.org
*/

#define SKIPSPACE(p)                            \
do {                                            \
	while(isspace((unsigned char)*(p)))     \
	      ++(p);                            \
} while(0)

#define SKIPNONSPACE(p)                                 \
do {                                                    \
	while(!isspace((unsigned char)*(p)) && *p)      \
	      ++(p);                                    \
} while(0)

/* portable case-insensitive string comparison */
static int string_equal(const char *a, const char *b, int n)
{
	while(*a && *b && n) {
		if(toupper((unsigned char)*a) != toupper((unsigned char)*b))
			return 0;
		a++;
		b++;
		n--;
	}
	return *a == *b;
}

/*
 * convert colour spec to RGB (in 0xrrggbb format).
 * return 1 if successful.
 */
static int color_to_rgb(const char *spec, int speclen, uint32_t *rgb)
{
	/* poor man's rgb.txt */
	static struct { const char *name; uint32_t rgb; } known[] = {
		{"none",  0xffffffff},
		{"black", 0x00000000},
		{"white", 0x00ffffff},
		{"red",   0x00ff0000},
		{"green", 0x0000ff00},
		{"blue",  0x000000ff},
		{"gray27",0x00454545},
		{"gray4", 0x000a0a0a},
	};

	if(spec[0] == '#') {
		char buf[7];
		switch(speclen) {
		case 4:
			buf[0] = buf[1] = spec[1];
			buf[2] = buf[3] = spec[2];
			buf[4] = buf[5] = spec[3];
			break;
		case 7:
			memcpy(buf, spec + 1, 6);
			break;
		case 13:
			buf[0] = spec[1];
			buf[1] = spec[2];
			buf[2] = spec[5];
			buf[3] = spec[6];
			buf[4] = spec[9];
			buf[5] = spec[10];
			break;
		}
		buf[6] = '\0';
		*rgb = strtol(buf, NULL, 16);
		return 1;
	} else {
		size_t i;
		for(i = 0; i < ARRAY_SIZE(known); i++)
			if(string_equal(known[i].name, spec, speclen)) {
				*rgb = known[i].rgb;
				return 1;
			}
		return 0;
	}
}

#define STARTING_HASH_SIZE 256
struct hash_entry {
	char *key;
	uint32_t color;
	struct hash_entry *next;
};

struct color_hash {
	struct hash_entry **table;
	struct hash_entry *entries; /* array of all entries */
	struct hash_entry *next_free;
	int size;
	int maxnum;
};

static int hash_key(const char *key, int cpp, int size)
{
	int hash;

	hash = 0;
	while ( cpp-- > 0 ) {
		hash = hash * 33 + *key++;
	}
	return hash & (size - 1);
}

static struct color_hash *create_colorhash(int maxnum)
{
	int bytes, s;
	struct color_hash *hash;

	/* we know how many entries we need, so we can allocate
	   everything here */
	hash = malloc(sizeof *hash);
	if(!hash)
		return NULL;

	/* use power-of-2 sized hash table for decoding speed */
	for(s = STARTING_HASH_SIZE; s < maxnum; s <<= 1)
		;
	hash->size = s;
	hash->maxnum = maxnum;
	bytes = hash->size * sizeof(struct hash_entry **);
	hash->entries = NULL;   /* in case malloc fails */
	hash->table = malloc(bytes);
	if(!hash->table)
		return NULL;
	memset(hash->table, 0, bytes);
	hash->entries = malloc(maxnum * sizeof(struct hash_entry));
	if(!hash->entries)
		return NULL;
	hash->next_free = hash->entries;
	return hash;
}

static int add_colorhash(struct color_hash *hash,
			 char *key, int cpp, uint32_t color)
{
	int h = hash_key(key, cpp, hash->size);
	struct hash_entry *e = hash->next_free++;
	e->color = color;
	e->key = key;
	e->next = hash->table[h];
	hash->table[h] = e;
	return 1;
}

/* fast lookup that works if cpp == 1 */
#define QUICK_COLORHASH(hash, key) ((hash)->table[*(uint8_t *)(key)]->color)

static uint32_t get_colorhash(struct color_hash *hash, const char *key, int cpp)
{
	struct hash_entry *entry = hash->table[hash_key(key, cpp, hash->size)];
	while(entry) {
		if(memcmp(key, entry->key, cpp) == 0)
			return entry->color;
		entry = entry->next;
	}
	return 0;               /* garbage in - garbage out */
}

static void free_colorhash(struct color_hash *hash)
{
	if(hash && hash->table) {
		free(hash->table);
		free(hash->entries);
		free(hash);
	}
}


int xpmdata(const char *data[], uint32_t **pixels, int *w, int *h)
{
	int n;
	int x, y;
	int ncolors, cpp;
	uint32_t *dst;
	struct color_hash *colors = NULL;
	char *keystrings = NULL, *nextkey;
	const char *line;
	const char ***xpmlines = NULL;
#define get_next_line(q) *(*q)++
	int error;

	error = 0;

	xpmlines = (const char ***) &data;

	line = get_next_line(xpmlines);
	if(!line) goto done;

	/*
	 * The header string of an XPMv3 image has the format
	 *
	 * <width> <height> <ncolors> <cpp> [ <hotspot_x> <hotspot_y> ]
	 *
	 * where the hotspot coords are intended for mouse cursors.
	 * Right now we don't use the hotspots but it should be handled
	 * one day.
	 */
	if(sscanf(line, "%d %d %d %d", w, h, &ncolors, &cpp) != 4
	   || *w <= 0 || *h <= 0 || ncolors <= 0 || cpp <= 0) {
		error = 1;
		goto done;
	}

	keystrings = malloc(ncolors * cpp);
	if(!keystrings) {
		error = 2;
		goto done;
	}
	nextkey = keystrings;

	*pixels = malloc(*w * *h * sizeof(*pixels));
	if (!*pixels) {
		error = 2;
		goto done;
	}

	/* Read the colors */
	colors = create_colorhash(ncolors);
	if (!colors) {
		error = 2;
		goto done;
	}

	for(n = 0; n < ncolors; ++n) {
		const char *p;
		line = get_next_line(xpmlines);
		if(!line)
			goto done;

		p = line + cpp + 1;

		/* parse a colour definition */
		for(;;) {
			char nametype;
			const char *colname;
			uint32_t rgb, pixel;

			SKIPSPACE(p);
			if(!*p) {
				error = 3;
				goto done;
			}
			nametype = *p;
			SKIPNONSPACE(p);
			SKIPSPACE(p);
			colname = p;
			SKIPNONSPACE(p);
			if(nametype == 's')
				continue;      /* skip symbolic colour names */

			if(!color_to_rgb(colname, p - colname, &rgb))
				continue;

			memcpy(nextkey, line, cpp);

			/* UINT32_MAX is transparent */
			pixel = (rgb == UINT32_C(0xFFFFFFFF) ? 0 : (rgb | UINT32_C(0xFF000000)));

			add_colorhash(colors, nextkey, cpp, pixel);
			nextkey += cpp;

			break;
		}
	}

	/* Read the pixels */
	dst = *pixels;
	for(y = 0; y < *h; y++) {
		line = get_next_line(xpmlines);
		for (x = 0; x < *w; x++)
			dst[x] = get_colorhash(colors, line + x * cpp, cpp);
		dst += *w;
	}

done:
	if (error) {
		free(*pixels);
		*pixels = NULL;
	}
	free(keystrings);
	free_colorhash(colors);
	return error;
}
