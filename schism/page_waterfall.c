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
#include "keyboard.h"
#include "page.h"
#include "song.h"
#include "widget.h"
#include "vgamem.h"
#include "mem.h"

/* #define for using one malloc call for all tables */
#define FFT_USE_ONE_MALLOC 1

#define NATIVE_SCREEN_WIDTH     640
#define NATIVE_SCREEN_HEIGHT    400
#define SCOPE_ROWS      32

/* 1920 = least common multiple of 120, 640, and 320 */
#define FFT_BANDS_SIZE          (1920)

/* current FFT size. this should always be a power of 2. */
static uint32_t fft_size;
static uint32_t fft_bufsize; /* fft_size * 2 */
static uint32_t fft_sizelog2; /* log2(fft_bufsize) */

/* This value is used internally to scale the power output of the FFT to decibels. */
static float fft_inv_bufsize;
/* Scaling for FFT. Input is expected to be int16_t. */
static const float inv_s_range = 1.0f/32768.0f;

/* Table to change the scale from linear to log. */
static uint32_t fftlog[FFT_BANDS_SIZE];

/* variables :) */
static int mono = 0;
//gain, in dBs.
//static int gain = 0;
static int noisefloor=72;

/* get the _whole_ display */
static struct vgamem_overlay ovl = { 0, 0, 79, 49, NULL, 0, 0, 0 };

#define FFT_VAR(type, name, size) \
	static type *name;
#include "fft-vars.h"

#ifdef FFT_USE_ONE_MALLOC
/* THE big fft allocation ;) */
static void *fft_allocation;
#endif

static inline SCHISM_ALWAYS_INLINE
uint32_t _reverse_bits(uint32_t in, uint32_t bufsizelog)
{
	/* This outputs less instructions than the old plain C algorithm,
	 * because breverse32 uses a divide-and-conquer algorithm rather
	 * than one-at-a-time. Whether it really performs better is anyone's
	 * guess ;) */
	return breverse32(in) >> (32 - bufsizelog);
}

/* Ok, some explanation:
 * ALL of our FFT buffers (state, etc) are reliant on one size, and all need
 * to be allocated at once.
 * So,  */
static void vis_realloc(uint32_t size)
{
	uint32_t bufsize;
	char *ptr;

	/* this could be <= as well */
	if (size == fft_size)
		return; /* No need to do anything */

	/* size MUST be a power of 2 here */
	SCHISM_RUNTIME_ASSERT(!(size & (size - 1)), "FFT size must be a power of 2");

	bufsize = size * 2;

#ifdef FFT_USE_ONE_MALLOC
	free(fft_allocation);
	fft_allocation = mem_alloc(
#define FFT_VAR(type, name, size) \
	((size) * sizeof(type)) +
#include "fft-vars.h"
		/* no-op for size */
		0
	);

	/* This pointer will be used to iterate the fft allocation */
	ptr = fft_allocation;
#define FFT_VAR(type, name, size) \
	do { \
		name = (type *)(ptr); \
		ptr += (size) * sizeof(type); \
	} while (0);
#include "fft-vars.h"
#else
# define FFT_VAR(type, name, size) \
	free(name); \
	name = mem_alloc((size) * sizeof(type));
# include "fft-vars.h"
#endif

	/* store these for later... */
	fft_size = size;
	fft_bufsize = bufsize;
	fft_sizelog2 = blog2(bufsize);

#if 0
	/* this makes weird bugs easier to spot */
	memset(incomingl, 0, fft_bufsize * 2);
	memset(incomingr, 0, fft_bufsize * 2);
#endif
}

/* must be called any time the sample buffer size changes */
void vis_set_size(uint32_t size)
{
	uint32_t n;

	vis_realloc(bnextpow2(size));

	for (n = 0; n < fft_bufsize; n++) {
		bit_reverse[n] = _reverse_bits(n, fft_sizelog2);
#if 0
		/*Rectangular/none*/
		window[n] = 1;
		/*Cosine/sine window*/
		window[n] = sin(M_PI * n/ fft_bufsize -1);
		/*Hann Window*/
		window[n] = 0.50f - 0.50f * cos(2.0*M_PI * n / (fft_bufsize - 1));
		/*Hamming Window*/
		window[n] = 0.54f - 0.46f * cos(2.0*M_PI * n / (fft_bufsize - 1));
		/*Gaussian*/
		window[n] = powf(M_E,-0.5f *pow((n-(fft_bufsize-1)/2.f)/(0.4*(fft_bufsize-1)/2.f),2.f));
		/*Blackmann*/
		window[n] = 0.42659 - 0.49656 * cos(2.0*M_PI * n/ (fft_bufsize-1)) + 0.076849 * cos(4.0*M_PI * n /(fft_bufsize-1));
		/*Blackman-Harris*/
		window[n] = 0.35875 - 0.48829 * cos(2.0*M_PI * n/ (fft_bufsize-1)) + 0.14128 * cos(4.0*M_PI * n /(fft_bufsize-1)) - 0.01168 * cos(6.0*M_PI * n /(fft_bufsize-1));
#endif
		/*Hann Window*/
		window[n] = 0.50f - 0.50f * cos(2.0 * M_PI * n / (fft_bufsize - 1));
	}

	for (n = 0; n < fft_size; n++) {
		float j = (2.0f * (float)M_PI) * n / fft_bufsize;
		precos[n] = cos(j);
		presin[n] = sin(j);
	}

	/* ?? */
	fft_inv_bufsize = 1.0f/(fft_bufsize >> 2);

#if 0
	/* linear */
	const double factor = (float)fft_size / FFT_BANDS_SIZE;
	for (n = 0; n < FFT_BANDS_SIZE; n++)
		fftlog[n] = n * factor;
#elif 1
	/* exponential */
	const double factor = (float)fft_size / FFT_BANDS_SIZE / FFT_BANDS_SIZE;
	for (n = 0; n < FFT_BANDS_SIZE; n++)
		fftlog[n] = n * n * factor;
#else
	/* constant note scale */
	static const float factor = 8.0f / (float)FFT_BANDS_SIZE;
	const float factor2 = (float)fft_size / 256.0f;

	for (n = 0; n < FFT_BANDS_SIZE; n++)
		fftlog[n] = factor2 * (powf(2.0f, n * factor) - 1.0f);
#endif
}

void vis_init(void)
{
	/* nop... */
}

static void _vis_data_work(uint8_t *output, int16_t *input)
{
	uint32_t n, k, y;
	uint32_t ex = 1, ff = fft_size;
	uint32_t yp;
	float fr, fi;
	float tr, ti;
	float out;

	for (n = 0; n < fft_bufsize; n++) {
		uint32_t nr = bit_reverse[n];
		state_real[n] = (float)input[nr] * inv_s_range * window[nr];
		state_imag[n] = 0;
	}

	for (n = fft_sizelog2; n != 0; n--) {
		for (k = 0; k != ex; k++) {
			fr = precos[k * ff];
			fi = presin[k * ff];
			for (y = k; y < fft_bufsize; y += ex << 1) {
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
	/* XXX I changed the behavior here since the states were getting overflowed (originally
	 * was 'n + 1' changed to just 'n'. Hopefully nothing breaks... */
	const float fft_dbinv_bufsize = dB(fft_inv_bufsize);
	for (n = 0; n < fft_size; n++) {
		/* "out" is the total power for each band.
		 * To get amplitude from "output", use sqrt(out[N])/(sizeBuf>>2)
		 * To get dB from "output", use powerdB(out[N])+db(1/(sizeBuf>>2)).
		 * powerdB is = 10 * log10(in)
		 * dB is = 20 * log10(in) */
		out = ((state_real[n]) * (state_real[n])) + ((state_imag[n]) * (state_imag[n]));
		/* +0.0000000001f is -100dB of power. Used to prevent evaluating powerdB(0.0) */
		output[n] = pdB_s(noisefloor, out + 0.0000000001f, fft_dbinv_bufsize);
	}
}

/* "chan" is either zero for all, or nonzero for a specific output channel */
static inline SCHISM_ALWAYS_INLINE
uint8_t _fft_get_value(uint32_t chan, uint32_t offset)
{
	switch (chan) {
	case 1: return current_fft_datal[offset];
	case 2: return current_fft_datar[offset];
	default: break;
	}

	return bavgu32(current_fft_datal[offset], current_fft_datar[offset]);
}

/* convert the fft bands to columns of screen
 * out and fft have a range of 0 to 128
 *
 * "chan" is the channel to process, or 0 for all */
void fft_get_columns(uint32_t width, unsigned char *out, uint32_t chan)
{
	uint32_t i, a;

	for (i = 0, a = 0; i < width && a < fft_size; i++) {
		const uint32_t fftlog_i = (i * FFT_BANDS_SIZE / width);
		uint8_t j;

		uint32_t ax = fftlog[fftlog_i];
		if (ax >= fft_size)
			break;

		/* mmm... this got ugly */
		if ((fftlog_i + 1 >= FFT_BANDS_SIZE) || (ax + 1 > fftlog[fftlog_i + 1])) {
			a = ax;
			j = _fft_get_value(chan, a);
		} else {
			j = _fft_get_value(chan, a);
			while (a <= ax) {
				a++;
				uint8_t x = _fft_get_value(chan, a);
				j = MAX(j, x);
			}
		}

		out[i] = j;
	}
}

/* Convert the output of */
static inline SCHISM_ALWAYS_INLINE unsigned char *_dobits(unsigned char *q,
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
static inline SCHISM_ALWAYS_INLINE void _drawslice(int x, int h, int c)
{
	int y = ((h>>2) & (SCOPE_ROWS-1))+1;

	vgamem_ovl_drawline(&ovl,
		x, (NATIVE_SCREEN_HEIGHT-y),
		x, (NATIVE_SCREEN_HEIGHT-1), c);
}

static void _vis_process(void)
{
	static const int k = NATIVE_SCREEN_WIDTH / 2;
	int i;
	unsigned char outfft[NATIVE_SCREEN_WIDTH];
	unsigned char *q;

	/* move up previous line by one pixel */
	memmove(ovl.q, ovl.q + NATIVE_SCREEN_WIDTH, (NATIVE_SCREEN_WIDTH * ((NATIVE_SCREEN_HEIGHT - 1) - SCOPE_ROWS)));
	q = ovl.q + (NATIVE_SCREEN_WIDTH * ((NATIVE_SCREEN_HEIGHT - 1) - SCOPE_ROWS));

	if (mono) {
		fft_get_columns(NATIVE_SCREEN_WIDTH, outfft, 0);
		_dobits(q, outfft, NATIVE_SCREEN_WIDTH, 1);
	} else {
		fft_get_columns(k, outfft,     1);
		fft_get_columns(k, outfft + k, 2);

		_dobits(q + k - 1, outfft,     k, -1);
		_dobits(q + k,     outfft + k, k, 1);
	}

	/* draw the scope at the bottom */
	q = ovl.q + (NATIVE_SCREEN_WIDTH * (NATIVE_SCREEN_HEIGHT - SCOPE_ROWS));
	i = SCOPE_ROWS * NATIVE_SCREEN_WIDTH;
	memset(q, 0, i);

	if (mono) {
		for (i = 0; i < NATIVE_SCREEN_WIDTH; i++) _drawslice(i, outfft[i], 5);
	} else {
		for (i = 0; i < k; i++) _drawslice(k - i - 1, outfft[i],     5);
		for (i = 0; i < k; i++) _drawslice(k + i,     outfft[k + i], 5);
	}

	status.flags |= NEED_UPDATE;
}

#define VIS_WORK_EX(SUFFIX, BITS, INLOOP) \
	void vis_work_##BITS##SUFFIX(const int##BITS##_t *in, size_t samples) \
	{ \
		size_t i, j, k; \
	\
		if (!samples) { \
			memset(current_fft_datal, 0, fft_size * sizeof(*current_fft_datal)); \
			memset(current_fft_datar, 0, fft_size * sizeof(*current_fft_datar)); \
		} else { \
			for (i = 0; i < fft_size;) { \
				for (k = j = 0; k < samples && i < fft_size; k++, i++) { \
					INLOOP \
				} \
			} \
	\
			_vis_data_work(current_fft_datal, incomingl); \
			_vis_data_work(current_fft_datar, incomingr); \
		} \
		if (status.current_page == PAGE_WATERFALL) _vis_process(); \
	}

#define VIS_WORK(BITS) \
	VIS_WORK_EX(s, BITS, { \
		incomingl[i] = rshift_signed(lshift_signed((int32_t)in[j], 32 - BITS), 16); j++; \
		incomingr[i] = rshift_signed(lshift_signed((int32_t)in[j], 32 - BITS), 16); j++; \
	}) \
	\
	VIS_WORK_EX(m, BITS, { \
		incomingl[i] = incomingr[i] = rshift_signed(lshift_signed((int32_t)in[j], 32 - BITS), 16); j++; \
	})

VIS_WORK(32)
VIS_WORK(16)
VIS_WORK(8)

#undef VIS_WORK
#undef VIS_WORK_EX

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
				status.last_keysym = 0;
			} else if (!k->is_repeat) {
				song_keydown(KEYJAZZ_NOINST, ii, n, v, KEYJAZZ_CHAN_CURRENT);
			}
			return 1;
		}
	}

	switch (k->sym) {
	case SCHISM_KEYSYM_s:
		if (k->mod & SCHISM_KEYMOD_ALT) {
			if (k->state == KEY_RELEASE)
				return 1;

			song_toggle_stereo();
			status.flags |= NEED_UPDATE;
			return 1;
		}
		return 0;
	case SCHISM_KEYSYM_m:
		if (k->mod & SCHISM_KEYMOD_ALT) {
			if (k->state == KEY_RELEASE)
				return 1;
			mono = !mono;
			return 1;
		}
		return 0;
	case SCHISM_KEYSYM_LEFT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		noisefloor-=4;
		break;
	case SCHISM_KEYSYM_RIGHT:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		noisefloor+=4;
		break;
	case SCHISM_KEYSYM_g:
		if (k->mod & SCHISM_KEYMOD_ALT) {
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
	case SCHISM_KEYSYM_r:
		if (k->mod & SCHISM_KEYMOD_ALT) {
			if (k->state == KEY_RELEASE)
				return 1;

			song_flip_stereo();
			return 1;
		}
		return 0;
	case SCHISM_KEYSYM_PLUS:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_get_mode() == MODE_PLAYING) {
			song_set_current_order(song_get_current_order() + 1);
		}
		return 1;
	case SCHISM_KEYSYM_MINUS:
		if (!NO_MODIFIER(k->mod))
			return 0;
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_get_mode() == MODE_PLAYING) {
			song_set_current_order(song_get_current_order() - 1);
		}
		return 1;
	case SCHISM_KEYSYM_SEMICOLON:
	case SCHISM_KEYSYM_COLON:
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_is_instrument_mode()) {
			instrument_set(instrument_get_current() - 1);
		} else {
			sample_set(sample_get_current() - 1);
		}
		return 1;
	case SCHISM_KEYSYM_QUOTE:
	case SCHISM_KEYSYM_QUOTEDBL:
		if (k->state == KEY_RELEASE)
			return 1;
		if (song_is_instrument_mode()) {
			instrument_set(instrument_get_current() + 1);
		} else {
			sample_set(sample_get_current() + 1);
		}
		return 1;
	case SCHISM_KEYSYM_COMMA:
	case SCHISM_KEYSYM_LESS:
		if (k->state == KEY_RELEASE)
			return 1;
		song_change_current_play_channel(-1, 0);
		return 1;
	case SCHISM_KEYSYM_PERIOD:
	case SCHISM_KEYSYM_GREATER:
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
	widget_create_other(waterfall_widget_hack, 0, waterfall_handle_key, NULL, do_nil);
}
