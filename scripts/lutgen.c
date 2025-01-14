#define compile \
	exec gcc -Wall -pedantic -std=gnu99 -lm lutgen.c -o lutgen || exit 255

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <inttypes.h>
#include <math.h>

/*
 *  cubic spline interpolation doc,
 *    (derived from "digital image warping", g. wolberg)
 *
 *    interpolation polynomial: f(x) = A3*(x-floor(x))**3 + A2*(x-floor(x))**2 + A1*(x-floor(x)) + A0
 *
 *    with Y = equispaced data points (dist=1), YD = first derivates of data points and IP = floor(x)
 *    the A[0..3] can be found by solving
 *      A0  = Y[IP]
 *      A1  = YD[IP]
 *      A2  = 3*(Y[IP+1]-Y[IP])-2.0*YD[IP]-YD[IP+1]
 *      A3  = -2.0 * (Y[IP+1]-Y[IP]) + YD[IP] - YD[IP+1]
 *
 *    with the first derivates as
 *      YD[IP]    = 0.5 * (Y[IP+1] - Y[IP-1]);
 *      YD[IP+1]  = 0.5 * (Y[IP+2] - Y[IP])
 *
 *    the coefs becomes
 *      A0  = Y[IP]
 *      A1  = YD[IP]
 *          =  0.5 * (Y[IP+1] - Y[IP-1]);
 *      A2  =  3.0 * (Y[IP+1]-Y[IP])-2.0*YD[IP]-YD[IP+1]
 *          =  3.0 * (Y[IP+1] - Y[IP]) - 0.5 * 2.0 * (Y[IP+1] - Y[IP-1]) - 0.5 * (Y[IP+2] - Y[IP])
 *          =  3.0 * Y[IP+1] - 3.0 * Y[IP] - Y[IP+1] + Y[IP-1] - 0.5 * Y[IP+2] + 0.5 * Y[IP]
 *          = -0.5 * Y[IP+2] + 2.0 * Y[IP+1] - 2.5 * Y[IP] + Y[IP-1]
 *          = Y[IP-1] + 2 * Y[IP+1] - 0.5 * (5.0 * Y[IP] + Y[IP+2])
 *      A3  = -2.0 * (Y[IP+1]-Y[IP]) + YD[IP] + YD[IP+1]
 *          = -2.0 * Y[IP+1] + 2.0 * Y[IP] + 0.5 * (Y[IP+1] - Y[IP-1]) + 0.5 * (Y[IP+2] - Y[IP])
 *          = -2.0 * Y[IP+1] + 2.0 * Y[IP] + 0.5 * Y[IP+1] - 0.5 * Y[IP-1] + 0.5 * Y[IP+2] - 0.5 * Y[IP]
 *          =  0.5 * Y[IP+2] - 1.5 * Y[IP+1] + 1.5 * Y[IP] - 0.5 * Y[IP-1]
 *          =  0.5 * (3.0 * (Y[IP] - Y[IP+1]) - Y[IP-1] + YP[IP+2])
 *
 *    then interpolated data value is (horner rule)
 *      out = (((A3*x)+A2)*x+A1)*x+A0
 *
 *    this gives parts of data points Y[IP-1] to Y[IP+2] of
 *      part       x**3    x**2    x**1    x**0
 *      Y[IP-1]    -0.5     1      -0.5    0
 *      Y[IP]       1.5    -2.5     0      1
 *      Y[IP+1]    -1.5     2       0.5    0
 *      Y[IP+2]     0.5    -0.5     0      0
 */

// number of bits used to scale spline coefs
#define SPLINE_QUANTBITS    14
#define SPLINE_QUANTSCALE   (1L << SPLINE_QUANTBITS)
#define SPLINE_8SHIFT       (SPLINE_QUANTBITS - 8)
#define SPLINE_16SHIFT      (SPLINE_QUANTBITS)

// forces coefsset to unity gain
#define SPLINE_CLAMPFORUNITY

// log2(number) of precalculated splines (range is [4..14])
#define SPLINE_FRACBITS 10
#define SPLINE_LUTLEN (1L << SPLINE_FRACBITS)


int16_t cubic_spline_lut[4 * SPLINE_LUTLEN];


void cubic_spline_init(void)
{
	int i;
	int len = SPLINE_LUTLEN;
	float flen = 1.0f / (float) SPLINE_LUTLEN;
	float scale = (float) SPLINE_QUANTSCALE;

	for (i = 0; i < len; i++) {
	float LCm1, LC0, LC1, LC2;
	float LX = ((float) i) * flen;
	int indx = i << 2;
#ifdef SPLINE_CLAMPFORUNITY
	int sum;
#endif

	LCm1 = (float) floor(0.5 + scale * (-0.5 * LX * LX * LX + 1.0 * LX * LX - 0.5 * LX       ));
	LC0  = (float) floor(0.5 + scale * ( 1.5 * LX * LX * LX - 2.5 * LX * LX             + 1.0));
	LC1  = (float) floor(0.5 + scale * (-1.5 * LX * LX * LX + 2.0 * LX * LX + 0.5 * LX       ));
	LC2  = (float) floor(0.5 + scale * ( 0.5 * LX * LX * LX - 0.5 * LX * LX                  ));

	cubic_spline_lut[indx + 0] = (int16_t) ((LCm1 < -scale) ? -scale : ((LCm1 > scale) ? scale : LCm1));
	cubic_spline_lut[indx + 1] = (int16_t) ((LC0  < -scale) ? -scale : ((LC0  > scale) ? scale : LC0 ));
	cubic_spline_lut[indx + 2] = (int16_t) ((LC1  < -scale) ? -scale : ((LC1  > scale) ? scale : LC1 ));
	cubic_spline_lut[indx + 3] = (int16_t) ((LC2  < -scale) ? -scale : ((LC2  > scale) ? scale : LC2 ));

#ifdef SPLINE_CLAMPFORUNITY
	sum = cubic_spline_lut[indx + 0] +
		  cubic_spline_lut[indx + 1] +
		  cubic_spline_lut[indx + 2] +
		  cubic_spline_lut[indx + 3];

	if (sum != SPLINE_QUANTSCALE) {
		int max = indx;

		if (cubic_spline_lut[indx + 1] > cubic_spline_lut[max]) max = indx + 1;
		if (cubic_spline_lut[indx + 2] > cubic_spline_lut[max]) max = indx + 2;
		if (cubic_spline_lut[indx + 3] > cubic_spline_lut[max]) max = indx + 3;

		cubic_spline_lut[max] += (SPLINE_QUANTSCALE - sum);
	}
#endif
	}
}


/* fir interpolation doc,
 *  (derived from "an engineer's guide to fir digital filters", n.j. loy)
 *
 *  calculate coefficients for ideal lowpass filter (with cutoff = fc in 0..1 (mapped to 0..nyquist))
 *    c[-N..N] = (i==0) ? fc : sin(fc*pi*i)/(pi*i)
 *
 *  then apply selected window to coefficients
 *    c[-N..N] *= w(0..N)
 *  with n in 2*N and w(n) being a window function (see loy)
 *
 *  then calculate gain and scale filter coefs to have unity gain.
 */

// quantizer scale of window coefs
#define WFIR_QUANTBITS      15
#define WFIR_QUANTSCALE     (1L << WFIR_QUANTBITS)
#define WFIR_8SHIFT         (WFIR_QUANTBITS - 8)
#define WFIR_16BITSHIFT     (WFIR_QUANTBITS)

// log2(number)-1 of precalculated taps range is [4..12]
#define WFIR_FRACBITS       10
#define WFIR_LUTLEN         ((1L << (WFIR_FRACBITS + 1)) + 1)

// number of samples in window
#define WFIR_LOG2WIDTH      3
#define WFIR_WIDTH          (1L << WFIR_LOG2WIDTH)
#define WFIR_SMPSPERWING    ((WFIR_WIDTH - 1) >> 1)

// cutoff (1.0 == pi/2)
#define WFIR_CUTOFF         0.90f

// wfir type
#define WFIR_HANN           0
#define WFIR_HAMMING        1
#define WFIR_BLACKMANEXACT  2
#define WFIR_BLACKMAN3T61   3
#define WFIR_BLACKMAN3T67   4
#define WFIR_BLACKMAN4T92   5
#define WFIR_BLACKMAN4T74   6
#define WFIR_KAISER4T       7
#define WFIR_TYPE           WFIR_BLACKMANEXACT

// wfir help
#ifndef M_zPI
#define M_zPI               3.1415926535897932384626433832795
#endif
#define M_zEPS              1e-8
#define M_zBESSELEPS        1e-21


float coef(int pc_nr, float p_ofs, float p_cut, int p_width, int p_type)
{
	double width_m1      = p_width - 1;
	double width_m1_half = 0.5 * width_m1;
	double pos_u         = (double) pc_nr - p_ofs;
	double pos           = pos_u - width_m1_half;
	double idl           = 2.0 * M_zPI / width_m1;
	double wc, si;

	if (fabs(pos) < M_zEPS) {
	wc    = 1.0;
	si    = p_cut;
	return p_cut;
	}

	switch (p_type) {
	case WFIR_HANN:
		wc = 0.50 - 0.50 * cos(idl * pos_u);
		break;

	case WFIR_HAMMING:
		wc = 0.54 - 0.46 * cos(idl * pos_u);
		break;

	case WFIR_BLACKMANEXACT:
		wc = 0.42 - 0.50 * cos(idl * pos_u) + 0.08 * cos(2.0 * idl * pos_u);
		break;

	case WFIR_BLACKMAN3T61:
		wc = 0.44959 - 0.49364 * cos(idl * pos_u) + 0.05677 * cos(2.0 * idl * pos_u);
		break;

	case WFIR_BLACKMAN3T67:
		wc = 0.42323 - 0.49755 * cos(idl * pos_u) + 0.07922 * cos(2.0 * idl * pos_u);
		break;

	case WFIR_BLACKMAN4T92:
		wc = 0.35875 - 0.48829 * cos(idl * pos_u) + 0.14128 * cos(2.0 * idl * pos_u) -
			0.01168 * cos(3.0 * idl * pos_u);
		break;

	case WFIR_BLACKMAN4T74:
		wc = 0.40217 - 0.49703 * cos(idl * pos_u) + 0.09392 * cos(2.0 * idl * pos_u) -
		0.00183 * cos(3.0*idl*pos_u);
		break;

	case WFIR_KAISER4T:
		wc = 0.40243 - 0.49804 * cos(idl * pos_u) + 0.09831 * cos(2.0 * idl * pos_u) -
		0.00122 * cos(3.0 * idl * pos_u);
		break;

	default:
		wc = 1.0;
		break;
	}

	pos *= M_zPI;
	si   = sin(p_cut * pos) / pos;

	return (float)(wc * si);
}



int16_t windowed_fir_lut[WFIR_LUTLEN*WFIR_WIDTH];

void windowed_fir_init(void)
{
	int pcl;
	// number of precalculated lines for 0..1 (-1..0)
	float pcllen = (float)(1L << WFIR_FRACBITS);
	float norm  = 1.0f / (float)(2.0f * pcllen);
	float cut   = WFIR_CUTOFF;
	float scale = (float) WFIR_QUANTSCALE;

	for (pcl = 0; pcl < WFIR_LUTLEN; pcl++) {
		float gain,coefs[WFIR_WIDTH];
		float ofs = ((float) pcl - pcllen) * norm;
		int cc, indx = pcl << WFIR_LOG2WIDTH;

		for (cc = 0, gain = 0.0f; cc < WFIR_WIDTH; cc++) {
			coefs[cc] = coef(cc, ofs, cut, WFIR_WIDTH, WFIR_TYPE);
			gain += coefs[cc];
		}

		gain = 1.0f / gain;

		for (cc = 0; cc < WFIR_WIDTH; cc++) {
			float coef = (float)floor( 0.5 + scale * coefs[cc] * gain);
			windowed_fir_lut[indx + cc] = (int16_t)((coef < -scale) ? - scale :
			((coef > scale) ? scale : coef));
		}
	}
}


#define LOOP(x, y) \
	printf("static int16_t %s[%" PRId32 "] = {\n", #x, (int32_t)y); \
	\
	for (int i = 0; i < y; i++) { \
		if (i && !(i % 64)) \
			printf("\n"); \
		printf(" %d,", x[i]); \
	} \
	\
	printf("\n};\n\n");


int main(int argc, char **argv)
{
	cubic_spline_init();
	windowed_fir_init();

	LOOP(cubic_spline_lut, (4 * SPLINE_LUTLEN));
	LOOP(windowed_fir_lut, (WFIR_LUTLEN * WFIR_WIDTH));

	return 0;
}

