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

#include "test.h"
#include "test-tempfile.h"
#include "test-assertions.h"

#include "disko.h"

/* shared test stuff */
static testresult_t test_disko(disko_t *ds)
{
	void *mem;

	/* make sure there is no data already in it */
	disko_seek(ds, 0, SEEK_END);
	ASSERT(disko_tell(ds) == 0);
	/* rewind */
	disko_seek(ds, 0, SEEK_SET);

	/* write some crap */
	disko_write(ds, "12345678910", 11);

	/* we should be at pos 11 now */
	ASSERT(disko_tell(ds) == 11);

	/* start memory write */
	mem = disko_memstart(ds, 16);

	/* dump dummy data */
	memcpy(mem, "1111111111111111", 16);

	/* end memory write, but truncate the data. */
	disko_memend(ds, mem, 13);

	disko_seek(ds, 0, SEEK_END);
	ASSERT(disko_tell(ds) == 24);

	RETURN_PASS;
}

testresult_t test_disko_mem(void)
{
	/* */
	disko_t ds;
	testresult_t r;

	REQUIRE(disko_memopen(&ds) >= 0);

	r = test_disko(&ds);

	/* ehhh */
	if (r == SCHISM_TESTRESULT_PASS && ds.length != 24 && ds.allocated < 24)
		r = SCHISM_TESTRESULT_FAIL;

	if (r == SCHISM_TESTRESULT_PASS && memcmp(ds.data, "123456789101111111111111111", 24))
		r = SCHISM_TESTRESULT_FAIL;

	disko_memclose(&ds, 0);

	return r;
}
