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
#define NATIVE_SCREEN_WIDTH             640
#define NATIVE_SCREEN_HEIGHT            400

/* should be the native res of the display (set once and never again)
 * assumes the user starts schism from the desktop and that the desktop
 * is the native res (or at least something with square pixels) */
static int display_native_x = -1;
static int display_native_y = -1;

#include "headers.h"
#include "it.h"
#include "osdefs.h"

/* bugs
 * ... in sdl. not in this file :)
 *
 *  - take special care to call SDL_SetVideoMode _exactly_ once
 *    when on a console (video.desktop.fb_hacks)
 *
 */

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
#if HAVE_SIGNAL_H
#include <signal.h>
#endif

/* for memcpy */
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

#include "sdlmain.h"

#include <unistd.h>
#include <fcntl.h>

#include "video.h"

#ifndef MACOSX
#ifdef WIN32
#include "auto/schismico.h"
#else
#include "auto/schismico_hires.h"
#endif
#endif

#ifndef APIENTRY
#define APIENTRY
#endif
#ifndef APIENTRYP
#define APIENTRYP APIENTRY *
#endif

extern int macosx_did_finderlaunch;

#define NVIDIA_PixelDataRange   1

#if !defined(USE_OPENGL)
typedef unsigned int GLuint;
typedef int GLint;
typedef int GLenum;
typedef int GLsizei;
typedef void GLvoid;
typedef float GLfloat;
typedef int GLboolean;
typedef int GLbitfield;
typedef float GLclampf;
typedef unsigned char GLubyte;
#endif

#if USE_OPENGL && NVIDIA_PixelDataRange

#ifndef WGL_NV_allocate_memory
#define WGL_NV_allocate_memory 1
typedef void * (APIENTRY * PFNWGLALLOCATEMEMORYNVPROC)
	(int size, float readfreq, float writefreq, float priority);
typedef void (APIENTRY * PFNWGLFREEMEMORYNVPROC) (void *pointer);
#endif

static PFNWGLALLOCATEMEMORYNVPROC db_glAllocateMemoryNV = NULL;
static PFNWGLFREEMEMORYNVPROC db_glFreeMemoryNV = NULL;

#ifndef GL_NV_pixel_data_range
#define GL_NV_pixel_data_range 1
#define GL_WRITE_PIXEL_DATA_RANGE_NV      0x8878
typedef void (APIENTRYP PFNGLPIXELDATARANGENVPROC) (GLenum target, GLsizei length, GLvoid *pointer);
typedef void (APIENTRYP PFNGLFLUSHPIXELDATARANGENVPROC) (GLenum target);
#endif

static PFNGLPIXELDATARANGENVPROC glPixelDataRangeNV = NULL;

#endif

/* leeto drawing skills */
#define MOUSE_HEIGHT    14
static const unsigned int _mouse_pointer[] = {
	/* x....... */  0x80,
	/* xx...... */  0xc0,
	/* xxx..... */  0xe0,
	/* xxxx.... */  0xf0,
	/* xxxxx... */  0xf8,
	/* xxxxxx.. */  0xfc,
	/* xxxxxxx. */  0xfe,
	/* xxxxxxxx */  0xff,
	/* xxxxxxx. */  0xfe,
	/* xxxxx... */  0xf8,
	/* x...xx.. */  0x8c,
	/* ....xx.. */  0x0c,
	/* .....xx. */  0x06,
	/* .....xx. */  0x06,

0,0
};


#ifdef WIN32
#include <windows.h>
#include "wine-ddraw.h"
struct private_hwdata {
	LPDIRECTDRAWSURFACE3 dd_surface;
	LPDIRECTDRAWSURFACE3 dd_writebuf;
};
#endif

struct video_cf {
	struct {
		unsigned int width;
		unsigned int height;

		int autoscale;
	} draw;
	struct {
		unsigned int width,height,bpp;
		int want_fixed;

		int swsurface;
		int fb_hacks;
		int fullscreen;
		int doublebuf;
		int want_type;
		int type;
#define VIDEO_SURFACE           0
#define VIDEO_DDRAW             1
#define VIDEO_YUV               2
#define VIDEO_GL                3
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
	unsigned int yuvlayout;
	SDL_Rect clip;
	SDL_Surface * surface;
	SDL_Overlay * overlay;
	/* to convert 32-bit color to 24-bit color */
	unsigned char cv32backing[NATIVE_SCREEN_WIDTH * 8];
	/* for tv mode */
	unsigned char cv8backing[NATIVE_SCREEN_WIDTH];
	struct {
		unsigned int x;
		unsigned int y;
		int visible;
	} mouse;

	unsigned int yuv_y[256];
	unsigned int yuv_u[256];
	unsigned int yuv_v[256];

	unsigned int pal[256];

	unsigned int tc_bgr32[256];
};
static struct video_cf video;

#ifdef USE_OPENGL
static int int_log2(int val) {
	int l = 0;
	while ((val >>= 1 ) != 0) l++;
	return l;
}
#endif

#ifdef MACOSX
#include <OpenGL/gl.h>
#include <OpenGL/glu.h>
#include <OpenGL/glext.h>

/* opengl is EVERYWHERE on macosx, and we can't use our dynamic linker mumbo
jumbo so easily...
*/
#define my_glCallList(z)        glCallList(z)
#define my_glTexSubImage2D(a,b,c,d,e,f,g,h,i)   glTexSubImage2D(a,b,c,d,e,f,g,h,i)
#define my_glDeleteLists(a,b)   glDeleteLists(a,b)
#define my_glTexParameteri(a,b,c)       glTexParameteri(a,b,c)
#define my_glBegin(g)           glBegin(g)
#define my_glEnd()              glEnd()
#define my_glEndList()  glEndList()
#define my_glTexCoord2f(a,b)    glTexCoord2f(a,b)
#define my_glVertex2f(a,b)      glVertex2f(a,b)
#define my_glNewList(a,b)       glNewList(a,b)
#define my_glIsList(a)          glIsList(a)
#define my_glGenLists(a)        glGenLists(a)
#define my_glLoadIdentity()     glLoadIdentity()
#define my_glMatrixMode(a)      glMatrixMode(a)
#define my_glEnable(a)          glEnable(a)
#define my_glDisable(a)         glDisable(a)
#define my_glBindTexture(a,b)   glBindTexture(a,b)
#define my_glClear(z)           glClear(z)
#define my_glClearColor(a,b,c,d) glClearColor(a,b,c,d)
#define my_glGetString(z)       glGetString(z)
#define my_glTexImage2D(a,b,c,d,e,f,g,h,i)      glTexImage2D(a,b,c,d,e,f,g,h,i)
#define my_glGetIntegerv(a,b)   glGetIntegerv(a,b)
#define my_glShadeModel(a)      glShadeModel(a)
#define my_glGenTextures(a,b)   glGenTextures(a,b)
#define my_glDeleteTextures(a,b)        glDeleteTextures(a,b)
#define my_glViewport(a,b,c,d)  glViewport(a,b,c,d)
#define my_glEnableClientState(z)       glEnableClientState(z)
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
	struct {
		unsigned int num;
		const char *name, *type;
	} yuv_layouts[] = {
		{VIDEO_YUV_IYUV, "IYUV", "planar"},
		{VIDEO_YUV_YV12_TV, "YV12", "planar+tv"},
		{VIDEO_YUV_IYUV_TV, "IYUV", "planar+tv"},
		{VIDEO_YUV_YVYU, "YVYU", "packed"},
		{VIDEO_YUV_UYVY, "UYVY", "packed"},
		{VIDEO_YUV_YUY2, "YUY2", "packed"},
		{VIDEO_YUV_RGBA, "RGBA", "packed"},
		{VIDEO_YUV_RGBT, "RGBT", "packed"},
		{VIDEO_YUV_RGB565, "RGB565", "packed"},
		{VIDEO_YUV_RGB24, "RGB24", "packed"},
		{VIDEO_YUV_RGB32, "RGB32", "packed"},
		{0, NULL, NULL},
	}, *layout = yuv_layouts;

	log_appendf(5, " Using driver '%s'", SDL_VideoDriverName(buf, 256));

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		log_appendf(5, " %s%s video surface",
			(video.surface->flags & SDL_HWSURFACE) ? "Hardware" : "Software",
			(video.surface->flags & SDL_HWACCEL) ? " accelerated" : "");
		if (SDL_MUSTLOCK(video.surface))
			log_append(4, 0, " Must lock surface");
		log_appendf(5, " Display format: %d bits/pixel", video.surface->format->BitsPerPixel);
		break;

	case VIDEO_YUV:
		/* if an overlay isn't hardware accelerated, what is it? I guess this works */
		log_appendf(5, " %s-accelerated video overlay",
			video.overlay->hw_overlay ? "Hardware" : "Non");
		while (video.yuvlayout != layout->num && layout->name != NULL)
			layout++;
		if (layout->name)
			log_appendf(5, " Display format: %s (%s)", layout->name, layout->type);
		else
			log_appendf(5, " Display format: %x", video.yuvlayout);
		break;
	case VIDEO_GL:
		log_appendf(5, " %s%s OpenGL interface",
			(video.surface->flags & SDL_HWSURFACE) ? "Hardware" : "Software",
			(video.surface->flags & SDL_HWACCEL) ? " accelerated" : "");
#if defined(NVIDIA_PixelDataRange)
		if (video.gl.pixel_data_range)
			log_append(5, 0, " NVidia pixel range extensions available");
#endif
		break;
	case VIDEO_DDRAW:
		// is this meaningful?
		log_appendf(5, " %s%s DirectDraw interface",
			(video.surface->flags & SDL_HWSURFACE) ? "Hardware" : "Software",
			(video.surface->flags & SDL_HWACCEL) ? " accelerated" : "");
		break;
	};
	log_appendf(5, " %d bits/pixel", video.surface->format->BitsPerPixel);
	if (video.desktop.fullscreen || video.desktop.fb_hacks) {
		log_appendf(5, " Display dimensions: %dx%d", video.desktop.width, video.desktop.height);
	}
}

// check if w and h are multiples of native res (and by the same multiplier)
static int best_resolution(int w, int h)
{
	if ((w % NATIVE_SCREEN_WIDTH == 0)
	&&  (h % NATIVE_SCREEN_HEIGHT == 0)
	&& ((w / NATIVE_SCREEN_WIDTH) == (h / NATIVE_SCREEN_HEIGHT))) {
		return 1;
	} else {
		return 0;
	}
}

int video_is_fullscreen(void)
{
	return video.desktop.fullscreen;
}
int video_width(void)
{
	return video.clip.w;
}
int video_height(void)
{
	return video.clip.h;
}
void video_shutdown(void)
{
	if (video.desktop.fullscreen) {
		video.desktop.fullscreen = 0;
		video_resize(0,0);
	}
}
void video_fullscreen(int tri)
{
	if (tri == 0 || video.desktop.fb_hacks) {
		video.desktop.fullscreen = 0;

	} else if (tri == 1) {
		video.desktop.fullscreen = 1;

	} else if (tri < 0) {
		video.desktop.fullscreen = video.desktop.fullscreen ? 0 : 1;
	}
	if (_did_init) {
		if (video.desktop.fullscreen) {
			video_resize(video.desktop.width, video.desktop.height);
		} else {
			video_resize(0, 0);
		}
		/* video_report(); - this should be done in main, not here */
	}
}

void video_setup(const char *driver)
{
	char *q;
	/* _SOME_ drivers can be switched to, but not all of them... */

	if (driver && strcasecmp(driver, "auto") == 0)
		driver = NULL;

	video.draw.width = NATIVE_SCREEN_WIDTH;
	video.draw.height = NATIVE_SCREEN_HEIGHT;
	video.mouse.visible = MOUSE_EMULATED;

	video.yuvlayout = VIDEO_YUV_NONE;
	if ((q=getenv("SCHISM_YUVLAYOUT")) || (q=getenv("YUVLAYOUT"))) {
		if (strcasecmp(q, "YUY2") == 0
		|| strcasecmp(q, "YUNV") == 0
		|| strcasecmp(q, "V422") == 0
		|| strcasecmp(q, "YUYV") == 0) {
			video.yuvlayout = VIDEO_YUV_YUY2;
		} else if (strcasecmp(q, "UYVY") == 0) {
			video.yuvlayout = VIDEO_YUV_UYVY;
		} else if (strcasecmp(q, "YVYU") == 0) {
			video.yuvlayout = VIDEO_YUV_YVYU;
		} else if (strcasecmp(q, "YV12") == 0) {
			video.yuvlayout = VIDEO_YUV_YV12;
		} else if (strcasecmp(q, "IYUV") == 0) {
			video.yuvlayout = VIDEO_YUV_IYUV;
		} else if (strcasecmp(q, "YV12/2") == 0) {
			video.yuvlayout = VIDEO_YUV_YV12_TV;
		} else if (strcasecmp(q, "IYUV/2") == 0) {
			video.yuvlayout = VIDEO_YUV_IYUV_TV;
		} else if (strcasecmp(q, "RGBA") == 0) {
			video.yuvlayout = VIDEO_YUV_RGBA;
		} else if (strcasecmp(q, "RGBT") == 0) {
			video.yuvlayout = VIDEO_YUV_RGBT;
		} else if (strcasecmp(q, "RGB565") == 0 || strcasecmp(q, "RGB2") == 0) {
			video.yuvlayout = VIDEO_YUV_RGB565;
		} else if (strcasecmp(q, "RGB24") == 0) {
			video.yuvlayout = VIDEO_YUV_RGB24;
		} else if (strcasecmp(q, "RGB32") == 0 || strcasecmp(q, "RGB") == 0) {
			video.yuvlayout = VIDEO_YUV_RGB32;
		} else if (sscanf(q, "%x", &video.yuvlayout) != 1) {
			video.yuvlayout = 0;
		}
	}

	q = getenv("SCHISM_DEBUG");
	if (q && strstr(q,"doublebuf")) {
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
#elif defined(GEKKO)
	if (!driver) {
		driver = "yuv";
	}
#elif defined(__APPLE__)
	if (!driver) {
		driver = "opengl";
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

	video.desktop.fb_hacks = 0;
	/* get xv info */
	if (video.yuvlayout == VIDEO_YUV_NONE)
		video.yuvlayout = os_yuvlayout();
	if ((video.yuvlayout != VIDEO_YUV_YV12_TV
	&& video.yuvlayout != VIDEO_YUV_IYUV_TV
	&& video.yuvlayout != VIDEO_YUV_NONE && 0) /* don't do this until we figure out how to make it better */
			 && !strcasecmp(driver, "x11")) {
		video.desktop.want_type = VIDEO_YUV;
		putenv((char *) "SDL_VIDEO_YUV_DIRECT=1");
		putenv((char *) "SDL_VIDEO_YUV_HWACCEL=1");
		putenv((char *) "SDL_VIDEODRIVER=x11");
#ifdef USE_X11
	} else if (!strcasecmp(driver, "dga")) {
		putenv((char *) "SDL_VIDEODRIVER=dga");
		video.desktop.want_type = VIDEO_SURFACE;
	} else if (!strcasecmp(driver, "directfb")) {
		putenv((char *) "SDL_VIDEODRIVER=directfb");
		video.desktop.want_type = VIDEO_SURFACE;
#endif
#if HAVE_LINUX_FB_H
	} else if (!strcasecmp(driver, "fbcon")
	|| !strcasecmp(driver, "linuxfb")
	|| !strcasecmp(driver, "fb")) {
		unset_env_var("DISPLAY");
		putenv((char *) "SDL_VIDEODRIVER=fbcon");
		video.desktop.want_type = VIDEO_SURFACE;

	} else if (strncmp(driver, "/dev/fb", 7) == 0) {
		unset_env_var("DISPLAY");
		putenv((char *) "SDL_VIDEODRIVER=fbcon");
		put_env_var("SDL_FBDEV", driver);
		video.desktop.want_type = VIDEO_SURFACE;

#endif

	} else if (!strcasecmp(driver, "aalib")
	|| !strcasecmp(driver, "aa")) {
		/* SDL needs to have been built this way... */
		unset_env_var("DISPLAY");
		putenv((char *) "SDL_VIDEODRIVER=aalib");
		video.desktop.want_type = VIDEO_SURFACE;
		video.desktop.fb_hacks = 1;

	} else if (video.yuvlayout != VIDEO_YUV_NONE && !strcasecmp(driver, "yuv")) {
		video.desktop.want_type = VIDEO_YUV;
		putenv((char *) "SDL_VIDEO_YUV_DIRECT=1");
		putenv((char *) "SDL_VIDEO_YUV_HWACCEL=1");
		/* leave everything else alone... */
	} else if (!strcasecmp(driver, "dummy")
	|| !strcasecmp(driver, "null")
	|| !strcasecmp(driver, "none")) {

		unset_env_var("DISPLAY");
		putenv((char *) "SDL_VIDEODRIVER=dummy");
		video.desktop.want_type = VIDEO_SURFACE;
		video.desktop.fb_hacks = 1;
#if HAVE_SIGNAL_H
		signal(SIGINT, SIG_DFL);
#endif

	} else if (!strcasecmp(driver, "gl") || !strcasecmp(driver, "opengl")) {
		video.desktop.want_type = VIDEO_GL;
	} else if (!strcasecmp(driver, "sdlauto")
	|| !strcasecmp(driver,"sdl")) {
		video.desktop.want_type = VIDEO_SURFACE;
	}
}

void video_startup(void)
{
	UNUSED static int did_this_2 = 0;
#if USE_OPENGL
	const char *gl_ext;
#endif
	char *q;
	SDL_Rect **modes;
	int i, j, x, y;

	/* get monitor native res (assumed to be user's desktop res)
	 * first time we start video */
	if (display_native_x < 0 || display_native_y < 0) {
		const SDL_VideoInfo* info = SDL_GetVideoInfo();
		display_native_x = info->current_w;
		display_native_y = info->current_h;
	}

	/* because first mode is 0 */
	vgamem_clear();
	vgamem_flip();

	SDL_WM_SetCaption("Schism Tracker", "Schism Tracker");
#ifndef MACOSX
/* apple/macs use a bundle; this overrides their nice pretty icon */
#ifdef WIN32
/* win32 icons must be 32x32 according to SDL 1.2 doc */
	SDL_Surface *icon = xpmdata(_schism_icon_xpm);
#else
	SDL_Surface *icon = xpmdata(_schism_icon_xpm_hires);
#endif
	SDL_WM_SetIcon(icon, NULL);
	SDL_FreeSurface(icon);
#endif

#ifndef MACOSX
	if (video.desktop.want_type == VIDEO_GL) {
#define Z(q) my_ ## q = SDL_GL_GetProcAddress( #q ); \
	if (! my_ ## q) { video.desktop.want_type = VIDEO_SURFACE; goto SKIP1; }
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
#if defined(USE_OPENGL)
		video.surface = SDL_SetVideoMode(640,400,0,SDL_OPENGL|SDL_RESIZABLE);
		if (!video.surface) {
			/* fallback */
			video.desktop.want_type = VIDEO_SURFACE;
		}
		video.gl.framebuf = NULL;
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
		video.gl.packed_pixel=(strstr(gl_ext,"EXT_packed_pixels") != NULL);
		video.gl.paletted_texture=(strstr(gl_ext,"EXT_paletted_texture") != NULL);
#if defined(NVIDIA_PixelDataRange)
		video.gl.pixel_data_range=(strstr(gl_ext,"GL_NV_pixel_data_range") != NULL)
			&& glPixelDataRangeNV && db_glAllocateMemoryNV && db_glFreeMemoryNV;
#endif
#endif // USE_OPENGL
	}

	x = y = -1;
	if ((q = getenv("SCHISM_VIDEO_RESOLUTION"))) {
		i = j = -1;
		if (sscanf(q,"%dx%d", &i,&j) == 2 && i >= 10 && j >= 10) {
			x = i;
			y = j;
		}
	}
#if HAVE_LINUX_FB_H
	if (!getenv("DISPLAY") && !video.desktop.fb_hacks) {
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
				if (x < 0 || y < 0) {
					x = s.xres;
					if (x < NATIVE_SCREEN_WIDTH)
						x = NATIVE_SCREEN_WIDTH;
					y = s.yres;
				}
				putenv((char *) "SDL_VIDEODRIVER=fbcon");
				video.desktop.bpp = s.bits_per_pixel;
				video.desktop.fb_hacks = 1;
				video.desktop.doublebuf = 1;
				video.desktop.fullscreen = 0;
				video.desktop.swsurface = 0;
				video.surface = SDL_SetVideoMode(x,y,
						video.desktop.bpp,
						SDL_HWSURFACE
						| SDL_DOUBLEBUF
						| SDL_ASYNCBLIT);
			}
			close(fb);
		}
	}
#endif
	if (!video.surface) {
		/* if we already got one... */
		video.surface = SDL_SetVideoMode(640,400,0,SDL_RESIZABLE);
		if (!video.surface) {
			perror("SDL_SetVideoMode");
			exit(255);
		}
	}
	video.desktop.bpp = video.surface->format->BitsPerPixel;

	if (x < 0 || y < 0) {
		modes = SDL_ListModes(NULL, SDL_FULLSCREEN | SDL_HWSURFACE);
		if (modes != (SDL_Rect**)0 && modes != (SDL_Rect**)-1) {
			for (i = 0; modes[i]; i++) {
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
					if (best_resolution(x,y))
						break;
				}
			}
		}
	}
	if (x < 0 || y < 0) {
		x = 640;
		y = 480;
	}

	video.desktop.want_fixed = -1;
	q = getenv("SCHISM_VIDEO_ASPECT");
	if (q && (strcasecmp(q,"nofixed") == 0 || strcasecmp(q,"full")==0
	|| strcasecmp(q,"fit") == 0 || strcasecmp(q,"wide") == 0
	|| strcasecmp(q,"no-fixed") == 0))
		video.desktop.want_fixed = 0;

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
	case VIDEO_YUV:
#ifdef MACOSX
		video.desktop.swsurface = 1;
#else
		video.desktop.swsurface = 0;
#endif
		break;

	case VIDEO_GL:
#ifdef MACOSX
		video.desktop.swsurface = 1;
#else
		video.desktop.swsurface = 0;
#endif
		video.gl.bilinear = cfg_video_gl_bilinear;
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
		video.desktop.swsurface = 1;
		video.desktop.want_type = VIDEO_SURFACE;
		break;
	};
	/* okay, i think we're ready */
	SDL_ShowCursor(SDL_DISABLE);
#if 0 /* Do we need these? Everything seems to work just fine without */
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
#endif

	_did_init = 1;
	video_fullscreen(video.desktop.fullscreen);
}

static SDL_Surface *_setup_surface(unsigned int w, unsigned int h, unsigned int sdlflags)
{
	int want_fixed = video.desktop.want_fixed;

	if (video.desktop.doublebuf)
		sdlflags |= (SDL_DOUBLEBUF|SDL_ASYNCBLIT);
	if (video.desktop.fullscreen) {
		w = video.desktop.width;
		h = video.desktop.height;
	} else {
		sdlflags |= SDL_RESIZABLE;
	}

	if (want_fixed == -1 && best_resolution(w,h)) {
		want_fixed = 0;
	}

	if (want_fixed) {
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

	if (video.desktop.fb_hacks && video.surface) {
		/* the original one will be _just fine_ */
	} else {
		if (video.desktop.fullscreen) {
			sdlflags &=~SDL_RESIZABLE;
			sdlflags |= SDL_FULLSCREEN;
		} else {
			sdlflags &=~SDL_FULLSCREEN;
			sdlflags |= SDL_RESIZABLE;
		}
		sdlflags |= (video.desktop.swsurface
				? SDL_SWSURFACE
				: SDL_HWSURFACE);
		
		/* if using swsurface, get a surface the size of the whole native monitor res
		/* to avoid issues with weirdo display modes
		/* get proper aspect ratio and surface of correct size */
		if (video.desktop.fullscreen && video.desktop.swsurface) {
			
			double ar = NATIVE_SCREEN_WIDTH / (double) NATIVE_SCREEN_HEIGHT;
			// ar = 4.0 / 3.0; want_fixed = 1; // uncomment for 4:3 fullscreen
			
			// get maximum size that can be this AR
			if ((display_native_y * ar) > display_native_x) {
				video.clip.h = display_native_x / ar;
				video.clip.w = display_native_x;
			} else {
				video.clip.h = display_native_y;
				video.clip.w = display_native_y * ar;
			}	
						
			// clip to size (i.e. letterbox if necessary)
			video.clip.x = (display_native_x - video.clip.w) / 2;
			video.clip.y = (display_native_y - video.clip.h) / 2;
			
			// get a surface the size of the whole screen @ native res
			w = display_native_x;
			h = display_native_y;
			
			/* if we don't care about getting the right aspect ratio,
			/* sod letterboxing and just get a surface the size of the entire display */
			if (!want_fixed) {
				video.clip.w = display_native_x;
				video.clip.h = display_native_y;
				video.clip.x = 0;
				video.clip.y = 0;
			}
			
		}
		
		video.surface = SDL_SetVideoMode(w, h,
			video.desktop.bpp, sdlflags);
	}
	if (!video.surface) {
		perror("SDL_SetVideoMode");
		exit(EXIT_FAILURE);
	}
	return video.surface;
}

#if USE_OPENGL
static void _set_gl_attributes(void)
{
	SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
	SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);
	SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE, 0);
	SDL_GL_SetAttribute(SDL_GL_ACCUM_ALPHA_SIZE, 0);
#if SDL_VERSION_ATLEAST(1,2,11)
	SDL_GL_SetAttribute(SDL_GL_SWAP_CONTROL, 0);
#endif
}
#endif

void video_resize(unsigned int width, unsigned int height)
{
#if USE_OPENGL
	GLfloat tex_width, tex_height;
	int texsize;
#endif

	if (!width) width = cfg_video_width;
	if (!height) height = cfg_video_height;
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
		if (video.ddblit.surface
		&& ((video.ddblit.surface->flags & SDL_HWSURFACE) == SDL_HWSURFACE)) {
			video.desktop.type = VIDEO_DDRAW;
			break;
		}
		/* fall through */
#endif
	case VIDEO_SURFACE:
RETRYSURF:      /* use SDL surfaces */
		video.draw.autoscale = 0;
		if (video.desktop.fb_hacks
		&& (video.desktop.width != NATIVE_SCREEN_WIDTH
			|| video.desktop.height != NATIVE_SCREEN_HEIGHT)) {
			video.draw.autoscale = 1;
		}
		_setup_surface(width, height, 0);
		video.desktop.type = VIDEO_SURFACE;
		break;

	case VIDEO_YUV:
		if (video.overlay) {
			SDL_FreeYUVOverlay(video.overlay);
			video.overlay = NULL;
		}
		_setup_surface(width, height, 0);
		/* TODO: switch? */
		switch (video.yuvlayout) {
		case VIDEO_YUV_YV12_TV:
			video.overlay = SDL_CreateYUVOverlay
				(NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT,
				 SDL_YV12_OVERLAY, video.surface);
			break;
		case VIDEO_YUV_IYUV_TV:
			video.overlay = SDL_CreateYUVOverlay
				(NATIVE_SCREEN_WIDTH, NATIVE_SCREEN_HEIGHT,
				 SDL_IYUV_OVERLAY, video.surface);
			break;
		case VIDEO_YUV_YV12:
			video.overlay = SDL_CreateYUVOverlay
				(2 * NATIVE_SCREEN_WIDTH,
				 2 * NATIVE_SCREEN_HEIGHT,
				 SDL_YV12_OVERLAY, video.surface);
			break;
		case VIDEO_YUV_IYUV:
			video.overlay = SDL_CreateYUVOverlay
				(2 * NATIVE_SCREEN_WIDTH,
				 2 * NATIVE_SCREEN_HEIGHT,
				 SDL_IYUV_OVERLAY, video.surface);
			break;
		case VIDEO_YUV_UYVY:
			video.overlay = SDL_CreateYUVOverlay
				(2 * NATIVE_SCREEN_WIDTH,
				 NATIVE_SCREEN_HEIGHT,
				 SDL_UYVY_OVERLAY, video.surface);
			break;
		case VIDEO_YUV_YVYU:
			video.overlay = SDL_CreateYUVOverlay
				(2 * NATIVE_SCREEN_WIDTH,
				 NATIVE_SCREEN_HEIGHT,
				 SDL_YVYU_OVERLAY, video.surface);
			break;
		case VIDEO_YUV_YUY2:
			video.overlay = SDL_CreateYUVOverlay
				(2 * NATIVE_SCREEN_WIDTH,
				 NATIVE_SCREEN_HEIGHT,
				 SDL_YUY2_OVERLAY, video.surface);
			break;
		case VIDEO_YUV_RGBA:
		case VIDEO_YUV_RGB32:
			video.overlay = SDL_CreateYUVOverlay
				(4*NATIVE_SCREEN_WIDTH,
				 NATIVE_SCREEN_HEIGHT,
				 video.yuvlayout, video.surface);
			break;
		case VIDEO_YUV_RGB24:
			video.overlay = SDL_CreateYUVOverlay
				(3*NATIVE_SCREEN_WIDTH,
				 NATIVE_SCREEN_HEIGHT,
				 video.yuvlayout, video.surface);
			break;
		case VIDEO_YUV_RGBT:
		case VIDEO_YUV_RGB565:
			video.overlay = SDL_CreateYUVOverlay
				(2*NATIVE_SCREEN_WIDTH,
				 NATIVE_SCREEN_HEIGHT,
				 video.yuvlayout, video.surface);
			break;

		default:
			/* unknown layout */
			goto RETRYSURF;
		}
		if (!video.overlay) {
			/* can't get an overlay */
			goto RETRYSURF;
		}
		switch (video.overlay->planes) {
		case 3:
		case 1:
			break;
		default:
			/* can't get a recognized planes */
			SDL_FreeYUVOverlay(video.overlay);
			video.overlay = NULL;
			goto RETRYSURF;
		};
		video.desktop.type = VIDEO_YUV;
		break;
#if defined(USE_OPENGL)
	case VIDEO_GL:
		_set_gl_attributes();
		_setup_surface(width, height, SDL_OPENGL);
		if (video.surface->format->BitsPerPixel < 15) {
			goto RETRYSURF;
		}
		/* grumble... */
		_set_gl_attributes();
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
			video.gl.framebuf = mem_alloc(NATIVE_SCREEN_WIDTH
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
			0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
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
		break;
#endif // USE_OPENGL
	};

	status.flags |= (NEED_UPDATE);
}
static void _make_yuv(unsigned int *y, unsigned int *u, unsigned int *v,
						int rgb[3])
{
	double red, green, blue, yy, cr, cb, ry, ru, rv;
	int r = rgb[0];
	int g = rgb[1];
	int b = rgb[2];

	red = (double)r / 255.0;
	green = (double)g / 255.0;
	blue = (double)b / 255.0;
	yy = 0.299 * red + 0.587 * green + 0.114 * blue;
	cb = blue - yy;
	cr = red - yy;
	ry = 16.0 + 219.0 * yy;
	ru = 128.0 + 126.0 * cb;
	rv = 128.0 + 160.0 * cr;
	*y = (uint8_t) ry;
	*u = (uint8_t) ru;
	*v = (uint8_t) rv;
}
static void _yuv_pal(int i, int rgb[3])
{
	unsigned int y,u,v;
	_make_yuv(&y, &u, &v, rgb);
	switch (video.yuvlayout) {
	/* planar modes */
	case VIDEO_YUV_YV12:
	case VIDEO_YUV_IYUV:
		/* this is fake; we simply record the infomration here */
		video.yuv_y[i] = y|(y<<8);
		video.yuv_u[i] = u;
		video.yuv_v[i] = v;
		break;

	/* tv planar modes */
	case VIDEO_YUV_YV12_TV:
	case VIDEO_YUV_IYUV_TV:
		/* _blitTV */
		video.yuv_y[i] = y;
		video.yuv_u[i] = (u >> 4) & 0xF;
		video.yuv_v[i] = (v >> 4) & 0xF;
		break;

	/* packed modes */
	case VIDEO_YUV_YVYU:
		/* y0 v0 y1 u0 */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		video.pal[i] = u | (y << 8) | (v << 16) | (y << 24);
#else
		video.pal[i] = y | (v << 8) | (y << 16) | (u << 24);
#endif
		break;
	case VIDEO_YUV_UYVY:
		/* u0 y0 v0 y1 */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		video.pal[i] = y | (v << 8) | (y << 16) | (u << 24);
#else
		video.pal[i] = u | (y << 8) | (v << 16) | (y << 24);
#endif
		break;
	case VIDEO_YUV_YUY2:
		/* y0 u0 y1 v0 */
#if SDL_BYTEORDER == SDL_BIG_ENDIAN
		video.pal[i] = v | (y << 8) | (u << 16) | (y << 24);
#else
		video.pal[i] = y | (u << 8) | (y << 16) | (v << 24);
#endif
		break;
	case VIDEO_YUV_RGBA:
	case VIDEO_YUV_RGB32:
		video.pal[i] = rgb[2] |
			(rgb[1] << 8) |
			(rgb[0] << 16) | (255 << 24);
		break;
	case VIDEO_YUV_RGB24:
		video.pal[i] = rgb[2] |
			(rgb[1] << 8) |
			(rgb[0] << 16);
		break;
	case VIDEO_YUV_RGBT:
		video.pal[i] =
			((rgb[0] <<  8) & 0x7c00)
		|       ((rgb[1] <<  3) & 0x3e0)
		|       ((rgb[2] >>  2) & 0x1f);
		break;
	case VIDEO_YUV_RGB565:
		video.pal[i] =
			((rgb[0] <<  9) & 0xf800)
		|       ((rgb[1] <<  4) & 0x7e0)
		|       ((rgb[2] >>  2) & 0x1f);
		break;
	};
}
static void _sdl_pal(int i, int rgb[3])
{
	video.pal[i] = SDL_MapRGB(video.surface->format,
			rgb[0], rgb[1], rgb[2]);
}
static void _bgr32_pal(int i, int rgb[3])
{
	video.tc_bgr32[i] = rgb[2] |
			(rgb[1] << 8) |
			(rgb[0] << 16) | (255 << 24);
}
static void _gl_pal(int i, int rgb[3])
{
	video.pal[i] = rgb[2] |
			(rgb[1] << 8) |
			(rgb[0] << 16) | (255 << 24);
}
void video_colors(unsigned char palette[16][3])
{
	static SDL_Color imap[16];
	void (*fun)(int i,int rgb[3]);
	const int lastmap[] = { 0,1,2,3,5 };
	int rgb[3], i, j, p;

	switch (video.desktop.type) {
	case VIDEO_SURFACE:
		if (video.surface->format->BytesPerPixel == 1) {
			const int depthmap[] = { 0, 15,14,7,
						8, 8, 9, 12,
						6, 1, 2, 2,
						10, 3, 11, 11 };
			/* okay, indexed color */
			for (i = 0; i < 16; i++) {
				video.pal[i] = i;
				imap[i].r = palette[i][0];
				imap[i].g = palette[i][1];
				imap[i].b = palette[i][2];

				rgb[0]=palette[i][0];
				rgb[1]=palette[i][1];
				rgb[2]=palette[i][2];
				_bgr32_pal(i, rgb);

			}
			for (i = 128; i < 256; i++) {
				video.pal[i] = depthmap[(i>>4)];
			}
			for (i = 128; i < 256; i++) {
				j = i - 128;
				p = lastmap[(j>>5)];
				rgb[0] = (int)palette[p][0] +
					(((int)(palette[p+1][0]
					- palette[p][0]) * (j&31)) /32);
				rgb[1] = (int)palette[p][1] +
					(((int)(palette[p+1][1]
					- palette[p][1]) * (j&31)) /32);
				rgb[2] = (int)palette[p][2] +
					(((int)(palette[p+1][2]
					- palette[p][2]) * (j&31)) /32);
				_bgr32_pal(i, rgb);
			}
			SDL_SetColors(video.surface, imap, 0, 16);
			return;
		}
		/* fall through */
	case VIDEO_DDRAW:
		fun = _sdl_pal;
		break;
	case VIDEO_YUV:
		fun = _yuv_pal;
		break;
	case VIDEO_GL:
		fun = _gl_pal;
		break;
	default:
		/* eh? */
		return;
	};
	/* make our "base" space */
	for (i = 0; i < 16; i++) {
		rgb[0]=palette[i][0];
		rgb[1]=palette[i][1];
		rgb[2]=palette[i][2];
		fun(i, rgb);
		_bgr32_pal(i, rgb);
	}
	/* make our "gradient" space */
	for (i = 128; i < 256; i++) {
		j = i - 128;
		p = lastmap[(j>>5)];
		rgb[0] = (int)palette[p][0] +
			(((int)(palette[p+1][0] - palette[p][0]) * (j&31)) /32);
		rgb[1] = (int)palette[p][1] +
			(((int)(palette[p+1][1] - palette[p][1]) * (j&31)) /32);
		rgb[2] = (int)palette[p][2] +
			(((int)(palette[p+1][2] - palette[p][2]) * (j&31)) /32);
		fun(i, rgb);
		_bgr32_pal(i, rgb);
	}
}

void video_refresh(void)
{
	vgamem_flip();
	vgamem_clear();
}

static inline void make_mouseline(unsigned int x, unsigned int v, unsigned int y, unsigned int mouseline[80])
{
	unsigned int z;

	memset(mouseline, 0, 80*sizeof(unsigned int));
	if (video.mouse.visible != MOUSE_EMULATED
	    || !(status.flags & IS_FOCUSED)
	    || y < video.mouse.y
	    || y >= video.mouse.y+MOUSE_HEIGHT) {
		return;
	}

	z = _mouse_pointer[ y - video.mouse.y ];
	mouseline[x] = z >> v;
	if (x < 79) mouseline[x+1] = (z << (8-v)) & 0xff;
}


#define FIXED_BITS 8
#define FIXED_MASK ((1 << FIXED_BITS) - 1)
#define ONE_HALF_FIXED (1 << (FIXED_BITS - 1))
#define INT2FIXED(x) ((x) << FIXED_BITS)
#define FIXED2INT(x) ((x) >> FIXED_BITS)
#define FRAC(x) ((x) & FIXED_MASK)

static void _blit1n(int bpp, unsigned char *pixels, unsigned int pitch)
{
	unsigned int *csp, *esp, *dp;
	unsigned int c00, c01, c10, c11;
	unsigned int outr, outg, outb;
	unsigned int pad;
	int fixedx, fixedy, scalex, scaley;
	unsigned int y, x,ey,ex,t1,t2;
	unsigned int mouseline[80];
	unsigned int mouseline_x, mouseline_v;
	int iny, lasty;

	mouseline_x = (video.mouse.x / 8);
	mouseline_v = (video.mouse.x % 8);

	csp = (unsigned int *)video.cv32backing;
	esp = csp + NATIVE_SCREEN_WIDTH;
	lasty = -2;
	iny = 0;
	pad = pitch - (video.clip.w * bpp);
	scalex = INT2FIXED(NATIVE_SCREEN_WIDTH-1) / video.clip.w;
	scaley = INT2FIXED(NATIVE_SCREEN_HEIGHT-1) / video.clip.h;
	for (y = 0, fixedy = 0; (y < video.clip.h); y++, fixedy += scaley) {
		iny = FIXED2INT(fixedy);
		if (iny != lasty) {
			make_mouseline(mouseline_x, mouseline_v, iny, mouseline);

			/* we'll downblit the colors later */
			if (iny == lasty + 1) {
				/* move up one line */
				vgamem_scan32(iny+1, csp, video.tc_bgr32, mouseline);
				dp = esp; esp = csp; csp=dp;
			} else {
				vgamem_scan32(iny, (csp = (unsigned int *)video.cv32backing),
					video.tc_bgr32, mouseline);
				vgamem_scan32(iny+1, (esp = (csp + NATIVE_SCREEN_WIDTH)),
					video.tc_bgr32, mouseline);
			}
			lasty = iny;
		}
		for (x = 0, fixedx = 0; x < video.clip.w; x++, fixedx += scalex) {
			ex = FRAC(fixedx);
			ey = FRAC(fixedy);

			c00 = csp[FIXED2INT(fixedx)];
			c01 = csp[FIXED2INT(fixedx) + 1];
			c10 = esp[FIXED2INT(fixedx)];
			c11 = esp[FIXED2INT(fixedx) + 1];

#if FIXED_BITS <= 8
			/* When there are enough bits between blue and
			 * red, do the RB channels together
			 * See http://www.virtualdub.org/blog/pivot/entry.php?id=117
			 * for a quick explanation */
#define REDBLUE(Q) ((Q) & 0x00FF00FF)
#define GREEN(Q) ((Q) & 0x0000FF00)
			t1 = REDBLUE((((REDBLUE(c01)-REDBLUE(c00))*ex) >> FIXED_BITS)+REDBLUE(c00));
			t2 = REDBLUE((((REDBLUE(c11)-REDBLUE(c10))*ex) >> FIXED_BITS)+REDBLUE(c10));
			outb = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

			t1 = GREEN((((GREEN(c01)-GREEN(c00))*ex) >> FIXED_BITS)+GREEN(c00));
			t2 = GREEN((((GREEN(c11)-GREEN(c10))*ex) >> FIXED_BITS)+GREEN(c10));
			outg = (((((t2-t1)*ey) >> FIXED_BITS) + t1) >> 8) & 0xFF;

			outr = (outb >> 16) & 0xFF;
			outb &= 0xFF;
#undef REDBLUE
#undef GREEN
#else
#define BLUE(Q) (Q & 255)
#define GREEN(Q) ((Q >> 8) & 255)
#define RED(Q) ((Q >> 16) & 255)
			t1 = ((((BLUE(c01)-BLUE(c00))*ex) >> FIXED_BITS)+BLUE(c00)) & 0xFF;
			t2 = ((((BLUE(c11)-BLUE(c10))*ex) >> FIXED_BITS)+BLUE(c10)) & 0xFF;
			outb = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

			t1 = ((((GREEN(c01)-GREEN(c00))*ex) >> FIXED_BITS)+GREEN(c00)) & 0xFF;
			t2 = ((((GREEN(c11)-GREEN(c10))*ex) >> FIXED_BITS)+GREEN(c10)) & 0xFF;
			outg = ((((t2-t1)*ey) >> FIXED_BITS) + t1);

			t1 = ((((RED(c01)-RED(c00))*ex) >> FIXED_BITS)+RED(c00)) & 0xFF;
			t2 = ((((RED(c11)-RED(c10))*ex) >> FIXED_BITS)+RED(c10)) & 0xFF;
			outr = ((((t2-t1)*ey) >> FIXED_BITS) + t1);
#undef RED
#undef GREEN
#undef BLUE
#endif
			/* write output "pixel" */
			switch (bpp) {
			case 4:
				/* inline MapRGB */
				(*(unsigned int *)pixels) = 0xFF000000 | (outr << 16) | (outg << 8) | outb;
				break;
			case 3:
				/* inline MapRGB */
				(*(unsigned int *)pixels) = (outr << 16) | (outg << 8) | outb;
				break;
			case 2:
				/* inline MapRGB if possible */
				if (video.surface->format->palette) {
					/* err... */
					(*(unsigned short *)pixels) = SDL_MapRGB(
						video.surface->format, outr, outg, outb);
				} else if (video.surface->format->Gloss == 2) {
					/* RGB565 */
					(*(unsigned short *)pixels) = ((outr << 8) & 0xF800) |
						((outg << 3) & 0x07E0) |
						(outb >> 3);
				} else {
					/* RGB555 */
					(*(unsigned short *)pixels) = 0x8000 |
						((outr << 7) & 0x7C00) |
						((outg << 2) & 0x03E0) |
						(outb >> 3);
				}
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
	}
}
static void _blitYY(unsigned char *pixels, unsigned int pitch, unsigned int *tpal)
{
	unsigned int mouseline_x = (video.mouse.x / 8);
	unsigned int mouseline_v = (video.mouse.x % 8);
	unsigned int mouseline[80];
	int y;

	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline);

		vgamem_scan16(y, (unsigned short *)pixels, tpal, mouseline);
		memcpy(pixels+pitch,pixels,pitch);
		pixels += pitch;
		pixels += pitch;
	}
}
static void _blitUV(unsigned char *pixels, unsigned int pitch, unsigned int *tpal)
{
	unsigned int mouseline_x = (video.mouse.x / 8);
	unsigned int mouseline_v = (video.mouse.x % 8);
	unsigned int mouseline[80];
	int y;
	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline);
		vgamem_scan8(y, (unsigned char *)pixels, tpal, mouseline);
		pixels += pitch;
	}
}
static void _blitTV(unsigned char *pixels, UNUSED unsigned int pitch, unsigned int *tpal)
{
	unsigned int mouseline_x = (video.mouse.x / 8);
	unsigned int mouseline_v = (video.mouse.x % 8);
	unsigned int mouseline[80];
	int y, x;
	for (y = 0; y < NATIVE_SCREEN_HEIGHT; y += 2) {
		make_mouseline(mouseline_x, mouseline_v, y, mouseline);
		vgamem_scan8(y, (unsigned char *)video.cv8backing, tpal, mouseline);
		for (x = 0; x < NATIVE_SCREEN_WIDTH; x += 2) {
			*pixels++ = video.cv8backing[x+1]
				| (video.cv8backing[x] << 4);
		}
	}
}

static void _blit11(int bpp, unsigned char *pixels, unsigned int pitch,
			unsigned int *tpal)
{
	unsigned int mouseline_x = (video.mouse.x / 8);
	unsigned int mouseline_v = (video.mouse.x % 8);
	unsigned int mouseline[80];
	unsigned char *pdata;
	unsigned int x, y;
	int pitch24;

	switch (bpp) {
	case 4:
		for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
			make_mouseline(mouseline_x, mouseline_v, y, mouseline);
			vgamem_scan32(y, (unsigned int *)pixels, tpal, mouseline);
			pixels += pitch;
		}
		break;
	case 3:
		/* ... */
		pitch24 = pitch - (NATIVE_SCREEN_WIDTH * 3);
		if (pitch24 < 0) {
			return; /* eh? */
		}
		for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
			make_mouseline(mouseline_x, mouseline_v, y, mouseline);
			vgamem_scan32(y,(unsigned int*)video.cv32backing,tpal, mouseline);
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
		for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
			make_mouseline(mouseline_x, mouseline_v, y, mouseline);
			vgamem_scan16(y, (unsigned short *)pixels, tpal, mouseline);
			pixels += pitch;
		}
		break;
	case 1:
		for (y = 0; y < NATIVE_SCREEN_HEIGHT; y++) {
			make_mouseline(mouseline_x, mouseline_v, y, mouseline);
			vgamem_scan8(y, (unsigned char *)pixels, tpal, mouseline);
			pixels += pitch;
		}
		break;
	};
}

static void _video_blit_planar(void) {
	SDL_LockYUVOverlay(video.overlay);
	vgamem_lock();

	switch (video.yuvlayout) {
	case VIDEO_YUV_YV12_TV:
		/* halfwidth Y+V+U */
		_blitUV(video.overlay->pixels[0], video.overlay->pitches[0], video.yuv_y);
		_blitTV(video.overlay->pixels[1], video.overlay->pitches[1], video.yuv_v);
		_blitTV(video.overlay->pixels[2], video.overlay->pitches[2], video.yuv_u);
		break;
	case VIDEO_YUV_IYUV_TV:
		/* halfwidth Y+U+V */
		_blitUV(video.overlay->pixels[0], video.overlay->pitches[0], video.yuv_y);
		_blitTV(video.overlay->pixels[1], video.overlay->pitches[1], video.yuv_u);
		_blitTV(video.overlay->pixels[2], video.overlay->pitches[2], video.yuv_v);
		break;

	case VIDEO_YUV_YV12:
		/* Y+V+U */
		_blitYY(video.overlay->pixels[0], video.overlay->pitches[0], video.yuv_y);
		_blitUV(video.overlay->pixels[1], video.overlay->pitches[1], video.yuv_v);
		_blitUV(video.overlay->pixels[2], video.overlay->pitches[2], video.yuv_u);
		break;
	case VIDEO_YUV_IYUV:
		/* Y+U+V */
		_blitYY(video.overlay->pixels[0], video.overlay->pitches[0], video.yuv_y);
		_blitUV(video.overlay->pixels[1], video.overlay->pitches[1], video.yuv_u);
		_blitUV(video.overlay->pixels[2], video.overlay->pitches[2], video.yuv_v);
		break;
	};

	vgamem_unlock();

	SDL_UnlockYUVOverlay(video.overlay);
	SDL_DisplayYUVOverlay(video.overlay, &video.clip);
}

void video_blit(void)
{
	unsigned char *pixels = NULL;
	unsigned int bpp = 0;
	unsigned int pitch = 0;

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
		if (video.overlay->planes == 3) {
			_video_blit_planar();
			return;
		}

		SDL_LockYUVOverlay(video.overlay);
		pixels = (unsigned char *)*(video.overlay->pixels);
		pitch = *(video.overlay->pitches);
		switch (video.yuvlayout) {
		case VIDEO_YUV_RGBT:   bpp = 2; break;
		case VIDEO_YUV_RGB565: bpp = 2; break;
		case VIDEO_YUV_RGB24:  bpp = 3; break;
		default:
			bpp = 4;
		};
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
	if (video.draw.autoscale
	    || (video.clip.w == NATIVE_SCREEN_WIDTH && video.clip.h == NATIVE_SCREEN_HEIGHT)) {
		/* scaling is provided by the hardware, or isn't necessary */
		_blit11(bpp, pixels, pitch, video.pal);
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
#if defined(USE_OPENGL)
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
#endif
	};
}

int video_mousecursor_visible(void)
{
	return video.mouse.visible;
}

void video_mousecursor(int vis)
{
	const char *state[] = {
		"Mouse disabled",
		"Software mouse cursor enabled",
		"Hardware mouse cursor enabled",
	};

	if (status.flags & NO_MOUSE) {
		// disable it no matter what
		video.mouse.visible = MOUSE_DISABLED;
		//SDL_ShowCursor(0);
		return;
	}

	switch (vis) {
	case MOUSE_CYCLE_STATE:
		vis = (video.mouse.visible + 1) % MOUSE_CYCLE_STATE;
		/* fall through */
	case MOUSE_DISABLED:
	case MOUSE_SYSTEM:
	case MOUSE_EMULATED:
		video.mouse.visible = vis;
		status_text_flash("%s", state[video.mouse.visible]);
	case MOUSE_RESET_STATE:
		break;
	default:
		video.mouse.visible = MOUSE_EMULATED;
	}

	SDL_ShowCursor(video.mouse.visible == MOUSE_SYSTEM);

	// Totally turn off mouse event sending when the mouse is disabled
	int evstate = video.mouse.visible == MOUSE_DISABLED ? SDL_DISABLE : SDL_ENABLE;
	if (evstate != SDL_EventState(SDL_MOUSEMOTION, SDL_QUERY)) {
		SDL_EventState(SDL_MOUSEMOTION, evstate);
		SDL_EventState(SDL_MOUSEBUTTONDOWN, evstate);
		SDL_EventState(SDL_MOUSEBUTTONUP, evstate);
	}
}

int video_gl_bilinear(void)
{
	return video.gl.bilinear;
}

void video_translate(unsigned int vx, unsigned int vy,
		unsigned int *x, unsigned int *y)
{
	if ((signed) vx < video.clip.x) vx = video.clip.x;
	vx -= video.clip.x;

	if ((signed) vy < video.clip.y) vy = video.clip.y;
	vy -= video.clip.y;

	if ((signed) vx > video.clip.w) vx = video.clip.w;
	if ((signed) vy > video.clip.h) vy = video.clip.h;

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

