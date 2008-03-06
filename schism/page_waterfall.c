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
#include "song.h"

#include <math.h>

#define NATIVE_SCREEN_WIDTH     640
#define NATIVE_SCREEN_HEIGHT    400
#define FUDGE_256_TO_WIDTH	4
#define SCOPE_ROWS	32


/* consts */
#define FFT_BUFFER_SIZE_LOG	9
#define FFT_BUFFER_SIZE		512 /*(1 << FFT_BUFFER_SIZE_LOG)*/
#define FFT_OUTPUT_SIZE		256 /* FFT_BUFFER_SIZE/2 */
#define PI      ((double)3.14159265358979323846)

short current_fft_data[2][FFT_OUTPUT_SIZE];


void vis_init(void);
void vis_work_16s(short *in, int inlen);
void vis_work_16m(short *in, int inlen);
void vis_work_8s(char *in, int inlen);
void vis_work_8m(char *in, int inlen);

/* variables :) */
static int mono = 0;
static int gain = -5;

/* get the _whole_ display */
static struct vgamem_overlay ovl = { 0, 0, 79, 49,
					0,0,0,0 };

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
static inline void _vis_data_work(short output[FFT_OUTPUT_SIZE],
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
		output[n] = ((int)sqrt(out));
		rp++;ip++;
	}

	/* scale these back a bit */
	output[0] /= 4;
	output[(FFT_OUTPUT_SIZE-1)] /= 4;
}
static inline unsigned char *_dobits(unsigned char *q,
			short d[FFT_OUTPUT_SIZE], int m, int y)
{
	int i, j, c;

	for (i = 0; i < FFT_OUTPUT_SIZE; i++) {
		/* eh... */
		j = d[i];
		if (gain < 0)
			j >>= -gain;
		else
			j <<= gain;

		c = 128 + j;
		if (c > 255) c = 255;
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
static inline void _drawslice(int x, int h, int c)
{
	int y;

	y = ((h>>10) & (SCOPE_ROWS-1))+1;
	vgamem_ovl_drawline(&ovl,
		x, (NATIVE_SCREEN_HEIGHT-y),
		x, (NATIVE_SCREEN_HEIGHT-1), c);
}
static void _vis_process(void)
{
	unsigned char *q;
	int i, j, k;

	vgamem_lock();

	/* move up by one pixel */
	memmove(ovl.q, ovl.q+NATIVE_SCREEN_WIDTH,
			(NATIVE_SCREEN_WIDTH*
				((NATIVE_SCREEN_HEIGHT-1)-SCOPE_ROWS)));
	q = ovl.q + (NATIVE_SCREEN_WIDTH*
			((NATIVE_SCREEN_HEIGHT-1)-SCOPE_ROWS));

	if (mono) {
		for (i = 0; i < FFT_OUTPUT_SIZE; i++)
			current_fft_data[0][i] = (current_fft_data[0][i]
					+ current_fft_data[1][i]) / 2;
		_dobits(q, current_fft_data[0], 1, 1);
	} else {
		_dobits(q+320, current_fft_data[0], 0, -1);
		_dobits(q+320, current_fft_data[1], 0, 1);
	}

	/* draw the scope at the bottom */
	q = ovl.q + (NATIVE_SCREEN_WIDTH*(NATIVE_SCREEN_HEIGHT-SCOPE_ROWS));
	i = SCOPE_ROWS*NATIVE_SCREEN_WIDTH;
	memset(q,0,i);
	if (mono) {
		for (i = j = 0; i < FFT_OUTPUT_SIZE; i++) {
			_drawslice(j, current_fft_data[0][i],5);
			j++;
			if ((i % FUDGE_256_TO_WIDTH) == 0) {
				_drawslice(j, current_fft_data[0][i],5);
				j++;
				_drawslice(j, current_fft_data[1][i],5);
				j++;
			}
			_drawslice(j, current_fft_data[1][i],5);
			j++;
		}
	} else {
		j = 0;
		k = NATIVE_SCREEN_WIDTH/2;
		for (i = 0; i < FFT_OUTPUT_SIZE; i++) {
			_drawslice(k-j, current_fft_data[0][i],5);
			_drawslice(k+j, current_fft_data[1][i],5);
			j++;
			if ((i % FUDGE_256_TO_WIDTH) == 0) {
				_drawslice(k-j, current_fft_data[0][i],5);
				_drawslice(k+j, current_fft_data[1][i],5);
				j++;
				_drawslice(k-j, current_fft_data[0][i],5);
				_drawslice(k+j, current_fft_data[1][i],5);
				j++;
			}
			_drawslice(k-j, current_fft_data[0][i],5);
			_drawslice(k+j, current_fft_data[1][i],5);
			j++;
		}
	}

	vgamem_unlock();
	status.flags |= NEED_UPDATE;
}

void vis_work_16s(short *in, int inlen)
{
	short dl[FFT_BUFFER_SIZE];
	short dr[FFT_BUFFER_SIZE];
	int i, j, k;

	if (!inlen) {
		memset(current_fft_data[0], 0, FFT_OUTPUT_SIZE*2);
		memset(current_fft_data[1], 0, FFT_OUTPUT_SIZE*2);
	} else {
		for (i = 0; i < FFT_BUFFER_SIZE;) {
			for (k = j = 0; k < inlen && i < FFT_BUFFER_SIZE; k++, i++) {
				dl[i] = in[j]; j++;
				dr[i] = in[j]; j++;
			}
		}
		_vis_data_work(current_fft_data[0], dl);
		_vis_data_work(current_fft_data[1], dr);
	}
	if (status.current_page == PAGE_WATERFALL) _vis_process();
}
void vis_work_16m(short *in, int inlen)
{
	short d[FFT_BUFFER_SIZE];
	int i, k;

	if (!inlen) {
		memset(current_fft_data[0], 0, FFT_OUTPUT_SIZE*2);
		memset(current_fft_data[1], 0, FFT_OUTPUT_SIZE*2);
	} else {
		for (i = 0; i < FFT_BUFFER_SIZE;) {
			for (k = 0; k < inlen && i < FFT_BUFFER_SIZE; k++, i++) {
				d[i] = in[k];
			}
		}
		_vis_data_work(current_fft_data[0], d);
		memcpy(current_fft_data[1], current_fft_data[0], FFT_OUTPUT_SIZE * 2);
	}
	if (status.current_page == PAGE_WATERFALL) _vis_process();
}

void vis_work_8s(char *in, int inlen)
{
	short dl[FFT_BUFFER_SIZE];
	short dr[FFT_BUFFER_SIZE];
	int i, j, k;

	if (!inlen) {
		memset(current_fft_data[0], 0, FFT_OUTPUT_SIZE*2);
		memset(current_fft_data[1], 0, FFT_OUTPUT_SIZE*2);
	} else {
		for (i = 0; i < FFT_BUFFER_SIZE;) {
			for (k = j = 0; k < inlen && i < FFT_BUFFER_SIZE; k++, i++) {
				dl[i] = ((short)in[j]) * 256; j++;
				dr[i] = ((short)in[j]) * 256; j++;
			}
		}
		_vis_data_work(current_fft_data[0], dl);
		_vis_data_work(current_fft_data[1], dr);
	}
	if (status.current_page == PAGE_WATERFALL) _vis_process();
}
void vis_work_8m(char *in, int inlen)
{
	short d[FFT_BUFFER_SIZE];
	int i, k;

	if (!inlen) {
		memset(current_fft_data[0], 0, FFT_OUTPUT_SIZE*2);
		memset(current_fft_data[1], 0, FFT_OUTPUT_SIZE*2);
	} else {
		for (i = 0; i < FFT_BUFFER_SIZE;) {
			for (k = 0; k < inlen && i < FFT_BUFFER_SIZE; k++, i++) {
				d[i] = ((short)in[k]) * 256;
			}
		}
		_vis_data_work(current_fft_data[0], d);
		memcpy(current_fft_data[1],
				 current_fft_data[0], FFT_OUTPUT_SIZE * 2);
	}
	if (status.current_page == PAGE_WATERFALL) _vis_process();
}

static void draw_screen(void)
{
	/* waterfall uses a single overlay */
	vgamem_ovl_apply(&ovl);
}

static int waterfall_handle_key(struct key_event *k)
{
	int n, v, order, ii;

	if (NO_MODIFIER(k->mod)) {
		if (k->midi_note > -1) {
			n = k->midi_note;
			if (k->midi_volume > -1) {
				v = k->midi_volume / 2;
			} else {
				v = 64;
			}
		} else {
			v = 64;
			n = kbd_get_note(k);
		}
		if (n > -1) {
			if (song_is_instrument_mode()) {
				ii = instrument_get_current();
			} else {
				ii = sample_get_current();
			}
			if (k->state) {
				song_keyup(-1, ii, n, KEYDOWN_CHAN_CURRENT, 0);
				status.last_keysym = 0;
			} else if (!k->is_repeat) {
				song_keydown(-1, ii, n, v, KEYDOWN_CHAN_CURRENT, 0);
			}
			return 1;
		}
	}

	switch (k->sym) {
        case SDLK_s:
		if (k->mod & KMOD_ALT) {
			if (k->state) return 1;
	
			song_toggle_stereo();
                	status.flags |= NEED_UPDATE;
	                return 1;
		}
		return 0;
	case SDLK_m:
		if (k->mod & KMOD_ALT) {
			if (k->state) return 1;
			mono = !mono;
			return 1;
		}
		return 0;
	case SDLK_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
		gain--;
		break;
	case SDLK_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
		gain++;
		break;
        case SDLK_g:
		if (k->mod & KMOD_ALT) {
			if (!k->state) return 1;

			order = song_get_current_order();
			if (song_get_mode() == MODE_PLAYING) {
				n = song_get_orderlist()[order];
			} else {
				n = song_get_playing_pattern();
			}
			if (n < 200) {
				set_current_order(order);
				set_current_pattern(n);
				set_current_row(song_get_current_row());
				set_page(PAGE_PATTERN_EDITOR);
			}
			return 1;
		}
		return 0;
        case SDLK_r:
                if (k->mod & KMOD_ALT) {
			if (k->state) return 1;

                        song_flip_stereo();
                        return 1;
                }
                return 0;
        case SDLK_PLUS:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                if (song_get_mode() == MODE_PLAYING) {
                        song_set_current_order(song_get_current_order() + 1);
                }
                return 1;
        case SDLK_MINUS:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state) return 1;
                if (song_get_mode() == MODE_PLAYING) {
                        song_set_current_order(song_get_current_order() - 1);
                }
                return 1;
	case SDLK_SEMICOLON:
	case SDLK_COLON:
		if (k->state) return 1;
		if (song_is_instrument_mode()) {
			instrument_set(instrument_get_current() - 1);
		} else {
			sample_set(sample_get_current() - 1);
		}
		return 1;
	case SDLK_QUOTE:
	case SDLK_QUOTEDBL:
		if (k->state) return 1;
		if (song_is_instrument_mode()) {
			instrument_set(instrument_get_current() + 1);
		} else {
			sample_set(sample_get_current() + 1);
		}
		return 1;
	case SDLK_COMMA:
	case SDLK_LESS:
		if (k->state) return 1;
		song_change_current_play_channel(-1, 0);
		return 1;
	case SDLK_PERIOD:
	case SDLK_GREATER:
		if (k->state) return 1;
		song_change_current_play_channel(1, 0);
		return 1;
	default:
		return 0;
	};

	gain = CLAMP(gain, -8, 8);
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
