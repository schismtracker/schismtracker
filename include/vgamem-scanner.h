/* should be included inside draw-char.c */
void F1(unsigned int ry, unsigned SIZE *out, unsigned int tc[16], unsigned int mouseline[80])
{
	unsigned int *bp;
	unsigned int dg;
	unsigned char *q;
	uint8_t *itf, *bios, *bioslow, *hf;
	unsigned int x, y;
	int fg, bg;

	q = ovl + (ry * 640);
	y = ry >> 3;
	bp = &vgamem_read[y * 80];
	itf = font_data + (ry & 7);
	bios = ((uint8_t*) font_default_upper_alt) + (ry & 7);
	bioslow = ((uint8_t *) font_default_lower) + (ry & 7);
	hf = font_half_data + ((ry & 7) >> 1);

	for (x = 0; x < 80; x++, bp++, q += 8) {
		if (*bp & 0x80000000) {
			*out++ = tc[ (q[0]^((mouseline[x] & 0x80)?15:0)) & 255];
			*out++ = tc[ (q[1]^((mouseline[x] & 0x40)?15:0)) & 255];
			*out++ = tc[ (q[2]^((mouseline[x] & 0x20)?15:0)) & 255];
			*out++ = tc[ (q[3]^((mouseline[x] & 0x10)?15:0)) & 255];
			*out++ = tc[ (q[4]^((mouseline[x] & 0x08)?15:0)) & 255];
			*out++ = tc[ (q[5]^((mouseline[x] & 0x04)?15:0)) & 255];
			*out++ = tc[ (q[6]^((mouseline[x] & 0x02)?15:0)) & 255];
			*out++ = tc[ (q[7]^((mouseline[x] & 0x01)?15:0)) & 255];

		} else if (*bp & 0x40000000) {
			/* half-width character */
			fg = (*bp >> 22) & 15;
			bg = (*bp >> 18) & 15;
			dg = hf[ _unpack_halfw((*bp >> 7) & 127) << 2];
			if (!(ry & 1))
				dg = (dg >> 4);
			dg ^= mouseline[x];

			*out++ = tc[(dg & 0x8) ? fg : bg];
			*out++ = tc[(dg & 0x4) ? fg : bg];
			*out++ = tc[(dg & 0x2) ? fg : bg];
			*out++ = tc[(dg & 0x1) ? fg : bg];
			fg = (*bp >> 26) & 15;
			bg = (*bp >> 14) & 15;
			dg = hf[ _unpack_halfw((*bp) & 127) << 2];
			if (!(ry & 1))
				dg = (dg >> 4);
			dg ^= mouseline[x];

			*out++ = tc[(dg & 0x8) ? fg : bg];
			*out++ = tc[(dg & 0x4) ? fg : bg];
			*out++ = tc[(dg & 0x2) ? fg : bg];
			*out++ = tc[(dg & 0x1) ? fg : bg];
		} else {
			/* regular character */
			fg = (*bp & 0x0F00) >> 8;
			bg = (*bp & 0xF000) >> 12;
			if (*bp & 0x10000000 && (*bp & 0x80)) {
				dg = bios[(*bp & 0x7F)<< 3];
			} else if (*bp & 0x10000000) {
				dg = bioslow[(*bp & 0x7F)<< 3];
			} else {
				dg = itf[(*bp & 0xFF)<< 3];
			}
			dg ^= mouseline[x];
			if (!(*bp & 0xFF))
				fg = 3;
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
