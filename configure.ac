dnl Process this file with autoconf to produce a configure script.

dnl Schism Tracker - a cross-platform Impulse Tracker clone
dnl copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
dnl copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
dnl copyright (c) 2009 Storlek & Mrs. Brisby
dnl copyright (c) 2010-2012 Storlek
dnl URL: http://schismtracker.org/
dnl
dnl This program is free software; you can redistribute it and/or modify
dnl it under the terms of the GNU General Public License as published by
dnl the Free Software Foundation; either version 2 of the License, or
dnl (at your option) any later version.
dnl
dnl This program is distributed in the hope that it will be useful,
dnl but WITHOUT ANY WARRANTY; without even the implied warranty of
dnl MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
dnl GNU General Public License for more details.
dnl
dnl You should have received a copy of the GNU General Public License
dnl along with this program; if not, write to the Free Software
dnl Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

dnl PACKAGE_VERSION will be either "" if not using git, or date of the last git commit in the form YYYYMMDD
m4_define([last_git_commit], patsubst(m4_esyscmd([git log -n 1 --date=short --format=format:%cd]), [[^0-9]]))
AC_INIT([schismtracker],[last_git_commit])

AC_CONFIG_SRCDIR([schism/main.c])

AM_INIT_AUTOMAKE([-Wall subdir-objects])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST

dnl -----------------------------------------------------------------------

dnl Check for standard programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S

dnl do we have Git
AC_CHECK_TOOL([GIT], [git])
AC_SUBST(GIT)
AM_CONDITIONAL([HAVE_GIT], [test "x$GIT" != "x"])

dnl Windows poop
AC_CHECK_TOOL([WINDRES], [windres])
AC_SUBST(WINDRES)
AM_CONDITIONAL([HAVE_WINDRES], [test "x$WINDRES" != "x"])

dnl Necessary for building the internal help.
AM_PATH_PYTHON([2.4])

dnl We're using C
AC_LANG([C])

dnl check endianness
AC_C_BIGENDIAN

dnl Check for SDL libs
AC_CHECK_TOOL([SDL_CONFIG], [sdl2-config])
AC_SUBST(SDL_CONFIG)
if test "x$SDL_CONFIG" = "x"; then
        AC_MSG_ERROR([*** sdl2-config not found.])
fi
AS_VERSION_COMPARE([$($SDL_CONFIG --version)], [2.0.5], [AC_MSG_ERROR([*** SDL version >= 2.0.5 not found.])])
SDL_CFLAGS=$($SDL_CONFIG --cflags)
SDL_LIBS=$($SDL_CONFIG --libs)

dnl Libs
AC_CHECK_LIB([dl], [dlopen])

dnl Functions
AC_CHECK_FUNCS(strchr memmove strerror strtol strcasecmp strncasecmp strverscmp stricmp strnicmp strcasestr strptime asprintf vasprintf memcmp mmap nice unsetenv dup fnmatch mkstemp)
AM_CONDITIONAL([NEED_ASPRINTF], [test "$ac_cv_func_asprintf" = "no"])
AM_CONDITIONAL([NEED_VASPRINTF], [test "$ac_cv_func_vasprintf" = "no"])
AM_CONDITIONAL([NEED_MEMCMP], [test "$ac_cv_func_memcmp" = "no"])
AM_CONDITIONAL([NEED_STRPTIME], [test "$ac_cv_func_strptime" = "no"])
AM_CONDITIONAL([NEED_MKSTEMP], [test "$ac_cv_func_mkstemp" = "no"])

AM_CONDITIONAL([USE_MMAP], [test "$ac_cv_func_mmap" = "yes"])

dnl Headers, typedef crap, et al.
AC_HEADER_DIRENT
AC_CHECK_HEADERS_ONCE([sys/time.h])

AC_CHECK_HEADERS(inttypes.h fcntl.h limits.h signal.h unistd.h sys/param.h sys/ioctl.h byteswap.h sys/socket.h sys/soundcard.h poll.h sys/poll.h)
AM_CONDITIONAL([USE_OSS], [test "x$ac_cv_header_sys_soundcard_h" = "xyes"])

AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
AC_SEARCH_LIBS([powf], [m])

dnl -----------------------------------------------------------------------

dnl Always use x11 by default unless overriden later per-platform
dnl (this is stupid, and only needed because the x11 check is dysfunctional)
defaultx11=yes


dnl CoreMIDI (Mac OS X)
AC_MSG_CHECKING(for Darwin host OS)
case "${host_os}" in
	darwin*)
		AC_MSG_RESULT(found)
		dnl frameworks aren't part of sdl... should be a separate variable

		SDL_LIBS="$SDL_LIBS -lSDL2main -framework Cocoa -framework CoreAudio -framework CoreMIDI -framework IOKit"
		AC_SUBST(SDL_LIBS)

		AM_CONDITIONAL([USE_MACOSX], true)
		AM_CONDITIONAL([am__fastdepOBJC], true)
		defaultx11=no
		;;
	*)
		AC_MSG_RESULT(not found)
		AM_CONDITIONAL([USE_MACOSX], false)
		AM_CONDITIONAL([am__fastdepOBJC], false)
		;;
esac



dnl winmm testing...
AC_CHECK_HEADERS(windows.h)
if test "X$ac_cv_header_windows_h" = "Xyes"; then
        AM_CONDITIONAL([USE_WIN32], true)
        SDL_LIBS="$SDL_LIBS -lwinmm"
        AC_SUBST(SDL_LIBS)
        defaultx11=no
else
        AM_CONDITIONAL([USE_WIN32], false)
fi


dnl Wii crap
AC_CHECK_LIB(ogc, IOS_ReloadIOS, libogc_found=yes, libogc_found=no, [-mrvl -L${DEVKITPRO}/libogc/lib/wii])
if test x"$libogc_found" = "xyes"; then
        AM_CONDITIONAL([USE_WII], true)
        wii_machdep="-DGEKKO -mrvl -mcpu=750 -meabi -mhard-float"
        CFLAGS="$CFLAGS $wii_machdep -I${DEVKITPRO}/libogc/include -I${DEVKITPPC}/powerpc-eabi/include"
        LIBS="$LIBS $wii_machdep -L${DEVKITPRO}/libogc/lib/wii -L{DEVKITPPC}/powerpc-eabi/lib"
        SDL_CFLAGS="$SDL_CFLAGS"
        SDL_LIBS="$SDL_LIBS"
        AC_SUBST(CFLAGS)
        AC_SUBST(LIBS)
        AC_SUBST(SDL_CFLAGS)
        AC_SUBST(SDL_LIBS)
        EXEEXT=.elf
        AC_SUBST(EXEEXT)
        defaultx11=no
else
        AM_CONDITIONAL([USE_WII], false)
fi


dnl should probably have some AC_MSG_CHECKING here, but meh
if test "x$defaultx11" = "xno"; then
        if test "x$with_x" = "x"; then
                AC_MSG_NOTICE([X11 disabled by default; use --with-x if you really want it])
                dnl (you don't want it, because it's probably broken anyway)
                with_x=no
        fi
fi

dnl -----------------------------------------------------------------------

saved_libs=$LIBS
alsa=no
AC_CHECK_LIB(asound, snd_seq_open,[alsa=yes])
alsadltrick=no
if test "$alsa" = "yes"; then
        if test "$ac_cv_header_sys_soundcard_h" = "yes"; then
                alsadltrick=yes
                LIBS=$saved_libs
        fi
fi
AM_CONDITIONAL([USE_ALSA], [test "$alsa" = yes])
AM_CONDITIONAL([USE_ALSA_DLTRICK], [test "$alsadltrick" = yes])


AC_PATH_XTRA()
if test "$no_x" = "yes"; then
        dnl no X11
        AM_CONDITIONAL([USE_X11], [false])
        AM_CONDITIONAL([USE_XV], [false])
else
        AM_CONDITIONAL([USE_X11], [true])

        SDL_LIBS="$SDL_LIBS -lX11 -lXext"
        AC_SUBST(SDL_LIBS)

        LIBS="$LIBS $X_LIBS"
        AC_SUBST(LIBS)

        dnl this previously checked X11/extensions/XKB.h, but nothing
        dnl ever used it (xkb.c still included X11/XKBlib.h even when
        dnl autoconf reported it as nonexistent -- weird!)
        AC_CHECK_HEADERS(X11/Xlib.h X11/XKBlib.h)
        AC_CHECK_LIB(Xv, XvQueryExtension,,,-lX11 -lXext)
        if test "x$ac_cv_lib_Xv_XvQueryExtension" = "xyes"; then
                AC_CHECK_HEADERS(X11/extensions/Xvlib.h,,,[[
#include <X11/Xlib.h>
                ]])
                if test "x$ac_cv_header_X11_extensions_xvlib_h" = "xyes"; then
                        SDL_LIBS="$SDL_LIBS -lXv -lXext"
                        AC_SUBST(SDL_LIBS)
                        AM_CONDITIONAL([USE_XV], [true])
                elif test "x$ac_cv_header_X11_extensions_Xvlib_h" = "xyes"; then
                        SDL_LIBS="$SDL_LIBS -lXv -lXext"
                        AC_SUBST(SDL_LIBS)
                        AM_CONDITIONAL([USE_XV], [true])
                else
                        AM_CONDITIONAL([USE_XV], [false])
                fi
        else
                AM_CONDITIONAL([USE_XV], [false])
        fi
fi

dnl asdjklfjklasdfaskdfjklads
saved_LIBS="$LIBS"
AC_SEARCH_LIBS(socket, socket network)
LIBS="$saved_LIBS"
if test "x$ac_cv_search_socket" = "xno"; then
        dnl windows!
        AC_CHECK_HEADERS([winsock.h winsock2.h])
        if test "x$ac_cv_header_winsock_h" = "xyes"; then
                AM_CONDITIONAL([USE_NETWORK], true)
                SDL_LIBS="$SDL_LIBS -lws2_32"
                AC_SUBST([SDL_LIBS])
        elif test "x$ac_cv_header_winsock2_h" = "xyes"; then
                AM_CONDITIONAL([USE_NETWORK], true)
                SDL_LIBS="$SDL_LIBS -lsock32"
                AC_SUBST([SDL_LIBS])
        else
                AM_CONDITIONAL([USE_NETWORK], false)
                AC_MSG_RESULT([*** No valid socket library found\!])
        fi
elif test "x$ac_cv_search_socket" = "xnone required"; then
        dnl sys/socket.h isn't present in devkitPPC but
        dnl the symbols are defined. ugh.

        if test "x$ac_cv_header_sys_socket_h" = "xyes"; then
                dnl free networking
                AM_CONDITIONAL([USE_NETWORK], true)
        else
                AM_CONDITIONAL([USE_NETWORK], false)
        fi
else
        SDL_LIBS="$SDL_LIBS $ac_cv_search_socket"
        AC_SUBST(SDL_LIBS)
        AM_CONDITIONAL([USE_NETWORK], true)
fi

dnl wee...
dnl this completely sucks...
OBJC=$CC
OBJCFLAGS=$CFLAGS
AC_SUBST(OBJC)
AC_SUBST(OBJCFLAGS)

dnl -----------------------------------------------------------------------
dnl (This ought to be above AC_PROG_CC, but that causes configure to fail
dnl when all the insane warnings are are enabled.)

AC_ARG_ENABLE(extra-opt,
        AS_HELP_STRING([--enable-extra-opt], [Add extra, system-dependent optimizations (do not use this together with debug or even profiling)]),
        ADD_OPT=$enableval,
        ADD_OPT=no)

AC_ARG_ENABLE(all-warnings,
        AS_HELP_STRING([--enable-all-warnings], [Enable ridiculous compiler warnings]),
        ADD_WARN=$enableval,
        ADD_WARN=no)

AC_ARG_ENABLE(debug,
        AS_HELP_STRING([--enable-debug], [Enable debug flags]),
        ADD_DEBUG=$enableval,
        ADD_DEBUG=no)

AC_ARG_ENABLE(profiling,
        AS_HELP_STRING([--enable-profiling], [Enable profiling flags (slows things down), debugging (--enable-debug) is also enabled by this]),
        ADD_PROFILING=$enableval,
        ADD_PROFILING=no)

AC_ARG_ENABLE(ludicrous-mode,
        AS_HELP_STRING([--enable-ludicrous-mode], [Enable all warnings, and treat as errors]),
        ADD_LUDICROUS=$enableval,
        ADD_LUDICROUS=no)

AC_ARG_ENABLE(fortify-source,
        AS_HELP_STRING([--enable-fortify-source], [Build with FORTIFY_SOURCE]),
        ADD_FORTIFY=$enableval,
        ADD_FORTIFY=no)

AC_ARG_WITH([flac],
	[AS_HELP_STRING([--with-flac],[Build with FLAC support @<:@default=check@:>@])],
	[],
	[with_flac=check])

dnl fortify needs -O; do this early so ADD_OPT can override with higher -O level
if test x$ADD_FORTIFY \!= xno; then
        CFLAGS="$CFLAGS -O -D_FORTIFY_SOURCE=2"
fi

dnl place extra optimizations after existing cflags so that they can override
dnl override whatever flags might exist by default (-g -O2 usually)
if test x$ADD_OPT \!= xno; then
        if test x$ADD_DEBUG \!= xno || test x$ADD_PROFILING \!= xno; then
                AC_MSG_NOTICE([You aren't supposed to use --enable-debug or --enable-profiling together with --enable-extra-opt!!])
        fi
        ADD_OPT="-Ofast -g0 -s -fno-exceptions -march=native"
        CFLAGS="$CFLAGS $ADD_OPT"
fi

if test x$ADD_LUDICROUS \!= xno; then
        ADD_WARN=yes
        CFLAGS="$CFLAGS -Werror"
fi

AM_CONDITIONAL([USE_FLAC], false)
if test "x$with_flac" = "xcheck" || test "x$with_flac" = "xyes"; then
        dnl use pkg-config, it'll deal with everything for us
        PKG_CHECK_MODULES([FLAC], [flac], [libflac_found=yes], [libflac_found=no])
	if test "x$libflac_found" = "xyes"; then
                AC_SUBST([FLAC_CFLAGS])
                AC_SUBST([FLAC_LIBS])
		AM_CONDITIONAL([USE_FLAC], true)
        elif test "x$with_flac" = "xyes"; then
                AC_MSG_ERROR([*** Failed to find libFLAC])
	fi
fi

dnl ... but put the warnings first, to make it possible to quiet certain
dnl warnings if necessary, while still providing most of the benefit
if test x$ADD_WARN \!= xno; then
        ADD_WARN="-Wall -Wextra -Winline -Wshadow -Wwrite-strings -Waggregate-return -Wpacked"
        ADD_WARN="$ADD_WARN -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs"
        CFLAGS="$ADD_WARN $CFLAGS"
fi

if test x$ADD_PROFILING \!= xno || test x$ADD_DEBUG \!= xno; then
        CFLAGS="$CFLAGS -g -O0"
        OBJCFLAGS="$OBJCFLAGS -g -O0"
        SDL_LIBS="$SDL_LIBS -g -O0"
        AC_SUBST(SDL_LIBS)
fi
if test x$ADD_PROFILING \!= xno; then
        CFLAGS="$CFLAGS -pg"
        OBJCFLAGS="$OBJCFLAGS -pg"
        SDL_LIBS="$SDL_LIBS -pg"
        AC_SUBST(SDL_LIBS)
fi

dnl - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
