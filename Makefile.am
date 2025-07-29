## vi:set bd=syn\ make: for elvis

AUTOMAKE_OPTIONS = foreign dist-bzip2 no-dist-gzip
AM_CFLAGS = $(project_CFLAGS)
AM_OBJCFLAGS = $(project_OBJCFLAGS)

## NOTE: helptexts should be in the same order as the enum in page.h
docs = \
	docs/building_on_linux.md	\
	docs/building_on_osx.md		\
	docs/building_on_windows.md	\
	docs/configuration.md
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
	helptext/sample-list		\
	helptext/palettes			\
	helptext/time-information
fonts = \
	font/default-lower.fnt		\
	font/default-upper-alt.fnt	\
	font/default-upper-itf.fnt	\
	font/half-width.fnt			\
	font/hiragana.fnt			\
	font/extended-latin.fnt		\
	font/greek.fnt \
	font/cp866.fnt
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
	icons/schism_logo.png		\
	icons/schismres.ico
sysfiles = \
	sys/fd.org/autopackage.apspec	\
	sys/fd.org/schism.desktop	\
	sys/macosx/Schism_Tracker.app/Contents/Info.plist			\
	sys/macosx/Schism_Tracker.app/Contents/PkgInfo				\
	sys/macosx/Schism_Tracker.app/Contents/Resources/appIcon.icns		\
	sys/macosx/Schism_Tracker.app/Contents/Resources/AppSettings.plist	\
	sys/macosx/Schism_Tracker.app/Contents/Resources/moduleIcon.icns	\
	sys/wii/schismtracker/icon.png	\
	sys/wii/schismtracker/meta.xml	\
	sys/win32/schism.nsis
scripts = \
	scripts/bin2h.sh		\
	scripts/build-font.sh		\
	scripts/genhelp.pl		\
	scripts/half2itf.py		\
	scripts/itcfg.py		\
	scripts/itf2half.py		\
	scripts/itmidicfg.py		\
	scripts/lutgen.c		\
	scripts/palette.py

EXTRA_DIST = \
	README.md include/auto/README	\
	$(docs)	\
	$(helptexts)			\
	$(fonts)			\
	$(icons)			\
	$(sysfiles)			\
	$(scripts)

bin_PROGRAMS = schismtracker
if BUILD_TESTS
bin_PROGRAMS += schismtrackertest
endif

noinst_HEADERS = \
	include/auto/logoit.h		\
	include/auto/logoschism.h	\
	include/auto/schismico_hires.h	\
	include/test.h     \
	include/bswap.h			\
	include/bshift.h        \
	include/charset.h		\
	include/clippy.h		\
	include/config-parser.h		\
	include/config.h        \
	include/disko.h			\
	include/dialog.h        \
	include/dmoz.h			\
	include/events.h		\
	include/fakemem.h       \
	include/fonts.h         \
	include/fmt-types.h		\
	include/fmt.h			\
	include/headers.h		\
	include/ieee-float.h    \
	include/it.h			\
	include/keyboard.h      \
	include/log.h			\
	include/loadso.h        \
	include/midi.h			\
	include/mem.h           \
	include/osdefs.h		\
	include/page.h			\
	include/palettes.h          \
	include/pattern-view.h		\
	include/sample-edit.h		\
	include/slurp.h			\
	include/song.h			\
	include/str.h           \
	include/mt.h       \
	include/timer.h         \
	include/test-assertions.h       \
	include/test-funcs.h			\
	include/tree.h			\
	include/util.h			\
	include/version.h		\
	include/vgamem.h      \
	include/video.h			\
	include/widget.h        \
	include/player/cmixer.h		\
	include/player/fmopl.h			\
	include/player/precomp_lut.h		\
	include/player/snd_fm.h		\
	include/player/snd_gm.h		\
	include/player/sndfile.h		\
	include/player/tables.h		\
	include/backend/audio.h     \
	include/backend/clippy.h     \
	include/backend/dmoz.h       \
	include/backend/events.h     \
	include/backend/mt.h     \
	include/backend/timer.h     \
	include/backend/video.h     \
	sys/wii/certs_bin.h		\
	sys/wii/isfs.h			\
	sys/wii/su_tik_bin.h		\
	sys/macos/dirent/macos-dirent.h \
	sys/wii/su_tmd_bin.h        \
	sys/sdl12/init.h \
	sys/sdl2/init.h \
	sys/x11/init.h \
	sys/win32/win32-vista.h \
	sys/win32/audio-asio-vtable.h \
	sys/win32/audio-asio.h

dist_man_MANS = sys/posix/schismtracker.1

desktopdir = $(datadir)/applications
desktop_DATA = sys/fd.org/schism.desktop

appicondir = $(datadir)/pixmaps
appicon_DATA= icons/schism-icon-128.png

noinst_SCRIPTS = $(scripts)

CLEANFILES = \
	auto/default-font.c	\
	auto/helptext.c

auto/default-font.c: Makefile.am scripts/bin2h.sh scripts/build-font.sh $(fonts)
	${MKDIR_P} auto
	sh $(srcdir)/scripts/build-font.sh $(srcdir) $(fonts) >$@

auto/helptext.c: Makefile.am scripts/genhelp.pl $(helptexts)
if HAVE_PERL
	${MKDIR_P} auto
	$(PERL) $(srcdir)/scripts/genhelp.pl $(srcdir) $(helptexts) >$@
else
	@echo "*** perl is required to regenerate $(@) ***"; exit 1;
endif

if USE_SDL3
# hax: we want to use specific CFLAGS...
sys/sdl3/schismtracker-audio.$(OBJEXT): CFLAGS += $(SDL3_CFLAGS)
sys/sdl3/schismtracker-dmoz.$(OBJEXT): CFLAGS += $(SDL3_CFLAGS)
sys/sdl3/schismtracker-clippy.$(OBJEXT): CFLAGS += $(SDL3_CFLAGS)
sys/sdl3/schismtracker-events.$(OBJEXT): CFLAGS += $(SDL3_CFLAGS)
sys/sdl3/schismtracker-init.$(OBJEXT): CFLAGS += $(SDL3_CFLAGS)
sys/sdl3/schismtracker-timer.$(OBJEXT): CFLAGS += $(SDL3_CFLAGS)
sys/sdl3/schismtracker-video.$(OBJEXT): CFLAGS += $(SDL3_CFLAGS)
sys/sdl3/schismtracker-mt.$(OBJEXT): CFLAGS += $(SDL3_CFLAGS)

files_sdl3 = sys/sdl3/audio.c sys/sdl3/dmoz.c sys/sdl3/clippy.c sys/sdl3/events.c sys/sdl3/init.c sys/sdl3/timer.c sys/sdl3/video.c sys/sdl3/mt.c

if LINK_TO_SDL3
libs_sdl3 = $(SDL3_LIBS)
endif

endif

if USE_SDL2
# hax: we want to use specific CFLAGS...
sys/sdl2/schismtracker-audio.$(OBJEXT): CFLAGS += $(SDL2_CFLAGS)
sys/sdl2/schismtracker-dmoz.$(OBJEXT): CFLAGS += $(SDL2_CFLAGS)
sys/sdl2/schismtracker-clippy.$(OBJEXT): CFLAGS += $(SDL2_CFLAGS)
sys/sdl2/schismtracker-events.$(OBJEXT): CFLAGS += $(SDL2_CFLAGS)
sys/sdl2/schismtracker-init.$(OBJEXT): CFLAGS += $(SDL2_CFLAGS)
sys/sdl2/schismtracker-timer.$(OBJEXT): CFLAGS += $(SDL2_CFLAGS)
sys/sdl2/schismtracker-video.$(OBJEXT): CFLAGS += $(SDL2_CFLAGS)
sys/sdl2/schismtracker-mt.$(OBJEXT): CFLAGS += $(SDL2_CFLAGS)

files_sdl2 = sys/sdl2/audio.c sys/sdl2/dmoz.c sys/sdl2/clippy.c sys/sdl2/events.c sys/sdl2/init.c sys/sdl2/timer.c sys/sdl2/video.c sys/sdl2/mt.c

if LINK_TO_SDL2
libs_sdl2 = $(SDL2_LIBS)
endif

endif

if USE_SDL12
sys/sdl12/schismtracker-audio.$(OBJEXT): CFLAGS += $(SDL12_CFLAGS)
sys/sdl12/schismtracker-events.$(OBJEXT): CFLAGS += $(SDL12_CFLAGS)
sys/sdl12/schismtracker-init.$(OBJEXT): CFLAGS += $(SDL12_CFLAGS)
sys/sdl12/schismtracker-timer.$(OBJEXT): CFLAGS += $(SDL12_CFLAGS)
sys/sdl12/schismtracker-video.$(OBJEXT): CFLAGS += $(SDL12_CFLAGS)
sys/sdl12/schismtracker-mt.$(OBJEXT): CFLAGS += $(SDL12_CFLAGS)

files_sdl12 = sys/sdl12/audio.c sys/sdl12/events.c sys/sdl12/init.c sys/sdl12/timer.c sys/sdl12/video.c sys/sdl12/mt.c

if LINK_TO_SDL12
libs_sdl12 = $(SDL12_LIBS)
endif

endif

if USE_ALSA
files_alsa = sys/alsa/midi-alsa.c

if LINK_TO_ALSA
lib_asound=-lasound
endif

endif

if USE_OSS
files_oss = sys/oss/midi-oss.c
endif

if USE_MMAP
files_mmap = sys/posix/slurp-mmap.c
endif

if USE_X11
files_x11 = sys/x11/clippy.c sys/x11/init.c sys/x11/events.c
cflags_x11 = -Isys/x11 $(X_CFLAGS)

if LINK_TO_X11
libs_x11 = $(X_LIBS) -lX11
endif

endif

if USE_WIN32
sys/win32/schismtracker-audio-dsound.$(OBJEXT): CFLAGS += $(DSOUND_CFLAGS)
files_win32 = \
	sys/win32/filetype.c \
	sys/win32/midi-win32mm.c	\
	sys/win32/osdefs.c		\
	sys/win32/slurp-win32.c \
	sys/win32/dmoz.c \
	sys/win32/clippy.c \
	sys/win32/timer.c \
	sys/win32/audio-waveout.c \
	sys/win32/audio-dsound.c \
	sys/win32/mt.c
cflags_win32=-I$(srcdir)/sys/win32
lib_win32=-lwinmm -lgdi32 $(DSOUND_LIBS)

if USE_MEDIAFOUNDATION
fmt/schismtracker-win32mf.$(OBJEXT): CFLAGS += $(MEDIAFOUNDATION_CFLAGS)

files_mediafoundation=fmt/win32mf.c
lib_mediafoundation=-luuid $(MEDIAFOUNDATION_LIBS)
endif

if USE_ASIO
files_asio = \
	sys/win32/audio-asio.c
cflags_asio =
lib_asio =
endif

cflags_win32 += -mwindows

if HAVE_WINDRES

## use today's date if we didn't get a commit date from git
if HAVE_GIT
wrcflags_version = -DWRC_VERSION=0,`echo '$(PACKAGE_VERSION)' | sed 's/\(....\)\(..\)\(..\).*/\1,\2,\3/' | sed 's/,0\+/,/g'`
else
wrcflags_version = -DWRC_VERSION=0,`date +%Y%m%d | sed 's/\(....\)\(..\)\(..\).*/\1,\2,\3/' | sed 's/,0\+/,/g'`
endif # HAVE_GIT

## --use-temp-file is needed to work around stupid bugs
WRCFLAGS = --use-temp-file -I. -I$(srcdir) $(cflags_version) $(wrcflags_version)
.rc.$(OBJEXT):
	$(WINDRES) $(WRCFLAGS) -i $< -o $@
files_windres=sys/win32/schismres.rc
sys/win32/schismres.$(OBJEXT): icons/schismres.ico build-config.h Makefile.am
endif # HAVE_WINDRES

endif # USE_WIN32

if USE_OS2

files_os2 = sys/os2/osdefs.c
cflags_os2=
libs_os2=mmpm2.lib libuls.lib libconv.lib

endif # USE_OS2

if USE_WII
files_wii=sys/wii/isfs.c sys/wii/osdefs.c
cflags_wii=-mrvl -mcpu=750 -meabi -mhard-float
cppflags_wii=-I$(srcdir)/sys/wii $(WII_CPPFLAGS)
libs_wii=$(wii_machdep) $(WII_LDFLAGS)
endif

if USE_WIIU
files_wiiu=sys/wiiu/osdefs.c
cflags_wiiu=-mcpu=750 -meabi -mhard-float
cppflags_wiiu=$(WIIU_CPPFLAGS)
libs_wiiu=$(WIIU_LDFLAGS)

# Wii U specific build rules for wuhb crap
schismtracker.wuhb: schismtracker.rpx
schismtracker.rpx: schismtracker.elf

.rpx.wuhb:
	$(WUHBTOOL) --name "Schism Tracker" --author "Storlek, Mrs. Brisby, et al." --icon=$(srcdir)/icons/schism-icon-128.png $< $@

.elf.rpx:
	$(ELF2RPL) $< $@
endif

if USE_XBOX
files_xbox = \
	sys/xbox/osdefs.c \
	sys/win32/mt.c
cflags_xbox =
libs_xbox =
endif

if USE_MACOSX
files_macosx = \
	sys/macosx/macosx-sdlmain.m	\
	sys/macosx/ibook-support.c	\
	sys/macosx/midi-macosx.c	\
	sys/macosx/osdefs.m \
	sys/macosx/clippy.m \
	sys/macosx/dmoz.m \
	sys/macosx/audio.c \
	fmt/macosxca.c
cflags_macosx=
libs_macosx=
ldflags_macosx=-framework Cocoa \
	-framework CoreAudio \
	-framework CoreMIDI \
	-framework IOKit \
	-framework AudioUnit \
	-framework AudioToolbox
endif

if USE_MACOS
files_macos = \
	sys/macos/audio-sndmgr.c \
	sys/macos/osdefs.c \
	sys/macos/dirent/macos-dirent.c \
	sys/macos/mt.c \
	sys/macos/dmoz.c \
	sys/macos/clippy.c
cflags_macos=-I$(top_srcdir)/sys/macos/dirent
libs_macos= \
	-lAppearanceLib \
	-lInputSprocketLib \
	-lControlStripLib \
	-lControlsLib \
	-lCursorDevicesGlue \
	-lMPLibrary \
	-lTextEncodingConverter \
	-lTextCommon \
	-lContextualMenu \
	-lDrawSprocketLib \
	-lDialogsLib

# Specific to the Retro68 toolchain to create MacBinary files
schismtracker.bin: schismtracker.pef $(top_srcdir)/sys/macos/resource.r
schismtracker.pef: schismtracker.xcoff

## use today's date if we didn't get a commit date from git
if HAVE_GIT
rezflags_version = -DVERSION=\"`echo $(PACKAGE_VERSION)`\" -DVERSION_YEAR=`echo '$(PACKAGE_VERSION)' | cut -c1-4 | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/'` -DVERSION_MONTH=`echo '$(PACKAGE_VERSION)' | cut -c5-6 | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/'` -DVERSION_DAY=`echo '$(PACKAGE_VERSION)' | cut -c7-8 | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/'`
else
rezflags_version = -DVERSION=\"`date +%Y%m%d`\" -DVERSION_YEAR=`date +%Y | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/'` -DVERSION_MONTH=`date +%m | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/'` -DVERSION_DAY=`date +%d | sed 's/^0*\([1-9]\)/\1/;s/^0*$$/0/'`
endif # HAVE_GIT

.pef.bin:
	$(REZ) $(rezflags_version) -I"$(RETRO68_ROOT)/powerpc-apple-macos/RIncludes" "$(top_srcdir)/sys/macos/resource.r" -o $@ -t "APPL" -c "????" --data $<

.xcoff.pef:
	$(MAKE_PEF) $< -o $@
endif

if USE_FLAC
files_flac = \
	fmt/flac.c
cflags_flac=$(FLAC_CFLAGS)

if LINK_TO_FLAC
libs_flac=$(FLAC_LIBS)
endif

endif

if USE_AVFORMAT
files_avformat = \
	fmt/avformat.c
cflags_avformat= \
	$(AVFORMAT_CFLAGS) \
	$(AVCODEC_CFLAGS) \
	$(AVUTIL_CFLAGS)

if LINK_TO_AVFORMAT
libs_avformat= \
	$(AVFORMAT_LIBS) \
	$(AVCODEC_LIBS) \
	$(AVUTIL_LIBS)
endif

endif

if USE_JACK
files_jack = \
	sys/jack/midi-jack.c

cflags_jack=$(JACK_CFLAGS)

if LINK_TO_JACK
libs_jack=$(JACK_LIBS)
endif

endif

if USE_NETWORK
cflags_network=
libs_network=$(NETWORK_LIBS)
endif

if USE_OPL2
files_opl = player/fmopl2.c
else
files_opl = player/fmopl3.c
endif

if ENABLE_WAVEFORMVIS
CFLAGS += -DENABLE_WAVEFORMVIS
endif

## Replacement functions for crappy systems
files_stdlib =
if NEED_ASPRINTF
files_stdlib += sys/stdlib/asprintf.c
endif
if NEED_VASPRINTF
files_stdlib += sys/stdlib/vasprintf.c
endif
if NEED_SNPRINTF
files_stdlib += sys/stdlib/snprintf.c
endif
if NEED_VSNPRINTF
files_stdlib += sys/stdlib/vsnprintf.c
endif
if NEED_MEMCMP
files_stdlib += sys/stdlib/memcmp.c
endif
if NEED_LOCALTIME_R
files_stdlib += sys/stdlib/localtime_r.c
endif
if NEED_SETENV
files_stdlib += sys/stdlib/setenv.c
endif
if NEED_UNSETENV
files_stdlib += sys/stdlib/unsetenv.c
endif
if NEED_GETOPT
files_stdlib += sys/stdlib/getopt.c
endif

## aaaaaaaaahhhhhhhhhhhhhhhhhhh!!!!!!!1
schismtracker_SOURCES = \
	auto/default-font.c		\
	auto/helptext.c			\
	fmt/669.c			\
	fmt/aiff.c			\
	fmt/ams.c			\
	fmt/au.c			\
	fmt/compression.c		\
	fmt/d00.c			\
	fmt/dsm.c			\
	fmt/edl.c			\
	fmt/f2r.c			\
	fmt/far.c			\
	fmt/generic.c			\
	fmt/imf.c			\
	fmt/iff.c           \
	fmt/it.c			\
	fmt/iti.c			\
	fmt/its.c			\
	fmt/liq.c			\
	fmt/mdl.c			\
	fmt/med.c			\
	fmt/mf.c			\
	fmt/mid.c                       \
	fmt/mmcmp.c			\
	fmt/mod.c			\
	fmt/mt2.c			\
	fmt/mtm.c			\
	fmt/mus.c			\
	fmt/ntk.c			\
	fmt/okt.c			\
	fmt/pat.c			\
	fmt/psm.c			\
	fmt/raw.c			\
	fmt/s3i.c			\
	fmt/s3m.c			\
	fmt/sf2.c			\
	fmt/sfx.c			\
	fmt/stm.c			\
	fmt/stx.c			\
	fmt/ult.c			\
	fmt/w64.c			\
	fmt/wav.c			\
	fmt/xi.c			\
	fmt/xm.c			\
	player/csndfile.c		\
	player/effects.c		\
	player/equalizer.c		\
	player/filters.c		\
	player/fmpatches.c		\
	player/mixer.c			\
	player/mixutil.c		\
	player/snd_fm.c			\
	player/snd_gm.c			\
	player/sndmix.c			\
	player/tables.c			\
	schism/audio_loadsave.c		\
	schism/audio_playback.c		\
	schism/bshift.c         \
	schism/bswap.c          \
	schism/charset.c		\
	schism/charset_stdlib.c \
	schism/charset_unicode.c   \
	schism/clippy.c			\
	schism/config-parser.c		\
	schism/config.c			\
	schism/dialog.c			\
	schism/disko.c			\
	schism/dmoz.c			\
	schism/events.c  		\
	schism/fakemem.c		\
	schism/fonts.c          \
	schism/ieee-float.c     \
	schism/itf.c			\
	schism/keyboard.c		\
	schism/loadso.c         \
	schism/main.c			\
	schism/mem.c           \
	schism/menu.c			\
	schism/midi-core.c		\
	schism/midi-ip.c		\
	schism/mplink.c			\
	schism/page.c			\
	schism/page_about.c		\
	schism/page_blank.c		\
	schism/page_config.c		\
	schism/page_help.c		\
	schism/page_info.c		\
	schism/page_instruments.c	\
	schism/page_loadinst.c		\
	schism/page_loadmodule.c	\
	schism/page_loadsample.c	\
	schism/page_log.c		\
	schism/page_message.c		\
	schism/page_midi.c		\
	schism/page_midiout.c		\
	schism/page_orderpan.c		\
	schism/page_palette.c		\
	schism/page_patedit.c		\
	schism/page_preferences.c	\
	schism/page_samples.c		\
	schism/page_timeinfo.c		\
	schism/page_vars.c		\
	schism/page_waterfall.c		\
	schism/palettes.c		\
	schism/pattern-view.c		\
	schism/sample-edit.c		\
	schism/slurp.c			\
	schism/status.c			\
	schism/str.c             \
	schism/mt.c        \
	schism/timer.c          \
	schism/util.c			\
	schism/version.c		\
	schism/vgamem.c        \
	schism/video.c			\
	schism/widget-keyhandler.c	\
	schism/widget.c			\
	schism/xpmdata.c		\
	sys/posix/osdefs.c \
	$(files_macosx)			\
	$(files_alsa)			\
	$(files_oss)			\
	$(files_win32)			\
	$(files_x11)			\
	$(files_stdlib)			\
	$(files_mmap)			\
	$(files_wii)			\
	$(files_windres)		\
	$(files_flac)			\
	$(files_jack)			\
	$(files_opl)			\
	$(files_mediafoundation) \
	$(files_wiiu)           \
	$(files_sdl3)			\
	$(files_sdl2)			\
	$(files_sdl12)			\
	$(files_macos)			\
	$(files_os2)			\
	$(files_xbox)			\
	$(files_avformat)		\
	$(files_asio)

# have version.o rely on all files
schism/version.$(OBJEXT): $(filter-out schism/version.$(OBJEXT),$(schismtrackertest_OBJECTS)) $(HEADERS)

component_cflags = $(cflags_alsa) $(cflags_oss) \
	$(cflags_network) $(cflags_x11) $(cflags_fmopl) \
	$(cflags_version) $(cflags_win32) $(cflags_wii) \
	$(cflags_macosx) $(cflags_flac) $(cflags_jack) \
	$(cflags_wiiu) $(cflags_macos) $(cflags_os2) \
	$(cflags_xbox) $(UTF8PROC_CFLAGS) $(cflags_avformat) \
	$(cflags_asio)
schismtracker_CPPFLAGS = -I$(srcdir)/include -I. $(cppflags_wii) $(cppflags_wiiu)
schismtracker_CFLAGS = $(AM_CFLAGS) $(component_cflags)
schismtracker_OBJCFLAGS = $(AM_OBJCFLAGS) $(component_cflags)

schismtracker_DEPENDENCIES = $(files_windres)
schismtracker_LDADD = $(LIBM) $(libs_sdl3) \
	$(libs_x11) $(libs_wii) $(libs_wiiu) \
	$(libs_jack) $(libs_macosx) $(lib_asound) \
	$(lib_win32) $(libs_network) $(libs_flac) \
	$(lib_mediafoundation) $(UTF8PROC_LIBS) \
	$(libs_sdl2) $(libs_sdl12) $(libs_macos) \
	$(libs_os2) $(libs_xbox) $(libs_avformat) \
	$(libs_asio)
schismtracker_LDFLAGS = $(ldflags_macosx)

### Automated testing -- some additional files get compiled, but
### these are only linked into schismtrackertest.

schismtrackertest_SOURCES =	\
	schism/main.c			\
	test/harness.c			\
	test/testresult.c		\
	test/log.c				\
	test/index.c			\
	test/cases/bshift.c

# err, this is flaky, but okay for now i guess
mains =									\
	schism/schismtracker-main.$(OBJEXT)	\
	sys/macosx/schismtracker-macosx-sdlmain.$(OBJEXT)

# Enable test paths -- overrides the entrypoint
schismtrackertest_CFLAGS = $(schismtracker_CFLAGS) -DSCHISM_TEST_BUILD

schismtrackertest_CPPFLAGS = $(schismtracker_CPPFLAGS)
schismtrackertest_OBJCFLAGS = $(schimtracker_OBJCFLAGS)
schismtrackertest_DEPENDENCIES = $(schismtracker_DEPENDENCIES) schismtracker$(EXEEXT)
schismtrackertest_LDADD = $(filter-out $(mains),$(schismtracker_OBJECTS)) $(schismtracker_LDADD)
schismtrackertest_LDFLAGS = $(schismtracker_LDFLAGS)

