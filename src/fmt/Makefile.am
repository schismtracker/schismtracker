noinst_LIBRARIES = libfmt.a

if USE_NON_TRACKED_TYPES
if HAVE_LIBID3TAG
mp3 = mp3.c
endif
if HAVE_VORBIS
ogg = ogg.c
endif
sid = sid.c
endif

libfmt_a_SOURCES = 669.c ams.c dtm.c f2r.c far.c it.c liq.c mdl.c mod.c \
	$(mp3) mt2.c mtm.c ntk.c $(ogg) rtm.c s3m.c $(sid) stm.c ult.c xm.c

INCLUDES = -D_GNU_SOURCE -I$(srcdir)/../include
CLEANFILES = *~
DISTCLEANFILES = .deps/*.P
