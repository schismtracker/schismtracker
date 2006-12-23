AUTOMAKE_OPTIONS = gnu dist-bzip2 no-dist-gzip

helptexts = \
	helptext/index			\
	helptext/global-keys		\
	helptext/info-page		\
	helptext/instrument-list	\
	helptext/message-editor		\
	helptext/midi-output		\
	helptext/orderlist-pan		\
	helptext/orderlist-vol		\
	helptext/pattern-editor		\
	helptext/sample-list
fonts = \
	font/default-lower.fnt		\
	font/default-upper-alt.fnt	\
	font/default-upper-itf.fnt	\
	font/half-width.fnt
icons = \
	icons/appIcon.icns		\
	icons/it_logo.png		\
	icons/moduleIcon.icns		\
	icons/schism-file-128.png	\
	icons/schism-icon-128.png	\
	icons/schism-icon-16.png	\
	icons/schism-icon-192.png	\
	icons/schism-icon-22.png	\
	icons/schism-icon-24.png	\
	icons/schism-icon-32.png	\
	icons/schism-icon-36.png	\
	icons/schism-icon-48.png	\
	icons/schism-icon-64.png	\
	icons/schism-icon-72.png	\
	icons/schism-icon-96.png	\
	icons/schism-icon.svg		\
	icons/schism_logo.png		\
	icons/schismres.ico

EXTRA_DIST = \
	COPYING.frag-opt		\
	COPYING.libmodplug		\
	include/auto/README		\
	$(helptexts)			\
	$(fonts)			\
	$(icons)

bin_PROGRAMS = schismtracker

## whatever
## this broke my build scripts. I'm putting it back -mrsb
#noinst_SCRIPTS = scripts/*

noinst_HEADERS = \
	include/auto/build-version.h	\
	include/auto/default-font.h	\
	include/auto/helpenum.h		\
	include/auto/helptext.h		\
	include/auto/logoit.h		\
	include/auto/logoschism.h	\
	include/auto/schismico.h	\
	include/acc.h			\
	include/charset.h		\
	include/clippy.h		\
	include/config-parser.h		\
	include/diskwriter.h		\
	include/dmoz.h			\
	include/draw-char.h		\
	include/event.h			\
	include/fmt.h			\
	include/frag-opt.h		\
	include/headers.h		\
	include/it.h			\
	include/macosx-sdlmain.h	\
	include/midi.h			\
	include/mixer.h			\
	include/mplink.h		\
	include/page.h			\
	include/palettes.h		\
	include/pattern-view.h		\
	include/sample-edit.h		\
	include/sdlmain.h		\
	include/slurp.h			\
	include/song.h			\
	include/util.h			\
	include/vgamem-scanner.h	\
	include/video.h			\
	modplug/it_defs.h		\
	modplug/sndfile.h		\
	modplug/stdafx.h

LIBOBJS=

if USE_X11
files_x11 = sys/x11/xscreensaver.c sys/x11/xkb.c sys/x11/xv.c
cflags_x11 = -DUSE_X11
endif

if USE_ALSA
files_alsa = sys/alsa/mixer-alsa.c sys/alsa/midi-alsa.c
if USE_ALSA_DLTRICK
cflags_alsa=-DUSE_ALSA -DUSE_DLTRICK_ALSA
else
cflags_alsa=-DUSE_ALSA
# automake complained about LIBOBJS too
#   /storlek
#LIBOBJS+=-lasound
lib_asound=-lasound
endif
endif

if USE_OSS
files_oss = sys/oss/mixer-oss.c sys/oss/midi-oss.c
cflags_oss=-DUSE_OSS
endif

if USE_MMAP
files_mmap = sys/posix/slurp-mmap.c
endif

if USE_WIN32MM
files_win32mm = sys/win32/modkey.c sys/win32/slurp-win32.c sys/win32/mixer-win32mm.c sys/win32/midi-win32mm.c
cflags_win32mm=-DUSE_WIN32MM
#LIBOBJS+=-lwinmm
lib_win32mm=-lwinmm
endif
if USE_WINDRES
# Hope this works, I can't test it -storlek
# If not, try uncommetning the next line --
#lib_windres=schismres.o
# and then add $(lib_windres) under schismtracker_LDADD below
%.o: %.rc
	$(WINDRES) -I@srcdir@ -i $< -o $@
files_windres=sys/win32/schismres.rc
endif

if USE_MACOSX
files_macosx=sys/macosx/macosx-sdlmain.m sys/macosx/ibook-support.c sys/macosx/midi-macosx.c
endif

if USE_NETWORK
cflags_network=-DUSE_NETWORK
endif

if NEED_GNU_ASPRINTF
files_gnu_asprintf=schism/asprintf.c
endif
if NEED_GNU_VASPRINTF
files_gnu_vasprintf=schism/vasprintf.c
endif
if NEED_GNU_REALPATH
files_gnu_realpath=schism/realpath.c
endif
if NEED_GNU_MEMCMP
files_gnu_memcmp=schism/memcmp.c
endif

## aaaaaaaaahhhhhhhhhhhhhhhhhhh!!!!!!!1
schismtracker_SOURCES = \
	schism/page_blank.c		\
	schism/charset.c		\
	schism/sample-edit.c		\
	schism/dmoz.c			\
	schism/frag-opt.c		\
	schism/page_info.c		\
	schism/page.c			\
	schism/page_palette.c		\
	schism/page_instruments.c	\
	schism/page_log.c		\
	schism/page_about.c		\
	schism/pattern-view.c		\
	schism/xpmdata.c		\
	schism/menu.c			\
	schism/mixer-core.c		\
	schism/diskwriter.cc		\
	schism/page_loadinst.c		\
	schism/mplink.cc		\
	schism/clippy.c			\
	schism/page_loadmodule.c	\
	schism/page_vars.c		\
	schism/fmt/raw.c		\
	schism/fmt/liq.c		\
	schism/fmt/f2r.c		\
	schism/fmt/s3m.c		\
	schism/fmt/mod.c		\
	schism/fmt/far.c		\
	schism/fmt/wav.cc		\
	schism/fmt/mid.cc		\
	schism/fmt/its.cc		\
	schism/fmt/imf.c		\
	schism/fmt/mtm.c		\
	schism/fmt/ult.c		\
	schism/fmt/ams.c		\
	schism/fmt/it.c			\
	schism/fmt/stm.c		\
	schism/fmt/mdl.c		\
	schism/fmt/669.c		\
	schism/fmt/au.c			\
	schism/fmt/xm.c			\
	schism/fmt/mt2.c		\
	schism/fmt/ntk.c		\
	schism/fmt/dtm.c		\
	schism/fmt/aiff.c		\
	schism/util.c			\
	schism/page_midi.c		\
	schism/diskwriter_dialog.c	\
	schism/draw-char.c		\
	schism/helptext.c		\
	schism/page_help.c		\
	schism/slurp.c			\
	schism/widget-keyhandler.c	\
	schism/main.c			\
	schism/page_midiout.c		\
	schism/page_message.c		\
	schism/page_loadsample.c	\
	schism/dialog.c			\
	schism/page_preferences.c	\
	schism/widget.c			\
	schism/config.c			\
	schism/status.c			\
	schism/video.c			\
	schism/sample-view.c		\
	schism/page_patedit.c		\
	schism/page_config.c		\
	schism/draw-pixel.c		\
	schism/keyboard.c		\
	schism/page_samples.c		\
	schism/itf.c			\
	schism/page_orderpan.c		\
	schism/midi-core.c		\
	schism/midi-ip.c		\
	schism/audio_playback.cc	\
	schism/config-parser.c		\
	schism/audio_loadsave.cc	\
	schism/draw-misc.c		\
	schism/fakemem.c		\
	schism/version.c		\
	modplug/load_xm.cpp		\
	modplug/load_psm.cpp		\
	modplug/load_s3m.cpp		\
	modplug/tables.cpp		\
	modplug/load_ult.cpp		\
	modplug/load_669.cpp		\
	modplug/load_stm.cpp		\
	modplug/load_far.cpp		\
	modplug/load_it.cpp		\
	modplug/load_mid.cpp		\
	modplug/load_ams.cpp		\
	modplug/snd_fx.cpp		\
	modplug/load_mod.cpp		\
	modplug/snd_flt.cpp		\
	modplug/load_wav.cpp		\
	modplug/load_dsm.cpp		\
	modplug/load_umx.cpp		\
	modplug/snd_dsp.cpp		\
	modplug/load_mt2.cpp		\
	modplug/snd_eq.cpp		\
	modplug/load_amf.cpp		\
	modplug/load_dmf.cpp		\
	modplug/sndfile.cpp		\
	modplug/load_okt.cpp		\
	modplug/load_mtm.cpp		\
	modplug/load_med.cpp		\
	modplug/load_dbm.cpp		\
	modplug/sndmix.cpp		\
	modplug/load_mdl.cpp		\
	modplug/load_ptm.cpp		\
	modplug/mmcmp.cpp		\
	modplug/fastmix.cpp		\
	$(files_macosx)			\
	$(files_alsa)			\
	$(files_oss)			\
	$(files_win32mm)		\
	$(files_x11)			\
	$(files_gnu_asprintf)		\
	$(files_gnu_vasprintf)		\
	$(files_gnu_realpath)		\
	$(files_gnu_memcmp)		\
	$(files_mmap)			\
	$(files_windres)

INCLUDES = -D_USE_AUTOCONF -D_GNU_SOURCE \
	-I$(srcdir)/include -I$(srcdir)/modplug -I. \
	@SDL_CFLAGS@ $(cflags_alsa) $(cflags_oss) $(cflags_win32mm) \
	$(cflags_network) $(cflags_x11)


schismtracker_DEPENDENCIES = $(files_windres)
schismtracker_LDADD = $(lib_asound) $(lib_win32mm) \
	@SDL_LIBS@ @LIBM@

## cvs snapshot distribution
## package it under directory "schism-cvs-snapshot/"
## total hack, but worksforme -storlek
snapdir = schism-cvs-snapshot
snapshot: distdir
	mv $(distdir) $(snapdir)
	tardir=$(snapdir) && $(am__tar) | bzip2 -9 -c >$(snapdir).tar.bz2
	mv $(snapdir) $(distdir)
	$(am__remove_distdir)
	test "$(USER)" == storlek && scp $(snapdir).tar.bz2 pair:public_html/schism/dl
