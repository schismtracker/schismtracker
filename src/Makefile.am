SUBDIRS = fmt libmodplug

# note! the order in the enum in it.h MUST agree with the order here.
# (otherwise, the helptexts will be all wrong)
helptexts = helptext/global-keys helptext/pattern-editor \
	helptext/sample-list helptext/instrument-list \
        helptext/info-page helptext/message-editor \
	helptext/orderlist-pan helptext/orderlist-vol

bin_PROGRAMS = schism itf
noinst_PROGRAMS = bin2h
noinst_SCRIPTS = create_help.sh

EXTRA_DIST = $(srcdir)/include/*.h create_help.sh $(helptexts) \
        default-lower.fnt default-upper-itf.fnt \
	default-upper-alt.fnt half-width.fnt

schism_SOURCES = config.c draw-char.c draw-misc.c \
        draw-pixel.c dialog.c helptext.c item.c item-keyhandler.c \
	keyboard.c main.c menu.c mixer.c page.c page_blank.c \
	page_help.c page_info.c page_instruments.c page_loadmodule.c \
	page_loadsample.c page_log.c page_midi.c page_orderpan.c \
	page_patedit.c page_samples.c page_settings.c page_message.c \
	page_palette.c page_vars.c pattern-view.c sample-edit.c \
	sample-view.c slurp.c status.c title.c util.c \
	audio_playback.cc audio_loadsave.cc mplink.cc

itf_SOURCES = draw-char.c draw-pixel.c itf.c slurp.c util.c
bin2h_SOURCES = bin2h.c slurp.c util.c

INCLUDES = -D_GNU_SOURCE -I$(srcdir)/include -I$(srcdir)/libmodplug \
	-I. @SDL_CFLAGS@

schism_LDADD = @SDL_LIBS@ fmt/libfmt.a libmodplug/libmodplug.a -lm -lcoman
itf_LDADD = @SDL_LIBS@
bin2h_LDADD =

CLEANFILES = *~ include/*~ helptext/*~ test.*
DISTCLEANFILES = .deps/*.P default-font.h helptext.h

## ------------------------------------------------------------------------
## Generation of derived headers
## This is likely the "wrong" way to do this, but I can't figure it out.
## (and I've read the documentation about ten times...)

draw-char.o: default-font.h
helptext.o: helptext.h

default-font.h: bin2h default-lower.fnt default-upper-alt.fnt \
	default-upper-itf.fnt half-width.fnt
	rm -f $(srcdir)/default-font.h
	./bin2h -d -n DEFAULT_LOWER     $(srcdir)/default-lower.fnt \
	        > default-font.h
	./bin2h -d -n DEFAULT_UPPER_ITF $(srcdir)/default-upper-itf.fnt \
	        >> default-font.h
	./bin2h -d -n DEFAULT_UPPER_ALT $(srcdir)/default-upper-alt.fnt \
	        >> default-font.h
	./bin2h -d -n HALF_WIDTH_FONT   $(srcdir)/half-width.fnt \
	        >> default-font.h

## create_help.sh <bin2h> <outfile> <srcdir> <infiles>
## (infiles should be in srcdir)
helptext.h: bin2h create_help.sh $(helptexts)
	$(srcdir)/create_help.sh ./bin2h ./helptext.h \
	        $(srcdir) $(helptexts)
