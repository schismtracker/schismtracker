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
