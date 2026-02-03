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

/* OS-dependent code implementations are defined here; the files for each target OS exist in sys/blah/osdefs.c,
and possibly other files as well. Only one osdefs.c should be in use at a time. */

#ifndef SCHISM_OSDEFS_H_
#define SCHISM_OSDEFS_H_

#include "headers.h"
#include "events.h"

/* message box styles. */
#define OS_MESSAGE_BOX_INFO     (0)
#define OS_MESSAGE_BOX_ERROR    (1)
#define OS_MESSAGE_BOX_WARNING  (2)

/*
os_sysinit: any platform-dependent setup that needs to occur directly upon startup.
This code is processed right as soon as main() starts.

os_event: preprocessing for events.
This is used to hack in system-dependent input methods (e.g. F16 and other scancodes on OS X; Wiimote buttons;
etc.) If defined, this function will be called after capturing an SDL event.
A return value of 0 indicates that the event should NOT be processed by the main event handler.

os_onframe: called every frame (or so)
*/
#if defined(SCHISM_WII)
# define os_sysinit wii_sysinit
# define os_sysexit wii_sysexit
#elif defined(SCHISM_WIIU)
# define os_sysinit wiiu_sysinit
# define os_sysexit wiiu_sysexit
# define os_onframe wiiu_onframe
#elif defined(SCHISM_WIN32)
# define os_sysinit win32_sysinit
# define os_sysexit win32_sysexit
# define os_get_modkey win32_get_modkey
# define os_fopen win32_fopen
# define os_stat win32_stat
# define os_mkdir win32_mkdir
# define os_access win32_access
# define os_get_key_repeat win32_get_key_repeat
# define os_show_message_box win32_show_message_box
#elif defined(SCHISM_MACOSX)
# define os_sysexit macosx_sysexit
# define os_sysinit macosx_sysinit
# define os_get_modkey macosx_get_modkey
# define os_get_key_repeat macosx_get_key_repeat
# define os_show_message_box macosx_show_message_box
#elif defined(SCHISM_MACOS)
# define os_mkdir macos_mkdir
# define os_stat macos_stat
# define os_show_message_box macos_show_message_box
# define os_sysinit macos_sysinit
# define os_get_key_repeat macos_get_key_repeat
# define os_fopen macos_fopen
#elif defined(SCHISM_OS2)
# define os_mkdir os2_mkdir
# define os_stat os2_stat
# define os_fopen os2_fopen
# define os_get_key_repeat os2_get_key_repeat
# define os_show_message_box os2_show_message_box
#elif defined(SCHISM_XBOX)
# define os_sysinit xbox_sysinit
# define os_stat xbox_stat
# define os_mkdir xbox_mkdir
# define os_fopen xbox_fopen
#endif

/* os_exec: runs a program, and optionally returns the status code.
 * If you want to check for success, you MUST obtain the status code.
 * The only thing os_exec returns, is that the program was ran. That's it.
 *
 * e.g.:
 * os_exec(&st, "/usr/bin", "schismtracker", "--audio-driver", "alsa", (char *)NULL); */
#if defined(SCHISM_WIN32)
# define os_exec win32_exec
# define os_run_hook win32_run_hook
#elif defined(HAVE_EXECL) && defined(HAVE_FORK) && (defined(HAVE_WAITID) || defined(HAVE_WAITPID))
# define os_exec posix_exec
# define os_run_hook posix_run_hook
#endif

#ifndef os_sysinit
# define os_sysinit(pargc,argv)
#endif
#ifndef os_sysexit
# define os_sysexit()
#endif
#ifndef os_get_modkey
#define os_get_modkey(m)
#endif
#ifndef os_onframe
# define os_onframe()
#endif
#ifndef os_fopen
# define os_fopen fopen
#endif
#ifndef os_stat
# define os_stat stat
#endif
#ifndef os_mkdir
# define os_mkdir mkdir
#endif
#ifndef os_access
# define os_access access
#else
# undef R_OK
# undef W_OK
# undef X_OK
# undef F_OK

# define R_OK 0x01
# define W_OK 0x02
# define X_OK 0x04

# define F_OK 0x00 /* ok */
#endif
#ifdef os_exec
# define HAVE_OS_EXEC
#else
/* varargs in preprocessor are C99 extension */
# define os_exec(pstatus, dir, name, ...) (0)
#endif
#ifndef os_get_key_repeat
# define os_get_key_repeat(pdelay, prate) (0)
#endif
#ifndef os_get_locale_format
# define os_get_locale_format(pdate, ptime) (0)
#endif
#ifndef os_show_message_box

# ifdef SCHISM_XBOX
#  include <hal/debug.h>
# endif

static inline void msgbox_printf_impl(const char *title, const char *text, int style)
{
	const char *styles[] = {
		[OS_MESSAGE_BOX_INFO]    = "INFO",
		[OS_MESSAGE_BOX_ERROR]   = "ERROR",
		[OS_MESSAGE_BOX_WARNING] = "WARNING",
	};

#ifdef SCHISM_XBOX
	debugPrint("[%s] %s: %s", styles[style], title, text);
#else
	{
		FILE *out = (style == OS_MESSAGE_BOX_INFO) ? stdout : stderr;

		fprintf(out, "[%s] %s: %s\n", styles[style], title, text);
	}
#endif
}
# define os_show_message_box(title, text, style) (msgbox_printf_impl(title, text, style))
#endif
#ifndef os_run_hook
# define os_run_hook(dir,exe,maybe_arg) (0)
#endif

/* Whether or not to compile ANSI variants of functions; we only actually do
 * this on IA-32 because of Win9x, all other architectures are WinNT-only.
 *
 * Note: mingw-w64 doesn't support anything below XP. Maybe we should disable
 * ANSI there as well? */
#ifdef SCHISM_WIN32
# if defined(_M_IX86) || defined(__i386__) || defined(__i386) || defined(i386)
#  define SCHISM_WIN32_COMPILE_ANSI 1
# endif
#endif

// Implementations for the above, and more.

int macosx_ibook_fnswitch(int setting);

void wiiu_sysinit(int *pargc, char ***pargv); // fixup HOME envvar
void wiiu_sysexit(void);
void wiiu_onframe(void);

void wii_sysinit(int *pargc, char ***pargv); // set up filesystem
void wii_sysexit(void); // close filesystem

int win32_event(schism_event_t *event);
void win32_sysinit(int *pargc, char ***pargv);
void win32_sysexit(void);
void win32_get_modkey(schism_keymod_t *m);
void win32_filecreated_callback(const char *filename);
void win32_toggle_menu(void *window, int on); // window should be a pointer to the window HWND
int win32_stat(const char *path, struct stat *st);
int win32_mkdir(const char *path, uint32_t mode);
FILE* win32_fopen(const char *path, const char *flags);
int win32_exec(int *status, int *abnormal_exit, const char *dir, const char *name, ...);
int win32_run_hook(const char *dir, const char *exe, const char *maybe_arg);
int win32_get_key_repeat(int *pdelay, int *prate);
void win32_show_message_box(const char *title, const char *text, int style);
int win32_audio_lookup_device_name(const void *nameguid, const uint32_t *waveoutdevid, char **result);
int win32_ntver_atleast(int major, int minor, int build);
int win32_ver_unicode(void);
int win32_access(const char *path, int amode);
void win32_init_dark_theme(void *window);

// audio-dsound.c
int win32_dsound_audio_lookup_waveout_name(const uint32_t *waveoutnamev, char **result);

int posix_exec(int *status, int *abnormal_exit, const char *dir, const char *name, ...);
int posix_run_hook(const char *dir, const char *exe, const char *maybe_arg);

int macosx_event(schism_event_t *event);
void macosx_sysexit(void);
void macosx_sysinit(int *pargc, char ***pargv); /* set up ibook helper */
void macosx_get_modkey(schism_keymod_t *m);
int macosx_get_key_repeat(int *pdelay, int *prate);
char *macosx_get_application_support_dir(void);
void macosx_show_message_box(const char *title, const char *text, int style);
int macosx_ver_atleast(int major, int minor, int patch);
int macosx_get_screen_rect(double *x, double *y, double *w, double *h);
int macosx_get_window_rect(double *x, double *y, double *w, double *h);
int macosx_set_window_coordinates(double x, double y);

int macos_mkdir(const char *path, uint32_t mode);
int macos_stat(const char *file, struct stat *st);
void macos_show_message_box(const char *title, const char *text, int style);
void macos_sysinit(int *pargc, char ***pargv);
int macos_get_key_repeat(int *pdelay, int *prate);
// do hooks even exist on mac os?
FILE *macos_fopen(const char *path, const char *flags);

int x11_event(schism_event_t *event);

int os2_stat(const char* path, struct stat* st);
int os2_mkdir(const char* path, uint32_t mode);
FILE* os2_fopen(const char* path, const char* flags);
int os2_get_key_repeat(int *pdelay, int *prate);
void os2_show_message_box(const char *title, const char *text, int style);

int xbox_stat(const char *path, struct stat *st);
int xbox_mkdir(const char *path, uint32_t mode);
FILE* xbox_fopen(const char* path, const char* flags);
void xbox_sysinit(int *pargc, char ***pargv);

/* ------------------------------------------------------------------------ */
/* stupid ugly windows/xbox crap */

/* okay, some explanation for this.
 * nxdk (xbox toolchain) doesn't support the unicode API AT ALL, which
 * means we need to build only the ANSI bits of schism. however, we want
 * to be able to support winnt clients as well. thus, I created this macro
 * which does what we want in all cases, without sprinkling stupid preprocessor
 * crap everywhere (see dmoz.c for some fun) */
#ifdef SCHISM_XBOX
# define SCHISM_ANSI_UNICODE(ansiblock, uniblock) \
	{ ansiblock }
#elif defined(SCHISM_WIN32)
# ifdef SCHISM_WIN32_COMPILE_ANSI
#  define SCHISM_ANSI_UNICODE(ansiblock, uniblock) \
	if (win32_ver_unicode()) { \
		uniblock \
	} else { \
		ansiblock \
	}
# else
#  define SCHISM_ANSI_UNICODE(ansiblock, uniblock) \
	{ uniblock }
# endif
#endif

/* ------------------------------------------------------------------------ */

static inline SCHISM_ALWAYS_INLINE
int os_shell(const char *name, const char *arg)
{
#ifdef HAVE_OS_EXEC
	int st;

	if (!os_exec(&st, NULL, name, arg, (char *)NULL))
		return -1; /* ? */

	return st;
#else
#ifdef ENOTSUP
	errno = ENOTSUP;
#endif
	return -1;
#endif
}

#endif /* SCHISM_OSDEFS_H_ */
