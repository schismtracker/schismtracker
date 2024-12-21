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

last_commit_date="last_git_commit"

AC_CONFIG_SRCDIR([schism/main.c])

AM_INIT_AUTOMAKE([-Wall subdir-objects])
AC_CONFIG_HEADERS([build-config.h])

AC_CANONICAL_HOST

if test "x$last_commit_date" = "x"; then
	AC_DEFINE([EMPTY_VERSION], [], [Empty version number (okay if you don't have Git)])
	echo "*** empty version"
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

AC_ARG_ENABLE(flac-linking,
	AS_HELP_STRING([--enable-flac-linking], [Link to FLAC rather than loading at runtime]),
	FLAC_LINKING=$enableval,
	FLAC_LINKING=no)

AC_ARG_ENABLE(jack-linking,
	AS_HELP_STRING([--enable-jack-linking], [Link to JACK rather than loading at runtime]),
	JACK_LINKING=$enableval,
	JACK_LINKING=no)

AC_ARG_ENABLE(alsa-linking,
	AS_HELP_STRING([--enable-alsa-linking], [Link to ALSA rather than loading at runtime]),
	ALSA_LINKING=$enableval,
	ALSA_LINKING=no)

AC_ARG_ENABLE(force-wii,
	AS_HELP_STRING([--enable-force-wii], [Force target the Wii]),
	FORCE_WII=$enableval,
	FORCE_WII=no)

AC_ARG_ENABLE(force-wiiu,
	AS_HELP_STRING([--enable-force-wiiu], [Force target the Wii U]),
	FORCE_WIIU=$enableval,
	FORCE_WIIU=no)

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
	[AS_HELP_STRING([--with-mediafoundation],[Build with Microsoft Media Foundation support @<:@default=check@:>@])],
	[],
	[with_mediafoundation=check])

AC_ARG_WITH([sdl12],
	[AS_HELP_STRING([--with-sdl12],[Build with SDL 1.2 backend @<:@default=check@:>@])],
	[],
	[with_sdl12=check])

AC_ARG_WITH([sdl2],
	[AS_HELP_STRING([--with-sdl2],[Build with SDL 2 backend @<:@default=check@:>@])],
	[],
	[with_sdl2=check])

AC_ARG_ENABLE(sdl12-linking,
	AS_HELP_STRING([--enable-sdl12-linking], [Link to SDL 1.2 rather than loading at runtime]),
	SDL12_LINKING=$enableval,
	SDL12_LINKING=no)

AC_ARG_ENABLE(sdl2-linking,
	AS_HELP_STRING([--enable-sdl2-linking], [Link to SDL 2 rather than loading at runtime]),
	SDL2_LINKING=$enableval,
	SDL2_LINKING=no)

AC_ARG_ENABLE(x11-linking,
	AS_HELP_STRING([--enable-x11-linking], [Link to X11 rather than loading at runtime]),
	X11_LINKING=$enableval,
	X11_LINKING=no)

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
AC_PROG_MKDIR_P
AC_PROG_FGREP
AC_CHECK_PROGS([SORT], [gsort sort], [false])
AS_IF([! "$SORT" -V </dev/null >/dev/null], [AC_MSG_WARN([sort(1) that supports the -V option is required to find dynamic libraries])])

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

dnl hijacked from libtool
LIBM=
case $host in
*-*-beos* | *-*-cegcc* | *-*-cygwin* | *-*-haiku* | *-*-mingw* | *-*-pw32* | *-*-darwin*)
  # These system don't have libm, or don't need it
  ;;
*-ncr-sysv4.3*)
  AC_CHECK_LIB(mw, _mwvalidcheckl, LIBM=-lmw)
  AC_CHECK_LIB(m, cos, LIBM="$LIBM -lm")
  ;;
*)
  AC_CHECK_LIB(m, cos, LIBM=-lm)
  ;;
esac
AC_SUBST([LIBM])

AC_SUBST([LIBM])

dnl Unicode crap
PKG_CHECK_MODULES([UTF8PROC], [libutf8proc], [utf8proc_found=yes], [utf8proc_found=no])
if test "x$utf8proc_found" = "xno"; then
	AC_MSG_ERROR([*** Failed to find utf8proc])
fi

AC_SUBST([UTF8PROC_CFLAGS])
AC_SUBST([UTF8PROC_LIBS])

dnl Functions
AC_CHECK_FUNCS(strchr memmove strerror strtol strcasecmp strncasecmp strverscmp stricmp strnicmp strcasestr strptime asprintf vasprintf memcmp nice setenv unsetenv dup fnmatch mkstemp localtime_r umask execl fork nanosleep usleep access getopt_long fdopen)
AM_CONDITIONAL([NEED_ASPRINTF], [test "x$ac_cv_func_asprintf" = "xno"])
AM_CONDITIONAL([NEED_VASPRINTF], [test "x$ac_cv_func_vasprintf" = "xno"])
AM_CONDITIONAL([NEED_MEMCMP], [test "x$ac_cv_func_memcmp" = "xno"])
AM_CONDITIONAL([NEED_STRPTIME], [test "x$ac_cv_func_strptime" = "xno"])
AM_CONDITIONAL([NEED_MKSTEMP], [test "x$ac_cv_func_mkstemp" = "xno"])
AM_CONDITIONAL([NEED_LOCALTIME_R], [test "x$ac_cv_func_localtime_r" = "xno"])
AM_CONDITIONAL([NEED_SETENV], [test "x$ac_cv_func_setenv" = "xno"])
AM_CONDITIONAL([NEED_UNSETENV], [test "x$ac_cv_func_unsetenv" = "xno"])
AM_CONDITIONAL([NEED_GETOPT], [test "x$ac_cv_func_getopt_long" = "xno"])

dnl Headers, typedef crap, et al.
AC_CHECK_HEADERS(sys/time.h dirent.h limits.h signal.h unistd.h sys/param.h sys/ioctl.h sys/socket.h sys/soundcard.h poll.h sys/poll.h linux/fb.h)

AM_CONDITIONAL([USE_OSS], [false])
if test "x$ac_cv_header_sys_soundcard_h" = "xyes"; then
	AM_CONDITIONAL([USE_OSS], [true])
	AC_DEFINE([USE_OSS], [1], [Open Sound System MIDI support])
fi

AC_CHECK_FUNC([mmap], [have_mmap=yes], [have_mmap=no])
AC_CHECK_HEADER([sys/mman.h], [have_sys_mman_h=yes], [have_sys_mman_h=no])

AM_CONDITIONAL([USE_MMAP], [false])
if test "x$have_mmap" = "xyes" && test "x$have_sys_mman_h" = "xyes"; then
	AM_CONDITIONAL([USE_MMAP], [true])
	AC_DEFINE([HAVE_MMAP], [1], [Have mmap system call])
fi

AC_C_CONST
AC_C_INLINE
AC_TYPE_OFF_T
AC_TYPE_SIZE_T
AC_STRUCT_TM

dnl -----------------------------------------------------------------------

dnl Platform-specific behavior
AC_MSG_CHECKING([host platform])
use_macosx=no
use_macos=no
use_win32=no
use_wii=no
use_wiiu=no
use_network=no
use_controller=no
posix_fseek=unknown

have_loadso=no

case "${host_os}" in
	darwin*)
		AC_MSG_RESULT([found Darwin host])

		AC_SUBST([OSX_LDADD], ["-framework Cocoa -framework CoreAudio -framework CoreMIDI -framework IOKit"])

		use_macosx=yes
		posix_fseek=yes
		AC_DEFINE([SCHISM_MACOSX], [1], [Mac OS X])
		;;
	macos*)
		# Retro68 toolchain
		AC_MSG_RESULT([found Classic Mac OS host])

		use_macos=yes
		AC_DEFINE([SCHISM_MACOS], [1], [Classic Mac OS])

		EXEEXT=.xcoff

		AC_PATH_PROG([REZ], [Rez])
		if test "x$REZ" = "x"; then
			AC_MSG_ERROR([*** Couldn't find Rez])
		fi

		AC_PATH_PROG([MAKE_PEF], [MakePEF])
		if test "x$MAKE_PEF" = "x"; then
			AC_MSG_ERROR([*** Couldn't find MakePEF])
		fi

		posix_fseek=no
		;;
	cygwin*|mingw*)
		AC_MSG_RESULT([found Windows host])
		WIN32_LIBS="-lwinmm"

		AC_CHECK_HEADERS([winsock.h winsock2.h])
		if test "x$ac_cv_header_winsock_h" = "xyes"; then
			AC_SUBST([NETWORK_LIBS], [-lwsock32])
			use_network=yes
		elif test "x$ac_cv_header_winsock2_h" = "xyes"; then
			AC_SUBST([NETWORK_LIBS], [-lws2_32])
			use_network=yes
		else
			AC_MSG_ERROR([*** Couldn't find winsock (how)])
		fi

		have_loadso=yes

		use_win32=yes
		posix_fseek=yes
		AC_DEFINE([SCHISM_WIN32], [1], [Windows])
		;;
	*)
		AC_MSG_RESULT([nothing to do here])

		dnl now check for other stuff
		saved_LIBS="$LIBS"
		AC_CHECK_LIB([ogc], [IOS_ReloadIOS], [libogc_found=yes], [libogc_found=no], [-mrvl -L${DEVKITPRO}/libogc/lib/wii])
		AC_CHECK_LIB([wut], [OSDynLoad_Acquire], [libwut_found=yes], [libwut_found=no], [-L${DEVKITPRO}/wut/lib])
		LIBS="$saved_LIBS"

		which=none

		if test "x$FORCE_WIIU" = "xyes"; then
			which=wiiu
		elif test "x$FORCE_WII" = "xyes"; then
			which=wii
		elif test "x$libogc_found" = "xyes"; then
			which=wii
		elif test "x$libwut_found" = "xyes"; then
			which=wiiu
		fi

		if test "x$which" = "xwii"; then
			WII_CPPFLAGS="-I${DEVKITPRO}/libogc/include -I${DEVKITPPC}/powerpc-eabi/include"
			WII_LDFLAGS="-logc -L${DEVKITPRO}/libogc/lib/wii -L${DEVKITPPC}/powerpc-eabi/lib"
			AC_SUBST([WII_CPPFLAGS])
			AC_SUBST([WII_LDFLAGS])

			EXEEXT=.elf
			AC_SUBST([EXEEXT])

			use_wii=yes
			use_controller=yes
			AC_DEFINE([SCHISM_WII], [1], [Wii])
			AC_DEFINE([SCHISM_CONTROLLER], [1], [Enable game controller support])
			posix_fseek=yes
		elif test "x$which" = "xwiiu"; then
			WIIU_CPPFLAGS="-I${DEVKITPRO}/wut/include -I${DEVKITPPC}/powerpc-eabi/include"
			WIIU_LDFLAGS="-lwut -T${DEVKITPRO}/wut/share/wut.ld -Wl,--gc-sections -Wl,--emit-relocs -Wl,--no-eh-frame-hdr -z nocopyreloc -L${DEVKITPRO}/wut/lib -L${DEVKITPPC}/powerpc-eabi/lib"
			AC_SUBST([WIIU_CPPFLAGS])
			AC_SUBST([WIIU_LDFLAGS])

			AC_PATH_PROG([WUHBTOOL], [wuhbtool])
			if test "x$WUHBTOOL" = "x"; then
				AC_MSG_ERROR([*** Couldn't find wuhbtool])
			fi

			AC_PATH_PROG([ELF2RPL],  [elf2rpl])
			if test "x$ELF2RPL" = "x"; then
				AC_MSG_ERROR([*** Couldn't find elf2rpl])
			fi

			AC_SUBST([WUHBTOOL])
			AC_SUBST([ELF2RPL])

			EXEEXT=.elf
			AC_SUBST([EXEEXT])

			use_wiiu=yes
			use_controller=yes
			AC_DEFINE([SCHISM_WIIU], [1], [Wii U])
			AC_DEFINE([SCHISM_CONTROLLER], [1], [Enable game controller support])
			posix_fseek=yes
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

dnl Libs
AC_SEARCH_LIBS([dlopen], [dl])
if test "x$ac_cv_search_dlopen" = "xyes" || test "x$ac_cv_search_dlopen" = "xnone required"; then
	AC_DEFINE([HAVE_LIBDL], [1], [ ])
	have_loadso=yes
fi

if test "x$use_macosx" = "xyes"; then
	AC_PROG_OBJC
else
	dnl stupid hack to make Objective-C optional
	AM_CONDITIONAL([am__fastdepOBJC], [false])
	OBJC="$CC"
fi

AM_CONDITIONAL([USE_WIN32], [test "x$use_win32" = "xyes"])
AM_CONDITIONAL([USE_WII], [test "x$use_wii" = "xyes"])
AM_CONDITIONAL([USE_WIIU], [test "x$use_wiiu" = "xyes"])
AM_CONDITIONAL([USE_CONTROLLER], [test "x$use_controller" = "xyes"])
AM_CONDITIONAL([USE_MACOSX], [test "x$use_macosx" = "xyes"])
AM_CONDITIONAL([USE_MACOS], [test "x$use_macos" = "xyes"])
AM_CONDITIONAL([am__fastdepOBJC], [test "x$use_macosx" = "xyes"])
AM_CONDITIONAL([USE_NETWORK], [test "x$use_network" = "xyes"])

dnl ------------------------------------------------------------------------

AC_MSG_CHECKING([for arithmetic right shift])
AC_COMPILE_IFELSE([AC_LANG_SOURCE([[int xxx[((-1 >> 1) == -1) ? 1 : -1];]])],
	[have_arithmetic_rshift=yes],
	[have_arithmetic_rshift=no])
AC_MSG_RESULT([$have_arithmetic_rshift])
if test "x$have_arithmetic_rshift" = "xyes"; then
	AC_DEFINE([HAVE_ARITHMETIC_RSHIFT], [1], [ ])
fi

# An implementation can technically do whatever it wants here...
AC_MSG_CHECKING([for sane signed left shift])
AC_RUN_IFELSE([AC_LANG_PROGRAM([[]], [[return ((-1 << 1) != -2);]])],
	[have_sane_signed_lshift=yes],
	[have_sane_signed_lshift=no],
	[have_sane_signed_lshift=no]) # assume no
AC_MSG_RESULT([$have_sane_signed_lshift])
if test "x$have_sane_signed_lshift" = "xyes"; then
	AC_DEFINE([HAVE_SANE_SIGNED_LSHIFT], [1], [ ])
fi

dnl --------------------------------------------------------------------------
dnl check whether fseek allows to seek past EOF or not

if test "x$posix_fseek" = "xunknown"; then
	AC_MSG_CHECKING([if fseek allows to go past EOF])
	AC_RUN_IFELSE([AC_LANG_PROGRAM([[
		#include <stdio.h>
		#include <string.h>
	]], [[
		unsigned char expected[] = {0x80, 0x00, 0x00, 0x00, 0x00, 0x00, 0xFF};
		unsigned char buf[sizeof(expected)];

		{
			FILE *fp = fopen("conftest.bin", "wb");
			if (!fp)
				return 1;

			if (fputc('\x80', fp) == EOF) {
				fclose(fp);
				return 2;
			}
			if (fseek(fp, 5, SEEK_CUR) < 0) {
				fclose(fp);
				return 3;
			}
			if (fputc('\xFF', fp) == EOF) {
				fclose(fp);
				return 4;
			}
			if (fclose(fp) == EOF)
				return 5;
		}

		{
			FILE *fp = fopen("conftest.bin", "rb");
			if (!fp)
				return 6;

			if (fseek(fp, 0, SEEK_END) < 0) {
				fclose(fp);
				return 7;
			}

			long length = ftell(fp);
			if (length < 0 || length != sizeof(expected)) {
				fclose(fp);
				return 8;
			}

			// back to the beginning
			if (fseek(fp, 0, SEEK_SET) < 0) {
				fclose(fp);
				return 9;
			}

			if (fread(buf, 1, sizeof(buf), fp) != sizeof(buf)) {
				fclose(fp);
				return 10;
			}

			if (fclose(fp) == EOF)
				return 11;
		}

		// compare what we got
		if (memcmp(buf, expected, sizeof(buf)))
			return 12;

		// yay!
		return 0;
	]])],
	[posix_fseek=yes],
	[posix_fseek=no],
	[] # err, the existing value above will suffice
	)

	AC_MSG_RESULT([$posix_fseek])
	if test "x$posix_fseek" != "xyes"; then
		AC_DEFINE([HAVE_NONPOSIX_FSEEK], [1], [fseek past EOF is broken])
	fi
fi

dnl --------------------------------------------------------------------------

dnl configure flags, optional dependencies

AM_CONDITIONAL([USE_FLAC], false)
AM_CONDITIONAL([LINK_TO_FLAC], false)
if test "x$with_flac" = "xcheck" || test "x$with_flac" = "xyes"; then
	dnl use pkg-config, it'll deal with everything for us
	PKG_CHECK_MODULES([FLAC], [flac], [libflac_found=yes], [libflac_found=no])
	if test "x$libflac_found" = "xyes"; then
		AC_SUBST([FLAC_CFLAGS])
		AC_DEFINE([USE_FLAC], [1], [FLAC support])
		AM_CONDITIONAL([USE_FLAC], true)
		if test "x$FLAC_LINKING" = "xyes"; then
			AC_SUBST([FLAC_LIBS])
			AM_CONDITIONAL([LINK_TO_FLAC], true)
		else
			if test "x$have_loadso" != "xyes"; then
				AC_MSG_ERROR([*** Failed to find a proper dynamic loading backend])
			fi
			AC_DEFINE([FLAC_DYNAMIC_LOAD], [1], [Dynamically load FLAC symbols])
		fi
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
			if test "x$have_loadso" != "xyes"; then
				AC_MSG_ERROR([*** Failed to find a proper dynamic loading backend])
			fi
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
			if test "x$have_loadso" != "xyes"; then
				AC_MSG_ERROR([*** Failed to find a proper dynamic loading backend])
			fi
			AC_DEFINE([ALSA_DYNAMIC_LOAD], [1], [Dynamically load ALSA symbols])
		fi
	elif test "x$with_alsa" = "xyes"; then
		AC_MSG_ERROR([*** Failed to find ALSA])
	fi
fi

AM_CONDITIONAL([USE_MEDIAFOUNDATION], false)
if test "x$with_mediafoundation" = "xcheck" || test "x$with_mediafoundation" = "xyes"; then
	dnl requires Windows 7 or newer, but doesn't link to the libraries to keep
	dnl XP/Vista also working
	if test "x$use_win32" = "xyes"; then
		AM_CONDITIONAL([USE_MEDIAFOUNDATION], true)
		AC_DEFINE([USE_MEDIAFOUNDATION], [1], [Use Media Foundation sample loading])
	elif test "x$with_mediafoundation" = "xyes"; then
		AC_MSG_ERROR([*** Failed to find Media Foundation with MFCreateMFByteStreamOnStream])
	fi
fi

AM_CONDITIONAL([USE_SDL2], false)
AM_CONDITIONAL([LINK_TO_SDL2], false)
if test "x$with_sdl2" = "xcheck" || test "x$with_sdl2" = "xyes"; then
	PKG_CHECK_MODULES([SDL2], [sdl2], [sdl2_found=yes], [sdl2_found=no])
	if test "x$sdl2_found" = "xyes"; then
		AC_SUBST([SDL2_CFLAGS])
		AM_CONDITIONAL([USE_SDL2], true)
		AC_DEFINE([SCHISM_SDL2], [1], [SDL 2 backend])
		if test "x$SDL2_LINKING" = "xyes"; then
			AC_SUBST([SDL2_LIBS])
			AM_CONDITIONAL([LINK_TO_SDL2], true)
		else
			if test "x$have_loadso" != "xyes"; then
				AC_MSG_ERROR([*** Failed to find a proper dynamic loading backend])
			fi
			AC_DEFINE([SDL2_DYNAMIC_LOAD], [1], [Dynamically load SDL2 symbols])
		fi
	elif test "x$with_sdl2" = "xyes"; then
		AC_MSG_ERROR([*** Failed to find SDL 2])
	fi
fi

AM_CONDITIONAL([USE_SDL12], false)
AM_CONDITIONAL([LINK_TO_SDL12], false)
if test "x$with_sdl12" = "xcheck" || test "x$with_sdl12" = "xyes"; then
	PKG_CHECK_MODULES([SDL12], [sdl >= 1.2.10], [sdl12_found=yes], [sdl12_found=no])
	if test "x$sdl12_found" = "xyes"; then
		AC_SUBST([SDL12_CFLAGS])
		AM_CONDITIONAL([USE_SDL12], true)
		AC_DEFINE([SCHISM_SDL12], [1], [SDL 1.2 backend])
		if test "x$SDL12_LINKING" = "xyes"; then
			AC_SUBST([SDL12_LIBS])
			AM_CONDITIONAL([LINK_TO_SDL12], true)
		else
			if test "x$have_loadso" != "xyes"; then
				AC_MSG_ERROR([*** Failed to find a proper dynamic loading backend])
			fi
			AC_DEFINE([SDL12_DYNAMIC_LOAD], [1], [Dynamically load SDL 1.2 symbols])
		fi
	elif test "x$with_sdl12" = "xyes"; then
		AC_MSG_ERROR([*** Failed to find SDL 1.2])
	fi
fi

dnl Function to find a library in the compiler search path, lifted from SDL
find_lib()
{
	gcc_bin_path=[`$CC -print-search-dirs 2>/dev/null | $FGREP programs: | sed 's/[^=]*=\(.*\)/\1/' | sed 's/:/ /g'`]
	gcc_lib_path=[`$CC -print-search-dirs 2>/dev/null | $FGREP libraries: | sed 's/[^=]*=\(.*\)/\1/' | sed 's/:/ /g'`]
	env_lib_path=[`echo $LIBS $LDFLAGS $* | sed 's/-L[ ]*//g'`]
	if test "$cross_compiling" = yes; then
		host_lib_path=""
	else
		host_lib_path="/usr/$base_libdir /usr/local/$base_libdir"
	fi
	for path in $env_lib_path $gcc_bin_path $gcc_lib_path $host_lib_path; do
		lib=[`ls -- $path/$1 2>/dev/null | sed 's,.*/,,' | "$SORT" -V -r | $AWK 'BEGIN{FS="."}{ print NF, $0 }' | "$SORT" -n -s | sed 's,[0-9]* ,,' | head -1`]
		if test x$lib != x; then
			echo $lib
			return
		fi
	done
}

AM_CONDITIONAL([USE_X11], [false])
AM_CONDITIONAL([LINK_TO_X11], [false])
AC_PATH_X
AC_PATH_XTRA
if test "x$have_x" = "xyes"; then
	case "$host" in
		*-*-darwin*)
			# Apple now puts this in /opt/X11 ?
			x11_lib='/opt/X11/lib/libX11.6.dylib'
			;;
		*-*-openbsd*)
			x11_lib='libX11.so'
			;;
		*)
			x11_lib=[`find_lib "libX11.so.*" "$X_LIBS -L/usr/X11/$base_libdir -L/usr/X11R6/$base_libdir" | sed 's/.*\/\(.*\)/\1/; q'`]
			;;
	esac

	if test "x$have_loadso" != "xyes" && test "x$X11_LINKING" = "xno"; then
		AC_MSG_ERROR([*** Failed to find a proper dynamic loading backend])
	fi

	AC_DEFINE([SCHISM_USE_X11], [1], [ ])

	AM_CONDITIONAL([USE_X11], [true])

	if test "x$have_loadso" = "xyes" && test "x$X11_LINKING" = "xno" && test "x$x11_lib" != "x"; then
		echo "-- dynamic libX11 -> $x11_lib"
		AC_DEFINE_UNQUOTED(X11_LIBRARY_LIBX11_PATH, "$x11_lib", [ ])
		AC_DEFINE([X11_DYNAMIC_LOAD], [1], [ ])
	else
		X11_LINKING=yes
		EXTRA_LDFLAGS="$EXTRA_LDFLAGS $X_LIBS -lX11"
		AM_CONDITIONAL([LINK_TO_X11], [true])
	fi

	AC_SUBST([X_CFLAGS])
	AC_SUBST([X_LIBS])
fi

dnl --------------------------------------------------------------------------

if test "x$USE_OPL2" = "xyes"; then
	AC_DEFINE([OPLSOURCE], [2], [Which OPL chip to emulate])
	AM_CONDITIONAL([USE_OPL2], [true])
else
	AC_DEFINE([OPLSOURCE], [3], [Which OPL chip to emulate])
	AM_CONDITIONAL([USE_OPL2], [false])
fi

dnl --------------------------------------------------------------------------

dnl fortify needs -O; do this early so ADD_OPT can override with higher -O level
project_CFLAGS=""
if test x$ADD_FORTIFY \!= xno; then
	project_CFLAGS="$project_CFLAGS -O -D_FORTIFY_SOURCE=2"
fi

dnl place extra optimizations after existing cflags so that they can override
dnl override whatever flags might exist by default (-g -O2 usually)
if test x$ADD_OPT \!= xno; then
	if test x$ADD_DEBUG \!= xno || test x$ADD_PROFILING \!= xno; then
		AC_MSG_NOTICE([You aren't supposed to use --enable-debug or --enable-profiling together with --enable-extra-opt!!])
	fi
	ADD_OPT="-Ofast -g0 -s -fno-exceptions -march=native"
	project_CFLAGS="$project_CFLAGS $ADD_OPT"
fi

if test x$ADD_LUDICROUS \!= xno; then
	ADD_WARN=yes
	project_CFLAGS="$project_CFLAGS -Werror"
fi

dnl ... but put the warnings first, to make it possible to quiet certain
dnl warnings if necessary, while still providing most of the benefit
if test x$ADD_WARN \!= xno; then
	ADD_WARN="-Wall -Wextra -Winline -Wshadow -Wwrite-strings -Waggregate-return -Wpacked"
	ADD_WARN="$ADD_WARN -Wstrict-prototypes -Wmissing-prototypes -Wmissing-declarations -Wnested-externs"
	project_CFLAGS="$ADD_WARN $project_CFLAGS"
fi

dnl Unlike a grand -Werror, this one could be rather important:
dnl functions returning random values are no good under any circumstances.
AS_IF([test ${ac_cv_c_compiler_gnu+y}], [CFLAGS="$CFLAGS -Werror=return-type"])

if test x$ADD_PROFILING \!= xno || test x$ADD_DEBUG \!= xno; then
	project_CFLAGS="$project_CFLAGS -g -O0"
	project_OBJCFLAGS="$project_OBJCFLAGS -g -O0"
	SDL_LIBS="$SDL_LIBS -g -O0"
	AC_SUBST(SDL_LIBS)
fi
if test x$ADD_PROFILING \!= xno; then
	project_CFLAGS="$project_CFLAGS -pg"
	project_OBJCFLAGS="$project_OBJCFLAGS -pg"
	SDL_LIBS="$SDL_LIBS -pg"
	AC_SUBST(SDL_LIBS)
fi

dnl --------------------------------------------------------------------------

AC_SUBST([project_CFLAGS])
AC_SUBST([project_OBJCFLAGS])
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

dnl --------------------------------------------------------------------------
dnl old build config filename; it overrides the regular include paths, so it
dnl has to be removed before we can start building

if test -e "config.h"; then
	rm "config.h"
fi
