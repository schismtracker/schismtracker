dnl Process this file with autoconf to produce a configure script.

dnl Schism Tracker - a cross-platform Impulse Tracker clone
dnl copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
dnl copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
dnl copyright (c) 2009 Storlek & Mrs. Brisby
dnl copyright (c) 2010-2011 Storlek
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

dnl 'hg', or a date in the form YYYYMMDD
AC_INIT([schismtracker], [hg])

AC_CONFIG_SRCDIR([schism/main.c])

dnl We'll need machine type later
AC_CANONICAL_TARGET([])
machtype="$target_cpu"

AM_INIT_AUTOMAKE([-Wall subdir-objects])
AC_CONFIG_HEADERS([config.h])

dnl -----------------------------------------------------------------------

dnl Check for standard programs
AC_PROG_CC
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_CC_C99

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
AM_PATH_SDL(1.2.10, , AC_MSG_ERROR([*** SDL version >= 1.2.10 not found.]))

dnl Functions
AC_CHECK_FUNCS(strchr memmove strerror strtol strcasecmp strncasecmp strverscmp stricmp strnicmp strcasestr strptime asprintf vasprintf memcmp mmap nice unsetenv dup fnmatch log2 mkstemp)
AM_CONDITIONAL([NEED_ASPRINTF], [test "$ac_cv_func_asprintf" = "no"])
AM_CONDITIONAL([NEED_VASPRINTF], [test "$ac_cv_func_vasprintf" = "no"])
AM_CONDITIONAL([NEED_MEMCMP], [test "$ac_cv_func_memcmp" = "no"])
AM_CONDITIONAL([NEED_STRPTIME], [test "$ac_cv_func_strptime" = "no"])
AM_CONDITIONAL([NEED_MKSTEMP], [test "$ac_cv_func_mkstemp" = "no"])

AM_CONDITIONAL([USE_MMAP], [test "$ac_cv_func_mmap" = "yes"])

dnl Headers, typedef crap, et al.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_TIME
AC_CHECK_HEADERS(inttypes.h fcntl.h limits.h signal.h unistd.h sys/param.h sys/ioctl.h sys/kd.h linux/fb.h byteswap.h sys/soundcard.h)
AM_CONDITIONAL([USE_OSS], [test "$ac_cv_header_sys_soundcard_h" = yes])

AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
LT_LIB_M
AC_SUBST(LIBM)

dnl -----------------------------------------------------------------------

dnl Always use x11 by default unless overriden later per-platform
dnl (this is stupid, and only needed because the x11 check is dysfunctional)
defaultx11=yes


dnl CoreMIDI (Mac OS X)
AC_MSG_CHECKING(for CoreAudio/CoreMIDI Framework)
if echo "$SDL_LIBS" | grep -- -framework >/dev/null 2>&1; then
        AC_MSG_RESULT(found)
        dnl frameworks aren't part of sdl... should be a separate variable
        SDL_LIBS="$SDL_LIBS -framework CoreAudio -framework CoreMIDI -framework IOKit -framework OpenGL"
        AC_SUBST(SDL_LIBS)

        AM_CONDITIONAL([USE_MACOSX], true)
        AM_CONDITIONAL([am__fastdepOBJC], true)
        defaultx11=no
else
        AC_MSG_RESULT(not found)
        AM_CONDITIONAL([USE_MACOSX], false)
        AM_CONDITIONAL([am__fastdepOBJC], false)
fi



dnl winmm testing...
AC_CHECK_HEADERS(winsock.h winsock2.h windows.h)
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
        CFLAGS="$CFLAGS $wii_machdep -I${DEVKITPRO}/libogc/include"
        LIBS="$LIBS $wii_machdep -L${DEVKITPRO}/libogc/lib/wii"
        SDL_CFLAGS="$SDL_CFLAGS -I${DEVKITPRO}/libogc/include/SDL"
        SDL_LIBS="$SDL_LIBS -lSDL -lfat -lwiiuse -lbte -logc -lm -lwiikeyboard"
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

dnl OpenGL crud - it doesn't always exist
saved_cppflags=$CPPFLAGS
CPPFLAGS="$CPPFLAGS $SDL_CFLAGS"
AC_CHECK_HEADER([SDL_opengl.h], [AC_DEFINE([USE_OPENGL], 1, [Define to 1 if SDL's OpenGL works])])
CPPFLAGS=$saved_cppflags

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
        if test "$ac_cv_lib_Xv_XvQueryExtension" = "yes"; then
                AC_CHECK_HEADERS(X11/extensions/Xvlib.h,,,[[
#include <X11/Xlib.h>
                ]])
                if test "$ac_cv_header_X11_extensions_xvlib_h" = "yes"; then
                        SDL_LIBS="$SDL_LIBS -lXv -lXext"
                        AC_SUBST(SDL_LIBS)
                        AM_CONDITIONAL([USE_XV], [true])
                elif test "$ac_cv_header_X11_extensions_Xvlib_h" = "yes"; then
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
        dnl Windows sucks (I don't even know what this is about, but it apparently works)
        AC_CHECK_HEADERS(winsock.h winsock2.h)
        if test "x$ac_cv_header_winsock_h" = "xyes"; then
                socketlib="-lwsock32"
        elif test "x$ac_cv_header_winsock2_h" = "xyes"; then
                socketlib="-lws2_32"
        fi
        if test "x$socketlib" = "x"; then
                echo "*** No sockets for you!"
                AM_CONDITIONAL([USE_NETWORK], false)
        else
                AM_CONDITIONAL([USE_NETWORK], true)
                SDL_LIBS="$SDL_LIBS $socketlib"
                AC_SUBST(SDL_LIBS)
        fi
elif test "x$ac_cv_search_socket" = "xnone required"; then
        dnl free networking
        AM_CONDITIONAL([USE_NETWORK], true)
else
        SDL_LIBS="$SDL_LIBS $ac_cv_search_socket"
        AC_SUBST(SDL_LIBS)
        AM_CONDITIONAL([USE_NETWORK], true)
fi

#AC_CHECK_LIB(kernel32, GetConsoleMode, SDL_LIBS="$SDL_LIBS -Wl,--subsystem,console")

dnl wee...
dnl this completely sucks...
OBJC=$CC
OBJCFLAGS=$CFLAGS
AC_SUBST(OBJC)
AC_SUBST(OBJCFLAGS)

dnl -----------------------------------------------------------------------
dnl stupid version hack

AC_ARG_ENABLE(mercurial,
        AS_HELP_STRING([--enable-mercurial], [Use Mercurial for version/timestamps]),
        USE_MERCURIAL=$enableval,
        USE_MERCURIAL=yes)

dnl We want to get the most accurate information of the version possible.
dnl If Mercurial is present, 'hg parents' will tell us what version is
dnl really being compiled.

AC_MSG_CHECKING(for Mercurial)
if test x$USE_MERCURIAL = xyes; then
        hg_dirstate=no
        if test -e "$ac_abs_confdir"/.hg/dirstate; then
                if hg --version >/dev/null; then
                        AC_MSG_RESULT(found)
                        hg_dirstate=yes
                else
                        AC_MSG_RESULT(not found)
                        echo "*** Warning: .hg/dirstate exists, but no 'hg' command in PATH"
                        echo "*** Mercurial version information cannot be compiled in"
                fi
        else
                AC_MSG_RESULT(not found)
        fi
else
        AC_MSG_RESULT(skipped)
fi

if test "x$hg_dirstate" = "xyes"; then
        AM_CONDITIONAL([USE_MERCURIAL], [true])
else
        AM_CONDITIONAL([USE_MERCURIAL], [false])
fi

dnl -----------------------------------------------------------------------
dnl (This ought to be above AC_PROG_CC, but that causes configure to fail
dnl when all the insane warnings are are enabled.)

AC_ARG_ENABLE(extra-opt,
        AS_HELP_STRING([--enable-extra-opt], [Add extra optimizations]),
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
        AS_HELP_STRING([--enable-profiling], [Enable profiling flags (slows things down)]),
        ADD_PROFILING=$enableval,
        ADD_PROFILING=no)

AC_ARG_ENABLE(ludicrous-mode,
        AS_HELP_STRING([--enable-ludicrous-mode], [Enable all warnings, and treat as errors]),
        ADD_LUDICROUS=$enableval,
        ADD_LUDICROUS=no)

dnl place extra optimizations after existing cflags so that they can override
dnl override whatever flags might exist by default (-g -O2 usually)
if test x$ADD_OPT \!= xno; then
        ADD_OPT="-g0 -s -O3 -ffast-math -fomit-frame-pointer -fno-exceptions"
        ADD_OPT="$ADD_OPT -funroll-loops -frerun-cse-after-loop -fno-ident"
        ADD_OPT="$ADD_OPT -fno-strength-reduce"
        CFLAGS="$CFLAGS $ADD_OPT"
fi

if test x$ADD_LUDICROUS \!= xno; then
        ADD_WARN=yes
        CFLAGS="$CFLAGS -Werror -D_FORTIFY_SOURCE=2"
fi

dnl ... but put the warnings first, to make it possible to quiet certain
dnl warnings if necessary, while still providing most of the benefit
if test x$ADD_WARN \!= xno; then
        ADD_WARN="-Wall -Wextra -Winline -Wshadow -Wwrite-strings -Waggregate-return -Wpacked"
        ADD_WARN="$ADD_WARN -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs"
        CFLAGS="$ADD_WARN $CFLAGS"
fi

if test x$ADD_DEBUG \!= xno; then
        CFLAGS="$CFLAGS -g"
        OBJCFLAGS="$OBJCFLAGS -g"
        SDL_LIBS="$SDL_LIBS -g"
        AC_SUBST(SDL_LIBS)
fi
if test x$ADD_PROFILING \!= xno; then
        CFLAGS="$CFLAGS -g -pg"
        OBJCFLAGS="$OBJCFLAGS -g -pg"
        SDL_LIBS="$SDL_LIBS -g -pg"
        AC_SUBST(SDL_LIBS)
fi

dnl - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
