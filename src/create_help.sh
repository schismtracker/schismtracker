#!/bin/sh
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

# create_help.sh <bin2h> <outfile> <srcdir> <infiles>
# (infiles should be in srcdir)
bin2h="$1"
outfile="$2"
srcdir="$3"
shift 3

if [ -d "$TMPDIR" ]; then
        tempfile="$TMPDIR"
elif [ -d "$TMP" ]; then
	tempfile="$TMP"
else
	tempfile=/tmp
fi
tempfile="$tempfile/create_help.$$"

(for f in "$@"; do
	cat "$srcdir/$f" || exit 1
	head -c1 /dev/zero
done) > "$tempfile"

rm -f "$outfile"
echo '/* this file should only be included by helptext.c */' > "$outfile"
bzip2 -9 < "$tempfile" | "$bin2h" -n compressed_help_text --data-type 'static unsigned char' >> "$outfile" || exit 1
echo "#define UNCOMPRESSED_HELP_TEXT_SIZE" `wc -c < "$tempfile"` >> \
	"$outfile" || exit 1

rm -f "$tempfile" || exit 1
