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
#include "fmt.h"
#include "mem.h"

#define MMD0_ID "MMD0"

struct MMD0
{
	/* 0 */  char     id[4];
	/* 4 */  uint32_t modlen;
	/* 8 */  uint32_t song_ptr; /* struct MMD0song */
	/* 12 */ uint16_t psecnum;    /* for the player routine, MMD2 only */
	/* 14 */ uint16_t pseq;       /*  "   "   "   "    */
	/* 16 */ uint32_t blockarr_ptr; /* struct MMD0Block *[] (array of pointers) */
	/* 20 */ uint8_t  mmdflags;
	/* 21 */ uint8_t  reserved[3];
	/* 24 */ uint32_t smplarr_ptr; /* struct InstrHdr *[] (array of pointers) */
	/* 28 */ uint32_t reserved2;
	/* 32 */ uint32_t expdata_ptr; /* struct MMD0exp <-- song name is hiding in here */
	/* 36 */ uint32_t reserved3;
	/* 40 */ uint16_t pstate;  /* some data for the player routine */
	/* 42 */ uint16_t pblock;
	/* 44 */ uint16_t pline;
	/* 46 */ uint16_t pseqnum;
	/* 48 */ int16_t  actplayline;
	/* 50 */ uint8_t  counter;
	/* 51 */ uint8_t  extra_songs; /* number of songs - 1 */
};

struct MMD0exp
{
	/*  0 */ uint32_t nextmod_ptr; /* struct MMD0 */
	/*  4 */ uint32_t exp_smp_ptr; /* struct InstrExp */
	/*  8 */ uint16_t s_ext_entries;
	/* 10 */ uint16_t s_ext_entrsz;
	/* 12 */ uint32_t annotxt_ptr; /* char[] */
	/* 16 */ uint32_t annolen;
	/* 20 */ uint32_t iinfo_ptr; /* struct MMDInstrInfo */
	/* 24 */ uint16_t i_ext_entries;
	/* 26 */ uint16_t i_ext_entrsz;
	/* 28 */ uint32_t jumpmask;
	/* 32 */ uint32_t rgbtable_ptr; /* uint16_t[] */
	/* 36 */ uint8_t  channelsplit[4];
	/* 40 */ uint32_t n_info_ptr /* struct NotationInfo */;
	/* 44 */ uint32_t songname_ptr; /* char[] */
	/* 48 */ uint32_t songnamelen;
	/* 52 */ uint32_t dumps_ptr; /* struct MMDDumpData */
	/* 56 */ uint32_t mmdinfo_ptr; /* struct MMDInfo */
	/* 60 */ uint32_t mmdrexx_ptr; /* struct MMDARexx */
	/* 64 */ uint32_t mmdcmd3x_ptr; /* struct MMDMIDICmd3x */
	/* 68 */ uint32_t reserved2[3];
	/* 80 */ uint32_t tag_end;
};

/* --------------------------------------------------------------------- */

int fmt_med_read_info(dmoz_file_t *file, slurp_t *fp)
{
	struct MMD0 hdr;
	struct MMD0exp exp;

	if (!slurp_available(fp, 52, SEEK_CUR)) /* sizeof(hdr) */
		return 0;

	if ((slurp_read(fp, hdr.id, 4) != 4)
			|| memcmp(hdr.id, MMD0_ID, 4))
		return 0;

	if (slurp_seek(fp, 32, SEEK_SET)
			|| (slurp_read(fp, &hdr.expdata_ptr, sizeof(hdr.expdata_ptr)) != sizeof(hdr.expdata_ptr)))
		return 0;

	uint32_t exp_struct_ptr = bswapBE32(hdr.expdata_ptr);

	if (!slurp_available(fp, exp_struct_ptr + 84 /* sizeof(struct MMD0exp) */, SEEK_SET)
			|| slurp_seek(fp, exp_struct_ptr + 44, SEEK_SET)
			|| (slurp_read(fp, &exp.songname_ptr, sizeof(exp.songname_ptr)) != sizeof(exp.songname_ptr))
			|| (slurp_read(fp, &exp.songnamelen, sizeof(exp.songnamelen)) != sizeof(exp.songnamelen)))
		return 0;

	uint32_t name_ptr = bswapBE32(exp.songname_ptr);
	uint32_t name_len = bswapBE32(exp.songnamelen);

	char *name_buffer = mem_alloc(name_len + 1);

	if (!name_buffer)
		return 0;

	name_buffer[name_len] = 0;

	if (!slurp_available(fp, name_ptr + name_len, SEEK_SET)
			|| slurp_seek(fp, name_ptr, SEEK_SET)
			|| (slurp_read(fp, name_buffer, name_len) != name_len)) {
		free(name_buffer);
		return 0;
	}

	file->description = "OctaMed";
	file->title = name_buffer;
	file->type = TYPE_MODULE_MOD; // err, more like XM for Amiga
	return 1;
}

