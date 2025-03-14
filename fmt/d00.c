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
#include "bswap.h"
#include "fmt.h"
#include "util.h"
#include "mem.h"

struct d00_header {
	unsigned char id[6];
	unsigned char type;
	unsigned char version;
	unsigned char speed; // apparently this is in Hz? wtf
	unsigned char subsongs; // ignored for now
	unsigned char soundcard;
	unsigned char title[32], author[32], reserved[32];

	// parapointers
	uint16_t tpoin; // not really sure what this is
	uint16_t sequence_paraptr; // patterns
	uint16_t instrument_paraptr; // adlib instruments
	uint16_t info_paraptr; // song message I guess
	uint16_t spfx_paraptr; // points to levpuls on v2 or spfx on v4
	uint16_t endmark; // what?
};

// This function, like many of the other read functions, also
// performs sanity checks on the data itself.
static int d00_header_read(struct d00_header *hdr, slurp_t *fp)
{
	// we check if the length is larger than UINT16_MAX because
	// the parapointers wouldn't be able to fit all of the bits
	// otherwise. 119 is just the size of the header.
	const size_t fplen = slurp_length(fp);
	if (fplen <= 119 || fplen > UINT16_MAX)
		return 0;

#define READ_VALUE(name) \
	do { if (slurp_read(fp, &hdr->name, sizeof(hdr->name)) != sizeof(hdr->name)) { unslurp(fp); return 0; } } while (0)

	READ_VALUE(id);
	if (memcmp(hdr->id, "JCH\x26\x02\x66", sizeof(hdr->id)))
		return 0;

	// this should always be zero?
	READ_VALUE(type);
	if (hdr->type)
		return 0;

	READ_VALUE(version);
	if (hdr->version != 4)
		return 0;

	READ_VALUE(speed);

	// > EdLib always sets offset 0009h to 01h. You cannot make more than
	// > one piece of music at a time in the editor.
	READ_VALUE(subsongs);
	if (hdr->subsongs != 1)
		return 0;

	READ_VALUE(soundcard);
	if (hdr->soundcard != 0)
		return 0;

	READ_VALUE(title);
	READ_VALUE(author);
	READ_VALUE(reserved);

	READ_VALUE(tpoin);
	hdr->tpoin = bswapLE16(hdr->tpoin);
	READ_VALUE(sequence_paraptr);
	hdr->sequence_paraptr = bswapLE16(hdr->sequence_paraptr);
	READ_VALUE(instrument_paraptr);
	hdr->instrument_paraptr = bswapLE16(hdr->instrument_paraptr);
	READ_VALUE(info_paraptr);
	hdr->info_paraptr = bswapLE16(hdr->info_paraptr);
	READ_VALUE(spfx_paraptr);
	hdr->spfx_paraptr = bswapLE16(hdr->spfx_paraptr);
	READ_VALUE(endmark);
	hdr->endmark = bswapLE16(hdr->endmark);

	// verify the parapointers
	if (hdr->tpoin < 119
		|| hdr->sequence_paraptr < 119
		|| hdr->instrument_paraptr < 119
		|| hdr->info_paraptr < 119
		|| hdr->spfx_paraptr < 119
		|| hdr->endmark < 119)
		return 0;

#undef READ_VALUE

	return 1;
}

int fmt_d00_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct d00_header hdr;
	if (!d00_header_read(&hdr, fp))
		return 0;

	file->title = strn_dup((const char *)hdr.title, sizeof(hdr.title));
	file->description = "EdLib Tracker D00";
	file->type = TYPE_MODULE_S3M;
	return 1;
}
