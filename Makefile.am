## vi:set bd=syn\ make: for elvis

AUTOMAKE_OPTIONS = foreign dist-bzip2 no-dist-gzip

## NOTE: helptexts should be in the same order as the enum in page.h
helptexts = \
	helptext/global-keys		\
	helptext/copyright		\
	helptext/info-page		\
	helptext/instrument-list	\
	helptext/message-editor		\
	helptext/midi-output		\
	helptext/orderlist-pan		\
	helptext/orderlist-vol		\
	helptext/pattern-editor		\
	helptext/adlib-sample		\
	helptext/sample-list
fonts = \
	font/default-lower.fnt		\
	font/default-upper-alt.fnt	\
	font/default-upper-itf.fnt	\
	font/half-width.fnt
icons = \
	icons/appIcon.icns		\
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
	icons/schism-itf-icon-128.png	\
	icons/schism-itf-icon-16.png	\
	icons/schism-itf-icon-192.png	\
	icons/schism-itf-icon-22.png	\
	icons/schism-itf-icon-24.png	\
	icons/schism-itf-icon-32.png	\
	icons/schism-itf-icon-36.png	\
	icons/schism-itf-icon-48.png	\
	icons/schism-itf-icon-64.png	\
	icons/schism-itf-icon-72.png	\
	icons/schism-itf-icon-96.png	\
	icons/schism-itf-icon.svg	\
	icons/it_logo.png		\
	icons/schism_logo.png		\
	icons/schismres.ico
sysfiles = \
	sys/fd.org/autopackage.apspec	\
	sys/fd.org/itf.desktop		\
	sys/fd.org/schism.desktop	\
	sys/macosx/Schism_Tracker.app/Contents/Info.plist			\
	sys/macosx/Schism_Tracker.app/Contents/PkgInfo				\
	sys/macosx/Schism_Tracker.app/Contents/Resources/AppSettings.plist	\
	sys/macosx/Schism_Tracker.app/Contents/Resources/appIcon.icns		\
	sys/macosx/Schism_Tracker.app/Contents/Resources/moduleIcon.icns	\
	sys/win32/schism.nsis		\
	sys/wii/schismtracker/icon.png	\
	sys/wii/schismtracker/meta.xml	\
	sys/sdl/README
scripts = \
	scripts/bin2h.sh		\
	scripts/build-font.sh		\
	scripts/genhelp.py		\
	scripts/itf2half.py		\
	scripts/half2itf.py		\
	scripts/itcfg.py		\
	scripts/itmidicfg.py		\
	scripts/lutgen.c		\
	scripts/palette.py

EXTRA_DIST = \
	include/auto/README		\
	$(helptexts)			\
	$(fonts)			\
	$(icons)			\
	$(sysfiles)			\
	$(scripts)

bin_PROGRAMS = schismtracker

noinst_HEADERS = \
	include/auto/logoit.h		\
	include/auto/logoschism.h	\
	include/auto/schismico.h	\
	include/charset.h		\
	include/clippy.h		\
	include/cmixer.h		\
	include/config-parser.h		\
	include/disko.h			\
	include/dmoz.h			\
	include/draw-char.h		\
	include/event.h			\
	include/fmopl2.h			\
	include/fmopl3.h			\
	include/fmt.h			\
	include/fmt-types.h		\
	include/headers.h		\
	include/it_defs.h		\
	include/it.h			\
	include/log.h			\
	include/midi.h			\
	include/osdefs.h		\
	include/page.h			\
	include/pattern-view.h		\
	include/precomp_lut.h		\
	include/sample-edit.h		\
	include/sdlmain.h		\
	include/slurp.h			\
	include/sndfile.h		\
	include/snd_fm.h		\
	include/snd_gm.h		\
	include/song.h			\
	include/tables.h		\
	include/tree.h			\
	include/util.h			\
	include/version.h		\
	include/vgamem-scanner.h	\
	include/video.h			\
	sys/wii/isfs.h			\
	sys/wii/certs_bin.h		\
	sys/wii/su_tik_bin.h		\
	sys/wii/su_tmd_bin.h		\
	sys/win32/wine-ddraw.h

dist_man_MANS = sys/posix/schismtracker.1

desktopdir = $(datadir)/applications
desktop_DATA = sys/fd.org/schism.desktop

appicondir = $(datadir)/pixmaps
appicon_DATA= icons/schism-icon-128.png

noinst_SCRIPTS = $(scripts)

nodist_schismtracker_SOURCES = \
	auto/default-font.c		\
	auto/helptext.c
CLEANFILES = \
	auto/default-font.c		\
	auto/helptext.c

auto/default-font.c: Makefile.am scripts/bin2h.sh scripts/build-font.sh $(fonts)
	sh $(srcdir)/scripts/build-font.sh $(srcdir) $(fonts) >$@
auto/helptext.c: Makefile.am scripts/genhelp.py $(helptexts)
	$(PYTHON) $(srcdir)/scripts/genhelp.py $(srcdir) $(helptexts) >$@

if USE_X11
files_x11 = sys/x11/xscreensaver.c sys/x11/xkb.c
cflags_x11 = -DUSE_X11
if USE_XV
files_x11 += sys/x11/xv.c
cflags_x11 += -DUSE_XV
endif
endif

if USE_ALSA
files_alsa = sys/alsa/init.c sys/alsa/volume-alsa.c sys/alsa/midi-alsa.c
if USE_ALSA_DLTRICK
cflags_alsa=-DUSE_ALSA -DUSE_DLTRICK_ALSA
else
cflags_alsa=-DUSE_ALSA
lib_asound=-lasound
endif
endif

if USE_OSS
files_oss = sys/oss/volume-oss.c sys/oss/midi-oss.c
cflags_oss=-DUSE_OSS
endif

if USE_MMAP
files_mmap = sys/posix/slurp-mmap.c
endif

if USE_WIN32
files_win32 = \
	sys/win32/osdefs.c		\
	sys/win32/slurp-win32.c		\
	sys/win32/volume-win32mm.c	\
	sys/win32/midi-win32mm.c	\
	sys/win32/filetype.c		\
	sys/win32/localtime_r.c
cflags_win32=-I$(srcdir)/sys/win32
lib_win32=-lwinmm
endif

if HAVE_WINDRES

## use today's date if we didn't get a commit date from git
if HAVE_GIT
wrcflags_version = -DWRC_VERSION=0,`echo '$(PACKAGE_VERSION)' | sed 's/\(....\)\(..\)\(..\).*/\1,\2,\3/'`
else
wrcflags_version = -DWRC_VERSION=0,`date +%Y%m%d | sed 's/\(....\)\(..\)\(..\).*/\1,\2,\3/'`
endif

## --use-temp-file is needed to work around stupid bugs
WRCFLAGS = --use-temp-file -I. -I$(srcdir) $(cflags_version) $(wrcflags_version)
.rc.$(OBJEXT):
	$(WINDRES) $(WRCFLAGS) -i $< -o $@
files_windres=sys/win32/schismres.rc
sys/win32/schismres.$(OBJEXT): icons/schismres.ico config.h Makefile.am
endif HAVE_WINDRES

if USE_WII
files_wii=sys/wii/isfs.c sys/wii/osdefs.c
cflags_wii=-I$(srcdir)/sys/wii
endif

if USE_MACOSX
files_macosx = \
	sys/macosx/macosx-sdlmain.m	\
	sys/macosx/ibook-support.c	\
	sys/macosx/midi-macosx.c	\
	sys/macosx/volume-macosx.c	\
	sys/macosx/osdefs.c
cflags_macosx=-mmacosx-version-min=10.9
endif

if USE_NETWORK
cflags_network=-DUSE_NETWORK
endif


## Replacement functions for crappy systems
files_stdlib =
if NEED_ASPRINTF
files_stdlib += sys/stdlib/asprintf.c
endif
if NEED_VASPRINTF
files_stdlib += sys/stdlib/vasprintf.c
endif
if NEED_MEMCMP
files_stdlib += sys/stdlib/memcmp.c
endif
if NEED_STRPTIME
files_stdlib += sys/stdlib/strptime.c
endif
if NEED_MKSTEMP
files_stdlib += sys/stdlib/mkstemp.c
endif


## aaaaaaaaahhhhhhhhhhhhhhhhhhh!!!!!!!1
schismtracker_SOURCES = \
	schism/page_blank.c		\
	schism/charset.c		\
	schism/sample-edit.c		\
	schism/dmoz.c			\
	schism/page_info.c		\
	schism/page.c			\
	schism/page_palette.c		\
	schism/page_instruments.c	\
	schism/page_log.c		\
	schism/page_about.c		\
	schism/pattern-view.c		\
	schism/xpmdata.c		\
	schism/menu.c			\
	schism/volume-core.c		\
	schism/disko.c			\
	schism/page_loadinst.c		\
	schism/mplink.c			\
	schism/clippy.c			\
	schism/page_loadmodule.c	\
	schism/page_vars.c		\
	schism/palettes.c		\
	fmt/compression.c		\
	fmt/generic.c			\
	fmt/mmcmp.c			\
	fmt/669.c			\
	fmt/aiff.c			\
	fmt/ams.c			\
	fmt/au.c			\
	fmt/f2r.c			\
	fmt/far.c			\
	fmt/imf.c			\
	fmt/it.c			\
	fmt/iti.c			\
	fmt/its.c			\
	fmt/liq.c			\
	fmt/mdl.c			\
	fmt/med.c			\
	fmt/mf.c			\
	fmt/mid.c                       \
	fmt/mod.c			\
	fmt/mt2.c			\
	fmt/mtm.c			\
	fmt/mus.c			\
	fmt/ntk.c			\
	fmt/okt.c			\
	fmt/pat.c			\
	fmt/raw.c			\
	fmt/s3i.c			\
	fmt/s3m.c			\
	fmt/sfx.c			\
	fmt/stm.c			\
	fmt/ult.c			\
	fmt/wav.c			\
	fmt/xi.c			\
	fmt/xm.c			\
	schism/util.c			\
	schism/page_midi.c		\
	schism/draw-char.c		\
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
	schism/keyboard.c		\
	schism/page_samples.c		\
	schism/itf.c			\
	schism/page_orderpan.c		\
	schism/page_waterfall.c		\
	schism/midi-core.c		\
	schism/midi-ip.c		\
	schism/audio_playback.c		\
	schism/config-parser.c		\
	schism/audio_loadsave.c		\
	schism/draw-misc.c		\
	schism/fakemem.c		\
	schism/version.c		\
	player/csndfile.c		\
	player/mixutil.c		\
	player/equalizer.c		\
	player/mixer.c			\
	player/filters.c		\
	player/fmopl2.c			\
	player/fmopl3.c			\
	player/fmpatches.c		\
	player/sndmix.c			\
	player/opl-util.c		\
	player/snd_fm.c			\
	player/effects.c		\
	player/snd_gm.c			\
	player/tables.c			\
	$(files_macosx)			\
	$(files_alsa)			\
	$(files_oss)			\
	$(files_win32)			\
	$(files_x11)			\
	$(files_stdlib)			\
	$(files_mmap)			\
	$(files_wii)			\
	$(files_windres)

# have version.o rely on all files
schism/version.$(OBJEXT): $(filter-out schism/version.$(OBJEXT),$(schismtracker_OBJECTS)) $(HEADERS)

cflags_fmopl=-DHAS_YM3812=1 -DHAS_Y8950=0 -DHAS_YM3526=0

AM_CPPFLAGS = -D_USE_AUTOCONF -D_GNU_SOURCE -I$(srcdir)/include -I.
AM_CFLAGS = $(SDL_CFLAGS) $(cflags_alsa) $(cflags_oss) \
	$(cflags_network) $(cflags_x11) $(cflags_fmopl) \
	$(cflags_version) $(cflags_win32) $(cflags_wii) \
	$(cflags_macosx)
AM_OBJCFLAGS = $(AM_CFLAGS)

schismtracker_DEPENDENCIES = $(files_windres)
schismtracker_LDADD = $(lib_asound) $(lib_win32) $(SDL_LIBS) $(LIBM)
