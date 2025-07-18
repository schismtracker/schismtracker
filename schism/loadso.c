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

#include "charset.h"
#include "loadso.h"
#include "osdefs.h"
#include "mem.h"

#ifdef SCHISM_WIN32
# include <windows.h>
#elif defined(SCHISM_OS2)
# define INCL_DOS
# include <os2.h>
# include <meerror.h>
#elif defined(HAVE_LIBDL)
# include <dlfcn.h>
#endif

void *loadso_object_load(const char *sofile)
{
#ifdef SCHISM_WIN32
	HMODULE module = NULL;

	// Suppress "library failed to load" errors on Win9x and NT 4
	DWORD em = SetErrorMode(0);
	SetErrorMode(em | SEM_FAILCRITICALERRORS);

	SCHISM_ANSI_UNICODE({
		// Windows 9x
		char *ansi;

		if (!charset_iconv(sofile, &ansi, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)) {
			module = LoadLibraryA(ansi);
			free(ansi);
		}
	}, {
		// Windows NT
		wchar_t *unicode;

		if (!charset_iconv(sofile, &unicode, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)) {
			module = LoadLibraryW(unicode);
			free(unicode);
		}
	})

	if (!module)
		module = LoadLibraryA(sofile);

	SetErrorMode(em);

	if (module)
		return module;
#elif defined(SCHISM_OS2)
	CHAR error_str[256];
	HMODULE module;
	ULONG rc;

	// TODO add charset_iconv support for OS/2
	rc = DosLoadModule(error_str, sizeof(error_str), sofile, &module);

	if (rc != NO_ERROR && !strrchr(sofile, '\\') && !strrchr(sofile, '/')) {
		/* strip .DLL extension and retry if there is no path
		 * (this is what SDL does; I suppose it won't hurt here) */
		size_t len = strlen(sofile);

		if (len > 4 && charset_strcasecmp(&sofile[len - 4], CHARSET_UTF8, ".dll", CHARSET_UTF8)) {
			char *sofile_strip = strn_dup(sofile, len - 4);
			rc = DosLoadModule(error_str, sizeof(error_str), sofile, &module);
			free(sofile_strip);
		}
	}

	if (rc == NO_ERROR)
		return (void *)module;
#elif defined(HAVE_LIBDL)
	void *object = dlopen(sofile, RTLD_NOW | RTLD_LOCAL);
	if (object)
		return object;
#endif

	return NULL;
}

void *loadso_function_load(void *handle, const char *name)
{
#ifdef SCHISM_WIN32
	void *symbol;

	symbol = GetProcAddress(handle, name);
	if (symbol)
		return symbol;
#elif defined(SCHISM_OS2)
	ULONG rc;
	PFN pfn;

	rc = DosQueryProcAddr((HMODULE)handle, 0, name, &pfn);
	if (rc != NO_ERROR) {
		char *p;
		if (asprintf(&p, "_%s", name) >= 0) {
			rc = DosQueryProcAddr((HMODULE)handle, 0, p, &pfn);
			free(p);
		}
	}

	if (rc == NO_ERROR)
		return (void *)pfn;
#elif defined(HAVE_LIBDL)
	void *symbol;

	symbol = dlsym(handle, name);
	if (symbol)
		return symbol;

	// some platforms require extra work.
	char *p;
	if (asprintf(&p, "_%s", name) >= 0) {
		symbol = dlsym(handle, p);
		free(p);
	}

	if (symbol)
		return symbol;
#endif

	// doh!
	return NULL;
}

void loadso_object_unload(void *object)
{
#ifdef SCHISM_WIN32
	if (object)
		FreeLibrary(object);
#elif defined(HAVE_LIBDL)
	if (object)
		dlclose(object);
#endif
}

/* ------------------------------------------------------------------------ */

static const char *loadso_libtool_fmts[] = {
#ifdef SCHISM_WIN32
	"lib%s-%d.dll",
	"%s-%d.dll", /* avformat-61.dll */
#elif defined(SCHISM_MACOSX)
	/* ripped from sdl2-compat with minor changes; most notably,
	 * we put the raw dylib within the Resources directory, while
	 * sdl2-compat expects it to be in a framework (stupid Xcode) */
	"@loader_path/lib%s.%d.dylib",
	"@loader_path/../Resources/lib%s.%d.dylib",
	"@executable_path/lib%s.%d.dylib",
	"@executable_path/../Resources/lib%s.%d.dylib",
	"lib%s.%d.dylib",
#else
	"lib%s.so.%d",
#endif
	NULL
};

static void *library_load_revision(const char *name, int revision)
{
	int i;
	void *res = NULL;

	for (i = 0; loadso_libtool_fmts[i] && !res; i++) {
		char *buf;

		if (asprintf(&buf, loadso_libtool_fmts[i], name, revision) < 0)
			continue;

		res = loadso_object_load(buf);

		free(buf);
	}

	return res;
}

/* loads a library using the current and age versions
 * as documented by GNU libtool. MAKE SURE the objects
 * you are importing are actually there in the library!
 * (i.e. check return values) */
static void *library_load_libtool(const char *name, int current, int age)
{
	int i;
	void *res = NULL;

	for (i = 0; i <= age; i++)
		if ((res = library_load_revision(name, current - i)))
			break;

	return res;
}

/* ------------------------------------------------------------------------ */

static const char *loadso_lib_fmts[] = {
#ifdef SCHISM_WIN32
	"lib%s.dll",
	"%s.dll",
#elif defined(SCHISM_MACOSX)
	"lib%s.dylib",
#else
	//"lib%s.so",
#endif
	NULL,
};

void *library_load(const char *name, int current, int age)
{
	void *object = NULL;

	object = library_load_libtool(name, current, age);
	if (object)
		return object;

	// ...
	int i;

	for (i = 0; loadso_lib_fmts[i] && !object; i++) {
		char *buf;

		if (asprintf(&buf, loadso_lib_fmts[i], name) < 0)
			continue;

		object = loadso_object_load(buf);

		free(buf);
	}

	return object;
}
