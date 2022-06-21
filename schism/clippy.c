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

#include "headers.h" /* always include this one first, kthx */

#include "clippy.h"
#include "event.h"

#include "util.h"

#include "sdlmain.h"
#include "video.h"

static char *_current_selection = NULL;
static char *_current_clipboard = NULL;
static struct widget *_widget_owner[16] = {NULL};

static int has_sys_clip;
#if defined(WIN32)
static HWND native_window, _hmem;
#elif defined(__QNXNTO__)
static unsigned short inputgroup;
#elif defined(USE_X11)
static Display *native_display = NULL;
static Window native_window;
static Atom atom_sel;
static Atom atom_clip;
static void __noop_v(void){};
#endif

#ifdef MACOSX
extern const char *macosx_clippy_get(void);
extern void macosx_clippy_put(const char *buf);
#endif

static void _clippy_copy_to_sys(int do_sel)
{
	int j;
	char *tmp;
	char *dst;
	char *freeme;
#if defined(__QNXNTO__)
	PhClipboardHdr clheader = {Ph_CLIPBOARD_TYPE_TEXT, 0, NULL};
	int *cldata;
	int status;
#endif

	freeme = NULL;
	if (!_current_selection) {
		dst = NULL;
		j = 0;
	} else
#if defined(WIN32)
	{
		int i;
		/* need twice the space since newlines are replaced with \r\n */
		freeme = tmp = malloc(strlen(_current_selection)*2 + 1);
		if (!tmp) return;
		for (i = j = 0; _current_selection[i]; i++) {
			if (_current_selection[i] == '\r' || _current_selection[i] == '\n') {
				tmp[j++] = '\r';
				tmp[j++] = '\n';
			} else {
				tmp[j++] = _current_selection[i];
			}
		}
		tmp[j] = '\0';
	}
#else
	if (has_sys_clip) {
		int i;
		/* convert to local */
		freeme = dst = malloc(strlen(_current_selection)+4);
		if (!dst) return;
		for (i = j = 0; _current_selection[i]; i++) {
			dst[j] = _current_selection[i];
			if (dst[j] == '\r') dst[j] = '\n';
			j++;
		}
		dst[j] = '\0';
	} else {
		dst = NULL;
		j = 0;
	}
#endif
#if defined(USE_X11)
	if (has_sys_clip) {
		if (!dst) dst = (char *) ""; /* blah */
		if (j < 0) j = 0;
		if (do_sel) {
			if (XGetSelectionOwner(native_display, XA_PRIMARY) != native_window) {
				XSetSelectionOwner(native_display, XA_PRIMARY, native_window, CurrentTime);
			}
			XChangeProperty(native_display,
				DefaultRootWindow(native_display),
				XA_CUT_BUFFER0, XA_STRING, 8,
				PropModeReplace, (unsigned char *)dst, j);
		} else {
			if (XGetSelectionOwner(native_display, atom_clip) != native_window) {
				XSetSelectionOwner(native_display, atom_clip, native_window, CurrentTime);
			}
			XChangeProperty(native_display,
				DefaultRootWindow(native_display),
				XA_CUT_BUFFER0, XA_STRING, 8,
				PropModeReplace, (unsigned char *)dst, j);
			XChangeProperty(native_display,
				DefaultRootWindow(native_display),
				XA_CUT_BUFFER1, XA_STRING, 8,
				PropModeReplace, (unsigned char *)dst, j);
		}
	}
#elif defined(WIN32)
	if (!do_sel && OpenClipboard(native_window)) {
		_hmem = GlobalAlloc((GMEM_MOVEABLE|GMEM_DDESHARE), j+1);
		if (_hmem) {
			dst = (char *)GlobalLock(_hmem);
			if (dst) {
				/* this seems wrong, but msdn does this */
				memcpy(dst, tmp, j);
				dst[j] = '\0';
				GlobalUnlock(_hmem);
				EmptyClipboard();
				SetClipboardData(CF_TEXT, _hmem);
			}
		}
		CloseClipboard();
		_hmem = NULL;
		dst = 0;
	}
#elif defined(__QNXNTO__)
	if (!do_sel) {
		tmp = (char *)malloc(j+4);
		if (!tmp) {
			cldata=(int*)tmp;
			*cldata = Ph_CL_TEXT;
			if (dst) memcpy(tmp+4, dst, j);
			clheader.data = tmp;
#if (NTO_VERSION < 620)
			if (clheader.length > 65535) clheader.length=65535;
#endif
			clheader.length = j + 4;
#if (NTO_VERSION < 620)
			PhClipboardCopy(inputgroup, 1, &clheader);
#else
			PhClipboardWrite(inputgroup, 1, &clheader);
#endif
			free(tmp);
		}
	}
#elif defined(MACOSX)
	if (!do_sel) macosx_clippy_put(_current_clipboard);
#else
	// some other system -- linux without x11, maybe
	// pretend we used the param to silence warnings
	(void) do_sel;
#endif
	if (freeme)
		free(freeme);
}

/* TODO: is the first parameter ever going to be used, or can we kill it? */
static void _string_paste(UNUSED int cb, const char *cbptr)
{
	SDL_Event event = {};

	event.user.type = SCHISM_EVENT_PASTE;
	event.user.data1 = str_dup(cbptr); /* current_clipboard... is it safe? */
	if (!event.user.data1) return; /* eh... */
	if (SDL_PushEvent(&event) == -1) {
		free(event.user.data1);
	}
}


#if defined(USE_X11)
static int _x11_clip_filter(void *userdata, SDL_Event *ev)
{
	XSelectionRequestEvent *req;
	XEvent sevent;
	Atom seln_type, seln_target;
	int seln_format;
	unsigned long nbytes;
	unsigned long overflow;
	unsigned char *seln_data;
	unsigned char *src;

	if (ev->type != SDL_SYSWMEVENT) return 1;
	if (ev->syswm.msg->msg.x11.event.type == SelectionNotify) {
		sevent = ev->syswm.msg->msg.x11.event;
		if (sevent.xselection.requestor == native_window) {
			src = NULL;
			if (XGetWindowProperty(native_display, native_window, atom_sel,
						0, 9000, False, XA_STRING,
						(Atom *)&seln_type,
						(int *)&seln_format,
						(unsigned long *)&nbytes,
						(unsigned long *)&overflow,
						(unsigned char **)&src) == Success) {
				if (seln_type == XA_STRING) {
					if (_current_selection != _current_clipboard) {
						free(_current_clipboard);
					}
					_current_clipboard = strn_dup((const char *)src, nbytes);
					_string_paste(CLIPPY_BUFFER, _current_clipboard);
					_widget_owner[CLIPPY_BUFFER]
							= _widget_owner[CLIPPY_SELECT];
				}
				XFree(src);
			}
		}
		return 1;
	} else if (ev->syswm.msg->msg.x11.event.type == PropertyNotify) {
		sevent = ev->syswm.msg->msg.x11.event;
		return 1;

	} else if (ev->syswm.msg->msg.x11.event.type != SelectionRequest) {
		return 1;
	}

	req = &ev->syswm.msg->msg.x11.event.xselectionrequest;
	sevent.xselection.type = SelectionNotify;
	sevent.xselection.display = req->display;
	sevent.xselection.selection = req->selection;
	sevent.xselection.target = req->target;
	sevent.xselection.property = None;
	sevent.xselection.requestor = req->requestor;
	sevent.xselection.time = req->time;
	if (XGetWindowProperty(native_display, DefaultRootWindow(native_display),
			XA_CUT_BUFFER0, 0, 9000, False, req->target,
			&seln_target, &seln_format,
			&nbytes, &overflow, &seln_data) == Success) {
		if (seln_target == req->target) {
			if (seln_target == XA_STRING) {
				if (nbytes && seln_data[nbytes-1] == '\0')
					nbytes--;
			}
			XChangeProperty(native_display, req->requestor, req->property,
				seln_target, seln_format, PropModeReplace,
				seln_data, nbytes);
			sevent.xselection.property = req->property;
		}
		XFree(seln_data);
	}
	XSendEvent(native_display, req->requestor, False, 0, &sevent);
	XSync(native_display, False);
	return 1;
}

static int (*orig_xlib_err)(Display *d, XErrorEvent *e) = NULL;
static int handle_xlib_err(Display *d, XErrorEvent *e)
{
	/* X_SetSelectionOwner == 22 */
	if (e->error_code == BadWindow && e->request_code == 22) {
		/* return 0 here to avoid dying as the result of a nonfatal race condition */
		return 0;
	}
	if (orig_xlib_err) return orig_xlib_err(d,e);
	return 0;
}

#endif


void clippy_init(void)
{
	SDL_SysWMinfo info = {};

	has_sys_clip = 0;
	SDL_VERSION(&info.version);
	if (SDL_GetWindowWMInfo(video_window(), &info)) {
#if defined(USE_X11)
		if (info.subsystem == SDL_SYSWM_X11) {
			native_display = info.info.x11.display;
			native_window = info.info.x11.window;
			SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
			SDL_SetEventFilter(_x11_clip_filter, NULL);
			has_sys_clip = 1;

			atom_sel = XInternAtom(native_display, "SDL_SELECTION", False);
			atom_clip = XInternAtom(native_display, "CLIPBOARD", False);

			orig_xlib_err = XSetErrorHandler(handle_xlib_err);
		}
#elif defined(WIN32)
		has_sys_clip = 1;
		native_window = info.info.win.window;
#elif defined(__QNXNTO__)
		has_sys_clip = 1;
		inputgroup = PhInputGroup(NULL);
#endif
	}
}

static char *_internal_clippy_paste(int cb)
{
#if defined(MACOSX)
	char *src;
#endif
#if defined(USE_X11)
	Window owner;
	int getme;
#elif defined(WIN32)
	char *src;
	int clen;
#elif defined(__QNXNTO__)
	void *clhandle;
	PhClipHeader *clheader;
	int *cldata;
#endif

	if (has_sys_clip) {
#if defined(USE_X11)
		if (cb == CLIPPY_SELECT) {
			getme = XA_PRIMARY;
		} else {
			getme = atom_clip;
		}
		owner = XGetSelectionOwner(native_display, getme);
		if (owner == None || owner == native_window) {
			/* fall through to default implementation */
		} else {
			XConvertSelection(native_display, getme, XA_STRING, atom_sel, native_window,
							CurrentTime);
			/* at some point in the near future, we'll get a SelectionNotify
			see _x11_clip_filter for more details;

			because of this (otherwise) oddity, we take the selection immediately...
			*/
			return NULL;
		}
#else
		if (cb == CLIPPY_BUFFER) {
#if defined(WIN32)
			if (IsClipboardFormatAvailable(CF_TEXT) && OpenClipboard(native_window)) {
				_hmem  = GetClipboardData(CF_TEXT);
				if (_hmem) {
					if (_current_selection != _current_clipboard) {
						free(_current_clipboard);
					}
					_current_clipboard = NULL;
					src = (char*)GlobalLock(_hmem);
					if (src) {
						clen = GlobalSize(_hmem);
						if (clen > 0) {
							_current_clipboard = strn_dup(src, clen);
						}
						GlobalUnlock(_hmem);
					}
				}
				CloseClipboard();
				_hmem = NULL;
			}
#elif defined(__QNXNTO__)
			if (_current_selection != _current_clipboard) {
				free(_current_clipboard);
			}
			_current_clipboard = NULL;
#if (NTO_VERSION < 620)
			clhandle = PhClipboardPasteStart(inputgroup);
			if (clhandle) {
				clheader = PhClipboardPasteType(clhandle,
								Ph_CLIPBOARD_TYPE_TEXT);
				if (clheader) {
					cldata = clheader->data;
					if (clheader->length > 4 && *cldata == Ph_CL_TEXT) {
						src = ((char *)clheader->data)+4;
						clen = clheader->length - 4;
						_current_clipboard = strn_dup(src, clen);

					}
					PhClipboardPasteFinish(clhandle);
				}
			}
#else
			/* argh! qnx */
			clheader = PhClipboardRead(inputgroup, Ph_CLIPBOARD_TYPE_TEXT);
			if (clheader) {
				cldata = clheader->data;
				if (clheader->length > 4 && *cldata == Ph_CL_TEXT) {
					src = ((char *)clheader->data)+4;
					clen = clheader->length - 4;
					_current_clipboard = strn_dup(src, clen);
				}
			}
#endif /* NTO version selector */
		/* okay, we either own the buffer, or it's a selection for folks without */
#endif /* win32/qnx */
		}
#endif /* x11/others */
		/* fall through; the current window owns it */
	}
	if (cb == CLIPPY_SELECT) return _current_selection;
#ifdef MACOSX
	if (cb == CLIPPY_BUFFER) {
		src = str_dup(macosx_clippy_get());
		if (_current_clipboard != _current_selection) {
			free(_current_clipboard);
		}
		_current_clipboard = src;
		if (!src) return (char *) ""; /* FIXME: de-const-ing is bad */
		return _current_clipboard;
	}
#else
	if (cb == CLIPPY_BUFFER) return _current_clipboard;
#endif
	return NULL;
}


void clippy_paste(int cb)
{
	char *q;
	q = _internal_clippy_paste(cb);
	if (!q) return;
	_string_paste(cb, q);
}

void clippy_select(struct widget *w, char *addr, int len)
{
	int i;

	if (_current_selection != _current_clipboard) {
		free(_current_selection);
	}
	if (!addr) {
		_current_selection = NULL;
		_widget_owner[CLIPPY_SELECT] = NULL;
	} else {
		for (i = 0; addr[i] && (len < 0 || i < len); i++) {
			/* nothing */
		}
		_current_selection = strn_dup(addr, i);
		_widget_owner[CLIPPY_SELECT] = w;

		/* update x11 Select (for xterms and stuff) */
		_clippy_copy_to_sys(1);
	}
}
struct widget *clippy_owner(int cb)
{
	if (cb == CLIPPY_SELECT || cb == CLIPPY_BUFFER)
		return _widget_owner[cb];
	return NULL;
}

void clippy_yank(void)
{
	if (_current_selection != _current_clipboard) {
		free(_current_clipboard);
	}
	_current_clipboard = _current_selection;
	_widget_owner[CLIPPY_BUFFER] = _widget_owner[CLIPPY_SELECT];

	if (_current_selection && strlen(_current_selection) > 0) {
		status_text_flash("Copied to selection buffer");
		_clippy_copy_to_sys(0);
	}
}
