dnl Process this file with autoconf to produce a configure script.

dnl Schism Tracker - a cross-platform Impulse Tracker clone
dnl copyright (c) 2003-2005 Storlek <storlek@rigelseven.com>
dnl copyright (c) 2005-2008 Mrs. Brisby <mrs.brisby@nimh.org>
dnl copyright (c) 2009 Storlek & Mrs. Brisby
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
AC_PROG_CXX
AC_PROG_CXXCPP
AC_PROG_INSTALL
AC_PROG_LN_S
AC_PROG_RANLIB
AC_PROG_CC_C99

dnl We're using C
AC_LANG([C])

dnl check endianness
AC_C_BIGENDIAN

dnl Check for SDL libs
AM_PATH_SDL(1.1.8, , AC_MSG_ERROR([*** SDL version >= 1.1.8 not found.]))

enable_sdlstatic=no
AC_ARG_ENABLE(static-sdl, [  --enable-static-sdl     Link SDL statically], enable_sdlstatic=yes,enable_sdlstatic=no)
if test "x$enable_sdlstatic" = "xyes"; then
	if test "x$SDL_CONFIG" = "xno"; then
		echo "*** SDL_CONFIG explicitly set to 'no' but you asked for static-sdl"
		echo "*** one of those has to go..."
		exit 1
	else
		SDL_LIBS=`$SDL_CONFIG $sdlconf_args --static-libs`
		AC_SUBST(SDL_LIBS)
	fi
fi

dnl CoreMIDI (Mac OS X)
AC_MSG_CHECKING(for CoreAudio/CoreMIDI Framework)
if echo "$SDL_LIBS" | grep -- -framework >/dev/null 2>&1; then
	AC_MSG_RESULT(found)
	dnl frameworks aren't part of sdl... should be a separate variable
	SDL_LIBS="$SDL_LIBS -framework CoreAudio -framework CoreMIDI -framework IOKit -framework OpenGL"
	AC_SUBST(SDL_LIBS)

	AM_CONDITIONAL([USE_MACOSX], true)
	AM_CONDITIONAL([am__fastdepOBJC], true)
	macosx=yes
else
	AC_MSG_RESULT(not found)
	AM_CONDITIONAL([USE_MACOSX], false)
	AM_CONDITIONAL([am__fastdepOBJC], false)
	macosx=no
fi

dnl Functions
AC_FUNC_STRFTIME
AC_CHECK_FUNCS(strchr memmove strerror strtol strcasecmp strncasecmp strverscmp stricmp strnicmp asprintf vasprintf memcmp mmap socket nice unsetenv)
AM_CONDITIONAL([NEED_GNU_ASPRINTF], [test "$ac_cv_func_asprintf" = "no"])
AM_CONDITIONAL([NEED_GNU_VASPRINTF], [test "$ac_cv_func_vasprintf" = "no"])
AM_CONDITIONAL([NEED_GNU_MEMCMP], [test "$ac_cv_func_memcmp" = "no"])
AM_CONDITIONAL([USE_MMAP], [test "$ac_cv_func_mmap" = "yes"])

dnl Headers, typedef crap, et al.
AC_HEADER_STDC
AC_HEADER_DIRENT
AC_HEADER_TIME
AC_CHECK_HEADERS(inttypes.h fcntl.h limits.h signal.h unistd.h sys/param.h sys/ioctl.h sys/kd.h linux/fb.h byteswap.h sys/soundcard.h fnmatch.h)
AM_CONDITIONAL([USE_OSS], [test "$ac_cv_header_sys_soundcard_h" = yes])

AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM
LT_LIB_M
AC_SUBST(LIBM)

AC_CHECK_FUNCS(log2)

dnl -----------------------------------------------------------------------

saved_libs=$LIBS
alsa=no
AC_CHECK_LIB(asound, snd_seq_open,[alsa=yes])
alsadltrick=no
if test "$alsa" = "yes"
then if test "$ac_cv_header_sys_soundcard_h" = "yes"
then alsadltrick=yes
LIBS=$saved_libs
fi
fi
AM_CONDITIONAL([USE_ALSA], [test "$alsa" = yes])
AM_CONDITIONAL([USE_ALSA_DLTRICK], [test "$alsadltrick" = yes])

dnl should probably have some AC_MSG_CHECKING here, buh meh
if test "x$macosx" = "xyes"; then
	if test "x$with_x" = "x"; then
		AC_MSG_NOTICE([X11 disabled by default on Mac OS X; use --with-x to enable])
		with_x=no
	fi
fi
AC_PATH_XTRA()
if test "$no_x" = "yes"
then
	dnl no X11
	AM_CONDITIONAL([USE_X11], [false])
	AM_CONDITIONAL([USE_XV], [false])
else
	AM_CONDITIONAL([USE_X11], [true])

	SDL_LIBS="$SDL_LIBS -lX11 -lXext"
	AC_SUBST(SDL_LIBS)
	
	LIBS="$LIBS $X_LIBS"
	AC_SUBST(LIBS)

	AC_CHECK_HEADERS(X11/Xlib.h X11/XKBlib.h X11/extensions/XKB.h)
	AC_CHECK_LIB(Xxf86misc, XF86MiscGetKbdSettings,,,-lX11 -lXext)
	if test "$ac_cv_lib_Xxf86misc_XF86MiscGetKbdSettings" = "yes"
	then 
		AC_CHECK_HEADERS(X11/extensions/xf86misc.h,,,[[
#include <X11/Xlib.h>
		]])
		if test "$ac_cv_header_X11_extensions_xf86misc_h" = "yes"
		then 
			SDL_LIBS="$SDL_LIBS -lXxf86misc -lXext"
			AC_SUBST(SDL_LIBS)
		fi
	fi
	AC_CHECK_LIB(Xv, XvQueryExtension,,,-lX11 -lXext)
	if test "$ac_cv_lib_Xv_XvQueryExtension" = "yes"
	then
		AC_CHECK_HEADERS(X11/extensions/Xvlib.h,,,[[
#include <X11/Xlib.h>
		]])
		if test "$ac_cv_header_X11_extensions_xvlib_h" = "yes"
		then 
			SDL_LIBS="$SDL_LIBS -lXv -lXext"
			AC_SUBST(SDL_LIBS)
			AM_CONDITIONAL([USE_XV], [true])
		else
			if test "$ac_cv_header_X11_extensions_Xvlib_h" = "yes"
			then
				SDL_LIBS="$SDL_LIBS -lXv -lXext"
				AC_SUBST(SDL_LIBS)
				AM_CONDITIONAL([USE_XV], [true])
			else
				AM_CONDITIONAL([USE_XV], [false])
			fi
		fi
	else
		AM_CONDITIONAL([USE_XV], [false])
	fi
fi


AC_ARG_WITH(windres,
	[  --with-windres=RSC      Name of windres tool (optional)],
	windres="$withval", windres="")
AM_CONDITIONAL([USE_WINDRES], [test "x$windres" != "x"])

WINDRES="$windres"
AC_SUBST(WINDRES)

dnl winmm testing...
AC_CHECK_HEADERS(winsock.h winsock2.h windows.h)
if test "X$ac_cv_header_windows_h" = "Xyes"
then
	AM_CONDITIONAL([USE_WIN32MM], true)
	SDL_LIBS="$SDL_LIBS -lwinmm"
	AC_SUBST(SDL_LIBS)
else
	AM_CONDITIONAL([USE_WIN32MM], false)
fi

if test "X$ac_cv_func_socket" = "Xyes"
then
	dnl free networking
	AM_CONDITIONAL([USE_NETWORK], [test "x" = "x"])
else
	AC_CHECK_HEADERS(winsock.h winsock2.h sys/socket.h)
	if test "x$ac_cv_header_winsock_h" = "xyes"
	then	AM_CONDITIONAL([USE_NETWORK], true)
		SDL_LIBS="$SDL_LIBS -lwsock32"
		AC_SUBST(SDL_LIBS)
	else if test "x$ac_cv_header_winsock2_h" = "xyes"
		then	AM_CONDITIONAL([USE_NETWORK], true)
			SDL_LIBS="$SDL_LIBS -lws2_32"
			AC_SUBST(SDL_LIBS)
		else if test "x$ac_cv_header_sys_socket_h" = "xyes"
			then	AM_CONDITIONAL([USE_NETWORK], true)
				SDL_LIBS="$SDL_LIBS -lsocket"
				AC_SUBST(SDL_LIBS)
			fi
		fi
	fi
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
dnl Optimizations borrowed and modified a bit from DGen.
dnl I really don't know what they all do, to be honest :)
dnl (This ought to be above AC_PROG_CC, but that causes configure to fail
dnl when all the insane warnings are are enabled.)

AC_ARG_ENABLE(extra-opt,
	AS_HELP_STRING([--enable-extra-opt], [Add extra optimizations (egcs/GCC >= 2.95 only)]),
	ADD_OPT=$enableval,
	ADD_OPT=no)

AC_ARG_ENABLE(all-warnings,
	AS_HELP_STRING([--enable-all-warnings], [Enable ridiculous compiler warnings (GCC)]),
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
	AS_HELP_STRING([--enable-ludicrous-mode], [Treat warnings as errors]),
	ADD_LUDICROUS=$enableval,
	ADD_LUDICROUS=no)

if test x$ADD_OPT \!= xno; then
	ADD_OPT="-g0 -s -O3 -ffast-math -fomit-frame-pointer -fno-exceptions"
	ADD_OPT="$ADD_OPT -funroll-loops -frerun-cse-after-loop -fno-ident"
	ADD_OPT="$ADD_OPT -fno-strength-reduce"
	CFLAGS="$CFLAGS $ADD_OPT"
	CXXFLAGS="$CXXFLAGS $ADD_OPT -fno-rtti -fno-enforce-eh-specs"
fi

if test x$ADD_WARN \!= xno; then
        ADD_WARN="-Wall -W -Winline -Wshadow -Wcast-align -Wwrite-strings"
        ADD_WARN="$ADD_WARN -Waggregate-return -Wpacked"
        CFLAGS="$CFLAGS $ADD_WARN -Wstrict-prototypes -Wmissing-prototypes"
        CFLAGS="$CFLAGS -Wmissing-declarations -Wnested-externs"
        CXXFLAGS="$CXXFLAGS $ADD_WARN"
fi

if test x$ADD_LUDICROUS \!= xno; then
        ADD_WARN=yes
        CFLAGS="$CFLAGS  -Werror -D_FORTIFY_SOURCE=2"
        CXXFLAGS="$CXXFLAGS -Weffc++ -Wno-aggregate-return -Wno-inline -Werror -D_FORTIFY_SOURCE=2"
        # In C++, the following mad warnings are disabled:
        #
        # Winline with Werror is pure madness, it provides no useful
        # programming goals. It causes warnings such as the following:
        # warning: inlining failed in call to
        #   'MIDIchannelNoteEvent::~MIDIchannelNoteEvent()':
        #   --param inline-unit-growth limit reached
        # This is about autogenerated code! And, whether inlining fails
        # or not depends on the exact compiler version. -Winline is a
        # warning to be used for analysis; it should not be combined with
        # -Werror, or you're forcing the developers to jump through
        # lots of unnecessary hoops that have nothing to do with
        # productivity, or efficient and robust code.
        #
        # Waggregate-return is also annoying, because there's no possible
        # way to avoid warnings such as:
        # .../bits/vector.tcc:122: warning: function returns an aggregate
        # .../bits/vector.tcc:124: warning: function call has aggregate value
        #
        # -Weffc++ however was enabled, because it includes warnings
        # that enable the production of less buggy code, though a lot
        # of what it suggests are not really necessary changes.
        #
fi

if test x$ADD_DEBUG \!= xno; then
        CFLAGS="$CFLAGS -g"
        CXXFLAGS="$CXXFLAGS -g"
	OBJCFLAGS="$OBJCFLAGS -g"
	SDL_LIBS="$SDL_LIBS -g"
	AC_SUBST(SDL_LIBS)
fi
if test x$ADD_PROFILING \!= xno; then
        CFLAGS="$CFLAGS -g -pg"
        CXXFLAGS="$CXXFLAGS -g -pg"
	OBJCFLAGS="$OBJCFLAGS -g -pg"
	SDL_LIBS="$SDL_LIBS -g -pg"
	AC_SUBST(SDL_LIBS)
fi

dnl - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
