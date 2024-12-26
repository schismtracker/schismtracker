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

#ifdef SCHISM_WIN32
# include <windows.h>
#elif defined(HAVE_LIBDL)
# include <dlfcn.h>
#endif

void *loadso_object_load(const char *sofile)
{
#ifdef SCHISM_WIN32
	HMODULE module = NULL;

	if (GetVersion() < 0x80000000) {
		// Windows 9x
		char *ansi;

		if (!charset_iconv(sofile, &ansi, CHARSET_UTF8, CHARSET_ANSI, SIZE_MAX)) {
			module = LoadLibraryA(ansi);
			free(ansi);
		}
	} else {
		// Windows NT
		wchar_t *unicode;

		if (!charset_iconv(sofile, &unicode, CHARSET_UTF8, CHARSET_WCHAR_T, SIZE_MAX)) {
			module = LoadLibraryW(unicode);
			free(unicode);
		}
	}

	if (!module)
		module = LoadLibraryA(sofile);

	if (module)
		return module;
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
#elif defined(HAVE_LIBDL)
	void *symbol;

	symbol = dlsym(handle, name);
	if (symbol)
		return symbol;

	// some platforms require extra work.
	char *p;
	if (asprintf(&p, "_%s", name) < 0)
		return NULL;

	symbol = dlsym(handle, p);
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
#elif defined(SCHISM_MACOSX)
	"lib%s.%d.dylib",
#else
	"lib%s.so.%d",
#endif
	NULL,
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

#undef LIBTOOL_FMT

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
	//"lib%s.dylib",
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
