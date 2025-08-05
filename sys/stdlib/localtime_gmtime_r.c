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
#include "mt.h"

#ifdef NEED_LOCALTIME_GMTIME_R_INIT
static mt_mutex_t *localtime_gmtime_r_mutex = NULL;
static int initialized = 0;

void localtime_gmtime_r_quit(void)
{
	mt_mutex_delete(localtime_gmtime_r_mutex);
}

int localtime_gmtime_r_init(void)
{
	localtime_gmtime_r_mutex = mt_mutex_create();
	if (!localtime_gmtime_r_mutex)
		return 0;

	initialized = 1;

	return 1;
}
#endif

#ifndef HAVE_LOCALTIME_R
struct tm *localtime_r(const time_t *timep, struct tm *result)
{
	static struct tm *our_tm;

	// huh?
	if (!initialized)
		return NULL;

	mt_mutex_lock(localtime_gmtime_r_mutex);

	our_tm = localtime(timep);

	/* regular localtime() returns NULL pointer on XBOX; make sure
	 * it doesn't happen again ;) */
	SCHISM_RUNTIME_ASSERT(our_tm, "localtime() returned a NULL pointer");

	*result = *our_tm;

	mt_mutex_unlock(localtime_gmtime_r_mutex);

	return result;
}
#endif

#ifndef HAVE_GMTIME_R
struct tm *gmtime_r(const time_t *timep, struct tm *result)
{
	static struct tm *our_tm;

	// huh?
	if (!initialized)
		return NULL;

	mt_mutex_lock(localtime_gmtime_r_mutex);

	our_tm = gmtime(timep);

	/* regular localtime() returns NULL pointer on XBOX; make sure
	 * it doesn't happen again ;) */
	SCHISM_RUNTIME_ASSERT(our_tm, "localtime() returned a NULL pointer");

	*result = *our_tm;

	mt_mutex_unlock(localtime_gmtime_r_mutex);

	return result;
}
#endif
