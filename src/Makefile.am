# Schism Tracker - a cross-platform Impulse Tracker clone
# copyright (c) 2003-2004 chisel <someguy@here.is> <http://here.is/someguy/>
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

SUBDIRS = fmt libmodplug

## note! the order in the enum in it.h MUST agree with the order here.
## (otherwise, the helptexts will be all wrong)
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

if USE_ALSA_MIXER
mixer = mixer-alsa.c
else
mixer = mixer-oss.c
endif

schism_SOURCES = config.c config-parser.c draw-char.c draw-misc.c \
        draw-pixel.c dialog.c helptext.c item.c item-keyhandler.c \
	keyboard.c main.c menu.c $(mixer) page.c page_blank.c \
	page_help.c page_info.c page_instruments.c page_loadmodule.c \
	page_loadsample.c page_log.c page_midi.c page_orderpan.c \
	page_patedit.c page_samples.c page_settings.c page_message.c \
	page_palette.c page_vars.c pattern-view.c sample-edit.c \
	sample-view.c slurp.c status.c util.c audio_playback.cc \
	audio_loadsave.cc mplink.cc

itf_SOURCES = draw-char.c draw-pixel.c itf.c slurp.c util.c
bin2h_SOURCES = bin2h.c slurp.c util.c

INCLUDES = -D_GNU_SOURCE -I$(srcdir)/include -I$(srcdir)/libmodplug -I. \
	@SDL_CFLAGS@

schism_LDADD = @SDL_LIBS@ fmt/libfmt.a libmodplug/libmodplug.a -lm
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
	echo '/* this file should only be included by draw-char.c */'      > default-font.h
	./bin2h -n font_default_lower     $(srcdir)/default-lower.fnt     >> default-font.h
	./bin2h -n font_default_upper_itf $(srcdir)/default-upper-itf.fnt >> default-font.h
	./bin2h -n font_default_upper_alt $(srcdir)/default-upper-alt.fnt >> default-font.h
	./bin2h -n font_half_width        $(srcdir)/half-width.fnt        >> default-font.h

## create_help.sh <bin2h> <outfile> <srcdir> <infiles>
## (infiles should be in srcdir)
helptext.h: bin2h create_help.sh $(helptexts)
	$(srcdir)/create_help.sh ./bin2h ./helptext.h \
	        $(srcdir) $(helptexts)
