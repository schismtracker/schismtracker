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

#include "loadso.h"

#ifdef SCHISM_WIN32
# include <windows.h>
#elif defined(HAVE_LIBDL)
# include <dlfcn.h>
#endif

void *loadso_object_load(const char *sofile)
{
#ifdef SCHISM_WIN32
	HMODULE module;

	{
		// Unicode conversion happens here
		LPWSTR wstr;

		if (charset_iconv(sofile, &wstr, CHARSET_UTF8, CHARSET_WCHAR_T) == CHARSET_ERROR_SUCCESS) {
			module = LoadLibraryW(wstr);
			free(wstr);
		} else {
			// okay, try just loading the passed filename
			module = LoadLibraryA(sofile);
		}
	}

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

	symbol = dlsym(handle, name);
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

#ifdef SCHISM_WIN32
# define LIBTOOL_FMT "lib%s-%d.dll"
#elif defined(SCHISM_MACOSX)
# define LIBTOOL_FMT "lib%s.%d.dylib"
#else
# define LIBTOOL_FMT "lib%s.so.%d"
#endif

static void *library_load_revision(const char *name, int revision)
{
	char *buf;

	if (asprintf(&buf, LIBTOOL_FMT, name, revision) < 0)
		return NULL;

	void *res = loadso_object_load(buf);

	free(buf);

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

#ifdef SCHISM_WIN32
# define LIB_FMT "lib%s.dll"
#elif defined(SCHISM_MACOSX)
# define LIB_FMT "lib%s.dylib"
#else
# define LIB_FMT "lib%s.so"
#endif

void *library_load(const char *name, int current, int age)
{
	void *object = library_load_libtool(name, current, age);
	if (object)
		return object;

	// ...
	char *buf;

	if (asprintf(&buf, LIB_FMT, name) < 0)
		return NULL;

	object = loadso_object_load(buf);

	free(buf);

	return object;
}