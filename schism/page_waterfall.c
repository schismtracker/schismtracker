/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
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
#include "it.h"
#include "page.h"

#include <math.h>

#define NATIVE_SCREEN_WIDTH     640
#define NATIVE_SCREEN_HEIGHT    400
#define FUDGE_256_TO_WIDTH	4
#define SCOPE_ROWS	32

void vis_init(void);
void vis_work_16s(short *in, int inlen);
void vis_work_16m(short *in, int inlen);
void vis_work_8s(char *in, int inlen);
void vis_work_8m(char *in, int inlen);

/* variables :) */
static int mono = 0;
static int depth = 1;

/* get the _whole_ display */
static struct vgamem_overlay ovl = { 0, 0, 79, 49,
					0,0,0,0 };

/* consts */
#define FFT_BUFFER_SIZE_LOG	9
#define FFT_BUFFER_SIZE		512 /*(1 << FFT_BUFFER_SIZE_LOG)*/
#define FFT_OUTPUT_SIZE		256 /* FFT_BUFFER_SIZE/2 */
#define PI      ((double)3.14159265358979323846)

/* tables */
static unsigned int bit_reverse[FFT_BUFFER_SIZE];
static float precos[FFT_OUTPUT_SIZE];
static float presin[FFT_OUTPUT_SIZE];

/* fft state */
static float state_real[FFT_BUFFER_SIZE];
static float state_imag[FFT_BUFFER_SIZE];

static int _reverse_bits(unsigned int in) {
	unsigned int r = 0, n;
	for (n = 0; n < FFT_BUFFER_SIZE_LOG; n++) {
		r <<= 1;
		r += (in & 1);
		in >>= 1;
	}
	return r;
}
void vis_init(void)
{
	unsigned n;

	for (n = 0; n < FFT_BUFFER_SIZE; n++) {
		bit_reverse[n] = _reverse_bits(n);
	}
	for (n = 0; n < FFT_OUTPUT_SIZE; n++) {
		float j = (2.0*PI) * n / FFT_BUFFER_SIZE;
		precos[n] = cos(j);
		presin[n] = sin(j);
	}
}

/* samples should already be averaged */
static void _vis_data_work(short output[FFT_OUTPUT_SIZE],
			short input[FFT_BUFFER_SIZE])
{
	unsigned int n, k, y;
	unsigned int ex, ff;
	float fr, fi;
	float tr, ti;
	float out;
	int yp;

	/* fft */
	float *rp = state_real;
	float *ip = state_imag;
	for (n = 0; n < FFT_BUFFER_SIZE; n++) {
		*rp++ = input[ bit_reverse[n] ];
		*ip++ = 0;
	}
	ex = 1;
	ff = FFT_OUTPUT_SIZE;
	for (n = FFT_BUFFER_SIZE_LOG; n != 0; n--) {
		for (k = 0; k != ex; k++) {
			fr = precos[k * ff];
			fi = presin[k * ff];
			for (y = k; y < FFT_BUFFER_SIZE; y += ex << 1) {
				yp = y + ex;
				tr = fr * state_real[yp] - fi * state_imag[yp];
				ti = fr * state_imag[yp] + fi * state_real[yp];
				state_real[yp] = state_real[y] - tr;
				state_imag[yp] = state_imag[y] - ti;
				state_real[y] += tr;
				state_imag[y] += ti;
			}
		}
		ex <<= 1;
		ff >>= 1;
	}

	/* collect fft */
	rp = state_real; rp++;
	ip = state_imag; ip++;
	for (n = 0; n < FFT_OUTPUT_SIZE; n++) {
		out = ((*rp) * (*rp)) + ((*ip) * (*ip));
		output[n] = ((int)sqrt(out)) >> 8;
		rp++;ip++;
	}

	/* scale these back a bit */
	output[0] /= 4;
	output[(FFT_OUTPUT_SIZE-1)] /= 4;
}
static int _logscale(int j) {
	if (j >= 0x4000) return 15;
	else if (j >= 0x2000) return 14;
	else if (j >= 0x1000) return 13;
	else if (j >= 0x0800) return 12;
	else if (j >= 0x0400) return 11;
	else if (j >= 0x0200) return 10;
	else if (j >= 0x0100) return 9;
	else if (j >= 0x0080) return 8;
	else if (j >= 0x0040) return 7;
	else if (j >= 0x0020) return 6;
	else if (j >= 0x0010) return 5;
	else if (j >= 0x0008) return 4;
	else if (j >= 0x0004) return 3;
	else if (j >= 0x0002) return 2;
	else if (j >= 0x0001) return 1;
	return 0;
}
static unsigned char *_dobits(unsigned char *q,
			short d[FFT_OUTPUT_SIZE], int m, int y)
{
	int i, j, c;
	const int cbits[] = { 0,7,1,2,6,12,11,3,3 };
	for (i = 0; i < FFT_OUTPUT_SIZE; i++) {
		/* eh... */
		j = d[i];
		if (depth < 0)
			j >>= -depth;
		else
			j <<= depth;

		c = cbits[ (_logscale(j)+1)>>1 ];
		*q = c; q += y;
		if (m) { *q = c; q += y; }
		if ((i % FUDGE_256_TO_WIDTH) == 0) {
			/* each band is 2.50 px wide;
			 * output display is 640 px
			 */
			*q = c; q += y;
			if (m) { *q = c; q += y; }
		}
	}
	return q;
}
static void _vis_process(short f[2][FFT_OUTPUT_SIZE])
{
	unsigned char *q;
	int i, j;

	vgamem_lock();

	/* move up by one pixel */
	memcpy(ovl.q, ovl.q+NATIVE_SCREEN_WIDTH,
			(NATIVE_SCREEN_WIDTH*
				((NATIVE_SCREEN_HEIGHT-1)-SCOPE_ROWS)));
	q = ovl.q + (NATIVE_SCREEN_WIDTH*
			((NATIVE_SCREEN_HEIGHT-1)-SCOPE_ROWS));

	if (mono) {
		for (i = 0; i < FFT_OUTPUT_SIZE; i++)
			f[0][i] = (f[0][i] + f[1][i]) / 2;
		_dobits(q, f[0], 1, 1);
	} else {
		_dobits(q+320, f[0], 0, -1);
		_dobits(q+320, f[1], 0, 1);
	}

	/* draw the scope at the bottom */
	q = ovl.q + (NATIVE_SCREEN_WIDTH*(NATIVE_SCREEN_HEIGHT-SCOPE_ROWS));
	i = SCOPE_ROWS*NATIVE_SCREEN_WIDTH;
	memset(q,0,i);
	for (i = j = 0; i < FFT_OUTPUT_SIZE; i++) {
		vgamem_ovl_drawline(&ovl,
			j, NATIVE_SCREEN_HEIGHT-(2*_logscale(f[0][i])),
			j, NATIVE_SCREEN_HEIGHT, 5);
		j++;
		if ((i % FUDGE_256_TO_WIDTH) == 0) {
			vgamem_ovl_drawline(&ovl,
				j, NATIVE_SCREEN_HEIGHT-(2*_logscale(f[0][i])),
				j, NATIVE_SCREEN_HEIGHT, 5);
			j++;
			vgamem_ovl_drawline(&ovl,
				j, NATIVE_SCREEN_HEIGHT-(2*_logscale(f[1][i])),
				j, NATIVE_SCREEN_HEIGHT, 5);
			j++;
		}
		vgamem_ovl_drawline(&ovl,
			j, NATIVE_SCREEN_HEIGHT-(2*_logscale(f[1][i])),
			j, NATIVE_SCREEN_HEIGHT, 5);
		j++;
	}

	vgamem_unlock();
}

void vis_work_16s(short *in, int inlen)
{
	short dl[FFT_BUFFER_SIZE];
	short dr[FFT_BUFFER_SIZE];
	short f[2][FFT_OUTPUT_SIZE];
	int i, j, k;

	for (i = 0; i < FFT_BUFFER_SIZE;) {
		for (k = j = 0; k < inlen && i < FFT_BUFFER_SIZE; k++, i++) {
			dl[i] = in[j]; j++;
			dr[i] = in[j]; j++;
		}
	}
	_vis_data_work(f[0], dl);
	_vis_data_work(f[1], dr);
	_vis_process(f);
}
void vis_work_16m(short *in, int inlen)
{
	short d[FFT_BUFFER_SIZE];
	short f[2][FFT_OUTPUT_SIZE];
	int i, k;

	for (i = 0; i < FFT_BUFFER_SIZE;) {
		for (k = 0; k < inlen && i < FFT_BUFFER_SIZE; k++, i++) {
			d[i] = in[k];
		}
	}
	_vis_data_work(f[0], d);
	memcpy(f[1], f[0], FFT_OUTPUT_SIZE * 2);
	_vis_process(f);
}

void vis_work_8s(char *in, int inlen)
{
	short dl[FFT_BUFFER_SIZE];
	short dr[FFT_BUFFER_SIZE];
	short f[2][FFT_OUTPUT_SIZE];
	int i, j, k;

	for (i = 0; i < FFT_BUFFER_SIZE;) {
		for (k = j = 0; k < inlen && i < FFT_BUFFER_SIZE; k++, i++) {
			dl[i] = ((short)in[j]) * 256; j++;
			dr[i] = ((short)in[j]) * 256; j++;
		}
	}
	_vis_data_work(f[0], dl);
	_vis_data_work(f[1], dr);
	_vis_process(f);
}
void vis_work_8m(char *in, int inlen)
{
	short d[FFT_BUFFER_SIZE];
	short f[2][FFT_OUTPUT_SIZE];
	int i, k;

	for (i = 0; i < FFT_BUFFER_SIZE;) {
		for (k = 0; k < inlen && i < FFT_BUFFER_SIZE; k++, i++) {
			d[i] = ((short)in[k]) * 256;
		}
	}
	_vis_data_work(f[0], d);
	memcpy(f[1], f[0], FFT_OUTPUT_SIZE * 2);
	_vis_process(f);
}

static void draw_screen(void)
{
	/* waterfall uses a single overlay */
	vgamem_ovl_apply(&ovl);
}

static int waterfall_handle_key(struct key_event *k)
{
	switch (k->sym) {
	case SDLK_s:
	case SDLK_m:
		if (k->state) return 1;
		mono = !mono;
		break;

	case SDLK_LEFT:
		if (k->state) return 1;
		depth--;
		break;
	case SDLK_RIGHT:
		if (k->state) return 1;
		depth++;
		break;

	default:
		return 0;
	};

	depth = CLAMP(depth, -5, 5);
	return 1;
}


static struct widget waterfall_widget_hack[1];
static void do_nil(void) {}

static void waterfall_set_page(void)
{
	vgamem_ovl_clear(&ovl, 0);
}
void waterfall_load_page(struct page *page)
{
	vgamem_ovl_alloc(&ovl);
        page->title = "";
        page->draw_full = draw_screen;
	page->set_page = waterfall_set_page;
        page->total_widgets = 1;
        page->widgets = waterfall_widget_hack;
	create_other(waterfall_widget_hack, 0, waterfall_handle_key, do_nil);
}
