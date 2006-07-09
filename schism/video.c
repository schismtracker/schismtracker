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
#define NATIVE_SCREEN_WIDTH		640
#define NATIVE_SCREEN_HEIGHT		400

#include "headers.h"
#include "it.h"

#if HAVE_SYS_KD_H
# include <sys/kd.h>
#endif
#if HAVE_LINUX_FB_H
# include <linux/fb.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#if HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

/* for memcpy */
#include <string.h>
#include <stdlib.h>

#include "sdlmain.h"

#include <unistd.h>
#include <fcntl.h>

#include "video.h"

#include "auto/schismico.h"

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

extern int macosx_did_finderlaunch;

#define NVIDIA_PixelDataRange	1

#ifdef NVIDIA_PixelDataRange

#ifndef WGL_NV_allocate_memory
#define WGL_NV_allocate_memory 1
typedef void * (APIENTRY * PFNWGLALLOCATEMEMORYNVPROC) (int size, float readfreq, float writefreq, float priority);
typedef void (APIENTRY * PFNWGLFREEMEMORYNVPROC) (void *pointer);
#endif

PFNWGLALLOCATEMEMORYNVPROC db_glAllocateMemoryNV = NULL;
PFNWGLFREEMEMORYNVPROC db_glFreeMemoryNV = NULL;

#ifndef GL_NV_pixel_data_range
#define GL_NV_pixel_data_range 1
#define GL_WRITE_PIXEL_DATA_RANGE_NV      0x8878
typedef void (APIENTRYP PFNGLPIXELDATARANGENVPROC) (GLenum target, GLsizei length, GLvoid *pointer);
typedef void (APIENTRYP PFNGLFLUSHPIXELDATARANGENVPROC) (GLenum target);
#endif

PFNGLPIXELDATARANGENVPROC glPixelDataRangeNV = NULL;

#endif

/* leeto drawing skills */
#define MOUSE_HEIGHT	14
static const unsigned int _mouse_pointer[] = {
	/* x....... */	0x80,
	/* xx...... */	0xc0,
	/* xxx..... */	0xe0,
	/* xxxx.... */	0xf0,
	/* xxxxx... */	0xf8,
	/* xxxxxx.. */	0xfc,
	/* xxxxxxx. */	0xfe,
	/* xxxxxxxx */	0xff,
	/* xxxxxxx. */	0xfe,
	/* xxxxx... */	0xf8,
	/* x...xx.. */	0x8c,
	/* ....xx.. */	0x0c,
	/* .....xx. */	0x06,
	/* .....xx. */	0x06,

0,0
};


#ifdef WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <ddraw.h>
struct private_hwdata {
	LPDIRECTDRAWSURFACE3 dd_surface;
	LPDIRECTDRAWSURFACE3 dd_writebuf;
};
#endif

static struct video_cf video;


struct video_cf {
	struct {
		unsigned int width;
		unsigned int height;

		int autoscale;

		/* stuff for scaling */
		int sx, sy;
		int *sax, *say;

		/* video "mode" */
		void (*pixels_clear)(void);
		void (*pixels_flip)(void);
		void (*pixels_8)(unsigned int y,
				unsigned char *out,
				unsigned int pal[16]);
		void (*pixels_16)(unsigned int y,
				unsigned short *out,
				unsigned int pal[16]);
		void (*pixels_32)(unsigned int y,
				unsigned int *out,
				unsigned int pal[16]);
	} draw;
	struct {
		unsigned int width,height,bpp;
		int want_fixed;

		int fullscreen;
		int doublebuf;
		int want_type;
		int type;
#define VIDEO_SURFACE		0
#define VIDEO_DDRAW		1
#define VIDEO_YUV		2
#define VIDEO_GL		3
	} desktop;
	struct {
		unsigned int pitch;
		void * framebuf;
		GLuint texture;
		GLuint displaylist;
		GLint max_texsize;
		int bilinear;
		int packed_pixel;
		int paletted_texture;
#if defined(NVIDIA_PixelDataRange)
		int pixel_data_range;
#endif
	} gl;
#if defined(WIN32)
	struct {
		SDL_Surface * surface;
		RECT rect;
		DDBLTFX fx;
	} ddblit;
#endif
	int yuvlayout;
#define YUV_UYVY	0x59565955
#define YUV_YUY2	0x32595559
	SDL_Rect clip;
	SDL_Surface * surface;
	SDL_Overlay * overlay;
	/* to convert 32-bit color to 24-bit color */
	unsigned char *cv32backing;
	struct {
		unsigned int x;
		unsigned int y;
		int visible;
	} mouse;

	unsigned int pal[16];

	unsigned int tc_bgr32[16];
	unsigned int tc_identity[16];
};
static int int_log2(int val) {
	int l = 0;
	while ((val >>= 1 ) != 0) l++;
	return l;
}

static void _video_dummy_scan(UNUSED unsigned int y, UNUSED void *plx,
				UNUSED unsigned int *tc)
{
	/* do nothing */
}

void video_mode(int num)
{
	if (num == 0) {
		video.draw.pixels_8 = vgamem_scan8;
		video.draw.pixels_16 = vgamem_scan16;
		video.draw.pixels_32 = vgamem_scan32;
		video.draw.pixels_clear = vgamem_clear;
		video.draw.pixels_flip = vgamem_flip;
	} else {
		video.draw.pixels_8 = (void*)_video_dummy_scan;
		video.draw.pixels_16 = (void*)_video_dummy_scan;
		video.draw.pixels_32 = (void*)_video_dummy_scan;
		/* eh... */
		video.draw.pixels_clear = vgamem_clear;
		video.draw.pixels_flip = vgamem_flip;
	}
}

void _video_scanmouse(unsigned int y,
		unsigned char *mousebox, unsigned int *x)
{
	unsigned int g,i, z,v;

	if (!video.mouse.visible
	|| !(status.flags & IS_FOCUSED)
	|| y < video.mouse.y
	|| y >= video.mouse.y+MOUSE_HEIGHT) {
		*x = 255;
		return;
	}
	*x = (video.mouse.x / 8);
	v = (video.mouse.x % 8);
	z = _mouse_pointer[ y - video.mouse.y ];
	mousebox[0] = z >> v;
	mousebox[1] = (z << (8-v)) & 0xff;
}

#ifdef MACOSX
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>

/* opengl is EVERYWHERE on macosx, and we can't use our dynamic linker mumbo
jumbo so easily...
*/
#define my_glCallList(z)	glCallList(z)
#define my_glTexSubImage2D(a,b,c,d,e,f,g,h,i)	glTexSubImage2D(a,b,c,d,e,f,g,h,i)
#define my_glDeleteLists(a,b)	glDeleteLists(a,b)
#define my_glTexParameteri(a,b,c)	glTexParameteri(a,b,c)
#define my_glBegin(g)		glBegin(g)
#define my_glEnd()		glEnd()
#define my_glEndList()	glEndList()
#define my_glTexCoord2f(a,b)	glTexCoord2f(a,b)
#define my_glVertex2f(a,b)	glVertex2f(a,b)
#define my_glNewList(a,b)	glNewList(a,b)
#define my_glIsList(a)		glIsList(a)
#define my_glGenLists(a)	glGenLists(a)
#define my_glLoadIdentity()	glLoadIdentity()
#define my_glMatrixMode(a)	glMatrixMode(a)
#define my_glEnable(a)		glEnable(a)
#define my_glDisable(a)		glDisable(a)
#define my_glBindTexture(a,b)	glBindTexture(a,b)
#define my_glClear(z)		glClear(z)
#define my_glClearColor(a,b,c,d) glClearColor(a,b,c,d)
#define my_glGetString(z)	glGetString(z)
#define my_glTexImage2D(a,b,c,d,e,f,g,h,i)	glTexImage2D(a,b,c,d,e,f,g,h,i)
#define my_glGetIntegerv(a,b)	glGetIntegerv(a,b)
#define my_glShadeModel(a)	glShadeModel(a)
#define my_glGenTextures(a,b)	glGenTextures(a,b)
#define my_glDeleteTextures(a,b)	glDeleteTextures(a,b)
#define my_glViewport(a,b,c,d)	glViewport(a,b,c,d)
#define my_glEnableClientState(z)	glEnableClientState(z)
#else
/* again with the dynamic linker nonsense; note these are APIENTRY for compatability
with win32. */
static void (APIENTRY *my_glCallList)(GLuint);
static void (APIENTRY *my_glTexSubImage2D)(GLenum,GLint,GLint,GLint,
				GLsizei,GLsizei,GLenum,GLenum,
				const GLvoid *);
static void (APIENTRY *my_glDeleteLists)(GLuint,GLsizei);
static void (APIENTRY *my_glTexParameteri)(GLenum,GLenum,GLint);
static void (APIENTRY *my_glBegin)(GLenum);
static void (APIENTRY *my_glEnd)(void);
static void (APIENTRY *my_glEndList)(void);
static void (APIENTRY *my_glTexCoord2f)(GLfloat,GLfloat);
static void (APIENTRY *my_glVertex2f)(GLfloat,GLfloat);
static void (APIENTRY *my_glNewList)(GLuint, GLenum);
static GLboolean (APIENTRY *my_glIsList)(GLuint);
static GLuint (APIENTRY *my_glGenLists)(GLsizei);
static void (APIENTRY *my_glLoadIdentity)(void);
static void (APIENTRY *my_glMatrixMode)(GLenum);
static void (APIENTRY *my_glEnable)(GLenum);
static void (APIENTRY *my_glDisable)(GLenum);
static void (APIENTRY *my_glBindTexture)(GLenum,GLuint);
static void (APIENTRY *my_glClear)(GLbitfield mask);
static void (APIENTRY *my_glClearColor)(GLclampf,GLclampf,GLclampf,GLclampf);
static const GLubyte* (APIENTRY *my_glGetString)(GLenum);
static void (APIENTRY *my_glTexImage2D)(GLenum,GLint,GLint,GLsizei,GLsizei,GLint,
			GLenum,GLenum,const GLvoid *);
static void (APIENTRY *my_glGetIntegerv)(GLenum, GLint *);
static void (APIENTRY *my_glShadeModel)(GLenum);
static void (APIENTRY *my_glGenTextures)(GLsizei,GLuint*);
static void (APIENTRY *my_glDeleteTextures)(GLsizei, const GLuint *);
static void (APIENTRY *my_glViewport)(GLint,GLint,GLsizei,GLsizei);
static void (APIENTRY *my_glEnableClientState)(GLenum);
#endif

static int _did_init = 0;

const char *video_driver_name(void)
{
	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		return "sdl";
	case VIDEO_YUV:
		return "yuv";
	case VIDEO_GL:
		return "opengl";
	case VIDEO_DDRAW:
		return "directdraw";
	};
	/* err.... */
	return "auto";
}

void video_report(void)
{
	char buf[256];
	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		log_appendf(2," Using %s%s surface SDL driver '%s'",
				((video.surface->flags & SDL_HWSURFACE)
					? "hardware" : "software"),
		((video.surface->flags & SDL_HWACCEL) ? " accelerated" : ""),
						SDL_VideoDriverName(buf,256));
		if (SDL_MUSTLOCK(video.surface))
			log_append(4,0," Must lock surface");
		log_appendf(5, " Display format: %d bits/pixel",
					video.surface->format->BitsPerPixel);
		break;
	case VIDEO_YUV:
		if (video.overlay->hw_overlay) {
			log_append(2,0, " Using hardware accelerated video overlay");
		} else {
			log_append(2,0, " Using video overlay");
		}
		if (video.yuvlayout == YUV_UYVY) {
			log_append(5,0, " Display format: UYVY");
		} else if (video.yuvlayout == YUV_YUY2) {
			log_append(5,0, " Display format: YUY2");
		}
		break;
	case VIDEO_GL:
		log_append(2,0, " Using OPENGL interface");
#if defined(NVIDIA_PixelDataRange)
		if (video.gl.pixel_data_range) {
			log_append(2,0, " Using NVidia pixel range extensions");
		}
#endif
		log_appendf(5, " Display format: %d bits/pixel",
					video.surface->format->BitsPerPixel);
		break;
	case VIDEO_DDRAW:
		log_append(2,0, " Using DirectDraw interface");
		log_appendf(5, " Display format: %d bits/pixel",
					video.surface->format->BitsPerPixel);
		break;
	};
	if (video.desktop.fullscreen) {
		log_appendf(2," at %dx%d", video.desktop.width, video.desktop.height);
	}
}

static int _did_preinit = 0;
static void _video_preinit(void)
{
	if (!_did_preinit) {
		memset(&video, 0, sizeof(video));
		_did_preinit = 1;
	}
}

int video_is_fullscreen(void)
{
	return video.desktop.fullscreen;
}
void video_fullscreen(int tri)
{
	_video_preinit();

	if (tri == 1) {
		video.desktop.fullscreen = 1;
	} else if (tri == 0) {
		video.desktop.fullscreen = 0;
	} else if (tri < 0) {
		video.desktop.fullscreen = video.desktop.fullscreen ? 0 : 1;
	}
	if (_did_init) {
		if (video.desktop.fullscreen) {
			video_resize(video.desktop.width, video.desktop.height);
		} else {
			video_resize(0, 0);
		}
		video_report();
	}
}

extern SDL_Surface *xpmdata(char *x[]);

void video_init(const char *driver)
{
	static int did_this_2 = 0;
	const char *gl_ext;
	char *q, *p;
	SDL_Rect **modes;
	int i, j, x, y;

	if (driver && strcasecmp(driver, "auto") == 0) driver = 0;

	_video_preinit();
	/* because first mode is 0 */
	vgamem_clear();
	vgamem_flip();

	for (i = 0; i < 16; i++) video.tc_identity[i] = i;

	SDL_WM_SetCaption("Schism Tracker CVS", "Schism Tracker");
#ifndef MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
	SDL_WM_SetIcon(xpmdata(_schism_icon_xpm), NULL);
#endif
	video.draw.width = NATIVE_SCREEN_WIDTH;
	video.draw.height = NATIVE_SCREEN_HEIGHT;
	video.mouse.visible = 1;

	/* used for scaling and 24bit conversion */
	if (!_did_init) {
		video.cv32backing = (unsigned char *)mem_alloc(NATIVE_SCREEN_WIDTH * 8);
		video_mode(0);
	}

	video.yuvlayout = YUV_UYVY;
	if ((q=getenv("SCHISM_YUVLAYOUT")) || (q=getenv("YUVLAYOUT"))) {
		if (strcasecmp(q, "YUY2") == 0
		|| strcasecmp(q, "YUNV") == 0
		|| strcasecmp(q, "V422") == 0
		|| strcasecmp(q, "YUYV") == 0) {
			video.yuvlayout = YUV_YUY2;
		}
	}

	q = getenv("SCHISM_DOUBLE_BUFFER");
	if (q && (atoi(q) > 0 || strcasecmp(q,"on") == 0 || strcasecmp(q,"yes") == 0)) {
		video.desktop.doublebuf = 1;
	}

	video.desktop.want_type = VIDEO_SURFACE;
#ifdef WIN32
	if (!driver) {
		/* alright, let's have some fun. */
		driver = "sdlauto"; /* err... */
	}

	if (!strcasecmp(driver, "windib")) {
		putenv("SDL_VIDEODRIVER=windib");
	} else if (!strcasecmp(driver, "ddraw") || !strcasecmp(driver,"directdraw")) {
		putenv("SDL_VIDEODRIVER=directx");
		video.desktop.want_type = VIDEO_DDRAW;
	} else if (!strcasecmp(driver, "sdlddraw")) {
		putenv("SDL_VIDEODRIVER=directx");
	}
#else
	if (!driver) {
		if (getenv("DISPLAY")) {
			driver = "x11";
		} else {
			driver = "sdlauto";
		}
	}
#endif
	if (!strcasecmp(driver, "x11")) {
		video.desktop.want_type = VIDEO_YUV;
		putenv("SDL_VIDEO_YUV_DIRECT=1");
		putenv("SDL_VIDEO_YUV_HWACCEL=1");
		putenv("SDL_VIDEODRIVER=x11");
	} else if (!strcasecmp(driver, "yuv")) {
		video.desktop.want_type = VIDEO_YUV;
		putenv("SDL_VIDEO_YUV_DIRECT=1");
		putenv("SDL_VIDEO_YUV_HWACCEL=1");
		/* leave everything else alone... */
	} else if (!strcasecmp(driver, "gl")) {
		video.desktop.want_type = VIDEO_GL;
	} else if (!strcasecmp(driver, "sdlauto") || !strcasecmp(driver,"sdl")) {
		/* okay.... */
	}

/* macosx might actually prefer opengl :) */
#ifndef MACOSX
	if (video.desktop.want_type == VIDEO_GL) {
#define Z(q) my_ ## q = SDL_GL_GetProcAddress( #q ); if (! my_ ## q) { video.desktop.want_type = VIDEO_SURFACE; goto SKIP1; }
		if (did_this_2) {
			/* do nothing */
		} else if (getenv("SDL_VIDEO_GL_DRIVER")
		&& SDL_GL_LoadLibrary(getenv("SDL_VIDEO_GL_DRIVER")) == 0) {
			/* ok */
		} else if (SDL_GL_LoadLibrary("GL") != 0)
		if (SDL_GL_LoadLibrary("libGL.so") != 0)
		if (SDL_GL_LoadLibrary("opengl32.dll") != 0)
		if (SDL_GL_LoadLibrary("opengl.dll") != 0)
	{ /* do nothing ... because the bottom routines will fail */ }
		did_this_2 = 1;
		Z(glCallList)
		Z(glTexSubImage2D)
		Z(glDeleteLists)
		Z(glTexParameteri)
		Z(glBegin)
		Z(glEnd)
		Z(glEndList)
		Z(glTexCoord2f)
		Z(glVertex2f)
		Z(glNewList)
		Z(glIsList)
		Z(glGenLists)
		Z(glLoadIdentity)
		Z(glMatrixMode)
		Z(glEnable)
		Z(glDisable)
		Z(glBindTexture)
		Z(glClear)
		Z(glClearColor)
		Z(glGetString)
		Z(glTexImage2D)
		Z(glGetIntegerv)
		Z(glShadeModel)
		Z(glGenTextures)
		Z(glDeleteTextures)
		Z(glViewport)
		Z(glEnableClientState)
#undef Z
	}
SKIP1:
#endif
	if (video.desktop.want_type == VIDEO_GL) {
		video.surface = SDL_SetVideoMode(640,400,0,SDL_OPENGL|SDL_RESIZABLE);
		if (!video.surface) {
			/* fallback */
			video.desktop.want_type = VIDEO_SURFACE;
		}
		video.gl.framebuf = 0;
		video.gl.texture = 0;
		video.gl.displaylist = 0;
		my_glGetIntegerv(GL_MAX_TEXTURE_SIZE, &video.gl.max_texsize);
#if defined(NVIDIA_PixelDataRange)
		glPixelDataRangeNV = (PFNGLPIXELDATARANGENVPROC)
				SDL_GL_GetProcAddress("glPixelDataRangeNV");
		db_glAllocateMemoryNV = (PFNWGLALLOCATEMEMORYNVPROC)
				SDL_GL_GetProcAddress("wglAllocateMemoryNV");
		db_glFreeMemoryNV = (PFNWGLFREEMEMORYNVPROC)
				SDL_GL_GetProcAddress("wglFreeMemoryNV");
#endif
		gl_ext = (const char *)my_glGetString(GL_EXTENSIONS);
		if (!gl_ext) gl_ext = (const char *)"";
		video.gl.packed_pixel=(strstr(gl_ext,"EXT_packed_pixels")>0);
		video.gl.paletted_texture=(strstr(gl_ext,"EXT_paletted_texture")>0);
#if defined(NVIDIA_PixelDataRange)
		video.gl.pixel_data_range=(strstr(gl_ext,"GL_NV_pixel_data_range")>0) && glPixelDataRangeNV && db_glAllocateMemoryNV && db_glFreeMemoryNV;
#endif
	}

	if (!video.surface) {
		/* if we already got one... */
		video.surface = SDL_SetVideoMode(640,400,0,SDL_RESIZABLE);
		if (!video.surface) {
			perror("SDL_SetVideoMode");
			exit(255);
		}
	}
	video.desktop.bpp = video.surface->format->BitsPerPixel;

	video.desktop.want_fixed = 1;
	q = getenv("SCHISM_VIDEO_ASPECT");
	if (q && (strcasecmp(q,"nofixed") == 0 || strcasecmp(q,"full")==0
	|| strcasecmp(q,"fit") == 0 || strcasecmp(q,"wide") == 0
	|| strcasecmp(q,"no-fixed") == 0))
		video.desktop.want_fixed = 0;

	modes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);
	x = y = -1;
	if (modes != (SDL_Rect**)0 && modes != (SDL_Rect**)-1) {
		for (i = 0; modes[i]; i++) {
/*
			log_appendf(2, "Host-acceptable video mode: %dx%d",
				modes[i]->w, modes[i]->h);
*/
			if (modes[i]->w < NATIVE_SCREEN_WIDTH) continue;
			if (modes[i]->h < NATIVE_SCREEN_HEIGHT)continue;
			if (x == -1 || y == -1 || modes[i]->w < x || modes[i]->h < y) {
				if (modes[i]->w != NATIVE_SCREEN_WIDTH
				||  modes[i]->h != NATIVE_SCREEN_HEIGHT) {
					if (x == NATIVE_SCREEN_WIDTH || y == NATIVE_SCREEN_HEIGHT)
						continue;
				}
				x = modes[i]->w;
				y = modes[i]->h;
				if (x == NATIVE_SCREEN_WIDTH && y == NATIVE_SCREEN_HEIGHT)
					break;
			}
		}
	}
	if (x < 0 || y < 0) {
		x = 640;
		y = 480;
	}

	if ((q = getenv("SCHISM_VIDEO_RESOLUTION"))) {
		i = j = -1;
		if (sscanf(q,"%dx%d", &i,&j) == 2 && i >= 10 && j >= 10) {
			x = i;
			y = j;
		}
	}

	if ((q = getenv("SCHISM_VIDEO_DEPTH"))) {
		i=atoi(q);
		if (i == 32) video.desktop.bpp=32;
		else if (i == 24) video.desktop.bpp=24;
		else if (i == 16) video.desktop.bpp=16;
		else if (i == 8) video.desktop.bpp=8;
	}

	/*log_appendf(2, "Ideal desktop size: %dx%d", x, y); */
	video.desktop.width = x;
	video.desktop.height = y;

	switch (video.desktop.want_type) {
	case VIDEO_GL:
		video.gl.bilinear = 1;
	case VIDEO_YUV:
		if (video.desktop.bpp == 32 || video.desktop.bpp == 16) break;
		/* fall through */
	case VIDEO_DDRAW:
#ifdef WIN32
		if (video.desktop.bpp == 32 || video.desktop.bpp == 16) {
			/* good enough for here! */
			video.desktop.want_type = VIDEO_DDRAW;
			break;
		}
#endif
		/* fall through */
	case VIDEO_SURFACE:
		/* no scaling when using the SDL surfaces directly */
#if HAVE_LINUX_FB_H
		if (!getenv("DISPLAY")) {
			struct fb_var_screeninfo s;
			int fb = -1;
			if (getenv("SDL_FBDEV")) {
				fb = open(getenv("SDL_FBDEV"), O_RDONLY);
			}
			if (fb == -1)
				fb = open("/dev/fb0", O_RDONLY);
			if (fb > -1) {
				if (ioctl(fb, FBIOGET_VSCREENINFO, &s) < 0) {
					perror("ioctl FBIOGET_VSCREENINFO");
				} else {
					x = s.xres;
					if (x < NATIVE_SCREEN_WIDTH)
						x = NATIVE_SCREEN_WIDTH;
					y = s.yres;
					video.desktop.bpp = s.bits_per_pixel;
					video.desktop.fullscreen = 1;
				}
				(void)close(fb);
			}
		}
#endif
		video.desktop.want_type = VIDEO_SURFACE;
		break;
	};
	/* okay, i think we're ready */
	SDL_ShowCursor(SDL_DISABLE);
	SDL_EventState(SDL_VIDEORESIZE, SDL_ENABLE);
	SDL_EventState(SDL_VIDEOEXPOSE, SDL_ENABLE);
	SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
	SDL_EventState(SDL_USEREVENT, SDL_ENABLE);
	SDL_EventState(SDL_ACTIVEEVENT, SDL_ENABLE);
	SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
	SDL_EventState(SDL_KEYUP, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_ENABLE);
	SDL_EventState(SDL_MOUSEBUTTONUP, SDL_ENABLE);
	_did_init = 1;
	video_fullscreen(video.desktop.fullscreen);
}
static SDL_Surface *_setup_surface(unsigned int w, unsigned int h, unsigned int sdlflags)
{
	unsigned int bpp;

	bpp = video.desktop.bpp;
	if (video.desktop.doublebuf) sdlflags |= (SDL_DOUBLEBUF|SDL_ASYNCBLIT);
	if (video.desktop.fullscreen) {
		w = video.desktop.width;
		h = video.desktop.height;
	} else {
		sdlflags |= SDL_RESIZABLE;
	}

	if (video.desktop.want_fixed) {
		double ratio_w = (double)w / (double)NATIVE_SCREEN_WIDTH;
		double ratio_h = (double)h / (double)NATIVE_SCREEN_HEIGHT;
		if (ratio_w < ratio_h) {
			video.clip.w = w;
			video.clip.h = (double)NATIVE_SCREEN_HEIGHT * ratio_w;
		} else {
			video.clip.h = h;
			video.clip.w = (double)NATIVE_SCREEN_WIDTH * ratio_h;
		}
		video.clip.x=(w-video.clip.w)/2;
		video.clip.y=(h-video.clip.h)/2;
	} else {
		video.clip.x = 0;
		video.clip.y = 0;
		video.clip.w = w;
		video.clip.h = h;
	}

	if (video.desktop.fullscreen) {
		sdlflags |= SDL_FULLSCREEN;
		sdlflags &=~SDL_RESIZABLE;
		video.surface = SDL_SetVideoMode(w, h, bpp,
				sdlflags | SDL_FULLSCREEN | SDL_HWSURFACE);
					
	} else {
		video.surface = SDL_SetVideoMode(w, h, bpp,
					sdlflags | SDL_HWSURFACE);
	}
	if (!video.surface) {
		perror("SDL_SetVideoMode");
		exit(EXIT_FAILURE);
	}
	return video.surface;
}
void video_resize(unsigned int width, unsigned int height)
{
	GLfloat tex_width, tex_height;
	int *csax, *csay;
	int x, y, csx, csy;
	unsigned int gt;
	int texsize;
	int bpp;

	if (!height) height = NATIVE_SCREEN_HEIGHT;
	if (!width) width = NATIVE_SCREEN_WIDTH;
	video.draw.width = width;
	video.draw.height = height;

	video.draw.autoscale = 1;

	switch (video.desktop.want_type) {
	case VIDEO_DDRAW:
#ifdef WIN32
		if (video.ddblit.surface) {
			SDL_FreeSurface(video.ddblit.surface);
			video.ddblit.surface = 0;
		}
		memset(&video.ddblit.fx, 0, sizeof(DDBLTFX));
		video.ddblit.fx.dwSize = sizeof(DDBLTFX);
		_setup_surface(width, height, SDL_DOUBLEBUF);
		video.ddblit.rect.top = video.clip.y;
		video.ddblit.rect.left = video.clip.x;
		video.ddblit.rect.right = video.clip.x + video.clip.w;
		video.ddblit.rect.bottom = video.clip.y + video.clip.h;
		video.ddblit.surface = SDL_CreateRGBSurface(SDL_HWSURFACE,
				NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT,
				video.surface->format->BitsPerPixel,
				video.surface->format->Rmask,
				video.surface->format->Gmask,
				video.surface->format->Bmask,0);
		bpp = video.surface->format->BytesPerPixel;
		if (video.ddblit.surface
		&& ((video.ddblit.surface->flags & SDL_HWSURFACE) == SDL_HWSURFACE)) {
			video.desktop.type = VIDEO_DDRAW;
			break;
		}
		/* fall through */
#endif
	case VIDEO_SURFACE:
RETRYSURF:	/* use SDL surfaces */
		video.draw.autoscale = 0;
		_setup_surface(width, height, 0);
		video.desktop.type = VIDEO_SURFACE;
		bpp = video.surface->format->BytesPerPixel;
		break;

	case VIDEO_YUV:
		if (video.overlay) {
			SDL_FreeYUVOverlay(video.overlay);
			video.overlay = 0;
		}
		_setup_surface(width, height, 0);
		if (video.yuvlayout == YUV_UYVY) {
			video.overlay = SDL_CreateYUVOverlay(
					NATIVE_SCREEN_WIDTH*2,
					NATIVE_SCREEN_HEIGHT,
					SDL_UYVY_OVERLAY,
					video.surface);
		} else if (video.yuvlayout == YUV_YUY2) {
			video.overlay = SDL_CreateYUVOverlay(
					NATIVE_SCREEN_WIDTH*2,
					NATIVE_SCREEN_HEIGHT,
					SDL_YUY2_OVERLAY,
					video.surface);
		} else {
			/* unknown layout */
			goto RETRYSURF;
		}
		if (!video.overlay) {
			/* can't get an overlay */
			goto RETRYSURF;
		}
		video.desktop.type = VIDEO_YUV;
		bpp = 4;
		break;
	case VIDEO_GL:
		SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
		_setup_surface(width, height, SDL_OPENGL);
		if (video.surface->format->BitsPerPixel < 15) {
			goto RETRYSURF;
		}
		my_glViewport(video.clip.x, video.clip.y,
			video.clip.w, video.clip.h);
		texsize = 2 << int_log2(NATIVE_SCREEN_WIDTH);
		if (texsize > video.gl.max_texsize) {
			/* can't do opengl! */
			goto RETRYSURF;
		}
#if defined(NVIDIA_PixelDataRange)
		if (video.gl.pixel_data_range) {
			if (!video.gl.framebuf) {
				video.gl.framebuf = db_glAllocateMemoryNV(
						NATIVE_SCREEN_WIDTH*NATIVE_SCREEN_HEIGHT*4,
						0.0, 1.0, 1.0);
			}
			glPixelDataRangeNV(GL_WRITE_PIXEL_DATA_RANGE_NV,
					NATIVE_SCREEN_WIDTH*NATIVE_SCREEN_HEIGHT*4,
					video.gl.framebuf);
			my_glEnableClientState(GL_WRITE_PIXEL_DATA_RANGE_NV);
		} else
#endif
		if (!video.gl.framebuf) {
			video.gl.framebuf = (void*)mem_alloc(NATIVE_SCREEN_WIDTH
							*NATIVE_SCREEN_HEIGHT*4);
		}
		
		video.gl.pitch = NATIVE_SCREEN_WIDTH * 4;
		my_glMatrixMode(GL_PROJECTION);
		my_glDeleteTextures(1, &video.gl.texture);
		my_glGenTextures(1, &video.gl.texture);
		my_glBindTexture(GL_TEXTURE_2D, video.gl.texture);
		my_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
		my_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
		if (video.gl.bilinear) {
			my_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
						GL_LINEAR);
			my_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
						GL_LINEAR);
		} else {
			my_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER,
						GL_NEAREST);
			my_glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER,
						GL_NEAREST);
		}
		my_glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, texsize, texsize,
			0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, 0);
		my_glClearColor(0.0, 0.0, 0.0, 1.0);
		my_glClear(GL_COLOR_BUFFER_BIT);
		SDL_GL_SwapBuffers();
		my_glClear(GL_COLOR_BUFFER_BIT);
		my_glShadeModel(GL_FLAT);
		my_glDisable(GL_DEPTH_TEST);
		my_glDisable(GL_LIGHTING);
		my_glDisable(GL_CULL_FACE);
		my_glEnable(GL_TEXTURE_2D);
		my_glMatrixMode(GL_MODELVIEW);
		my_glLoadIdentity();

		tex_width = ((GLfloat)(NATIVE_SCREEN_WIDTH)/(GLfloat)texsize);
		tex_height = ((GLfloat)(NATIVE_SCREEN_HEIGHT)/(GLfloat)texsize);
		if (my_glIsList(video.gl.displaylist))
			my_glDeleteLists(video.gl.displaylist, 1);
		video.gl.displaylist = my_glGenLists(1);
		my_glNewList(video.gl.displaylist, GL_COMPILE);
		my_glBindTexture(GL_TEXTURE_2D, video.gl.texture);
		my_glBegin(GL_QUADS);
		my_glTexCoord2f(0,tex_height); my_glVertex2f(-1.0f,-1.0f);
		my_glTexCoord2f(tex_width,tex_height);my_glVertex2f(1.0f,-1.0f);
		my_glTexCoord2f(tex_width,0); my_glVertex2f(1.0f, 1.0f);
		my_glTexCoord2f(0,0); my_glVertex2f(-1.0f, 1.0f);
		my_glEnd();
		my_glEndList();
		video.desktop.type = VIDEO_GL;
		bpp = 4;
		break;
	};
	if (!video.draw.autoscale && (video.clip.w != NATIVE_SCREEN_WIDTH
				|| video.clip.h != NATIVE_SCREEN_HEIGHT)) {
		if (video.draw.sax) free(video.draw.sax);
		if (video.draw.say) free(video.draw.say);
		video.draw.sax = (int*)mem_alloc((video.clip.w+1)*sizeof(int));
		video.draw.say = (int*)mem_alloc((video.clip.h+1)*sizeof(int));
		video.draw.sx = (int)(65536.0*(float)(NATIVE_SCREEN_WIDTH-1)
				/ (video.clip.w));
		video.draw.sy = (int)(65536.0*(float)(NATIVE_SCREEN_HEIGHT-1)
				/ (video.clip.h));
		
		/* precalculate increments */
		csx = 0;
		csax = video.draw.sax;
		for (x = 0; x <= video.clip.w; x++) {
			*csax = csx;
			csax++;
			csx &= 65535;
			csx += video.draw.sx;
		}
		csy = 0;
		csay = video.draw.say;
		for (y = 0; y <= video.clip.h; y++) {
			*csay = csy;
			csay++;
			csy &= 65535;
			csy += video.draw.sy;
		}

	} else {
		video.draw.sx = video.draw.sy = 0;
	}

	status.flags |= (NEED_UPDATE);
}
void video_colors(unsigned char palette[16][3])
{
	static SDL_Color imap[16];
	unsigned int y,u,v;
	int i;

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		if (video.surface->format->BytesPerPixel == 1) {
			/* okay, indexed color */
			for (i = 0; i < 16; i++) {
				video.pal[i] = i;
				imap[i].r = palette[i][0];
				imap[i].g = palette[i][1];
				imap[i].b = palette[i][2];
			}
			SDL_SetColors(video.surface, imap, 0, 16);
			return;
		}
		/* fall through */
	case VIDEO_DDRAW:
		for (i = 0; i < 16; i++) {
			video.pal[i] = SDL_MapRGB(video.surface->format,
						palette[i][0],
						palette[i][1],
						palette[i][2]);
		}
		break;
	case VIDEO_YUV:
		for (i = 0; i < 16; i++) {
			y =  ( 9797*(palette[i][0]) + 19237*(palette[i][1])
						+  3734*(palette[i][2]) ) >> 15;
			u =  (18492*((palette[i][2])-(y)) >> 15) + 128;
			v =  (23372*((palette[i][0])-(y)) >> 15) + 128;

			switch (video.yuvlayout) {
			case YUV_UYVY:
				/* u0 y0 v0 y1 */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				video.pal[i] = y | (v << 8) | (y << 16) | (u << 24);
#else
				video.pal[i] = u | (y << 8) | (v << 16) | (y << 24);
#endif
				break;
			case YUV_YUY2:
				/* y0 u0 y1 v0 */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
				video.pal[i] = v | (y << 8) | (u << 16) | (y << 24);
#else
				video.pal[i] = y | (u << 8) | (y << 16) | (v << 24);
#endif
				break;
			};
		}
		break;
	case VIDEO_GL:
		/* BGRA */
		for (i = 0; i < 16; i++) {
			video.pal[i] = palette[i][2] |
				(palette[i][1] << 8) |
				(palette[i][0] << 16) | (255 << 24);
		}
		break;
	};

	/* BGR; for scaling */
	for (i = 0; i < 16; i++) {
		video.tc_bgr32[i] = palette[i][2] |
			(palette[i][1] << 8) |
			(palette[i][0] << 16) | (255 << 24);
	}
}

void video_refresh(void)
{
	video.draw.pixels_flip();
	video.draw.pixels_clear();
}

static void inline _blit1n(int bpp, unsigned char *pixels, unsigned int pitch)
{
	unsigned int *csp, *esp, *dp;
	int *csay, *csax;
	unsigned int *c00, *c01, *c10, *c11;
	unsigned int outr, outg, outb;
	unsigned int pad;
	int y, x, ey, ex, t1, t2, sstep;
	int iny, lasty;
	void (*fn)(unsigned int y, ...);

	fn = (void *)video.draw.pixels_32;
	csay = video.draw.say;
	csp = (unsigned int *)video.cv32backing;
	esp = csp + NATIVE_SCREEN_WIDTH;
	lasty = -2;
	iny = 0;
	pad = pitch - (video.clip.w * bpp);
	for (y = 0; y < video.clip.h; y++) {
		if (iny != lasty) {
			/* we'll downblit the colors later */
			if (iny == lasty + 1) {
				/* move up one line */
				fn(iny+1, csp, video.tc_bgr32);
				dp = esp; esp = csp; csp=dp;
			} else {
				fn(iny, (csp = (unsigned int *)video.cv32backing),
					video.tc_bgr32);
				fn(iny+1, (esp = (csp + NATIVE_SCREEN_WIDTH)),
					video.tc_bgr32);
			}
			lasty = iny;
		}
		c00 = csp;
		c01 = csp; c01++;
		c10 = esp;
		c11 = esp; c11++;
		csax = video.draw.sax;
		for (x = 0; x < video.clip.w; x++) {
			ex = (*csax & 65535);
			ey = (*csay & 65535);
#define BLUE(Q) (Q&255)
#define GREEN(Q) (Q >> 8)&255
#define RED(Q) (Q >> 16)&255
			t1 = ((((BLUE(*c01)-BLUE(*c00))*ex) >> 16)+BLUE(*c00)) & 255;
			t2 = ((((BLUE(*c11)-BLUE(*c10))*ex) >> 16)+BLUE(*c10)) & 255;
			outb = (((t2-t1)*ey) >> 16) + t1;
			
			t1 = ((((GREEN(*c01)-GREEN(*c00))*ex) >> 16)+GREEN(*c00)) & 255;
			t2 = ((((GREEN(*c11)-GREEN(*c10))*ex) >> 16)+GREEN(*c10)) & 255;
			outg = (((t2-t1)*ey) >> 16) + t1;

			t1 = ((((RED(*c01)-RED(*c00))*ex) >> 16)+RED(*c00)) & 255;
			t2 = ((((RED(*c11)-RED(*c10))*ex) >> 16)+RED(*c10)) & 255;
			outr = (((t2-t1)*ey) >> 16) + t1;
#undef RED
#undef GREEN
#undef BLUE
			csax++;	
			sstep = (*csax >> 16);
			c00 += sstep;
			c01 += sstep;
			c10 += sstep;
			c11 += sstep;

			/* write output "pixel" */
			switch (bpp) {
			case 4:
			case 3:
				(*(unsigned int *)pixels) = SDL_MapRGB(
					video.surface->format, outr, outg, outb);
				break;
			case 2:
				/* err... */
				(*(unsigned short *)pixels) = SDL_MapRGB(
					video.surface->format, outr, outg, outb);
				break;
			case 1:
				/* er... */
				(*pixels) = SDL_MapRGB(
					video.surface->format, outr, outg, outb);
				break;
			};
			pixels += bpp;
		}
		pixels += pad;
		csay++;
		/* move y position on source */
		iny += (*csay >> 16);
	}
}
static void inline _blit11(int bpp, unsigned char *pixels, unsigned int pitch)
{
	unsigned char *pdata;
	unsigned int x, y;
	int pitch24;
	void (*fn)(unsigned int y, ...);

	switch (bpp) {
	case 4:
		fn = (void *)video.draw.pixels_32;
		for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
			fn(y, (unsigned int *)pixels, video.pal);
			pixels += pitch;
		}
		break;
	case 3:
		/* ... */
		fn = (void *)video.draw.pixels_32;
		pitch24 = pitch - (NATIVE_SCREEN_WIDTH * 3);
		if (pitch24 < 0) {
			return; /* eh? */
		}
		for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
			fn(y, (unsigned int *)video.cv32backing, video.pal);
			/* okay... */
			pdata = video.cv32backing;
			for (x = 0; x < NATIVE_SCREEN_WIDTH; x++) {
#if WORDS_BIGENDIAN
				memcpy(pixels, pdata+1, 3);
#else
				memcpy(pixels, pdata, 3);
#endif
				pdata += 4;
				pixels += 3;
			}
			pixels += pitch24;
		}
		break;
	case 2:
		fn = (void *)video.draw.pixels_16;
		for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
			fn(y, (unsigned short *)pixels, video.pal);
			pixels += pitch;
		}
		break;
	case 1:
		fn = (void *)video.draw.pixels_8;
		for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
			fn(y, (unsigned char *)pixels, video.tc_identity);
			pixels += pitch;
		}
		break;
	};
}


void video_blit(void)
{
	unsigned char *pixels;
	unsigned int bpp;
	unsigned int pitch;

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		if (SDL_MUSTLOCK(video.surface)) {
			while (SDL_LockSurface(video.surface) == -1) {
				SDL_Delay(10);
			}
		}
		bpp = video.surface->format->BytesPerPixel;
		pixels = (unsigned char *)video.surface->pixels;
		pixels += video.clip.y * video.surface->pitch;
		pixels += video.clip.x * bpp;
		pitch = video.surface->pitch;
		break;
	case VIDEO_YUV:
		SDL_LockYUVOverlay(video.overlay);
		pixels = (unsigned char *)*(video.overlay->pixels);
		pitch = *(video.overlay->pitches);
		bpp = 4;
		break;
	case VIDEO_GL:
		pixels = (unsigned char *)video.gl.framebuf;
		pitch = video.gl.pitch;
		bpp = 4;
		break;
	case VIDEO_DDRAW:
#ifdef WIN32
		if (SDL_MUSTLOCK(video.ddblit.surface)) {
			while (SDL_LockSurface(video.ddblit.surface) == -1) {
				SDL_Delay(10);
			}
		}
		pixels = (unsigned char *)video.ddblit.surface->pixels;
		pitch = video.ddblit.surface->pitch;
		bpp = video.surface->format->BytesPerPixel;
		break;
#else
		return; /* eh? */
#endif
	};

	vgamem_lock();
	if (video.draw.autoscale || (!video.draw.sx && !video.draw.sy)) {
		/* scaling is provided by the hardware, or isn't necessary */
		_blit11(bpp, pixels, pitch);
	} else {
		_blit1n(bpp, pixels, pitch);
	}
	vgamem_unlock();

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		if (SDL_MUSTLOCK(video.surface)) {
			SDL_UnlockSurface(video.surface);
		}
		SDL_Flip(video.surface);
		break;
#ifdef WIN32
	case VIDEO_DDRAW:
		if (SDL_MUSTLOCK(video.ddblit.surface)) {
			SDL_UnlockSurface(video.ddblit.surface);
		}
		switch (IDirectDrawSurface3_Blt(
				video.surface->hwdata->dd_writebuf,
				&video.ddblit.rect,
				video.ddblit.surface->hwdata->dd_surface,0,
				DDBLT_WAIT, NULL)) {
		case DD_OK:
			break;
		case DDERR_SURFACELOST:
			IDirectDrawSurface3_Restore(video.ddblit.surface->hwdata->dd_surface);
			IDirectDrawSurface3_Restore(video.surface->hwdata->dd_surface);
			break;
		default:
			break;
		};
		SDL_Flip(video.surface);
		break;
#endif
	case VIDEO_YUV:
		SDL_UnlockYUVOverlay(video.overlay);
		SDL_DisplayYUVOverlay(video.overlay, &video.clip);
		break;
	case VIDEO_GL:
		my_glBindTexture(GL_TEXTURE_2D, video.gl.texture);
		my_glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
			NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT,
			GL_BGRA_EXT,
			GL_UNSIGNED_INT_8_8_8_8_REV,
			video.gl.framebuf);
		my_glCallList(video.gl.displaylist);
		SDL_GL_SwapBuffers();
		break;
	};
}

void video_mousecursor(int vis)
{
	if (vis == -1) {
		/* toggle */
		video.mouse.visible = video.mouse.visible ? 0 : 1;
	} else if (vis == 0 || vis == 1) {
		video.mouse.visible = vis;
	}
	if (!video.mouse.visible && !video.desktop.fullscreen) {
		/* display the system cursor when not fullscreen-
		even when the mouse cursor is invisible */
		SDL_ShowCursor(SDL_ENABLE);
	} else {
		SDL_ShowCursor(SDL_DISABLE);
	}
}

void video_translate(unsigned int vx, unsigned int vy,
		unsigned int *x, unsigned int *y)
{
	if (vx < video.clip.x) vx = video.clip.x;
	vx -= video.clip.x;

	if (vy < video.clip.y) vy = video.clip.y;
	vy -= video.clip.y;

	if (vx > video.clip.w) vx = video.clip.w;
	if (vy > video.clip.h) vy = video.clip.h;

	vx *= NATIVE_SCREEN_WIDTH;
	vy *= NATIVE_SCREEN_HEIGHT;
	vx /= (video.draw.width - (video.draw.width - video.clip.w));
	vy /= (video.draw.height - (video.draw.height - video.clip.h));

	if (video.mouse.visible && (video.mouse.x != vx || video.mouse.y != vy)) {
		status.flags |= SOFTWARE_MOUSE_MOVED;
	}
	video.mouse.x = vx;
	video.mouse.y = vy;
	if (x) *x = vx;
	if (y) *y = vy;
}
