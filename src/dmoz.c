/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * URL: http://rigelseven.com/schism/
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

#define NEED_DIRENT
#define NEED_TIME
#include "headers.h"

#include "it.h"
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

/* The type list should be arranged so that the types with the most specific checks are first, and the vaguest
ones are down at the bottom. This is to ensure that some lousy type doesn't "steal" files of a different type.
For example, if IT came before S3M, any S3M file starting with "IMPM" (which is unlikely, but possible, and in
fact quite easy to do) would be picked up by the IT check. In fact, Impulse Tracker itself has this problem.

Also, a format that might need to do a lot of work to tell if a file is of the right type (i.e. the MDL format
practically requires reading through the entire file to find the title block) should be down farther on the
list for performance purposes.

Don't rearrange the formats that are already here unless you have a VERY good reason to do so. I spent a good
3-4 hours reading all the format specifications, testing files, checking notes, and trying to break the
program by giving it weird files, and I'm pretty sure that this ordering won't fail unless you really try
doing weird stuff like hacking the files, but then you're just asking for trouble. ;) */

#define READ_INFO(t) fmt_##t##_read_info

static const fmt_read_info_func read_info_funcs[] = {
	/* 669 has lots of checks to compensate for a really crappy 2-byte magic. (It's even a common English
	word ffs... "if"?!) Still, it's better than STM. The only reason this is first is because the position
	of the SCRM magic lies within the 669 message field, and the 669 check is much more complex (and thus
	more likely to be right). */
	READ_INFO(669),

	/* Since so many programs have added noncompatible extensions to the mod format, there are about 30
	strings to compare against for the magic. Also, there are special cases for WOW files, which even
	share the same magic as plain ProTracker, but are quite different; there are some really nasty
	heuristics to detect these... ugh, ugh, ugh. However, it has to be above the formats with the magic
	at the beginning... */
	READ_INFO(mod),

	/* S3M needs to be before a lot of stuff. */
	READ_INFO(s3m),
	/* FAR and S3M have different magic in the same place, so it doesn't really matter which one goes
	where. I just have S3M first since it's a more common format. */
	READ_INFO(far),

	/* These next formats have their magic at the beginning of the data, so none of them can possibly
	conflict with other ones. I've organized them pretty much in order of popularity. */
	READ_INFO(xm),
	/* There's a bit of weirdness with some IT files (including "Acid Dreams" by Legend, a demo song for
	version 2.08) requiring two different checks and three memcmp's. However, since it's so widely used
	<opinion>'cuz Impulse Tracker owns</opinion>, I'm putting it up here anyway. */
	READ_INFO(it),
	READ_INFO(mt2),
	READ_INFO(mtm),
	READ_INFO(ntk),
#ifdef USE_NON_TRACKED_TYPES
	READ_INFO(sid),  /* 6581 0wnz j00! */
#endif
	READ_INFO(mdl),

#ifdef USE_SAMPLE_TYPES
	READ_INFO(its),
	READ_INFO(au),
	READ_INFO(aiff),
#endif

	READ_INFO(ult),
	READ_INFO(liq),
	/* I have NEVER seen any of these next three */
	READ_INFO(ams),
	READ_INFO(f2r),
	READ_INFO(dtm),  /* not sure about the placement here */
	
	READ_INFO(imf), /* not sure here either */
	
	/* bleh */
#if defined(USE_NON_TRACKED_TYPES) && defined(HAVE_VORBIS)
	READ_INFO(ogg),
#endif

	/* STM seems to have a case insensitive magic string with several possible values, and only one byte
	is guaranteed to be the same in the whole file... yeagh. */
	READ_INFO(stm),

	/* An ID3 tag could actually be anywhere in an MP3 file, and there's no guarantee that it even exists
	at all. I might move this toward the top if I can figure out how to identify an MP3 more precisely. */
#ifdef USE_NON_TRACKED_TYPES
	READ_INFO(mp3),
#endif

	NULL /* This needs to be at the bottom of the list! */
};

#undef READ_INFO

/* --------------------------------------------------------------------------------------------------------- */
/* path string hacking */

/* This function should:
	- strip out any parent directory references ("/sheep/../goat" => "/goat")
	- switch slashes to backslashes for MS systems ("c:/winnt" => "c:\\winnt")
	- condense multiple slashes into one ("/sheep//goat" => "/sheep/goat")
	- remove any trailing slashes
It shouldn't really check the path to see if it exists, but it does. :P
return: the new path, or NULL if there was a problem (i.e. it wasn't recognisable as a path) */
char *dmoz_path_normal(const char *path)
{
	char buf[PATH_MAX], *ret;

	/* FIXME: don't use realpath! */
	if (realpath(path, buf) == NULL)
		return NULL;

	ret = calloc(strlen(buf) + 2, sizeof(char));
	strcpy(ret, buf);
	return ret;
}

char *dmoz_path_concat(const char *a, const char *b)
{
	return dmoz_path_concat_len(a, b, strlen(a), strlen(b));
}

char *dmoz_path_concat_len(const char *a, const char *b, int alen, int blen)
{
	char *ret = calloc(alen + blen + 2, sizeof(char));

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
		flist->files = malloc(FILE_BLOCK_SIZE * sizeof(dmoz_filelist_t *));
	} else {
		flist->alloc_size *= 2;
		flist->files = realloc(flist->files, flist->alloc_size * sizeof(dmoz_filelist_t *));
	}
}

static void allocate_more_dirs(dmoz_dirlist_t *dlist)
{
	if (dlist->alloc_size == 0) {
		dlist->alloc_size = DIR_BLOCK_SIZE;
		dlist->dirs = malloc(DIR_BLOCK_SIZE * sizeof(dmoz_dirlist_t *));
	} else {
		dlist->alloc_size *= 2;
		dlist->dirs = realloc(dlist->dirs, dlist->alloc_size * sizeof(dmoz_dirlist_t *));
	}
}

static void free_file(dmoz_file_t *file)
{
	if (!file)
		return;
	free(file->path);
	free(file->base);
	if (file->type & TYPE_EXT_DATA_MASK) {
		if (file->artist)
			free(file->artist);
		free(file->title);
		/* if (file->sample) {
			if (file->sample->data)
				song_sample_free(file->sample->data);
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

/* --------------------------------------------------------------------------------------------------------- */
/* adding to the lists */

dmoz_file_t *dmoz_add_file(dmoz_filelist_t *flist, char *path, char *base, struct stat *st, int sort_order)
{
	dmoz_file_t *file = calloc(1, sizeof(dmoz_file_t));

	file->path = path;
	file->base = base;
	file->sort_order = sort_order;
	
	if (st == NULL || S_ISDIR(st->st_mode)) {
		file->type = TYPE_DIRECTORY;
		/* have to fill everything in for directories */
		file->description = DESCR_DIRECTORY;
		file->title = strdup(TITLE_DIRECTORY);
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
	dmoz_dir_t *dir = calloc(1, sizeof(dmoz_dir_t));

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

static int qsort_cmp_file(const void *_a, const void *_b)
{
	const dmoz_file_t *a = *(const dmoz_file_t **) _a;
	const dmoz_file_t *b = *(const dmoz_file_t **) _b;
	
	if (a->sort_order < b->sort_order)
		return -1; /* a goes first */
	if (b->sort_order < a->sort_order)
		return 1; /* b goes first */
	return strverscmp(a->base, b->base);
}

static int qsort_cmp_dir(const void *_a, const void *_b)
{
	const dmoz_dir_t *a = *(const dmoz_dir_t **) _a;
	const dmoz_dir_t *b = *(const dmoz_dir_t **) _b;

	/* Same code as above, but a different structure. Actually, since dmoz_file_t is just a superset of
	dmoz_dir_t, I could use the same function, but doing so would reduce flexibility, in case I decided
	to rearrange one of them for some reason. */
	if (a->sort_order < b->sort_order)
		return -1; /* a goes first */
	if (b->sort_order < a->sort_order)
		return 1; /* b goes first */
	return strverscmp(a->base, b->base);
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
					 *	pString[i] = pTemp[i + 1]; */
					memcpy(pString, pTemp + 1, pTemp[0]);
					pString[pTemp[0]] = '\0';
					dmoz_add_file_or_dir(flist, dlist, pString, strdup(pString),
							     NULL, order++);
				}
			}
		}
		IDOS->UnLockDosList(LDF_VOLUMES);
	}
#elif defined(WIN32)
	/* I have looked at the MSDN example code for getting a list of drives, and it's completely
	disgusting. If someone else wants to do this, go ahead; I'm certainly not going to. */
	dmoz_add_file_or_dir(flist, dlist, strdup("A:\\"), strdup("A:\\"), NULL, -1024);
	dmoz_add_file_or_dir(flist, dlist, strdup("C:\\"), strdup("C:\\"), NULL, -1023);
	dmoz_add_file_or_dir(flist, dlist, strdup("D:\\"), strdup("D:\\"), NULL, -1022);
#else /* assume POSIX */
	dmoz_add_file_or_dir(flist, dlist, strdup("/"), strdup("/"), NULL, -1024);
	/* home directory? */
#endif /* platform */
	
	ptr = get_parent_directory(path);
	if (ptr)
		dmoz_add_file_or_dir(flist, dlist, ptr, strdup(".."), NULL, -10);
}

/* --------------------------------------------------------------------------------------------------------- */

/* on success, this will fill the lists and return 0. if something goes
wrong, it adds a 'stub' entry for the root directory, and returns -1. */
int dmoz_read(const char *path, dmoz_filelist_t *flist, dmoz_dirlist_t *dlist)
{
	DIR *dir;
	struct dirent *ent;
	char *ptr;
	struct stat st;
	int pathlen, namlen, err = 0;

	dir = opendir(path);
	if (dir) {
		pathlen = strlen(path);
		while ((ent = readdir(dir)) != NULL) {
			namlen = _D_EXACT_NAMLEN(ent);
			/* ignore hidden/backup files (TODO: make this code more portable;
			some OSes have different ideas of whether a file is hidden) */
			if (ent->d_name[0] == '.' || ent->d_name[namlen - 1] == '~')
				continue;
			ptr = dmoz_path_concat_len(path, ent->d_name, pathlen, namlen);
			if (stat(ptr, &st) < 0) {
				/* doesn't exist? */
				perror(ptr);
				free(ptr);
				continue; /* better luck next time */
			}
			if (S_ISDIR(st.st_mode))
				dmoz_add_file_or_dir(flist, dlist, ptr, strdup(ent->d_name), &st, 0);
			else if (S_ISREG(st.st_mode))
				dmoz_add_file(flist, ptr, strdup(ent->d_name), &st, 1);
			else
				free(ptr);
		}
		closedir(dir);
	} else if (errno == ENOTDIR) {
		/* oops, it's a file! -- load it as a library */
		if (dmoz_read_sample_library(path, flist, dlist) != 0)
			err = errno;
	} else {
		/* opendir failed? that's unpossible! */
		err = errno;
	}

	/* more directories! */
	add_platform_dirs(path, flist, dlist);

	/* finally... sort it */
	qsort(flist->files, flist->num_files, sizeof(dmoz_file_t *), qsort_cmp_file);
	if (dlist)
		qsort(dlist->dirs, dlist->num_dirs, sizeof(dmoz_dir_t *), qsort_cmp_dir);

	if (err) {
		errno = err;
		return -1;
	} else {
		return 0;
	}
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
	for (func = read_info_funcs; *func; func++) {
		if ((*func) (file, t->data, t->length)) {
			if (file->artist)
				trim_string(file->artist);
			if (file->title == NULL)
				file->title = strdup(""); /* or the basename? */
			trim_string(file->title);
			break;
		}
	}
	unslurp(t);
	return file->title ? FINF_SUCCESS : FINF_UNSUPPORTED;
}

/* return: 1 on success, 0 on error. in either case, it fills the data in with *something*. */
int dmoz_fill_ext_data(dmoz_file_t *file)
{
	int ret;

	if (file->type & TYPE_EXT_DATA_MASK) {
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
		return a static buffer), and strdup'ing EVERY description is kind of a waste of memory. */
		perror(file->base);
		file->description = "File error";
		break;
	default:
		/* shouldn't ever happen */
		file->description = "Internal error";
		break;
	}
	file->type = TYPE_UNKNOWN;
	file->title = strdup("");
	return 0;
}
