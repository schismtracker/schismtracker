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

#include "test.h"
#include "test-assertions.h"
#include "test-vmem.h"

#include "video.h"

/* dummy palette */
static const uint32_t dpal[256];

#define TEST_VIDEO_BLIT(NAME, ALLOC, BLIT) \
	testresult_t test_video_blit##NAME(void) \
	{ \
		void *x = vmem_alloc((ALLOC), VMEM_WRITE); \
		BLIT \
		vmem_free(x); \
	\
		RETURN_PASS; \
	} \
	\
	testresult_t test_video_blit##NAME##_overflow(void) \
	{ \
		uintptr_t pg, rem, sz, szrnd; \
		void *a, *x; \
	\
		/* TODO abstract this logic away */ \
		pg = vmem_pagesize(); \
		sz = (ALLOC); \
		rem = (sz % pg); \
		szrnd = (rem) ? (sz + pg - rem) : (sz); \
	\
		a = vmem_alloc(szrnd, VMEM_WRITE); \
		x = (char *)a + szrnd - sz; \
		BLIT \
		vmem_free(a); \
	\
		RETURN_PASS; \
	}	

#define TEST_VIDEO_BLIT11(BPP) \
	TEST_VIDEO_BLIT(11_##BPP##bpp, 640 * 400 * (BPP), { video_blit11(BPP, x, 640 * (BPP), dpal); })

TEST_VIDEO_BLIT11(1)
TEST_VIDEO_BLIT11(2)
TEST_VIDEO_BLIT11(3)
TEST_VIDEO_BLIT11(4)

#undef TEST_VIDEO_BLIT11

TEST_VIDEO_BLIT(YY, 640 * 400 * 4, { video_blitYY(x, 640 * 2, dpal); })
TEST_VIDEO_BLIT(UV, 640 * 400, { video_blitUV(x, 640, dpal); })
TEST_VIDEO_BLIT(TV, (640 * 400) / 4, { video_blitTV(x, 640, dpal); })

static uint32_t maprgb(void *opaque, uint8_t r, uint8_t g, uint8_t b)
{
	return 0; /* not important */
}

#define TEST_VIDEO_BLITSC_EX(BPP, WIDTH, HEIGHT) \
	TEST_VIDEO_BLIT(NN_##BPP##bpp_##WIDTH##x##HEIGHT, (WIDTH) * (HEIGHT) * (BPP), { video_blitNN(BPP, x, (WIDTH) * (BPP), dpal, (WIDTH), (HEIGHT)); }) \
	TEST_VIDEO_BLIT(LN_##BPP##bpp_##WIDTH##x##HEIGHT, (WIDTH) * (HEIGHT) * (BPP), { video_blitLN(BPP, x, (WIDTH) * (BPP), maprgb, NULL, (WIDTH), (HEIGHT)); })

#define TEST_VIDEO_BLITSC(WIDTH, HEIGHT) \
	TEST_VIDEO_BLITSC_EX(1, WIDTH, HEIGHT) \
	TEST_VIDEO_BLITSC_EX(2, WIDTH, HEIGHT) \
	TEST_VIDEO_BLITSC_EX(3, WIDTH, HEIGHT) \
	TEST_VIDEO_BLITSC_EX(4, WIDTH, HEIGHT)

TEST_VIDEO_BLITSC(720, 480)
TEST_VIDEO_BLITSC(1280, 720)
TEST_VIDEO_BLITSC(1280, 800)
TEST_VIDEO_BLITSC(1920, 1080)

/* now for some totally phucked up ones */
TEST_VIDEO_BLITSC(1, 1000)
TEST_VIDEO_BLITSC(1000, 1)

TEST_VIDEO_BLITSC(70, 2)
TEST_VIDEO_BLITSC(5, 7)

#undef TEST_VIDEO_BLITSC
#undef TEST_VIDEO_BLITSC_EX

#undef TEST_VIDEO_BLIT
