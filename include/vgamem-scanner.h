/* should be included inside draw-char.c */
void F1(unsigned int ry,
		unsigned SIZE *out, unsigned int tc[16])
{
	unsigned int *bp;
	unsigned int dg;
	unsigned char mb[2];
	unsigned int mx;
	byte *bf, *hf, *ef;
	int x, y, fg,bg, c;

	_video_scanmouse(ry, mb, &mx);

	y = ry >> 3;
	bp = &vgamem_read[y * 80];
	bf = font_data + (ry & 7);
	hf = font_half_data + ((ry & 7) >> 1);
	ef = font_extra + (ry & 7);

	for (x = 0; x < 80; x++, bp++) {
		if (*bp & 0x80000000) {
			/* extra character */
			fg = (*bp >> 23) & 7;
			bg = (*bp >> 27) & 7;
			dg = ef[(*bp & 0xFFFF)<< 3];
			if (x == mx) dg ^= mb[0];
			else if (x == mx+1) dg ^= mb[1];
			*out++ = tc[(dg & 0x80) ? fg : bg];
			*out++ = tc[(dg & 0x40) ? fg : bg];
			*out++ = tc[(dg & 0x20) ? fg : bg];
			*out++ = tc[(dg & 0x10) ? fg : bg];
			*out++ = tc[(dg & 0x8) ? fg : bg];
			*out++ = tc[(dg & 0x4) ? fg : bg];
			*out++ = tc[(dg & 0x2) ? fg : bg];
			*out++ = tc[(dg & 0x1) ? fg : bg];

		} else if (*bp & 0x40000000) {
			/* half-width character */
			fg = (*bp >> 22) & 15;
			bg = (*bp >> 18) & 15;
			dg = hf[ _unpack_halfw((*bp >>9) & 31) << 2];
			if (!(ry & 1)) dg = (dg >> 4);
			if (x == mx) dg ^= mb[0];
			else if (x == mx+1) dg ^= mb[1];

			*out++ = tc[(dg & 0x8) ? fg : bg];
			*out++ = tc[(dg & 0x4) ? fg : bg];
			*out++ = tc[(dg & 0x2) ? fg : bg];
			*out++ = tc[(dg & 0x1) ? fg : bg];
			fg = (*bp >> 26) & 15;
			bg = (*bp >> 14) & 15;
			dg = hf[ _unpack_halfw((*bp >> 4) & 31) << 2];
			if (!(ry & 1)) dg = (dg >> 4);
			if (x == mx) dg ^= mb[0];
			else if (x == mx+1) dg ^= mb[1];

			*out++ = tc[(dg & 0x8) ? fg : bg];
			*out++ = tc[(dg & 0x4) ? fg : bg];
			*out++ = tc[(dg & 0x2) ? fg : bg];
			*out++ = tc[(dg & 0x1) ? fg : bg];
		} else {
			/* regular character */
			fg = (*bp & 0x0F00) >> 8;
			bg = (*bp & 0xF000) >> 12;
			dg = bf[(*bp & 0xFF)<< 3];
			if (x == mx) dg ^= mb[0];
			else if (x == mx+1) dg ^= mb[1];
			if (!(*bp & 0xFF)) fg = 3;
			*out++ = tc[(dg & 0x80) ? fg : bg];
			*out++ = tc[(dg & 0x40) ? fg : bg];
			*out++ = tc[(dg & 0x20) ? fg : bg];
			*out++ = tc[(dg & 0x10) ? fg : bg];
			*out++ = tc[(dg & 0x8) ? fg : bg];
			*out++ = tc[(dg & 0x4) ? fg : bg];
			*out++ = tc[(dg & 0x2) ? fg : bg];
			*out++ = tc[(dg & 0x1) ? fg : bg];
		}
	}
}
