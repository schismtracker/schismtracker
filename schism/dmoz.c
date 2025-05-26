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
#include "config-parser.h"
#include "config.h"
#include "charset.h"
#include "song.h"
#include "dmoz.h"
#include "slurp.h"
#include "util.h"
#include "osdefs.h"
#include "loadso.h"
#include "mem.h"
#include "str.h"

#include "backend/dmoz.h"

#include "fmt.h"

#include <unistd.h>
#include <ctype.h>

#ifdef __amigaos4__
# include <proto/dos.h>
#endif

#if defined(SCHISM_WIN32)
# include <windows.h>
# include <winbase.h>
# include <shlobj.h>
# include <direct.h>
#elif defined(SCHISM_MACOS)
# include <Files.h>
# include <Folders.h>
# include <Resources.h>
# include <Script.h>

# include "macos-dirent.h"
#elif defined(SCHISM_OS2)
# include <dirent.h> // TODO we shouldn't need this

# define INCL_DOSFILEMGR
# include <os2.h>
#elif defined(SCHISM_XBOX)
# include <windows.h>
# include <winbase.h>
# include <shlobj.h>
#elif defined(HAVE_DIRENT_H)
# include <dirent.h>
#else
# error Unknown platform. Add code for this platform, or make a wrapper for dirent.h.
#endif

#ifdef SCHISM_WII
#include <sys/dir.h>
// isfs is pretty much useless, but it might be interesting to browse it I guess
static const char *devices[] = {
	"sd:/",
	"isfs:/",
	NULL,
};
#elif defined(SCHISM_WIIU)
static const char *devices[] = {
	"sd:/",
	/* There are a lot of possible USB devices. */
	"usb1:/",  "usb2:/",  "usb3:/",  "usb4:/",
	"usb5:/",  "usb6:/",  "usb7:/",  "usb8:/",
	"usb9:/",  "usb10:/", "usb11:/", "usb12:/",
	"usb13:/", "usb14:/", "usb15:/", "usb16:/",
	"usb17:/", "usb18:/", "usb19:/", "usb20:/",
	"usb21:/", "usb22:/", "usb23:/", "usb24:/",
	"usb25:/", "usb26:/", "usb27:/", "usb28:/",
	"usb29:/", "usb30:/", "usb31:/", "usb32:/",
	"usb33:/", "usb34:/", "usb35:/", "usb36:/",
	"usb37:/", "usb38:/", "usb39:/", "usb40:/",
	"usb41:/", "usb42:/", "usb43:/", "usb44:/",
	"usb45:/", "usb46:/", "usb47:/", "usb48:/",
	"usb49:/", "usb50:/", "usb51:/", "usb52:/",
	"usb53:/", "usb54:/", "usb55:/", "usb56:/",
	"usb57:/", "usb58:/", "usb59:/", "usb60:/",
	"usb61:/", "usb62:/", "usb63:/", "usb64:/",
	"usb65:/", "usb66:/", "usb67:/", "usb68:/",
	"usb69:/", "usb70:/", "usb71:/", "usb72:/",
	"usb73:/", "usb74:/", "usb75:/", "usb76:/",
	"usb77:/", "usb78:/", "usb79:/", "usb80:/",
	"usb81:/", "usb82:/", "usb83:/", "usb84:/",
	"usb85:/", "usb86:/", "usb87:/", "usb88:/",
	"usb89:/", "usb90:/", "usb91:/", "usb92:/",
	"usb93:/", "usb94:/", "usb95:/", "usb96:/",
	"usb97:/", "usb98:/",
	"slccmpt:/",
	"slc:/",
	"mlc:/",
	NULL,
};
#endif

#if defined(__amigaos4__)
# define FALLBACK_DIR "." /* not used... */
#elif defined(SCHISM_WIN32) || defined(SCHISM_OS2)
# define FALLBACK_DIR "C:\\"
#elif defined(SCHISM_XBOX)
# define FALLBACK_DIR "E:\\"
#elif defined(SCHISM_WII)
# define FALLBACK_DIR "isfs:/" // always exists, seldom useful
#elif defined(SCHISM_WIIU)
# define FALLBACK_DIR "fs:/" // useless but "valid"
#elif defined(SCHISM_MACOS)
// resolves to the current working directory.
// fairly useless but "always" exists ;)
# define FALLBACK_DIR ":"
#else /* POSIX? */
# define FALLBACK_DIR "/"
#endif

// backend for stuff
static const schism_dmoz_backend_t *backend = NULL;

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
_DECL_CMP(strverscmp)
_DECL_CMP(strcaseverscmp)
static int dmoz_fcmp_timestamp(const dmoz_file_t *a, const dmoz_file_t *b);

static dmoz_fcmp_t dmoz_file_cmp = dmoz_fcmp_strcaseverscmp;
static dmoz_dcmp_t dmoz_dir_cmp = dmoz_dcmp_strcaseverscmp;

static struct {
	const char *name;
	dmoz_fcmp_t fcmp;
	dmoz_dcmp_t dcmp;
} compare_funcs[] = {
	{"strcmp", dmoz_fcmp_strcmp, dmoz_dcmp_strcmp},
	{"strcasecmp", dmoz_fcmp_strcasecmp, dmoz_dcmp_strcasecmp},
	{"strverscmp", dmoz_fcmp_strverscmp, dmoz_dcmp_strverscmp},
	{"timestamp", dmoz_fcmp_timestamp, dmoz_dcmp_strverscmp},
	{"strcaseverscmp", dmoz_fcmp_strcaseverscmp, dmoz_dcmp_strcaseverscmp},
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
	filen = filen ? dmoz_path_get_basename(filen) : NULL;
	dirn = dirn ? dmoz_path_get_basename(dirn) : NULL;
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
/* get info about paths */

#ifdef SCHISM_MACOS
/* Okay this is a bit dumb:
 *
 * Classic Mac OS's standard file API doesn't take in paths.
 * This is similar to the way the Wii U's filesystem API works,
 * and both suck and should be killed with fire.
 *
 * unfortunately, unlike the Wii U, there is no compatibility
 * layer provided by the standard library we're using, which
 * means we have to do all of this stupid conversion ourselves.
 *
 *  - paper */
int dmoz_path_from_fsspec(const void *pvref, char **path)
{
	OSErr err = noErr;
	FSSpec spec = *(const FSSpec *)pvref;

	// just dynamically allocate, whatever
	*path = mem_calloc(1, sizeof(char));

	// the length of path INCLUDING the nul terminator
	size_t path_len = 1;

	short index = 0;
	do {
		CInfoPBRec cinfo;
		cinfo.dirInfo.ioNamePtr = spec.name;
		cinfo.dirInfo.ioVRefNum = spec.vRefNum;
		cinfo.dirInfo.ioFDirIndex = index;
		cinfo.dirInfo.ioDrDirID = spec.parID;
		err = PBGetCatInfoSync(&cinfo);
		
		if (err == noErr) {
			unsigned char name_len = spec.name[0];

			// reallocate and move existing data
			*path = mem_realloc(*path, path_len + name_len + 1);
			memmove(*path + name_len + 1, *path, path_len);
			path_len += name_len + 1;

			// copy to the start of the buffer
			memcpy(*path + 1, &spec.name[1], name_len);
			(*path)[0] = ':';

			// from here on out, ignore the input file name
			index = -1;
			
			// move up to the parent
			spec.parID = cinfo.dirInfo.ioDrParID;
		}
	} while (err == noErr && spec.parID != fsRtParID);

	// remove the preceding colon
	memmove(*path, *path + 1, --path_len);

	// paranoia...
	(*path)[path_len] = '\0';

	// we're done here
	return (err == noErr);
}

int dmoz_path_to_fsspec(const char *path, void *pvref)
{
	OSErr err = noErr;
	FSSpec spec = *(const FSSpec *)pvref;
	Str255 name; // Must be long enough for a partial pathname consisting of two segments (64 bytes)
	
	const char* p = path;
	const char* pEnd;
	size_t segLen;
	// Find the end of the path segment
	for (pEnd = ++p; *pEnd && *pEnd != ':'; ++pEnd);
	segLen = pEnd - p;
	
	// Try to find a volume that matches this name
	for (ItemCount volIndex = 1; err == noErr; ++volIndex)
	{
		FSVolumeRefNum volRefNum;
		HParamBlockRec hfsParams;
		name[0] = 0;
		hfsParams.volumeParam.ioNamePtr  = name;
		hfsParams.volumeParam.ioVRefNum  = 0;
		hfsParams.volumeParam.ioVolIndex = volIndex;
		err = PBHGetVInfoSync(&hfsParams);
		volRefNum = hfsParams.volumeParam.ioVRefNum;
	
		// Compare against our path segment
		if (err == noErr && segLen == StrLength(name)) {
			// FIXME FIXME READ ALL ABOUT IT:
			// HFS paths are not in codepage 437. The only reason it's like this here
			// is because we need a simple mostly-ASCII-compatible 8-bit fixed width
			// encoding, which codepage 437 just happens to be.

			if (!charset_strncasecmp(&name[1], CHARSET_CP437, path, CHARSET_UTF8, name[0])) {
				// we found our volume: fill in the spec
				err = FSMakeFSSpec(volRefNum, fsRtDirID, NULL, &spec);
				break;
			}
		}
	}
	
	p = pEnd;
	
	// okay, now we have the root directory.
	// NOW, we have to parse the rest of the path.
	// I have no idea if '.' or '..' are actual real filenames on Mac OS.
	while (err == noErr && *p) {
		switch (*p) {
		case ':': // skip any path separators
			++p;
			break;
	
		case '.': // "current directory" or "parent directory"
			if (p[1] == '/' || p[1] == 0) { // "current directory"
				++p;
				break;
			} else if (p[1] == '.' && (p[2] == '/' || p[2] == '\0')) { // "parent directory"
				p += 2;  // Get the parent of our parent

				CInfoPBRec catInfo;
				catInfo.dirInfo.ioNamePtr = NULL;
				catInfo.dirInfo.ioVRefNum = spec.vRefNum;
				catInfo.dirInfo.ioFDirIndex = -1;
				catInfo.dirInfo.ioDrDirID = spec.parID;
				err = PBGetCatInfoSync(&catInfo);

				// Check that we didn't go too far
				if (err != noErr || catInfo.dirInfo.ioDrParID == fsRtParID)
					return false;

				// update the FSSpec
				if (err == noErr)
					err = FSMakeFSSpec(spec.vRefNum, catInfo.dirInfo.ioDrDirID, NULL, &spec);

				break;
			}
			SCHISM_FALLTHROUGH;
		default: {
			// Find the end of the path segment
			for (pEnd = p; *pEnd && *pEnd != ':'; ++pEnd) ;
			segLen = pEnd - p;

			// Check for name length overflow (XXX: what is this magic constant)
			if (segLen > 31)
					return 0;

			// Make a partial pathname from our current spec to the new object
			unsigned char *partial = &name[1];

			// Partial filename leads with :
			*partial++ = ':';
			const unsigned char *specName = spec.name; // Copy in spec name
			for (int cnt = *specName++; cnt > 0; --cnt) // vulgarities in my for loop
				*partial++ = *specName++;

			*partial++ = ':'; // Separator
			while (p != pEnd)
				*partial++ = *p++; // copy in the new element
			  
			name[0] = partial - &name[1]; // Set the name length

			// Update the spec
			err = FSMakeFSSpec(spec.vRefNum, spec.parID, name, &spec);
			break;
		}
		}
	}
	
	// copy the result
	memcpy(pvref, &spec, sizeof(spec));

	return err == noErr;
}
#endif

const char *dmoz_path_get_basename(const char *filename)
{
	const char *base = strrchr(filename, DIR_SEPARATOR);
	if (base) {
		/* skip the slash */
		base++;
	}

	if (!base || !*base) {
		/* well, there isn't one, so just return the filename */
		base = filename;
	}

	return base;
}

const char *dmoz_path_get_extension(const char *filename)
{
	filename = dmoz_path_get_basename(filename);

	const char *extension = strrchr(filename, '.');
	if (!extension) {
		/* no extension? bummer. point to the \0 at the end of the string. */
		extension = strchr(filename, '\0');
	}

	return extension;
}

/* this function should output as you expect
 * e.g. if input is / it returns NULL,
 *      if input is /home it returns / and
 *      if input is /home/ it returns /
 *
 * the equivalent windows paths also work here
*/
char *dmoz_path_get_parent_directory(const char *dirname)
{
	if (!dirname || !dirname[0])
		return NULL;

	/* need the root path, including the separator */
	const char* root = strchr(dirname, DIR_SEPARATOR);
	if (!root)
		return NULL;
	root++;

	/* okay, now we need to find the final token */
	const char* pos = root + strlen(root) - 1;
	if (IS_DIR_SEPARATOR(*pos)) /* strip off an excess separator, if any */
		pos--;

	while (--pos > root) {
		if (IS_DIR_SEPARATOR(*pos))
			break;
	}

	ptrdiff_t n = pos - dirname;

	/* sanity check */
	if (n <= 0)
		return NULL;

	char *ret = mem_alloc((n + 1) * sizeof(char));
	memcpy(ret, dirname, n * sizeof(char));
	ret[n] = '\0';

	if (!strcmp(dirname, ret) || !ret[0]) {
		free(ret);
		return NULL;
	}

	return ret;
}

/* 0 = success, !0 = failed (check errno) */
int dmoz_path_make_backup(const char *filename, int numbered)
{
	char *buf = mem_alloc(strlen(filename) + 7 + 1);

	if (numbered) {
		/* If some crazy person needs more than 65536 backup files,
		   they probably have more serious issues to tend to. */
		int n = 1, ret;
		do {
			sprintf(buf, "%s.%d~", filename, n++);
			ret = dmoz_path_rename(filename, buf, 0);
		} while (ret != 0 && errno == EEXIST && n < 65536);
		free(buf);
		return ret;
	} else {
		int ret;
		sprintf(buf, "%s~", filename);
		ret = dmoz_path_rename(filename, buf, 1);
		free(buf);
		return ret;
	}
}

#if defined(SCHISM_WIN32)
# define DMOZ_PATH_RENAME_IMPL(TYPE, CHARSET, SUFFIX, CHMOD) \
	static int dmoz_path_rename##SUFFIX(const char *old, const char *new, int overwrite) \
	{ \
		int res = -1; \
	\
		TYPE *old_w = NULL, *new_w = NULL; \
		if (!charset_iconv(old, &old_w, CHARSET_UTF8, CHARSET, SIZE_MAX) \
			&& !charset_iconv(new, &new_w, CHARSET_UTF8, CHARSET, SIZE_MAX)) { \
			if (MoveFile##SUFFIX(old_w, new_w)) { \
				win32_filecreated_callback(new); \
				res = 0; \
			} else if (overwrite) { \
				if (MoveFileEx##SUFFIX(old_w, new_w, MOVEFILE_REPLACE_EXISTING)) { \
					/* no callback here; file already existed */ \
					res = 0; \
				} else { \
					CHMOD(new_w, 0777); \
					CHMOD(old_w, 0777); \
	\
					SetFileAttributes##SUFFIX(new_w, FILE_ATTRIBUTE_NORMAL); \
					SetFileAttributes##SUFFIX(new_w, FILE_ATTRIBUTE_TEMPORARY); \
	\
					/* wee */ \
					if (MoveFile##SUFFIX(old_w, new_w) || (DeleteFile##SUFFIX(new_w) && MoveFile##SUFFIX(old_w, new_w))) { \
						win32_filecreated_callback(new); \
						res = 0; \
					} \
				} \
			} \
		} \
	\
		free(new_w); \
		free(old_w); \
	\
		return res; \
	}

# ifdef SCHISM_WIN32_COMPILE_ANSI
DMOZ_PATH_RENAME_IMPL(CHAR, CHARSET_ANSI, A, _chmod)
# endif
DMOZ_PATH_RENAME_IMPL(WCHAR, CHARSET_WCHAR_T, W, _wchmod)

# undef DMOZ_PATH_RENAME_IMPL

#endif

/* 0 = success, !0 = failed (check errno) */
int dmoz_path_rename(const char *old, const char *new, int overwrite)
{
#if defined(SCHISM_WIN32)
	// this is a mess, extracted from the old code
	// schism had in util.c
	//
	// much of this is probably unnecessary for
	// newer versions of windows, but I've kept
	// it here for builds compiled for ancient
	// windows.
	int ret = -1;
	DWORD em = SetErrorMode(0);

# ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		ret = dmoz_path_renameA(old, new, overwrite);
	} else
# endif
	{
		ret = dmoz_path_renameW(old, new, overwrite);
	}

	switch (GetLastError()) {
	case ERROR_ALREADY_EXISTS:
	case ERROR_FILE_EXISTS:
		if (!overwrite)
			errno = EEXIST;
		break;
	default:
		break;
	}

	// reset this...
	SetErrorMode(em);

	return ret;
#elif defined(SCHISM_XBOX)
	wchar_t *old_w, *new_w;
	int ret;

	old_w = charset_iconv_easy(old, CHARSET_UTF8, CHARSET_WCHAR_T);
	new_w = charset_iconv_easy(old, CHARSET_UTF8, CHARSET_WCHAR_T);

	if (!old_w || !new_w) {
		free(old_w);
		free(new_w);
		return -1;
	}
	
	free(old_w);
	free(new_w);

	return ret;
#elif defined(SCHISM_MACOS)
	// This is stupid, but it's what MacPerl does, so I guess
	// this is probbaly the only way we can pull this off
	// sanely.
	//
	// Note: there is in fact a HFS function to do exactly this
	// (namely PBHMoveRename) but unfortunately I think it only
	// actually works on network shares which is completely
	// useless for us.
	OSErr err = noErr;
	unsigned char pold[256], pnew[256];

	int truncated;
	str_to_pascal(old, pold, &truncated);
	if (truncated) {
		errno = ENAMETOOLONG;
		return -1;
	}

	str_to_pascal(new, pnew, &truncated);
	if (truncated) {
		errno = ENAMETOOLONG;
		return -1;
	}

	FSSpec old_spec, new_spec;

	err = FSMakeFSSpec(0, 0, pold, &old_spec);
	switch (err) {
	case noErr:
		break;
	case nsvErr:
	case fnfErr:
		errno = ENOENT;
		return -1;
	default:
		log_appendf(4, "Unknown FSMakeFSSpec error: %d", (int)err);
		return -1;
	}

	err = FSMakeFSSpec(0, 0, pnew, &new_spec);
	if (err == fnfErr) {
		// Create the output file
		err = FSpCreate(&new_spec, '?\??\?', 'TEXT', smSystemScript);
		if (err != noErr) {
			log_appendf(4, "FSpCreate: %d", (int)err);
			return -1;
		}

		FSpCreateResFile(&new_spec, '?\??\?', 'TEXT', smSystemScript);
		err = ResError();
		if (err != noErr) {
			log_appendf(4, "FSpCreateResFile: %d\n", (int)err);
			return -1;
		}
	} else if (overwrite && err == noErr) {
		// do nothing, exchange the contents and delete
	} else {
		// handle error value and set error value appropriately
		switch (err) {
		case noErr:
			errno = EEXIST;
			return -1;
		case nsvErr:
			errno = ENOTDIR;
			return -1;
		default:
			log_appendf(4, "Unknown FSMakeFSSpec error: %d", (int)err);
			return -1;
		}
	}

	// Exchange the data in the two files
	err = FSpExchangeFiles(&old_spec, &new_spec);
	if (err != noErr) {
		log_appendf(4, "FSpExchangeFiles: %d", (int)err);
		return -1;
	}

	err = FSpDelete(&old_spec);
	if (err != noErr) {
		log_appendf(4, "FSpDelete: %d", (int)err);
		return -1;
	}

	return 0;
#else
# ifdef SCHISM_OS2
	// XXX maybe this should be the regular implementation too?
	if (!overwrite && (access(new, F_OK) == -1)) {
		errno = EEXIST;
		return -1;
	}
# else
	if (!overwrite) {
		if (link(old, new) == -1)
			return -1;

		/* This can occur when people are using a system with
		 * broken link() semantics, or if the user can create files
		 * that he cannot remove. these systems are decidedly not POSIX.1
		 * but they may try to compile schism, and we won't know they
		 * are broken unless we warn them.
		*/
		if (unlink(old) == -1)
			fprintf(stderr, "link() succeeded, but unlink() failed. something is very wrong\n");

		return 0;
	} else
# endif
	{
		int r = rename(old, new);

		/* Broken rename()? Try smashing the old file first,
		 * and hope *that* doesn't also fail ;) */
		if (r != 0 && errno == EEXIST && (remove(old) != 0 || rename(old, new) == -1))
			return -1;

		return r;
	}
#endif
}

// recursive mkdir (equivalent to `mkdir -p`)
int dmoz_path_mkdir_recursive(const char *path, int mode)
{
	size_t i;
	int first = 0; // ugly hack: skip the first separator

	char *npath = dmoz_path_normal(path);
	if (!npath) {
		printf("nope\n");
		return -1;
	}

	for (i = 0; npath[i]; i++) {
		struct stat st;

		if (!IS_DIR_SEPARATOR(npath[i]))
			continue;

		if (!first) {
			first = 1;
			continue;
		}

		// cut
		npath[i] = '\0';

		printf("%s\n", npath);

		if (os_stat(npath, &st) < 0) {
			// path doesn't exist
			if (os_mkdir(npath, mode)) {
				free(npath);
				// errno is already useful here
				return -1;
			}
		} else {
			// path exists already, make sure it's a directory
			if (!S_ISDIR(st.st_mode)) {
				free(npath);
				errno = ENOTDIR;
				return -1;
			}
		}

		// paste
		npath[i] = DIR_SEPARATOR;
	}

	if (os_mkdir(npath, mode)) {
		free(npath);
		// errno is already useful here
		return -1;
	}

	free(npath);

	return 0;
}

int dmoz_path_is_file(const char *filename)
{
	struct stat buf;

	if (os_stat(filename, &buf) == -1) {
		/* Well, at least we tried. */
		return 0;
	}

	return S_ISREG(buf.st_mode);
}

int dmoz_path_is_directory(const char *filename)
{
	struct stat buf;

	if (os_stat(filename, &buf) == -1) {
		/* Well, at least we tried. */
		return 0;
	}

	return S_ISDIR(buf.st_mode);
}

unsigned long long dmoz_path_get_file_size(const char *filename) {
	struct stat buf;

	if (os_stat(filename, &buf) < 0)
		return EOF;

	if (S_ISDIR(buf.st_mode)) {
		errno = EISDIR;
		return EOF;
	}
	return buf.st_size;
}

/* --------------------------------------------------------------------------------------------------------- */
/* directories... */

#ifdef SCHISM_WIN32
// Functions that only exist on new systems; see dmoz_win32_get_csidl_directory
static HRESULT (WINAPI *WIN32_SHGetFolderPathW)(HWND hwnd,int csidl,HANDLE hToken,DWORD dwFlags,LPWSTR pszPath) = NULL;
static BOOL (WINAPI *WIN32_SHGetSpecialFolderPathA)(HWND  hwnd, LPSTR pszPath, int csidl, BOOL  fCreate) = NULL;
static BOOL (WINAPI *WIN32_SHGetSpecialFolderPathW)(HWND  hwnd, LPWSTR pszPath, int csidl, BOOL  fCreate) = NULL;
#endif

char *dmoz_get_current_directory(void)
{
#ifdef SCHISM_MACOS
	FSSpec spec;

	if (FSMakeFSSpec(0, 0, NULL, &spec) == noErr) {
		char *path;
		if (dmoz_path_from_fsspec(&spec, &path))
			return path;
	}
#elif defined(SCHISM_XBOX)
	/* nothing */
#elif defined(SCHISM_WIN32)
	{
#define TRY_GETCWD(TYPE, SUFFIX, CHARSET) \
	do { \
		DWORD bufsize; \
		TYPE *buf; \
		char *utf8; \
	\
		bufsize = GetCurrentDirectory##SUFFIX(0, NULL); \
		if (!bufsize) \
			break; \
	\
		buf = mem_alloc((bufsize + 1) * sizeof(TYPE)); \
	\
		GetCurrentDirectory##SUFFIX(bufsize + 1, buf); \
	\
		utf8 = charset_iconv_easy(buf, CHARSET, CHARSET_UTF8); \
		if (utf8) \
			return utf8; \
	} while (0)

		TRY_GETCWD(WCHAR, W, CHARSET_WCHAR_T);
		TRY_GETCWD(CHAR, A, CHARSET_ANSI);

#undef TRY_GETCWD
	}
#else
    /* Double the buffer size until getcwd() succeeds or we run out
	 * of memory. Not using get_current_dir_name() and
	 * getcwd(NULL, n) to only use methods defined by POSIX. */
	size_t n = SCHISM_PATH_MAX; // good starting point?
	char *buf = malloc(n);
	while (buf && !getcwd(buf, n)) {
		n *= 2;

		free(buf);

		buf = malloc(n);
		if (!buf)
			break;
	}


	if (buf)
		return buf;
#endif

	return str_dup(".");
}

#if defined(SCHISM_WIN32)
// SHGetFolderPath only exists under Windows XP and newer.
// To combat this, we only use SHGetFolderPath if it
// actually exists. Otherwise, we fallback to the obsolete
// (and unsupported!) SHGetSpecialFolderPath, which exists
// under systems with at least Internet Explorer 4 installed.
// If that also doesn't work, we read the relevant registry
// keys. If *that* doesn't work, we'll fallback to standard
// environment variables. THEN we fallback to FALLBACK_DIR.

// wow this sucks
#define DMOZ_WIN32_GET_CSIDL_DIRECTORY_IMPL(TYPE, CHARSET, SUFFIX, GETENV, LITERAL) \
	static inline SCHISM_ALWAYS_INLINE char *dmoz_win32_get_csidl_directory##SUFFIX(int csidl, const TYPE *registry, const TYPE *envvar) \
	{ \
		{ \
			TYPE buf[MAX_PATH]; \
	\
			if (WIN32_SHGetSpecialFolderPath##SUFFIX && WIN32_SHGetSpecialFolderPath##SUFFIX(NULL, buf, csidl, 1)) { \
				char *utf8; \
				if (!charset_iconv(buf, &utf8, CHARSET, CHARSET_UTF8, sizeof(buf))) \
					return utf8; \
			} \
		} \
	\
		if (registry) { \
			static const TYPE *regpaths[] = { \
				LITERAL##"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\User Shell Folders", \
				LITERAL##"SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Explorer\\Shell Folders", /* Win95 */ \
			}; \
			int i; \
			for (i = 0; i < ARRAY_SIZE(regpaths); i++) {\
				HKEY shell_folders; \
				if (RegOpenKeyEx##SUFFIX(HKEY_CURRENT_USER, regpaths[i], 0, KEY_READ, &shell_folders) != ERROR_SUCCESS) \
					continue; \
	\
				DWORD type; \
				DWORD length; \
	\
				if (RegQueryValueEx##SUFFIX(shell_folders, registry, NULL, &type, NULL, &length) != ERROR_SUCCESS \
					|| (type != REG_EXPAND_SZ && type != REG_SZ)) \
					continue; \
	\
				TYPE *data = mem_alloc(length); \
	\
				if (RegQueryValueEx##SUFFIX(shell_folders, registry, NULL, NULL, (LPBYTE)data, &length) != ERROR_SUCCESS) { \
					free(data); \
					continue; \
				} \
	\
				if (type == REG_EXPAND_SZ) {\
					TYPE expanded[MAX_PATH]; \
					if (ExpandEnvironmentStrings##SUFFIX((void *)data, expanded, ARRAY_SIZE(expanded))) { \
						char *utf8; \
						if (!charset_iconv(expanded, &utf8, CHARSET, CHARSET_UTF8, sizeof(expanded))) { \
							free(data); \
							return utf8; \
						} \
					} \
				} else if (type == REG_SZ) { \
					char *utf8; \
					if (!charset_iconv(data, &utf8, CHARSET, CHARSET_UTF8, length)) { \
						free(data); \
						return utf8; \
					} \
				} \
	\
				free(data); \
			} \
		} \
	\
		if (envvar) { \
			TYPE *ptr = GETENV(envvar); \
			if (ptr) { \
				char *utf8; \
	\
				if (!charset_iconv(ptr, &utf8, CHARSET, CHARSET_UTF8, MAX_PATH * sizeof(TYPE))) \
					return utf8; \
			} \
		} \
	\
		return NULL; \
	}

#ifdef SCHISM_WIN32_COMPILE_ANSI
DMOZ_WIN32_GET_CSIDL_DIRECTORY_IMPL(CHAR, CHARSET_ANSI, A, getenv, /* none */)
#endif
DMOZ_WIN32_GET_CSIDL_DIRECTORY_IMPL(WCHAR, CHARSET_WCHAR_T, W, _wgetenv, L)

#undef DMOZ_WIN32_GET_CSIDL_DIRECTORY_IMPL

// Don't use this function directly! see the macro defined below
static char *dmoz_win32_get_csidl_directory(int csidl, const wchar_t *registryw, const char *registry, const wchar_t *envvarw, const char *envvar)
{
#ifdef SCHISM_WIN32_COMPILE_ANSI
	if (GetVersion() & UINT32_C(0x80000000)) {
		char *utf8 = dmoz_win32_get_csidl_directoryA(csidl, registry, envvar);
		if (utf8)
			return utf8;
	} else
#endif
	{
		// Windows NT.
		{
			// special case: SHGetFolderPathW
			wchar_t bufw[MAX_PATH];
			char *utf8;

			if (WIN32_SHGetFolderPathW && WIN32_SHGetFolderPathW(NULL, csidl, NULL, 0, bufw) == S_OK && !charset_iconv(bufw, &utf8, CHARSET_WCHAR_T, CHARSET_UTF8, MAX_PATH))
				return utf8;
		}

		char *utf8 = dmoz_win32_get_csidl_directoryW(csidl, registryw, envvarw);
		if (utf8)
			return utf8;
	}

	// we'll get em next time
	return NULL;
}

// this just makes stuff simpler.
// registry and envvar need to be compile time constants,
// but really they should already be that anyway
# define DMOZ_GET_WIN32_DIRECTORY(csidl, registry, envvar) dmoz_win32_get_csidl_directory(csidl, L ## registry, registry, L ## envvar, envvar)
#elif defined(SCHISM_MACOS)
static char *dmoz_macos_find_folder(OSType folder_type)
{
	FSSpec spec;
	short vrefnum;
	long dir_id;

	// FIXME: i don't know if this first value should be kOnSystemDisk
	// or some other constant, since we want to work with multiple users
	if (FindFolder(kOnSystemDisk, folder_type, kCreateFolder, &vrefnum, &dir_id) == noErr
		&& FSMakeFSSpec(vrefnum, dir_id, NULL, &spec) == noErr) {
		char *path;
		if (dmoz_path_from_fsspec(&spec, &path))
			return path;
	}

	return NULL;
}
#endif

char *dmoz_get_home_directory(void)
{
#if defined(__amigaos4__)
	return str_dup("PROGDIR:");
#elif defined(SCHISM_WIN32)
	// NOTE: USERPROFILE is not the documents folder, it's the user folder...
	char *ptr = DMOZ_GET_WIN32_DIRECTORY(CSIDL_PERSONAL, "Personal", "USERPROFILE");
	if (ptr)
		return ptr;
#elif defined(SCHISM_MACOS)
	// Taking heed from Windows, default to the Documents Folder.
	//
	// kDocumentsFolderType is supported by Mac OS 8 or later; I believe
	// we're already limited to just Mac OS 9 because we're using the
	// Multiprocessing library, so I won't add it as a fallback here.
	char *ptr = dmoz_macos_find_folder(kDocumentsFolderType);
	if (ptr)
		return ptr;
#elif defined(SCHISM_OS2)
	{
		// First try these (this is what SDL does...)
		char *ptr;

		ptr = getenv("HOME");
		if (ptr) return str_dup(ptr);

		ptr = getenv("ETC");
		if (ptr) return str_dup(ptr);
	}
#else
	{
		char *ptr = getenv("HOME");
		if (ptr)
			return str_dup(ptr);
	}
#endif

	/* hmm. fall back to the current dir */
	char* path = dmoz_get_current_directory();
	if (strcmp(path, "."))
		return path;

	free(path);

	/* still don't have a directory? sheesh. */
	return str_dup(FALLBACK_DIR);
}

char *dmoz_get_dot_directory(void)
{
#ifdef SCHISM_WIN32
	char *ptr = DMOZ_GET_WIN32_DIRECTORY(CSIDL_APPDATA, "AppData", "APPDATA");
	if (ptr)
		return ptr;

	// else fall back to home (mesopotamian era windows I guess)
#elif defined(SCHISM_MACOS)
	char *ptr = dmoz_macos_find_folder(kPreferencesFolderType);
	if (ptr)
		return ptr;

	// fall back to home (what?)
#elif defined(SCHISM_MACOSX)
	char *ptr;

	ptr = macosx_get_application_support_dir();
	if (ptr)
		return ptr;

	// this should never happen but prepare for it if it does
	char *home = dmoz_get_home_directory();
	ptr = dmoz_path_concat(home, "Library/Application Support");
	free(home);

	if (ptr)
		return ptr;
#endif

	return dmoz_get_home_directory();
}

char *dmoz_get_exe_directory(void)
{
	if (backend && backend->get_exe_path) {
		char *path = backend->get_exe_path();
		if (path)
			return path;
	}

	// unknown, don't care
	return NULL;
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
	int count;

	/* The result cannot be larger than the input PATH. */
	result = str_dup(path);

	rooted = dmoz_path_is_absolute(path, &count);
	base = result + (rooted ? count : 0);
	stub_char = rooted ? DIR_SEPARATOR : '.';

#if defined(SCHISM_WIN32) || defined(SCHISM_OS2) || defined(SCHISM_XBOX)
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

// count is only meaningful if this function returns 1.
// otherwise, the value should be discarded.
int dmoz_path_is_absolute(const char *path, int *count)
{
	if (!path || !*path)
		return 0;

#if defined(SCHISM_WIN32) || defined(SCHISM_OS2) || defined(SCHISM_XBOX)
	if (isalpha(path[0]) && path[1] == ':') {
		if (count) *count = IS_DIR_SEPARATOR(path[2]) ? 3 : 2;
		return 1;
	}
#elif defined(__amigaos4__)
	/* Entirely a guess -- could some fine Amiga user please tell me if this is right or not? */
	char *colon = strchr(path, ':'), *slash = strchr(path, '/');
	if (colon && (colon < slash || (colon && !slash && colon[1] == '\0'))) {
		int x = colon - path + 1;
		if (count) *count = x;
		return !!x;
	}
#elif defined(SCHISM_WII) || defined(SCHISM_WIIU)
	char *colon = strchr(path, ':'), *slash = strchr(path, '/');
	if (colon + 1 == slash) {
		int x = slash - path + 1;
		if (count) *count = x;
		return !!x;
	}
#elif defined(SCHISM_MACOS)
	/* From Apple's documentation:
	 *   A leading colon indicates a relative path, otherwise
	 *   the first path component denotes the volume.
	 * This is pretty much the opposite of what every other
	 * OS does, and means the code below is simply wrong. */
	if (IS_DIR_SEPARATOR(path[0]))
		return 0;

	if (count) *count = 0;
	return 1;
#endif
	/* presumably, /foo (or \foo) is an absolute path on all platforms */
	if (!IS_DIR_SEPARATOR(path[0]))
		return 0;

	/* POSIX says to allow two leading slashes, but not more.
	 * (This also catches win32 \\share\blah\blah semantics) */
	if (count) *count = ((IS_DIR_SEPARATOR(path[1]) && !IS_DIR_SEPARATOR(path[2])) ? 2 : 1);
	return 1;
}


/* See dmoz_path_concat_len. This function is a convenience for when the lengths aren't already known. */
char *dmoz_path_concat(const char *a, const char *b)
{
	return dmoz_path_concat_len(a, b, strlen(a), strlen(b));
}

/* Concatenate two paths. Additionally, if 'b' is an absolute path, ignore 'a' and return a copy of 'b'. */
char *dmoz_path_concat_len(const char *a, const char *b, int alen, int blen)
{
	char *ret, *p;
#ifndef SCHISM_MACOS /* blah, this is dumb */
	if (dmoz_path_is_absolute(b, NULL))
		return str_dup(b);
#endif

	// allocates enough space for a, b, a separator, and a NUL terminator
	// (the terminator is included with ARRAY_SIZE(literal))
	p = ret = mem_alloc(alen + blen + ARRAY_SIZE(DIR_SEPARATOR_STR));

	if (alen) {
		char last = a[alen - 1];

		memcpy(p, a, alen);
		p += alen;

		/* need a slash? */
#if defined(__amigaos4__)
		if (last != ':' && last != '/')
			*(p++) = '/';
#else
		if (!IS_DIR_SEPARATOR(last)) {
			memcpy(ret + alen, DIR_SEPARATOR_STR, ARRAY_SIZE(DIR_SEPARATOR_STR) - 1);
			p += ARRAY_SIZE(DIR_SEPARATOR_STR) - 1;
		}
#endif
	}

	memcpy(p, b, blen);
	p += blen;

	*p = '\0';

	return ret;
}

char *dmoz_path_pretty_name(const char *filename)
{
	char *ret, *temp;
	const char *ptr;
	int len;

	ptr = strrchr(filename, DIR_SEPARATOR);
	ptr = ((ptr && ptr[1]) ? ptr + 1 : filename);
	len = strrchr(ptr, '.') - ptr;
	if (len <= 0) {
		ret = str_dup(ptr);
	} else {
		ret = mem_calloc(len + 1, sizeof(char));
		strncpy(ret, ptr, len);
		ret[len] = 0;
	}

	/* change underscores to spaces (of course, this could be adapted
	 * to use strpbrk and strip any number of characters) */
	while ((temp = strchr(ret, '_')) != NULL)
		*temp = ' ';

	/* TODO | the first letter, and any letter following a space,
	 * TODO | should be capitalized; multiple spaces should be cut
	 * TODO | down to one */

	str_trim(ret);
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
_DEF_CMP_CHARSET(strverscmp)
_DEF_CMP_CHARSET(strcaseverscmp)
#undef _DEF_CMP_CHARSET

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
#elif defined(SCHISM_WIN32) || defined(SCHISM_OS2) || defined(SCHISM_XBOX)
	/* this is FUGLY */
# if defined(SCHISM_WIN32) || defined(SCHISM_XBOX)
	const DWORD x = GetLogicalDrives();
#  if defined(SCHISM_WIN32)
	/* Can we just get rid of this? Why is this here? */
	UINT em = SetErrorMode(0);
#  endif
# elif defined(SCHISM_OS2)
	ULONG drive, x;
	DosQueryCurrentDisk(&drive, &x);
# endif
	char sbuf[] = "A:\\";

	for (; x && sbuf[0] <= 'Z'; sbuf[0]++) {
		if ((x >> (sbuf[0] - 'A')) & 1) {
			dmoz_add_file_or_dir(flist, dlist, str_dup(sbuf),
				str_dup(sbuf), NULL, -(1024 - 'A' - sbuf[0]));
		}
	}
# ifdef SCHISM_WIN32
	em = SetErrorMode(em);
# endif
#elif defined(SCHISM_WII) || defined(SCHISM_WIIU)
	int i;
	for (i = 0; devices[i]; i++) {
		DIR *dir = opendir(devices[i]);
		if (!dir)
			continue;
		closedir(dir);
		dmoz_add_file_or_dir(flist, dlist, str_dup(devices[i]), str_dup(devices[i]), NULL, -(1024 - i));
	}
#elif defined(SCHISM_MACOS)
	for (ItemCount index = 1; ; index++) {
		unsigned char ppath[256];
		ppath[0] = 0;

		HParamBlockRec hfsParams = {
			.volumeParam = {
				.ioNamePtr = ppath,
				.ioVRefNum = 0,
				.ioVolIndex = index,
			},
		};

		OSErr err = PBHGetVInfoSync(&hfsParams);
		if (err != noErr)
			break;
	
		// FIXME: Need conversion to & from UTF-8
		dmoz_add_file_or_dir(flist, dlist, strn_dup(&ppath[1], ppath[0]), strn_dup(&ppath[1], ppath[0]), NULL, -(1024 - index + 1));
	}
#else /* assume POSIX */
/*	char *home;
	home = get_home_directory();*/
	dmoz_add_file_or_dir(flist, dlist, str_dup("/"), str_dup("/"), NULL, -1024);
/*      dmoz_add_file_or_dir(flist, dlist, home, str_dup("~"), NULL, -5); */

#endif /* platform */

	ptr = dmoz_path_get_parent_directory(path);
	if (ptr)
		dmoz_add_file_or_dir(flist, dlist, ptr, str_dup(".."), NULL, -10);
}

/* --------------------------------------------------------------------------------------------------------- */

/* on success, this will fill the lists and return 0. if something goes
wrong, it adds a 'stub' entry for the root directory, and returns -1. */
int dmoz_read(const char *path, dmoz_filelist_t *flist, dmoz_dirlist_t *dlist,
		int (*load_library)(const char *path, dmoz_filelist_t *flist, dmoz_dirlist_t *dlist))
{
#if defined(SCHISM_WIN32) || defined(SCHISM_XBOX)
	DWORD attrib;

	SCHISM_ANSI_UNICODE({
		char* path_a = NULL;
		if (charset_iconv(path, &path_a, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX))
			return -1;

		attrib = GetFileAttributesA(path_a);

		free(path_a);
	}, {
		wchar_t* path_w = NULL;
		if (charset_iconv(path, &path_w, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX))
			return -1;

		attrib = GetFileAttributesW(path_w);

		free(path_w);
	});

	const size_t pathlen = strlen(path);

	if (attrib & FILE_ATTRIBUTE_DIRECTORY) {
		union {
# if defined(SCHISM_WIN32_COMPILE_ANSI) || defined(SCHISM_XBOX)
			WIN32_FIND_DATAA a;
# endif
# ifdef SCHISM_WIN32
			WIN32_FIND_DATAW w;
# endif
		} ffd;

		HANDLE find = NULL;
		{
			char* searchpath_n = dmoz_path_concat_len(path, "*", strlen(path), 1);
			if (!searchpath_n)
				return -1;

			SCHISM_ANSI_UNICODE({
				char *searchpath;
				if (!charset_iconv(searchpath_n, &searchpath, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)) {
					find = FindFirstFileA(searchpath, &ffd.a);
					free(searchpath);
				}
			}, {
				wchar_t* searchpath;
				if (!charset_iconv(searchpath_n, &searchpath, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)) {
					find = FindFirstFileW(searchpath, &ffd.w);
					free(searchpath);
				}
			});

			free(searchpath_n);
		}

		// give up
		if (find == INVALID_HANDLE_VALUE) {
			add_platform_dirs(path, flist, dlist);
			return -1;
		}

		/* do-while to process the first file... */
		for (;;) {
			DWORD file_attrib = 0;
			char *filename = NULL;
			char *fullpath = NULL;

			SCHISM_ANSI_UNICODE({
				// ANSI
				if (FindNextFileA(find, &ffd.a)) {
					file_attrib = ffd.a.dwFileAttributes;
					if (charset_iconv(ffd.a.cFileName, &filename, CHARSET_ANSI, CHARSET_UTF8, sizeof(ffd.a.cFileName)))
						continue;
					fullpath = dmoz_path_concat_len(path, filename, pathlen, strlen(filename));
				} else {
					// ...
					break;
				}
			}, {
				if (FindNextFileW(find, &ffd.w)) {
					file_attrib = ffd.w.dwFileAttributes;
					if (charset_iconv(ffd.w.cFileName, &filename, CHARSET_WCHAR_T, CHARSET_UTF8, sizeof(ffd.w.cFileName)))
						continue;
					fullpath = dmoz_path_concat_len(path, filename, pathlen, strlen(filename));
				} else {
					break;
				}
			});
			
			if (!strcmp(filename, ".")
				|| !strcmp(filename, "..")
				|| filename[strlen(filename)-1] == '~'
				|| file_attrib & FILE_ATTRIBUTE_HIDDEN) {
				free(filename);
				free(fullpath);
				continue;
			}

			struct stat st;
			if (os_stat(fullpath, &st) < 0) {
				/* doesn't exist? */
				log_perror(fullpath);
				free(fullpath);
				free(filename);
				continue; /* better luck next time */
			}
	
			st.st_mtime = MAX(0, st.st_mtime);
	
			if (file_attrib & FILE_ATTRIBUTE_DIRECTORY) {
				dmoz_add_file_or_dir(flist, dlist, fullpath, filename, &st, 0);
			} else if (file_attrib == INVALID_FILE_ATTRIBUTES) {
				free(fullpath);
				free(filename);
			} else {
				dmoz_add_file(flist, fullpath, filename, &st, 1);
			}
		}

		FindClose(find);

		add_platform_dirs(path, flist, dlist);
	} else if (attrib == INVALID_FILE_ATTRIBUTES) {
		add_platform_dirs(path, flist, dlist);
		return -1;
	} else {
		/* file? probably.
		 * make sure when editing this code to keep it in sync
		 * with the below code for non-Windows platforms */
		if (load_library && !load_library(path, flist, dlist)) {
			char* ptr = dmoz_path_get_parent_directory(path);
			if (ptr)
				dmoz_add_file_or_dir(flist, dlist, ptr, str_dup("."), NULL, -10);
			else
				add_platform_dirs(path, flist, dlist);
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
		path = "/";

	pathlen = strlen(path);

#ifdef SCHISM_WII /* and Wii U maybe? */
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
			namlen = strlen(ent->d_name);
			/* ignore hidden/backup files (TODO: make this code more portable;
			some OSes have different ideas of whether a file is hidden) */

			if (ent->d_name[0] == '.')
				continue;

			if (ent->d_name[namlen - 1] == '~')
				continue;

			{
				char *utf8;
#ifdef SCHISM_OS2
				utf8 = charset_iconv_easy(ent->d_name, CHARSET_DOSCP, CHARSET_UTF8);
#else
				utf8 = ent->d_name;
#endif
				ptr = dmoz_path_concat_len(path, utf8, pathlen, namlen);
#ifdef SCHISM_OS2
				free(utf8);
#endif
			}

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
	if (lib && (ptr = dmoz_path_get_parent_directory(path)) != NULL)
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
	slurp_t t;
	if (file->filesize == 0)
		return FINF_EMPTY;

	if (slurp(&t, file->path, NULL, file->filesize) < 0)
		return FINF_ERRNO;

	file->artist = NULL;
	file->title = NULL;
	file->smp_defvol = 64;
	file->smp_gblvol = 64;
	for (const fmt_read_info_func *func = read_info_funcs; *func; func++) {
		slurp_rewind(&t);
		if ((*func) (file, &t)) {
			if (file->artist)
				str_trim(file->artist);
			if (file->title == NULL)
				file->title = str_dup(""); /* or the basename? */
			str_trim(file->title);
			break;
		}
	}
	unslurp(&t);
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

/* ------------------------------------------------------------------------ */

#ifdef SCHISM_WIN32
static void *lib_shell32 = NULL;
#endif

int dmoz_init(void)
{
	static const schism_dmoz_backend_t *backends[] = {
		// ordered by preference
#ifdef SCHISM_WIN32
		&schism_dmoz_backend_win32,
#endif
#ifdef SCHISM_MACOSX
		&schism_dmoz_backend_macosx,
#endif
#ifdef SCHISM_MACOS
		&schism_dmoz_backend_macos,
#endif
#ifdef SCHISM_SDL3
		&schism_dmoz_backend_sdl3,
#endif
#ifdef SCHISM_SDL2
		&schism_dmoz_backend_sdl2,
#endif
		NULL,
	};

	int i;

	for (i = 0; backends[i]; i++) {
		backend = backends[i];
		if (backend->init())
			break;

		backend = NULL;
	}

#ifdef SCHISM_WIN32
	lib_shell32 = loadso_object_load("shell32.dll");
	if (lib_shell32) {
		WIN32_SHGetFolderPathW = loadso_function_load(lib_shell32, "SHGetFolderPathW");
		WIN32_SHGetSpecialFolderPathW = loadso_function_load(lib_shell32, "SHGetSpecialFolderPathW");

		WIN32_SHGetSpecialFolderPathA = loadso_function_load(lib_shell32, "SHGetSpecialFolderPathA");
		if (!WIN32_SHGetSpecialFolderPathA)
			WIN32_SHGetSpecialFolderPathA = loadso_function_load(lib_shell32, "SHGetSpecialFolderPath");
	}
#endif

	if (!backend)
		return 0;

	return 1;
}

void dmoz_quit(void)
{
	if (backend) {
		backend->quit();
		backend = NULL;
	}

#ifdef SCHISM_WIN32
	if (lib_shell32)
		loadso_object_unload(lib_shell32);
#endif
}
