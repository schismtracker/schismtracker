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
#include "it.h"
#include "page.h"
#include "song.h"

#include <math.h>

#define NATIVE_SCREEN_WIDTH     640
#define NATIVE_SCREEN_HEIGHT    400
#define FUDGE_256_TO_WIDTH      4
#define SCOPE_ROWS      32


/* consts */
#define FFT_BUFFER_SIZE_LOG     11
#define FFT_BUFFER_SIZE         2048 /*(1 << FFT_BUFFER_SIZE_LOG)*/
#define FFT_OUTPUT_SIZE         1024 /* FFT_BUFFER_SIZE/2 */  /*WARNING: Hardcoded in page.c when declaring current_fft_data*/
#define FFT_BANDS_SIZE          256    /*WARNING: Hardcoded in page.c when declaring fftlog and when using it in vis_fft*/
#define PI      ((double)3.14159265358979323846)
/*This value is used internally to scale the power output of the FFT to decibells.*/
static const float fft_inv_bufsize = 1.0f/(FFT_BUFFER_SIZE>>2);
/*Scaling for FFT. Input is expected to be signed short int.*/
static const float inv_s_range = 1.f/32768.f;

short current_fft_data[2][FFT_OUTPUT_SIZE];
/*Table to change the scale from linear to log.*/
short fftlog[FFT_BANDS_SIZE];

void vis_init(void);
void vis_work_16s(short *in, int inlen);
void vis_work_16m(short *in, int inlen);
void vis_work_8s(char *in, int inlen);
void vis_work_8m(char *in, int inlen);

/* variables :) */
static int mono = 0;
//gain, in dBs.
//static int gain = 0;
static int noisefloor=72;

/* get the _whole_ display */
static struct vgamem_overlay ovl = { 0, 0, 79, 49, NULL, 0, 0, 0 };

/* tables */
static unsigned int bit_reverse[FFT_BUFFER_SIZE];
static float window[FFT_BUFFER_SIZE];
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
#if 0
		/*Rectangular/none*/
		window[n] = 1;
		/*Cosine/sine window*/
		window[n] = sin(PI * n/ FFT_BUFFER_SIZE -1);
		/*Hann Window*/
		window[n] = 0.50f - 0.50f * cos(2.0*PI * n / (FFT_BUFFER_SIZE - 1));
		/*Hamming Window*/
		window[n] = 0.54f - 0.46f * cos(2.0*PI * n / (FFT_BUFFER_SIZE - 1));
		/*Gaussian*/
		window[n] = powf(M_E,-0.5f *pow((n-(FFT_BUFFER_SIZE-1)/2.f)/(0.4*(FFT_BUFFER_SIZE-1)/2.f),2.f));
		/*Blackmann*/
		window[n] = 0.42659 - 0.49656 * cos(2.0*PI * n/ (FFT_BUFFER_SIZE-1)) + 0.076849 * cos(4.0*PI * n /(FFT_BUFFER_SIZE-1));
		/*Blackman-Harris*/
		window[n] = 0.35875 - 0.48829 * cos(2.0*PI * n/ (FFT_BUFFER_SIZE-1)) + 0.14128 * cos(4.0*PI * n /(FFT_BUFFER_SIZE-1)) - 0.01168 * cos(6.0*PI * n /(FFT_BUFFER_SIZE-1));
#endif
		/*Hann Window*/
		window[n] = 0.50f - 0.50f * cos(2.0*PI * n / (FFT_BUFFER_SIZE - 1));
	}
	for (n = 0; n < FFT_OUTPUT_SIZE; n++) {
		float j = (2.0*PI) * n / FFT_BUFFER_SIZE;
		precos[n] = cos(j);
		presin[n] = sin(j);
	}
#if 0
	/*linear*/
	fftlog[n]=n;
#elif 1
	/*exponential.*/
	float factor = (float)FFT_OUTPUT_SIZE/(FFT_BANDS_SIZE*FFT_BANDS_SIZE);
	for (n = 0; n < FFT_BANDS_SIZE; n++ ) {
		fftlog[n]=n*n*factor;
	}
#else
	/*constant note scale.*/
	float factor = 8.f/(float)FFT_BANDS_SIZE;
	float factor2 = (float)FFT_OUTPUT_SIZE/256.f;
	for (n = 0; n < FFT_BANDS_SIZE; n++ ) {
		fftlog[n]=(powf(2.0f,n*factor)-1.f)*factor2;
	}
#endif
}

/*
* Understanding In and Out:
* input is the samples (so, it is amplitude). The scale is expected to be signed 16bits.
*    The window function calculated in "window" will automatically be applied.
* output is a value between 0 and 128 representing 0 = noisefloor variable
*    and 128 = 0dBFS (deciBell, FullScale) for each band.
*/
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
		int nr = bit_reverse[n];
		*rp++ = (float)input[ nr ] * inv_s_range * window[nr];
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
	const float fft_dbinv_bufsize = dB(fft_inv_bufsize);
	for (n = 0; n < FFT_OUTPUT_SIZE; n++) {
		/* "out" is the total power for each band.
		* To get amplitude from "output", use sqrt(out[N])/(sizeBuf>>2)
		* To get dB from "output", use powerdB(out[N])+db(1/(sizeBuf>>2)).
		* powerdB is = 10 * log10(in)
		* dB is = 20 * log10(in)
		*/
		out = ((*rp) * (*rp)) + ((*ip) * (*ip));
		/* +0.0000000001f is -100dB of power. Used to prevent evaluating powerdB(0.0) */
		output[n] = pdB_s(noisefloor, out+0.0000000001f,fft_dbinv_bufsize);
		rp++;ip++;
	}
}
/* convert the fft bands to columns of screen
out and d have a range of 0 to 128 */
static inline void _get_columns_from_fft(unsigned char *out,
				short d[FFT_OUTPUT_SIZE], int m)
{
	int i, j, a;
	for (i = 0, a=0; i < FFT_BANDS_SIZE; i++)  {
		float afloat = fftlog[i];
		float floora = floor(afloat);
		if ((i == FFT_BANDS_SIZE -1) || (afloat + 1.0f > fftlog[i+1])) {
			a = (int)floora;
			j = d[a] + (d[a+1]-d[a])*(afloat-floora);
			a = floor(afloat+0.5f);
		}
		else {
			j=d[a];
			while(a<=afloat){
				j = MAX(j,d[a]);
				a++;
			}
		}
		*out = j; out++;
		/*If mono, repeat the value.*/
		if (m) { *out = j; out++; }
		if ((i % FUDGE_256_TO_WIDTH) == 0) {
			/* each band is 2.50 px wide;
			 * output display is 640 px
			 */
			*out = j; out++;
			/*If mono, repeat the value.*/
			if (m) { *out = j; out++; }
		}
	}
}

/* Convert the output of */
static inline unsigned char *_dobits(unsigned char *q,
			unsigned char *in, int length, int y)
{
	int i, c;
	for (i = 0; i < length; i++)  {
		/* j is has range 0 to 128. Now use the upper side for drawing.*/
		c = 128 + in[i];
		if (c > 255) c = 255;
		*q = c; q += y;
	}
	return q;
}
/*x = screen.x, h = 0..128, c = colour */
static inline void _drawslice(int x, int h, int c)
{
	int y;

	y = ((h>>2) & (SCOPE_ROWS-1))+1;
	vgamem_ovl_drawline(&ovl,
		x, (NATIVE_SCREEN_HEIGHT-y),
		x, (NATIVE_SCREEN_HEIGHT-1), c);
}
static void _vis_process(void)
{
	unsigned char *q;
	int i, k;
	k = NATIVE_SCREEN_WIDTH/2;
	unsigned char outfft[NATIVE_SCREEN_WIDTH];

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
		_get_columns_from_fft(outfft, current_fft_data[0], 1);
		_dobits(q, outfft, NATIVE_SCREEN_WIDTH, 1);
	} else {
		_get_columns_from_fft(outfft, current_fft_data[0], 0);
		_dobits(q+k, outfft, k, -1);
		_get_columns_from_fft(outfft+k, current_fft_data[1], 0);
		_dobits(q+k, outfft+k, k, 1);
	}

	/* draw the scope at the bottom */
	q = ovl.q + (NATIVE_SCREEN_WIDTH*(NATIVE_SCREEN_HEIGHT-SCOPE_ROWS));
	i = SCOPE_ROWS*NATIVE_SCREEN_WIDTH;
	memset(q,0,i);
	if (mono) {
		for (i = 0; i < NATIVE_SCREEN_WIDTH; i++) {
			_drawslice(i, outfft[i],5);
		}
	} else {
		for (i = 0; i < k; i++) {
			_drawslice(k-i-1, outfft[i],5);
		}
		for (i = 0; i < k; i++) {
			_drawslice(k+i, outfft[k+i],5);
		}
	}

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
			if (k->state == KEY_RELEASE) {
				song_keyup(KEYJAZZ_NOINST, ii, n);
				status.last_keysym.sym = 0;
			} else if (!k->is_repeat) {
				song_keydown(KEYJAZZ_NOINST, ii, n, v, KEYJAZZ_CHAN_CURRENT);
			}
			return 1;
		}
	}

	switch (k->sym.sym) {
	case SDLK_s:
		if (k->mod & KMOD_ALT) {
			if (k->state == KEY_RELEASE)
				return 1;

			song_toggle_stereo();
			status.flags |= NEED_UPDATE;
			return 1;
		}
		return 0;
	case SDLK_m:
		if (k->mod & KMOD_ALT) {
			if (k->state == KEY_RELEASE)
				return 1;
			mono = !mono;
			return 1;
		}
		return 0;
	case SDLK_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		noisefloor-=4;
		break;
	case SDLK_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		noisefloor+=4;
		break;
	case SDLK_g:
		if (k->mod & KMOD_ALT) {
			if (k->state == KEY_PRESS)
				return 1;

			order = song_get_current_order();
			if (song_get_mode() == MODE_PLAYING) {
				n = current_song->orderlist[order];
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
			if (k->state == KEY_RELEASE)
				return 1;

			song_flip_stereo();
			return 1;
		}
		return 0;
	case SDLK_PLUS:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_get_mode() == MODE_PLAYING) {
			song_set_current_order(song_get_current_order() + 1);
		}
		return 1;
	case SDLK_MINUS:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_get_mode() == MODE_PLAYING) {
			song_set_current_order(song_get_current_order() - 1);
		}
		return 1;
	case SDLK_SEMICOLON:
	case SDLK_COLON:
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_is_instrument_mode()) {
			instrument_set(instrument_get_current() - 1);
		} else {
			sample_set(sample_get_current() - 1);
		}
		return 1;
	case SDLK_QUOTE:
	case SDLK_QUOTEDBL:
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_is_instrument_mode()) {
			instrument_set(instrument_get_current() + 1);
		} else {
			sample_set(sample_get_current() + 1);
		}
		return 1;
	case SDLK_COMMA:
	case SDLK_LESS:
		if (k->state == KEY_RELEASE)
			return 1;
		song_change_current_play_channel(-1, 0);
		return 1;
	case SDLK_PERIOD:
	case SDLK_GREATER:
		if (k->state == KEY_RELEASE)
			return 1;
		song_change_current_play_channel(1, 0);
		return 1;
	default:
		return 0;
	};

	noisefloor = CLAMP(noisefloor, 36, 96);
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
