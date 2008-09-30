/*
 * Schism Tracker - a cross-platform Impulse Tracker clone
 * copyright (c) 2003-2005 chisel <schism@chisel.cjb.net>
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

#include <X11/X.h>
#include <X11/Xlib.h>
#ifdef HAVE_X11_EXTENSIONS_XVLIB_H
#include <X11/extensions/Xvlib.h>
#endif

#include "sdlmain.h"
#include "video.h"

unsigned int xv_yuvlayout(void)
{
#ifdef HAVE_X11_EXTENSIONS_XVLIB_H
	unsigned int ver, rev, eventB, reqB, errorB;
	XvImageFormatValues *formats;
	XvAdaptorInfo *ainfo;
	XvEncodingInfo *encodings;
	SDL_SysWMinfo info;
	Display *dpy;
	unsigned int nencode, nadaptors;
	unsigned int resc;
	int numImages;
	int i, j, k, nscreens;
	unsigned int w, h;
	int best;

	resc = -1;
	memset(&info, 0, sizeof(info));
	SDL_VERSION(&info.version);
	if (SDL_GetWMInfo(&info)) {
		dpy = info.info.x11.display;
	} else {
		dpy = 0;
	}
	if (!dpy) {
		dpy = XOpenDisplay(0);
		memset(&info, 0, sizeof(info));
		if (!dpy) return 0;
	}
	ver=rev=reqB=eventB=errorB=0;
	if ((Success != XvQueryExtension(dpy, &ver, &rev, &reqB,
						&eventB, &errorB))) {
		/* no XV support */
		return 0;
	}

	nscreens = ScreenCount(dpy);
	w = h = 0;
	for (i = 0; i < nscreens; i++) {
		XvQueryAdaptors(dpy, RootWindow(dpy, i), &nadaptors, &ainfo);
		for (j = 0; j < (signed) nadaptors; j++) {
			XvQueryEncodings(dpy, ainfo[j].base_id, &nencode, &encodings);
			best = -1;
			for (k = 0; k < (signed) nencode; k++) {
				if(strcmp(encodings[k].name, "XV_IMAGE"))
					continue;
                                if (encodings[k].width > w || encodings[k].height > h) {
					w = encodings[k].width;
					h = encodings[k].height;
					best = k;
				}
				
			}
			if (best == -1) continue;
			if (w < 640 || h < 400) continue;

			formats = XvListImageFormats(dpy, ainfo[j].base_id, &numImages);
			for (k = 0; k < numImages; k++) {
				if (formats[k].type == XvRGB) continue;
				if (w < 1280 || h < 400) {
					/* not enough xv memory for packed */
					switch (formats[k].id) {
					case VIDEO_YUV_YV12:
						resc = VIDEO_YUV_YV12_TV;
						break;
					case VIDEO_YUV_IYUV:
						resc = VIDEO_YUV_IYUV_TV;
						break;
					};
					continue;
				}
				switch (formats[k].id) {
				case VIDEO_YUV_UYVY:
				case VIDEO_YUV_YUY2:
				case VIDEO_YUV_YVYU:
					/* a packed format, and we have enough memory... */
					return formats[k].id;

				case VIDEO_YUV_YV12:
				case VIDEO_YUV_IYUV:
					resc = formats[k].id;
					break;
				};
			}
		}
	}
	return resc;
#else
	return -1;
#endif
}

