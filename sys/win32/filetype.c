/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
 * copyright (c) 2005-2006 Mrs. Brisby <mrs.brisby@nimh.org>
 * URL: http://nimh.org/schism/
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
#include "headers.h"

#include <shlobj.h>
#include <windows.h>
#include <string.h>

static int simple(const char *k, const char *v, const char *exe)
{
	HKEY h;
	DWORD rstype, rslen;
	char *tmp;
	int ok, n;

	if (RegCreateKey(HKEY_CLASSES_ROOT, k, &h) != ERROR_SUCCESS) {
		if (RegOpenKey(HKEY_CLASSES_ROOT, k, &h) != ERROR_SUCCESS)
			return 0;
		if (v == NULL) {
			RegCloseKey(h);
			return 0;
		}
	}
	if (v == NULL) {
		RegCloseKey(h);
		return 1;
	}

	n = strlen(v)+1;
	tmp = malloc(n+3);
	if (!tmp) {
		RegCloseKey(h);
		return 0;
	}

	ok = 1;
	rslen = n+1;
	memset(tmp, 0, n);
	if (RegQueryValueEx(h, NULL, NULL, &rstype, tmp, &rslen)
			== ERROR_SUCCESS) {
		if (rslen && tmp[0]) {
			tmp[n+1]='\0';
			if (!exe || strstr(tmp,exe) == NULL) ok = 0;
		}
	}

	if (ok && RegSetValueEx(h, NULL, 0, REG_SZ, v, n)
			!= ERROR_SUCCESS) 
		ok = 0;
	RegCloseKey(h);
	free(tmp);
	return ok;
}


void win32_filetype_setup(const char *argv0)
{
	const char *q, *exe;
	char *s, *p;
	int up = 0;

	p = s = malloc((strlen(argv0)*2) + 8);
	*p++ = '"';
	exe = argv0;
	for (q = argv0; *q; q++) {
		if (*q == '"' || *q == '\\') {
			*p++ = '\\';
			exe = q+1;
		}
		*p++ = *q;
	}
	*p++ = '"';
	*p++ = ' ';
	*p++ = '"';
	*p++ = '%';
	*p++ = 'l';
	*p++ = '"';
	*p = '\0';

	if (simple(".it", "SchismTracker",NULL)) up = 1;
	if (simple(".mod", "SchismTracker",NULL)) up = 1;
	if (simple(".s3m", "SchismTracker",NULL)) up = 1;
	if (simple(".xm", "SchismTracker",NULL)) up = 1;
	if (simple("SchismTracker", "Schism Tracker Module",NULL)) up = 1;

	if (simple("SchismTracker\\DefaultIcon", argv0,exe)) up=1;

	if (simple("SchismTracker\\Shell", "Open",exe)) up=1;

	(void)simple("SchismTracker\\Shell\\Open", NULL,NULL);

	if (simple("SchismTracker\\Shell\\Open\\command", s,exe)) up=1;

	if (up) {
		/* let explorer know we did something */
		SHChangeNotify(SHCNE_ASSOCCHANGED, 0, NULL, NULL);
		SHChangeNotify(SHCNE_ASSOCCHANGED, SHCNF_IDLIST, NULL, NULL);
	}

	free(s);
}
void win32_filecreated_callback(const char *filename)
{
	/* let explorer know when we create a file. */
	SHChangeNotify(SHCNE_CREATE, SHCNF_PATH|SHCNF_FLUSHNOWAIT, filename, NULL);
	SHChangeNotify(SHCNE_UPDATEITEM, SHCNF_PATH|SHCNF_FLUSHNOWAIT, filename, NULL);
}
