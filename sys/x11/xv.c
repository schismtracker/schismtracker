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

#include <X11/X.h>
#include <X11/Xlib.h>
#include <X11/extensions/Xvlib.h>

#ifndef HAVE_X11_EXTENSIONS_XVLIB_H
# error what
#endif

#include "sdlmain.h"
#include "video.h"
#include "osdefs.h"

unsigned int xv_yuvlayout(void)
{
	unsigned int ver, rev, eventB, reqB, errorB;
	XvImageFormatValues *formats;
	XvAdaptorInfo *ainfo;
	XvEncodingInfo *encodings;
	SDL_SysWMinfo info = {};
	Display *dpy;
	unsigned int nencode, nadaptors;
	unsigned int fmt = VIDEO_YUV_NONE;
	int numImages;
	int screen, nscreens, img;
	unsigned int adaptor, enc;
	unsigned int w, h;
	unsigned int best;

	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(video_window(), &info)) {
		dpy = info.info.x11.display;
	} else {
		dpy = NULL;
		printf("sdl_getwminfo?\n");
	}

	if (!dpy) {
#if 0
		/* this never closes the display, thus causing a memleak
		(and we can't reasonably call XCloseDisplay ourselves) */
		dpy = XOpenDisplay(NULL);
		if (!dpy)
			return VIDEO_YUV_NONE;
#else
		return VIDEO_YUV_NONE;
#endif
	}


	ver = rev = reqB = eventB = errorB = 0;
	if (XvQueryExtension(dpy, &ver, &rev, &reqB, &eventB, &errorB) != Success) {
		/* no XV support */
		return VIDEO_YUV_NONE;
	}

	nscreens = ScreenCount(dpy);
	w = h = 0;
	for (screen = 0; screen < nscreens; screen++) {
		XvQueryAdaptors(dpy, RootWindow(dpy, screen), &nadaptors, &ainfo);
		for (adaptor = 0; adaptor < nadaptors; adaptor++) {
			XvQueryEncodings(dpy, ainfo[adaptor].base_id, &nencode, &encodings);
			best = nencode; // impossible value
			for (enc = 0; enc < nencode; enc++) {
				if (strcmp(encodings[enc].name, "XV_IMAGE") != 0)
					continue;
				if (encodings[enc].width > w || encodings[enc].height > h) {
					w = encodings[enc].width;
					h = encodings[enc].height;
					best = enc;
				}
			}
			XvFreeEncodingInfo(encodings);

			if (best == nencode || w < 640 || h < 400)
				continue;

			formats = XvListImageFormats(dpy, ainfo[adaptor].base_id, &numImages);
			for (img = 0; img < numImages; img++) {
				if (formats[img].type == XvRGB) continue;
				if (w < 1280 || h < 400) {
					/* not enough xv memory for packed */
					switch (formats[img].id) {
					case VIDEO_YUV_YV12:
						fmt = VIDEO_YUV_YV12_TV;
						break;
					case VIDEO_YUV_IYUV:
						fmt = VIDEO_YUV_IYUV_TV;
						break;
					}
					continue;
				}
				switch (formats[img].id) {
				case VIDEO_YUV_UYVY:
				case VIDEO_YUV_YUY2:
				case VIDEO_YUV_YVYU:
					/* a packed format, and we have enough memory... */
					fmt = formats[img].id;
					XFree(formats);
					XvFreeAdaptorInfo(ainfo);
					return fmt;

				case VIDEO_YUV_YV12:
				case VIDEO_YUV_IYUV:
					fmt = formats[img].id;
					break;
				}
			}
			XFree(formats);
		}
		XvFreeAdaptorInfo(ainfo);
	}
	return fmt;
}

