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
AC_INIT([schismtracker], [last_git_commit])

last_commit_date=[last_git_commit]

AC_CONFIG_SRCDIR([schism/main.c])

AM_INIT_AUTOMAKE([-Wall subdir-objects])
AC_CONFIG_HEADERS([config.h])

AC_CANONICAL_HOST

if test "x$last_commit_date" = "x"; then
	AC_DEFINE([EMPTY_VERSION], [], [Empty version number (okay if you don't have Git)])
fi

dnl -----------------------------------------------------------------------

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

AC_ARG_ENABLE(jack-linking,
	AS_HELP_STRING([--enable-jack-linking], [Link to JACK rather than loading at runtime]),
	JACK_LINKING=$enableval,
	JACK_LINKING=no)

AC_ARG_ENABLE(alsa-linking,
	AS_HELP_STRING([--enable-alsa-linking], [Link to ALSA rather than loading at runtime]),
	ALSA_LINKING=$enableval,
	ALSA_LINKING=no)

AC_ARG_ENABLE(opl2,
	AS_HELP_STRING([--enable-opl2], [Use OPL2 instead of OPL3]),
	USE_OPL2=$enableval,
	USE_OPL2=no)

AC_ARG_WITH([flac],
	[AS_HELP_STRING([--with-flac],[Build with FLAC support @<:@default=check@:>@])],
	[],
	[with_flac=check])

AC_ARG_WITH([jack],
	[AS_HELP_STRING([--with-jack],[Build with JACK support @<:@default=check@:>@])],
	[],
	[with_jack=check])

AC_ARG_WITH([alsa],
	[AS_HELP_STRING([--with-alsa],[Build with ALSA support @<:@default=check@:>@])],
	[],
	[with_alsa=check])

AC_ARG_WITH([mediafoundation],
	[AS_HELP_STRING([--with-mediafoundation],[Build with Microsoft Media Foundation support @<:@default=no@:>@])],
	[],
	[with_mediafoundation=no])

dnl ------------------------------------------------------------------------

dnl Check for standard programs
AC_PROG_CC
if test "x$ac_cv_prog_cc_c11" = "xno" && test "x$ac_cv_prog_cc_c99" = "xno"; then
	AC_MSG_ERROR([*** Failed to find a proper C99 compiler])
fi
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
AC_CHECK_TOOL([PERL], [perl])
AC_SUBST([PERL])
AM_CONDITIONAL([HAVE_PERL], [test "x$PERL" != "x"])

dnl We're using C
AC_LANG([C])

dnl check endianness
AC_C_BIGENDIAN

dnl Check for SDL libs
AM_PATH_SDL2([2.0.0], [], [AC_MSG_ERROR([*** SDL2 couldn't be found.])])
AC_SUBST([SDL_CFLAGS])
AC_SUBST([SDL_LIBS])

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

AM_CONDITIONAL([USE_OSS], [false])
if test "x$ac_cv_header_sys_soundcard_h" = "xyes"; then
	AM_CONDITIONAL([USE_OSS], [true])
	AC_DEFINE([USE_OSS], [1], [Open Sound System MIDI support])
fi

AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl -----------------------------------------------------------------------

dnl -lm

saved_LIBS="$LIBS"
AC_SEARCH_LIBS([powf], [m])
LIBS="$saved_LIBS"
if test "x$ac_cv_search_powf" = "xno"; then
	AC_MSG_ERROR([*** Couldn't find a standard math library.])
elif ! test "x$ac_cv_search_powf" = "xnone required"; then
	AC_SUBST([LIB_MATH], ["$ac_cv_search_powf"])
fi

dnl -----------------------------------------------------------------------

dnl Platform-specific behavior
AC_MSG_CHECKING([host platform])
use_macosx=no
use_win32=no
use_wii=no
use_network=no

case "${host_os}" in
	darwin*)
		AC_MSG_RESULT([found Darwin host])

		AC_SUBST([OSX_LDADD], ["-lSDL2main -framework Cocoa -framework CoreAudio -framework CoreMIDI -framework IOKit"])

		use_macosx=yes
		AC_DEFINE([SCHISM_MACOSX], [1], [Mac OS X])
		;;
	cygwin*|mingw*)
		AC_MSG_RESULT([found Windows host])
		WIN32_LIBS="-lwinmm"

		AC_CHECK_HEADERS([winsock.h winsock2.h])
		if test "x$ac_cv_header_winsock2_h" = "xyes"; then
			AC_SUBST([NETWORK_LIBS], [-lws2_32])
			use_network=yes
		elif test "x$ac_cv_header_winsock_h" = "xyes"; then
			AC_SUBST([NETWORK_LIBS], [-lsock32])
			use_network=yes
		else
			AC_MSG_ERROR([*** Couldn't find winsock\! (how\?)])
		fi

		use_win32=yes
		AC_DEFINE([SCHISM_WIN32], [1], [Windows])
		;;
	*)
		AC_MSG_RESULT([nothing to do here])

		dnl now check for other stuff
		AC_CHECK_LIB([ogc], [IOS_ReloadIOS], [libogc_found=yes], [libogc_found=no], [-mrvl -L${DEVKITPRO}/libogc/lib/wii])

		if test "x$libogc_found" = "xyes"; then
			WII_CPPFLAGS="-I${DEVKITPRO}/libogc/include -I${DEVKITPPC}/powerpc-eabi/include"
			WII_LDFLAGS="-L${DEVKITPRO}/libogc/lib/wii -L${DEVKITPPC}/powerpc-eabi/lib"
			AC_SUBST([WII_CPPFLAGS])
			AC_SUBST([WII_LDFLAGS])

			EXEEXT=.elf
			AC_SUBST([EXEEXT])

			use_wii=yes
			AC_DEFINE([SCHISM_WII], [1], [Wii])
		fi

		dnl Networking...

		saved_LIBS="$LIBS"
		AC_SEARCH_LIBS([socket], [socket network])
		LIBS="$saved_LIBS"

		if test "x$ac_cv_search_socket" = "xno"; then
			AC_MSG_WARN([*** Couldn't find a proper sockets library])
		elif test "x$ac_cv_search_socket" = "xnone required"; then
			if test "x$ac_cv_header_sys_socket_h" = "xyes"; then
				dnl free networking
				AC_DEFINE([USE_NETWORK], [1], [Networking])
				use_network=yes
			else
				use_network=no
			fi
		else
			AC_SUBST([NETWORK_LIBS], [$ac_cv_search_socket])
			AC_DEFINE([USE_NETWORK], [1], [Networking])
			use_network=yes
		fi

		;;
esac

if test "x$use_macosx" = "xyes"; then
	AC_PROG_OBJC
else
	dnl stupid hack to make Objective-C optional
	AM_CONDITIONAL([am__fastdepOBJC], [false])
	OBJC="$CC"
fi

AM_CONDITIONAL([USE_WIN32], [test "x$use_win32" = "xyes"])
AM_CONDITIONAL([USE_WII], [test "x$use_wii" = "xyes"])
AM_CONDITIONAL([USE_MACOSX], [test "x$use_macosx" = "xyes"])
AM_CONDITIONAL([am__fastdepOBJC], [test "x$use_macosx" = "xyes"])
AM_CONDITIONAL([USE_NETWORK], [test "x$use_network" = "xyes"])

dnl --------------------------------------------------------------------------

dnl configure flags, optional dependencies

AM_CONDITIONAL([USE_FLAC], false)
if test "x$with_flac" = "xcheck" || test "x$with_flac" = "xyes"; then
	dnl use pkg-config, it'll deal with everything for us
	PKG_CHECK_MODULES([FLAC], [flac], [libflac_found=yes], [libflac_found=no])
	if test "x$libflac_found" = "xyes"; then
		AC_SUBST([FLAC_CFLAGS])
		AC_SUBST([FLAC_LIBS])
		AC_DEFINE([USE_FLAC], [1], [FLAC support])
		AM_CONDITIONAL([USE_FLAC], true)
	elif test "x$with_flac" = "xyes"; then
		AC_MSG_ERROR([*** Failed to find libFLAC])
	fi
fi

AM_CONDITIONAL([USE_JACK], false)
AM_CONDITIONAL([LINK_TO_JACK], false)
if test "x$with_jack" = "xcheck" || test "x$with_jack" = "xyes"; then
	PKG_CHECK_MODULES([JACK], [jack], [jack_found=yes], [jack_found=no])
	if test "x$jack_found" = "xyes"; then
		AC_SUBST([JACK_CFLAGS])
		AM_CONDITIONAL([USE_JACK], true)
		AC_DEFINE([USE_JACK], [1], [JACK MIDI support])
		if test "x$JACK_LINKING" = "xyes"; then
			AC_SUBST([JACK_LIBS])
			AM_CONDITIONAL([LINK_TO_JACK], true)
		else
			AC_DEFINE([JACK_DYNAMIC_LOAD], [1], [Dynamically load JACK symbols])
		fi
	elif test "x$with_jack" = "xyes"; then
		AC_MSG_ERROR([*** Failed to find JACK])
	fi
fi

AM_CONDITIONAL([USE_ALSA], false)
AM_CONDITIONAL([LINK_TO_ALSA], false)
if test "x$with_alsa" = "xcheck" || test "x$with_alsa" = "xyes"; then
	saved_LIBS="$LIBS"
	AC_CHECK_LIB([asound], [snd_seq_open], [alsa_found=yes], [alsa_found=no])
	if test "x$alsa_found" = "xyes"; then
		AM_CONDITIONAL([USE_ALSA], true)
		AC_DEFINE([USE_ALSA], [1], [ALSA MIDI support])
		if test "x$ALSA_LINKING" = "xyes"; then
			AM_CONDITIONAL([LINK_TO_ALSA], true)
		else
			LIBS="$saved_LIBS"
			AC_DEFINE([ALSA_DYNAMIC_LOAD], [1], [Dynamically load ALSA symbols])
		fi
	elif test "x$with_alsa" = "xyes"; then
		AC_MSG_ERROR([*** Failed to find ALSA])
	fi
fi

AM_CONDITIONAL([USE_MEDIAFOUNDATION], false)
if test "x$with_mediafoundation" = "xcheck" || test "x$with_mediafoundation" = "xyes"; then
	dnl requires Windows 7 or newer
	saved_LIBS="$LIBS"
	AC_CHECK_LIB([mfplat], [MFCreateMFByteStreamOnStream], [mf_found=yes], [mf_found=no])
	if test "x$mf_found" = "xyes"; then
		AM_CONDITIONAL([USE_MEDIAFOUNDATION], true)
		AC_DEFINE([USE_MEDIAFOUNDATION], [1], [Use Media Foundation sample loading])
		LIBS="$saved_LIBS"
	elif test "x$with_mediafoundation" = "xyes"; then
		AC_MSG_ERROR([*** Failed to find Media Foundation with MFCreateMFByteStreamOnStream])
	fi
fi

if test "x$USE_OPL2" = "xyes"; then
	AC_DEFINE([OPLSOURCE], [2], [Which OPL chip to emulate])
	AM_CONDITIONAL([USE_OPL2], [true])
else
	AC_DEFINE([OPLSOURCE], [3], [Which OPL chip to emulate])
	AM_CONDITIONAL([USE_OPL2], [false])
fi

dnl --------------------------------------------------------------------------

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

dnl --------------------------------------------------------------------------

AC_CONFIG_FILES([Makefile])
AC_OUTPUT

dnl --------------------------------------------------------------------------
dnl These are shipped in the release tarball, so we don't want to regenerate
dnl them after configure is run.

if test -e "auto/default-font.c"; then
	touch "auto/default-font.c"
fi

if test "x$PERL" = "x"; then
	if test -e "auto/helptext.c"; then
		touch "auto/helptext.c"
	fi
fi
