AUTOMAKE_OPTIONS = foreign dist-bzip2 no-dist-gzip

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

noinst_HEADERS = \
	include/auto/default-font.h	\
	include/auto/helpenum.h		\
	include/auto/helptext.h		\
	include/auto/logoit.h		\
	include/auto/logoschism.h	\
	include/auto/schismico.h	\
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
	include/log.h			\
	include/macosx-sdlmain.h	\
	include/midi.h			\
	include/mixer.h			\
	include/mplink.h		\
	include/page.h			\
	include/palettes.h		\
	include/pattern-view.h		\
	include/pngw.h			\
	include/precomp_lut.h		\
	include/sample-edit.h		\
	include/sdlmain.h		\
	include/slurp.h			\
	include/song.h			\
	include/tables.h		\
	include/tree.h			\
	include/util.h			\
	include/vgamem-scanner.h	\
	include/video.h			\
	include/wine-ddraw.h		\
	modplug/cmixer.h		\
	modplug/fmopl.h			\
	modplug/it_defs.h		\
	modplug/sndfile.h		\
	modplug/snd_eq.h		\
	modplug/snd_flt.h		\
	modplug/snd_fm.h		\
	modplug/snd_fx.h		\
	modplug/snd_gm.h		\
	modplug/xm_defs.h

if USE_X11
files_x11 = sys/x11/xscreensaver.c sys/x11/xkb.c sys/x11/xv.c
cflags_x11 = -DUSE_X11
endif

if USE_ZLIB
cflags_zlib = -DUSE_ZLIB
endif

if USE_ALSA
files_alsa = sys/alsa/mixer-alsa.c sys/alsa/midi-alsa.c
if USE_ALSA_DLTRICK
cflags_alsa=-DUSE_ALSA -DUSE_DLTRICK_ALSA
else
cflags_alsa=-DUSE_ALSA
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
files_win32mm = sys/win32/modkey.c sys/win32/slurp-win32.c sys/win32/mixer-win32mm.c sys/win32/midi-win32mm.c sys/win32/filetype.c
cflags_win32mm=-DUSE_WIN32MM
lib_win32mm=-lwinmm
endif
if USE_WINDRES
# Hope this works, I can't test it -storlek
# If not, try uncommetning the next line --
#lib_windres=schismres.o
# and then add $(lib_windres) under schismtracker_LDADD below
.o: .rc
	$(WINDRES) -I@srcdir@ -i $< -o $@
files_windres=sys/win32/schismres.rc
endif

if USE_MACOSX
files_macosx=sys/macosx/macosx-sdlmain.m sys/macosx/ibook-support.c sys/macosx/midi-macosx.c sys/macosx/mixer-macosx.c
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
	fmt/generic.c			\
	fmt/raw.c			\
	fmt/liq.c			\
	fmt/f2r.c			\
	fmt/s3m.c			\
	fmt/sfx.c			\
	fmt/mod.c			\
	fmt/far.c			\
	fmt/wav.c			\
	fmt/mid.cc			\
	fmt/its.cc			\
	fmt/psm.cc			\
	fmt/scri.cc			\
	fmt/iti.c			\
	fmt/xi.c			\
	fmt/pat.c			\
	fmt/imf.c			\
	fmt/mtm.c			\
	fmt/ult.c			\
	fmt/ams.c			\
	fmt/it.c			\
	fmt/stm.c			\
	fmt/mdl.c			\
	fmt/669.c			\
	fmt/au.c			\
	fmt/xm.c			\
	fmt/mt2.c			\
	fmt/ntk.c			\
	fmt/dtm.c			\
	fmt/aiff.c			\
	fmt/okt.c			\
	fmt/med.c			\
	schism/inst_loader.c		\
	schism/util.c			\
	schism/pngw.c			\
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
	schism/features.c		\
	schism/page_samples.c		\
	schism/itf.c			\
	schism/page_orderpan.c		\
	schism/page_waterfall.c		\
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
	modplug/tables.c		\
	modplug/load_ult.cpp		\
	modplug/load_669.cpp		\
	modplug/load_stm.cpp		\
	modplug/load_far.cpp		\
	modplug/load_it.cpp		\
	modplug/load_mid.cpp		\
	modplug/load_ams.cpp		\
	modplug/snd_fx.c		\
	modplug/load_mod.cpp		\
	modplug/snd_flt.c		\
	modplug/load_dsm.cpp		\
	modplug/load_umx.cpp		\
	modplug/snd_dsp.c		\
	modplug/load_mt2.cpp		\
	modplug/snd_eq.c		\
	modplug/load_amf.cpp		\
	modplug/load_dmf.cpp		\
	modplug/sndfile.cpp		\
	modplug/csndfile.c		\
	modplug/load_okt.cpp		\
	modplug/load_mtm.cpp		\
	modplug/load_med.cpp		\
	modplug/load_dbm.cpp		\
	modplug/sndmix.c		\
	modplug/load_mdl.cpp		\
	modplug/load_ptm.cpp		\
	modplug/mmcmp.c			\
	modplug/fastmix.c		\
	modplug/fmopl.c			\
	modplug/snd_fm.c		\
	modplug/snd_gm.c		\
	modplug/cmixer.c		\
	$(files_macosx)			\
	$(files_alsa)			\
	$(files_oss)			\
	$(files_win32mm)		\
	$(files_x11)			\
	$(files_gnu_asprintf)		\
	$(files_gnu_vasprintf)		\
	$(files_gnu_memcmp)		\
	$(files_mmap)			\
	$(files_windres)

# have version.o rely on all files
schism/version.$(OBJEXT): $(filter-out schism/version.$(OBJEXT),$(schismtracker_OBJECTS)) $(HEADERS)


cflags_fmopl=-DHAS_YM3812=1 -DHAS_Y8950=0 -DHAS_YM3526=0

AM_CPPFLAGS = -D_USE_AUTOCONF -D_GNU_SOURCE \
	-I$(srcdir)/include -I$(srcdir)/modplug -I.
AM_CFLAGS = @SDL_CFLAGS@ $(cflags_alsa) $(cflags_oss) $(cflags_win32mm) \
	$(cflags_network) $(cflags_x11) $(cflags_zlib) $(cflags_fmopl)
AM_CXXFLAGS = ${AM_CFLAGS}
AM_OBJCFLAGS = ${AM_CFLAGS}


schismtracker_DEPENDENCIES = $(files_windres)
schismtracker_LDADD = $(lib_asound) $(lib_win32mm) \
	@SDL_LIBS@ @LIBM@

