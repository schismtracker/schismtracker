/*
 * aria2 - The high speed download utility
 *
 * Copyright (C) 2007 Tatsuhiro Tsujikawa
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * (Modified slightly to build with Schism Tracker)
 */

#define NEED_TIME
#include "headers.h"

#include <windows.h>


static CRITICAL_SECTION localtime_r_cs;

static void localtime_r_atexit(void)
{
	DeleteCriticalSection(&localtime_r_cs);
}

struct tm * localtime_r(const time_t *timep, struct tm *result)
{
	static struct tm *local_tm;
	static int initialized = 0;

	if (!initialized) {
		++initialized;
		InitializeCriticalSection(&localtime_r_cs);
		atexit(localtime_r_atexit);
	}

	EnterCriticalSection(&localtime_r_cs);
	local_tm = localtime(timep);
	memcpy(result, local_tm, sizeof(struct tm));
	LeaveCriticalSection(&localtime_r_cs);
	return result;
}

