noinst_LIBRARIES = libmodplug.a
libmodplug_a_SOURCES = tables.cpp          \
                        sndmix.cpp         \
                        sndfile.cpp        \
                        snd_fx.cpp         \
                        snd_flt.cpp        \
                        snd_dsp.cpp        \
                        fastmix.cpp        \
                        mmcmp.cpp          \
                        load_xm.cpp        \
                        load_wav.cpp       \
                        load_umx.cpp       \
                        load_ult.cpp       \
                        load_stm.cpp       \
                        load_s3m.cpp       \
                        load_ptm.cpp       \
                        load_okt.cpp       \
                        load_mtm.cpp       \
                        load_mod.cpp       \
                        load_med.cpp       \
                        load_mdl.cpp       \
                        load_it.cpp        \
                        load_far.cpp       \
                        load_dsm.cpp       \
                        load_dmf.cpp       \
                        load_dbm.cpp       \
                        load_ams.cpp       \
                        load_amf.cpp       \
                        load_669.cpp       \
                        load_j2b.cpp       \
                        load_mt2.cpp       \
                        load_psm.cpp

INCLUDES = -DMODPLUG_PLAYER -DMODPLUG_TRACKER -I$(srcdir)/../include

## automake complains about me changing CXXFLAGS here. however, if i use
## AM_CXXFLAGS like it suggests, then -Wno-unused goes *before* the rest of
## the flags, and the -W cancels it out later!! plus, its complaint is that
## i shouldn't *override* this. i'm not, i'm ADDING to it.... arghbbq.
CXXFLAGS += -Wno-unused

CLEANFILES = *~
DISTCLEANFILES = .deps/*.P

noinst_HEADERS = it_defs.h stdafx.h sndfile.h
