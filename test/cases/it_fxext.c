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
#include "test-assertions.h"

#include "disko.h"
#include "fmt.h"
#include "slurp.h"
#include "song.h"

#define IT_INST_BASE_SIZE 554
#define IT_INST_FXEXT_SIZE 246
#define IT_INST_FXEXT_MAGIC UINT32_C(0x58484353) /* SCHX */

static testresult_t it_fxext_roundtrip(int extended)
{
	song_t *song = csf_allocate();
	song_instrument_t *ins = csf_allocate_instrument();
	disko_t ds;
	slurp_t fp;
	uint32_t magic;

	current_song = song;
	song->instruments[1] = ins;

	ins->sample_map[0] = 1;
	if (extended) {
		ins->effect_map[0] = FX_PANNING;
		ins->param_map[0] = 0x80;
		ins->effect_map[42] = FX_VIBRATO;
		ins->param_map[42] = 0x34;
		ins->flags |= INST_NOTE_FXMAP;
	}

	REQUIRE(disko_memopen(&ds) >= 0);
	save_iti_instrument(&ds, song, ins, 0);

	if (extended) {
		ASSERT(ds.length == IT_INST_BASE_SIZE + IT_INST_FXEXT_SIZE);
		memcpy(&magic, (const char *)ds.data + IT_INST_BASE_SIZE, sizeof(magic));
		ASSERT(magic == IT_INST_FXEXT_MAGIC);
	} else {
		ASSERT(ds.length == IT_INST_BASE_SIZE);
	}

	REQUIRE(slurp_memstream(&fp, ds.data, ds.length) >= 0);

	song_instrument_t *loaded = csf_allocate_instrument();
	REQUIRE(load_it_instrument(NULL, loaded, &fp) != 0);
	load_it_instrument_fxext(loaded, &fp);

	if (extended) {
		ASSERT(loaded->flags & INST_NOTE_FXMAP);
		ASSERT(loaded->effect_map[0] == FX_PANNING);
		ASSERT(loaded->param_map[0] == 0x80);
		ASSERT(loaded->effect_map[42] == FX_VIBRATO);
		ASSERT(loaded->param_map[42] == 0x34);
	} else {
		ASSERT(!(loaded->flags & INST_NOTE_FXMAP));
		ASSERT(loaded->effect_map[0] == FX_NONE);
		ASSERT(loaded->param_map[0] == 0);
	}

	csf_free_instrument(loaded);
	unslurp(&fp);
	disko_memclose(&ds, 0);
	csf_free(song);
	current_song = NULL;

	RETURN_PASS;
}

testresult_t test_it_fxext_roundtrip_extended(void)
{
	return it_fxext_roundtrip(1);
}

testresult_t test_it_fxext_roundtrip_plain(void)
{
	return it_fxext_roundtrip(0);
}
