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

#include "osdefs.h"

int os_run_hook(const char *dir, const char *name, const char *maybe_arg)
{
#ifdef HAVE_OS_EXEC
	int st;

	return os_exec(dir, 1, name, maybe_arg, &st) && (st == 0);
#else
	errno = ENOTSUP;
	return 0;
#endif
}

int os_shell(const char *name, const char *arg)
{
#ifdef HAVE_OS_EXEC
	int st;

	if (!os_exec(NULL, 0, name, arg, &st))
		return -1; /* ? */

	return st;
#else
	errno = ENOTSUP;
	return -1;
#endif
}

