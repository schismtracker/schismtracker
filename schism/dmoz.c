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

/* No, this has nothing whatsoever to do with dmoz.org, except for the 'directory' part. :) */

#include "headers.h"

#include "it.h"
#include "charset.h"
#include "song.h"
#include "dmoz.h"
#include "slurp.h"
#include "util.h"

#include "fmt.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <ctype.h>
#include <errno.h>

#ifdef __amigaos4__
# include <proto/dos.h>
#endif

#ifdef SCHISM_WIN32
#include <windows.h>
#include <winbase.h>
#endif

#ifdef SCHISM_WII
#include <sys/dir.h>
// isfs is pretty much useless, but it might be interesting to browse it I guess
static const char *devices[] = {
	"sd:/",
	"isfs:/",
	NULL
};
#endif

/* --------------------------------------------------------------------------------------------------------- */
/* constants */

/* note: this has do be split up like this; otherwise it gets read as '\x9ad' which is the Wrong Thing. */
#define TITLE_DIRECTORY "\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a" \
	"Directory\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a"
#define TITLE_LIBRARY "\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9aLibrary\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a\x9a"
#define DESCR_DIRECTORY "Directory"
#define DESCR_UNKNOWN "Unknown sample format"

/* memory allocation: how many files/dirs are allocated at a time */
#define FILE_BLOCK_SIZE 256
#define DIR_BLOCK_SIZE 32

/* --------------------------------------------------------------------------------------------------------- */
/* file format tables */

#define READ_INFO(t) fmt_##t##_read_info,

static const fmt_read_info_func read_info_funcs[] = {
#include "fmt-types.h"
	NULL /* This needs to be at the bottom of the list! */
};

/* --------------------------------------------------------------------------------------------------------- */
/* sorting stuff */

typedef int (*dmoz_fcmp_t) (const dmoz_file_t *a, const dmoz_file_t *b);
typedef int (*dmoz_dcmp_t) (const dmoz_dir_t *a, const dmoz_dir_t *b);

#define _DECL_CMP(name)                                                                 \
	static int dmoz_fcmp_##name(const dmoz_file_t *a, const dmoz_file_t *b);        \
	static int dmoz_dcmp_##name(const dmoz_dir_t *a, const dmoz_dir_t *b);
_DECL_CMP(strcmp)
_DECL_CMP(strcasecmp)
#if HAVE_STRVERSCMP
_DECL_CMP(strverscmp)
#endif
static int dmoz_fcmp_timestamp(const dmoz_file_t *a, const dmoz_file_t *b);

#if HAVE_STRVERSCMP
static dmoz_fcmp_t dmoz_file_cmp = dmoz_fcmp_strverscmp;
static dmoz_dcmp_t dmoz_dir_cmp = dmoz_dcmp_strverscmp;
#else
static dmoz_fcmp_t dmoz_file_cmp = dmoz_fcmp_strcasecmp;
static dmoz_dcmp_t dmoz_dir_cmp = dmoz_dcmp_strcasecmp;
#endif

static struct {
	const char *name;
	dmoz_fcmp_t fcmp;
	dmoz_dcmp_t dcmp;
} compare_funcs[] = {
	{"strcmp", dmoz_fcmp_strcmp, dmoz_dcmp_strcmp},
	{"strcasecmp", dmoz_fcmp_strcasecmp, dmoz_dcmp_strcasecmp},
#if HAVE_STRVERSCMP
	{"strverscmp", dmoz_fcmp_strverscmp, dmoz_dcmp_strverscmp},
	{"timestamp", dmoz_fcmp_timestamp, dmoz_dcmp_strverscmp},
#else
	{"timestamp", dmoz_fcmp_timestamp, dmoz_dcmp_strcasecmp},
#endif
	{NULL, NULL, NULL}
};

/* --------------------------------------------------------------------------------------------------------- */
/* "selected" and cache */

struct dmoz_cache {
	struct dmoz_cache *next;
	char *path;
	char *cache_filen;
	char *cache_dirn;
};

static struct dmoz_cache *cache_top = NULL;

void dmoz_cache_update(const char *path, dmoz_filelist_t *fl, dmoz_dirlist_t *dl)
{
	char *fn, *dn;
	if (fl && fl->selected > -1 && fl->selected < fl->num_files && fl->files[fl->selected])
		fn = fl->files[fl->selected]->base;
	else
		fn = NULL;
	if (dl && dl->selected > -1 && dl->selected < dl->num_dirs && dl->dirs[dl->selected])
		dn = dl->dirs[dl->selected]->base;
	else
		dn = NULL;
	dmoz_cache_update_names(path,fn,dn);
}

void dmoz_cache_update_names(const char *path, const char *filen, const char *dirn)
{
	struct dmoz_cache *p, *lp;
	char *q;
	q = str_dup(path);
	lp = NULL;
	filen = filen ? get_basename(filen) : NULL;
	dirn = dirn ? get_basename(dirn) : NULL;
	if (filen && strcmp(filen, "..") == 0)
		filen = NULL;
	if (dirn && strcmp(dirn, "..") == 0)
		dirn = NULL;
	for (p = cache_top; p; p = p->next) {
		if (strcmp(p->path,q)==0) {
			free(q);
			if (filen) {
				free(p->cache_filen);
				p->cache_filen = str_dup(filen);
			}
			if (dirn) {
				free(p->cache_dirn);
				p->cache_dirn = str_dup(dirn);
			}
			if (lp) {
				lp->next = p->next;
				/* !lp means we're already cache_top */
				p->next = cache_top;
				cache_top = p;
			}
			return;
		}
		lp = p;
	}
	p = mem_alloc(sizeof(struct dmoz_cache));
	p->path = q;
	p->cache_filen = filen ? str_dup(filen) : NULL;
	p->cache_dirn = dirn ? str_dup(dirn) : NULL;
	p->next = cache_top;
	cache_top = p;
}

void dmoz_cache_lookup(const char *path, dmoz_filelist_t *fl, dmoz_dirlist_t *dl)
{
	struct dmoz_cache *p;
	int i;

	if (fl) fl->selected = 0;
	if (dl) dl->selected = 0;
	for (p = cache_top; p; p = p->next) {
		if (strcmp(p->path,path) == 0) {
			if (fl && p->cache_filen) {
				for (i = 0; i < fl->num_files; i++) {
					if (!fl->files[i]) continue;
					if (strcmp(fl->files[i]->base, p->cache_filen) == 0) {
						fl->selected = i;
						break;
					}
				}
			}
			if (dl && p->cache_dirn) {
				for (i = 0; i < dl->num_dirs; i++) {
					if (!dl->dirs[i]) continue;
					if (strcmp(dl->dirs[i]->base, p->cache_dirn) == 0) {
						dl->selected = i;
						break;
					}
				}
			}
			break;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------- */
/* path string hacking */

/* This function should:
	- strip out any parent directory references ("/sheep/../goat" => "/goat")
	- switch slashes to backslashes for MS systems ("c:/winnt" => "c:\\winnt")
	- condense multiple slashes into one ("/sheep//goat" => "/sheep/goat")
	- remove any trailing slashes

[Amiga note: is foo:/bar the same as foo:bar, is it like /bar, or something else?] */
char *dmoz_path_normal(const char *path)
{
	char stub_char;
	char *result, *p, *q, *base, *dotdot;
	int rooted;

	/* The result cannot be larger than the input PATH. */
	result = strdup(path);

	rooted = dmoz_path_is_absolute(path);
	base = result + rooted;
	stub_char = rooted ? DIR_SEPARATOR : '.';

#ifdef SCHISM_WIN32
	/* Stupid hack -- fix up any initial slashes in the absolute part of the path.
	(The rest of them will be handled as the path components are processed.) */
	for (q = result; q < base; q++)
		if (*q == '/')
			*q = '\\';
#endif

	/* invariants:
		base points to the portion of the path we want to modify
		p points at beginning of path element we're considering.
		q points just past the last path element we wrote (no slash).
		dotdot points just past the point where .. cannot backtrack
		any further (no slash). */
	p = q = dotdot = base;

	while (*p) {
		if (IS_DIR_SEPARATOR(p[0])) {
			/* null element */
			p++;
		} else if (p[0] == '.' && (!p[1] || IS_DIR_SEPARATOR(p[1]))) {
			/* . and ./ */
			p += 1; /* don't count the separator in case it is nul */
		} else if (p[0] == '.' && p[1] == '.' && (!p[2] || IS_DIR_SEPARATOR(p[2]))) {
			/* .. and ../ */
			p += 2; /* skip `..' */
			if (q > dotdot) {       /* can backtrack */
				while (--q > dotdot && !IS_DIR_SEPARATOR(*q)) {
					/* nothing */
				}
			} else if (!rooted) {
				/* /.. is / but ./../ is .. */
				if (q != base)
					*q++ = DIR_SEPARATOR;
				*q++ = '.';
				*q++ = '.';
				dotdot = q;
			}
		} else {
			/* real path element */
			/* add separator if not at start of work portion of result */
			if (q != base)
				*q++ = DIR_SEPARATOR;
			while (*p && !IS_DIR_SEPARATOR(*p))
				*q++ = *p++;
		}
	}

	/* Empty string is really ``.'' or `/', depending on what we started with. */
	if (q == result)
		*q++ = stub_char;
	*q = '\0';

	return result;
}

int dmoz_path_is_absolute(const char *path)
{
	if (!path || !*path)
		return 0;
#if defined(SCHISM_WIN32)
	if (isalpha(path[0]) && path[1] == ':')
		return IS_DIR_SEPARATOR(path[2]) ? 3 : 2;
#elif defined(__amigaos4__)
	/* Entirely a guess -- could some fine Amiga user please tell me if this is right or not? */
	char *colon = strchr(path, ':'), *slash = strchr(path, '/');
	if (colon && (colon < slash || (colon && !slash && colon[1] == '\0')))
		return colon - path + 1;
#elif defined(SCHISM_WII)
	char *colon = strchr(path, ':'), *slash = strchr(path, '/');
	if (colon + 1 == slash)
		return slash - path + 1;
#endif
	/* presumably, /foo (or \foo) is an absolute path on all platforms */
	if (!IS_DIR_SEPARATOR(path[0]))
		return 0;
	/* POSIX says to allow two leading slashes, but not more.
	(This also catches win32 \\share\blah\blah semantics) */
	return (IS_DIR_SEPARATOR(path[1]) && !IS_DIR_SEPARATOR(path[2])) ? 2 : 1;
}


/* See dmoz_path_concat_len. This function is a convenience for when the lengths aren't already known. */
char *dmoz_path_concat(const char *a, const char *b)
{
	return dmoz_path_concat_len(a, b, strlen(a), strlen(b));
}

/* Concatenate two paths. Additionally, if 'b' is an absolute path, ignore 'a' and return a copy of 'b'. */
char *dmoz_path_concat_len(const char *a, const char *b, int alen, int blen)
{
	char *ret;
	if (dmoz_path_is_absolute(b))
		return strdup(b);

	ret = mem_alloc(alen + blen + 2);

	if (alen) {
		char last = a[alen - 1];

		strcpy(ret, a);

		/* need a slash? */
#if defined(__amigaos4__)
		if (last != ':' && last != '/')
			strcat(ret, "/");
#else
		if (last != DIR_SEPARATOR)
			strcat(ret, DIR_SEPARATOR_STR);
#endif
	}
	strcat(ret, b);

	return ret;
}

/* --------------------------------------------------------------------------------------------------------- */
/* memory management */

static void allocate_more_files(dmoz_filelist_t *flist)
{
	if (flist->alloc_size == 0) {
		flist->alloc_size = FILE_BLOCK_SIZE;
		flist->files = (dmoz_file_t **)mem_alloc(FILE_BLOCK_SIZE * sizeof(dmoz_file_t *));
	} else {
		flist->alloc_size *= 2;
		flist->files = (dmoz_file_t **)mem_realloc(flist->files,
			flist->alloc_size * sizeof(dmoz_filelist_t *));
	}
}

static void allocate_more_dirs(dmoz_dirlist_t *dlist)
{
	if (dlist->alloc_size == 0) {
		dlist->alloc_size = DIR_BLOCK_SIZE;
		dlist->dirs = (dmoz_dir_t **)mem_alloc(DIR_BLOCK_SIZE * sizeof(dmoz_dir_t *));
	} else {
		dlist->alloc_size *= 2;
		dlist->dirs = (dmoz_dir_t **)mem_realloc(dlist->dirs,
			dlist->alloc_size * sizeof(dmoz_dir_t *));
	}
}

static void free_file(dmoz_file_t *file)
{
	if (!file)
		return;
	if (file->smp_filename != file->base && file->smp_filename != file->title) {
		free(file->smp_filename);
	}
	free(file->path);
	free(file->base);
	if (file->type & TYPE_EXT_DATA_MASK) {
		if (file->artist)
			free(file->artist);
		free(file->title);
		/* if (file->sample) {
			if (file->sample->data)
				csf_free_sample(file->sample->data);
			free(file->sample);
		} */
	}
	free(file);
}

static void free_dir(dmoz_dir_t *dir)
{
	if (!dir)
		return;
	free(dir->path);
	free(dir->base);
	free(dir);
}

void dmoz_free(dmoz_filelist_t *flist, dmoz_dirlist_t *dlist)
{
	int n;

	if (flist) {
		for (n = 0; n < flist->num_files; n++)
			free_file(flist->files[n]);
		free(flist->files);
		flist->files = NULL;
		flist->num_files = 0;
		flist->alloc_size = 0;
	}

	if (dlist) {
		for (n = 0; n < dlist->num_dirs; n++)
			free_dir(dlist->dirs[n]);
		free(dlist->dirs);
		dlist->dirs = NULL;
		dlist->num_dirs = 0;
		dlist->alloc_size = 0;
	}
}

static int current_dmoz_file = 0;
static dmoz_filelist_t *current_dmoz_filelist = NULL;
static int (*current_dmoz_filter)(dmoz_file_t *) = NULL;
static int *current_dmoz_file_pointer = NULL;
static void (*dmoz_worker_onmove)(void) = NULL;

int dmoz_worker(void)
{
	dmoz_file_t *nf;

	if (!current_dmoz_filelist || !current_dmoz_filter)
		return 0;
	if (current_dmoz_file >= current_dmoz_filelist->num_files) {
		current_dmoz_filelist = NULL;
		current_dmoz_filter = NULL;
		if (dmoz_worker_onmove)
			dmoz_worker_onmove();
		return 0;
	}

	if (!current_dmoz_filter(current_dmoz_filelist->files[ current_dmoz_file ])) {
		if (current_dmoz_filelist->num_files == current_dmoz_file+1) {
			current_dmoz_filelist->num_files--;
			current_dmoz_filelist = NULL;
			current_dmoz_filter = NULL;
			if (dmoz_worker_onmove)
				dmoz_worker_onmove();
			return 0;
		}

		nf = current_dmoz_filelist->files[ current_dmoz_file ];
		memmove(&current_dmoz_filelist->files[ current_dmoz_file ],
			&current_dmoz_filelist->files[ current_dmoz_file+1 ],
			sizeof(dmoz_file_t *) * (current_dmoz_filelist->num_files
						- current_dmoz_file));
		free_file(nf);
		current_dmoz_filelist->num_files--;
		if (current_dmoz_file_pointer && *current_dmoz_file_pointer >=
					current_dmoz_file) {
			(*current_dmoz_file_pointer) = (*current_dmoz_file_pointer) - 1;
			if (dmoz_worker_onmove) dmoz_worker_onmove();
		}
		if (current_dmoz_file_pointer && *current_dmoz_file_pointer >=
					current_dmoz_filelist->num_files) {
			(*current_dmoz_file_pointer) = (current_dmoz_filelist->num_files-1);
			if (dmoz_worker_onmove) dmoz_worker_onmove();
		}
		status.flags |= NEED_UPDATE;
	} else {
		current_dmoz_file++;
	}
	return 1;
}


/* filters a filelist and removes rejected entries. this works in-place
so it can't generate error conditions. */
void dmoz_filter_filelist(dmoz_filelist_t *flist, int (*grep)(dmoz_file_t *f), int *pointer, void (*fn)(void))
{
	current_dmoz_filelist = flist;
	current_dmoz_filter = grep;
	current_dmoz_file = 0;
	current_dmoz_file_pointer = pointer;
	dmoz_worker_onmove = fn;
}

/* TODO:
- create a one-shot filter that runs all its files at once
- make these filters not actually drop the files from the list, but instead set the hidden flag
- add a 'num_unfiltered' variable to the struct that indicates the total number
*/

/* --------------------------------------------------------------------------------------------------------- */
/* adding to the lists */

dmoz_file_t *dmoz_add_file(dmoz_filelist_t *flist, char *path, char *base, struct stat *st, int sort_order)
{
	dmoz_file_t *file = mem_calloc(1, sizeof(dmoz_file_t));

	file->path = path;
	file->base = base;
	file->sort_order = sort_order;
	file->sampsize = 0;
	file->instnum = -1;

	if (st == NULL || S_ISDIR(st->st_mode)) {
		file->type = TYPE_DIRECTORY;
		/* have to fill everything in for directories */
		file->description = DESCR_DIRECTORY;
		file->title = str_dup(TITLE_DIRECTORY);
	} else if (S_ISREG(st->st_mode)) {
		file->type = TYPE_FILE_MASK; /* really ought to have a separate TYPE_UNCHECKED_FILE... */
	} else {
		file->type = TYPE_NON_REGULAR;
	}

	if (st) {
		file->timestamp = st->st_mtime;
		file->filesize = st->st_size;
	} else {
		file->timestamp = 0;
		file->filesize = 0;
	}

	if (flist->num_files >= flist->alloc_size)
		allocate_more_files(flist);
	flist->files[flist->num_files++] = file;

	return file;
}

dmoz_dir_t *dmoz_add_dir(dmoz_dirlist_t *dlist, char *path, char *base, int sort_order)
{
	dmoz_dir_t *dir = mem_calloc(1, sizeof(dmoz_dir_t));

	dir->path = path;
	dir->base = base;
	dir->sort_order = sort_order;

	if (dlist->num_dirs >= dlist->alloc_size)
		allocate_more_dirs(dlist);
	dlist->dirs[dlist->num_dirs++] = dir;

	return dir;
}

void dmoz_add_file_or_dir(dmoz_filelist_t *flist, dmoz_dirlist_t *dlist,
			  char *path, char *base, struct stat *st, int sort_order)
{
	if (dlist)
		dmoz_add_dir(dlist, path, base, sort_order);
	else
		dmoz_add_file(flist, path, base, st, sort_order);
}

/* --------------------------------------------------------------------------------------------------------- */
/* sorting */

#define _DEF_CMP_CHARSET(name)                                                                  \
	static int dmoz_fcmp_##name(const dmoz_file_t *a, const dmoz_file_t *b)         \
	{                                                                               \
		return charset_##name(a->base, CHARSET_CHAR, b->base, CHARSET_CHAR);                  \
	}                                                                               \
	static int dmoz_dcmp_##name(const dmoz_dir_t *a, const dmoz_dir_t *b)           \
	{                                                                               \
		return charset_##name(a->base, CHARSET_CHAR, b->base, CHARSET_CHAR);                  \
	}
_DEF_CMP_CHARSET(strcmp)
_DEF_CMP_CHARSET(strcasecmp)
#if HAVE_STRVERSCMP
static int dmoz_fcmp_strverscmp(const dmoz_file_t *a, const dmoz_file_t *b)
{
	return strverscmp(a->base, b->base);
}
static int dmoz_dcmp_strverscmp(const dmoz_dir_t *a, const dmoz_dir_t *b)
{
	return strverscmp(a->base, b->base);
}
#endif

/* timestamp only works for files, so can't macro-def it */
static int dmoz_fcmp_timestamp(const dmoz_file_t *a, const dmoz_file_t *b)
{
	return b->timestamp - a->timestamp;
}

static int qsort_cmp_file(const void *_a, const void *_b)
{
	const dmoz_file_t *a = *(const dmoz_file_t **) _a;
	const dmoz_file_t *b = *(const dmoz_file_t **) _b;

	if ((b->type & TYPE_HIDDEN) && !(a->type & TYPE_HIDDEN))
		return -1; /* b goes first */
	if ((a->type & TYPE_HIDDEN) && !(b->type & TYPE_HIDDEN))
		return 1; /* b goes first */
	if (a->sort_order < b->sort_order)
		return -1; /* a goes first */
	if (b->sort_order < a->sort_order)
		return 1; /* b goes first */
	return (*dmoz_file_cmp)(a, b);
}

static int qsort_cmp_dir(const void *_a, const void *_b)
{
	const dmoz_dir_t *a = *(const dmoz_dir_t **) _a;
	const dmoz_dir_t *b = *(const dmoz_dir_t **) _b;

	if (a->sort_order < b->sort_order)
		return -1; /* a goes first */
	if (b->sort_order < a->sort_order)
		return 1; /* b goes first */
	return (*dmoz_dir_cmp)(a, b);
}

void dmoz_sort(dmoz_filelist_t *flist, dmoz_dirlist_t *dlist)
{
	qsort(flist->files, flist->num_files, sizeof(dmoz_file_t *), qsort_cmp_file);
	if (dlist)
		qsort(dlist->dirs, dlist->num_dirs, sizeof(dmoz_dir_t *), qsort_cmp_dir);
}

void cfg_load_dmoz(cfg_file_t *cfg)
{
	const char *ptr;
	int i;

	ptr = cfg_get_string(cfg, "Directories", "sort_with", NULL, 0, NULL);
	if (ptr) {
		for (i = 0; compare_funcs[i].name; i++) {
			if (strcasecmp(compare_funcs[i].name, ptr) == 0) {
				dmoz_file_cmp = compare_funcs[i].fcmp;
				dmoz_dir_cmp = compare_funcs[i].dcmp;
				break;
			}
		}
	}
}

void cfg_save_dmoz(cfg_file_t *cfg)
{
	int i;

	for (i = 0; compare_funcs[i].name; i++) {
		if (dmoz_file_cmp == compare_funcs[i].fcmp) {
			cfg_set_string(cfg, "Directories", "sort_with", compare_funcs[i].name);
			break;
		}
	}
}

/* --------------------------------------------------------------------------------------------------------- */
/* platform-specific directory navigation */

/* TODO: stat these? (certainly not critical, but would be nice) */
static void add_platform_dirs(const char *path, dmoz_filelist_t *flist, dmoz_dirlist_t *dlist)
{
	char *ptr;
#if defined(__amigaos4__)
	/* Amiga OS volume list from Juha Niemimäki */
	struct DosList *pList;
	char *pTemp, *pString;
	int i, order = -1024;

	if ((pList = IDOS->LockDosList(LDF_VOLUMES | LDF_READ))) {
		while ((pList = IDOS->NextDosEntry(pList, LDF_VOLUMES))) {
			pTemp = pList->dol_Name << 2;
			if (pTemp && pTemp[0] > 0) {
				pString = calloc(pTemp[0] + 1, sizeof(char));
				if (pString) {
					/* for (i = 0; i < pTemp[0]; i++)
					 *      pString[i] = pTemp[i + 1]; */
					memcpy(pString, pTemp + 1, pTemp[0]);
					pString[pTemp[0]] = '\0';
					dmoz_add_file_or_dir(flist, dlist, pString, str_dup(pString),
							     NULL, order++);
				}
			}
		}
		IDOS->UnLockDosList(LDF_VOLUMES);
	}
#elif defined(SCHISM_WIN32)
	const DWORD x = GetLogicalDrives();
	UINT em = SetErrorMode(0);
	char sbuf[] = "A:\\";

	for (; x && sbuf[0] <= 'Z'; sbuf[0]++) {
		if ((x >> (sbuf[0] - 'A')) & 1) {
				dmoz_add_file_or_dir(flist, dlist, str_dup(sbuf),
						str_dup(sbuf), NULL, -(1024 - 'A' - sbuf[0]));
		}
	}
	em = SetErrorMode(em);
#elif defined(SCHISM_WII)
	int i;
	for (i = 0; devices[i]; i++) {
		DIR *dir = opendir(devices[i]);
		if (!dir)
			continue;
		closedir(dir);
		dmoz_add_file_or_dir(flist, dlist, str_dup(devices[i]), str_dup(devices[i]), NULL, -(1024 - i));
	}
#else /* assume POSIX */
/*      char *home;
	home = get_home_directory();*/
	dmoz_add_file_or_dir(flist, dlist, str_dup("/"), str_dup("/"), NULL, -1024);
/*      dmoz_add_file_or_dir(flist, dlist, home, str_dup("~"), NULL, -5); */

#endif /* platform */

	ptr = get_parent_directory(path);
	if (ptr)
		dmoz_add_file_or_dir(flist, dlist, ptr, str_dup(".."), NULL, -10);
}

/* --------------------------------------------------------------------------------------------------------- */

#if defined(SCHISM_WIN32)
# define FAILSAFE_PATH "C:\\" /* hopefully! */
#else
# define FAILSAFE_PATH "/"
#endif

/* on success, this will fill the lists and return 0. if something goes
wrong, it adds a 'stub' entry for the root directory, and returns -1. */
int dmoz_read(const char *path, dmoz_filelist_t *flist, dmoz_dirlist_t *dlist,
		int (*load_library)(const char *path, dmoz_filelist_t *flist, dmoz_dirlist_t *dlist))
{
#ifdef SCHISM_WIN32
	wchar_t* path_w = NULL;
	if (charset_iconv((uint8_t*)path, (uint8_t**)&path_w, CHARSET_UTF8, CHARSET_WCHAR_T))
		return -1;

	DWORD attrib = GetFileAttributesW(path_w);
	if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
		free(path_w); /* don't need this anymore */

		char* searchpath = dmoz_path_concat_len(path, "*", strlen(path), 1);
		wchar_t* searchpath_w = NULL;
		if (charset_iconv((uint8_t*)searchpath, (uint8_t**)&searchpath_w, CHARSET_UTF8, CHARSET_WCHAR_T)) {
			free(searchpath);
			return -1;
		}
		free(searchpath);

		WIN32_FIND_DATAW ffd = {0};
		HANDLE find = FindFirstFileW(searchpath_w, &ffd);

		if (find == INVALID_HANDLE_VALUE) {
			log_appendf(4, "dmoz_read: FindFirstFile failed!");
			return -1;
		}

		/* do-while to process the first file... */
		do {
			if (!wcscmp(ffd.cFileName, L".") || !wcscmp(ffd.cFileName, L".."))
				continue;

			char* filename = NULL;
			if (charset_iconv((uint8_t*)ffd.cFileName, (uint8_t**)&filename, CHARSET_WCHAR_T, CHARSET_UTF8))
				continue;

			char* fullpath = dmoz_path_concat_len(path, filename, strlen(path), strlen(filename));
	
			if (ffd.dwFileAttributes & (FILE_ATTRIBUTE_HIDDEN|FILE_ATTRIBUTE_SYSTEM)) {
				free(fullpath);
				free(filename);
				continue;
			}
	
			struct stat st;
			if (os_stat(fullpath, &st) < 0) {
				/* doesn't exist? */
				log_perror(fullpath);
				free(fullpath);
				continue; /* better luck next time */
			}
	
			st.st_mtime = MAX(0, st.st_mtime);
	
			if (ffd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
				dmoz_add_file_or_dir(flist, dlist, fullpath, filename, &st, 0);
			} else if (ffd.dwFileAttributes == INVALID_FILE_ATTRIBUTES) {
				free(fullpath);
				free(filename);
			} else {
				dmoz_add_file(flist, fullpath, filename, &st, 1);
			}
		} while (FindNextFileW(find, &ffd));

		FindClose(find);
		
		DWORD err = GetLastError();
		if (err != ERROR_NO_MORE_FILES)
			return -1;

		add_platform_dirs(path, flist, dlist);
	} else if (attrib == INVALID_FILE_ATTRIBUTES) {
		free(path_w);
		return -1;
	} else {
		free(path_w);
		/* file? probably.
		 * make sure when editing this code to keep it in sync
		 * with the below code for non-Windows platforms */
		if (!load_library || !load_library(path, flist, dlist)) {
			char* ptr = get_parent_directory(path);
			if (ptr)
				dmoz_add_file_or_dir(flist, dlist, ptr, str_dup("."), NULL, -10);
		}
	}

	/* sort */
	dmoz_sort(flist, dlist);

	return 0;
#else
	DIR *dir;
	struct dirent *ent;
	char *ptr;
	struct stat st;
	int pathlen, namlen, lib = 0, err = 0;

	if (!path || !*path)
		path = FAILSAFE_PATH;
	pathlen = strlen(path);

#ifdef SCHISM_WII
	/* awful hack: libfat's file reads bail if a device is given without a slash. */
	if (strchr(path, ':') != NULL && strchr(path, '/') == NULL) {
		int i;
		for (i = 0; devices[i]; i++) {
			if (strncmp(path, devices[i], pathlen) == 0) {
				path = devices[i];
				break;
			}
		}
	}
#endif
	dir = opendir(path);
	if (dir) {
		while ((ent = readdir(dir)) != NULL) {
			namlen = _D_EXACT_NAMLEN(ent);
			/* ignore hidden/backup files (TODO: make this code more portable;
			some OSes have different ideas of whether a file is hidden) */

			if (ent->d_name[0] == '.')
				continue;

			if (ent->d_name[namlen - 1] == '~')
				continue;

			ptr = dmoz_path_concat_len(path, ent->d_name, pathlen, namlen);

			if (os_stat(ptr, &st) < 0) {
				/* doesn't exist? */
				log_perror(ptr);
				free(ptr);
				continue; /* better luck next time */
			}
			if (st.st_mtime < 0) st.st_mtime = 0;
			if (S_ISDIR(st.st_mode))
				dmoz_add_file_or_dir(flist, dlist, ptr, str_dup(ent->d_name), &st, 0);
			else if (S_ISREG(st.st_mode))
				dmoz_add_file(flist, ptr, str_dup(ent->d_name), &st, 1);
			else
				free(ptr);
		}
		closedir(dir);
	} else if (errno == ENOTDIR) {
		/* oops, it's a file! -- load it as a library */
		if (load_library && load_library(path, flist, dlist) != 0)
			err = errno;
		else
			lib = 1;
	} else {
		/* opendir failed? that's unpossible! */
		err = errno;
	}

	/* more directories!
	 * If this is actually a file, make a fake "." that actually points to the directory.
	 * If something weird happens when trying to get the directory name, this falls back
	 * to add_platform_dirs to keep from getting "stuck". */
	if (lib && (ptr = get_parent_directory(path)) != NULL)
		dmoz_add_file_or_dir(flist, dlist, ptr, str_dup("."), NULL, -10);
	else
		add_platform_dirs(path, flist, dlist);

	/* finally... sort it */
	dmoz_sort(flist, dlist);

	if (err) {
		errno = err;
		return -1;
	} else {
		return 0;
	}
#endif
}

/* --------------------------------------------------------------------------------------------------------- */

enum {
	FINF_SUCCESS = (0),     /* nothing wrong */
	FINF_UNSUPPORTED = (1), /* unsupported file type */
	FINF_EMPTY = (2),       /* zero-byte-long file */
	FINF_ERRNO = (-1),      /* check errno */
};

static int file_info_get(dmoz_file_t *file)
{
	slurp_t *t;
	const fmt_read_info_func *func;

	if (file->filesize == 0)
		return FINF_EMPTY;
	t = slurp(file->path, NULL, file->filesize);
	if (t == NULL)
		return FINF_ERRNO;
	file->artist = NULL;
	file->title = NULL;
	file->smp_defvol = 64;
	file->smp_gblvol = 64;
	for (func = read_info_funcs; *func; func++) {
		if ((*func) (file, t->data, t->length)) {
			if (file->artist)
				trim_string(file->artist);
			if (file->title == NULL)
				file->title = str_dup(""); /* or the basename? */
			trim_string(file->title);
			break;
		}
	}
	unslurp(t);
	return file->title ? FINF_SUCCESS : FINF_UNSUPPORTED;
}

/* return: 1 on success, 0 on error. in either case, it fills the data in with *something*. */
int dmoz_filter_ext_data(dmoz_file_t *file)
{
	int ret;

	if ((file->type & TYPE_EXT_DATA_MASK)
	|| (file->type == TYPE_DIRECTORY)) {
		/* nothing to do */
		return 1;
	}
	ret = file_info_get(file);
	switch (ret) {
	case FINF_SUCCESS:
		return 1;
	case FINF_UNSUPPORTED:
		file->description = "Unsupported file format"; /* used to be "Unsupported module format" */
		break;
	case FINF_EMPTY:
		file->description = "Empty file";
		break;
	case FINF_ERRNO:
		/* It would be nice to use the error string for the description, but there doesn't seem to be
		any easy/portable way to do that without dynamically allocating it (since strerror might
		return a static buffer), and str_dup'ing EVERY description is kind of a waste of memory. */
		log_perror(file->base);
		file->description = "File error";
		break;
	default:
		/* shouldn't ever happen */
		file->description = "Internal error";
		break;
	}
	file->type = TYPE_UNKNOWN;
	file->title = str_dup("");
	return 0;
}

/* same as dmoz_filter_ext_data, except without the filtering effect when used with dmoz_filter_filelist */
int dmoz_fill_ext_data(dmoz_file_t *file)
{
	dmoz_filter_ext_data(file);
	return 1;
}

