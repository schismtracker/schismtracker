#!/bin/sh
#
# this script builds all of the "auto" headers
#

## make help text
echo "making include/auto/helptext.h" >&2
tempfile="include/auto/.helptext.tmp"
while read REPLY; do
	set $REPLY
	cat helptext/$1
	awk 'BEGIN { printf "%c", 0 }' < /dev/null
	echo "	helptext/$1" >&2
done > $tempfile < helptext/index
tempfile2="include/auto/.helptext.h"

echo "/* this file should only be included by helptext.c */" > $tempfile2
sh scripts/bin2h.sh -n help_text -t 'static unsigned char' $tempfile >> $tempfile2 || exit 1
mv "$tempfile2" "include/auto/helptext.h" || exit 1

echo "making include/auto/helpenum.h" >&2
echo "enum {" > $tempfile || exit 1
while read REPLY; do
	set $REPLY
	echo "HELP_$2,"
	echo "	$2" >&2
done >> $tempfile < helptext/index
echo "HELP_NUM_ITEMS" >> $tempfile || exit 1
echo "};" >> $tempfile || exit 1
mv "$tempfile" "include/auto/helpenum.h" || exit 1



## make WM icon
echo "making include/auto/schismico.h" >&2
pngtopnm -alpha < icons/schism-icon-32.png >.a.tmp || exit 1
pngtopnm < icons/schism-icon-32.png >.b.tmp || exit 1
ppmtoxpm -rgb /dev/null -name _schism_icon_xpm -alphamask .a.tmp < .b.tmp > .c.tmp || exit 1
sed 's/char/const char/' < .c.tmp > include/auto/schismico.h
rm -f .a.tmp .b.tmp .c.tmp

## make schism logo
echo "making include/auto/logoschism.h" >&2
pngtopnm -alpha < icons/schism_logo.png > .a.tmp || exit 1
pngtopnm < icons/schism_logo.png > .b.tmp || exit 1
ppmtoxpm -rgb /dev/null -name _logo_schism_xpm -alphamask .a.tmp < .b.tmp > .c.tmp || exit 1
sed 's/char/const char/' < .c.tmp > include/auto/logoschism.h
rm -f .a.tmp .b.tmp .c.tmp

## make IT logo
echo "making include/auto/logoit.h" >&2
pngtopnm -alpha < icons/it_logo.png > .a.tmp || exit 1
pngtopnm < icons/it_logo.png > .b.tmp || exit 1
ppmtoxpm -rgb /dev/null -name _logo_it_xpm -alphamask .a.tmp < .b.tmp > .c.tmp || exit 1
sed 's/char/const char/' < .c.tmp > include/auto/logoit.h
rm -f .a.tmp .b.tmp .c.tmp || exit 1

## make default (builtin) font
echo "making include/auto/default-font.h" >&2
tempfile="include/auto/.tempf.tmp"
echo '/* this file should only be included by draw-char.c */' > $tempfile || exit 1
sh scripts/bin2h.sh -n font_default_lower font/default-lower.fnt >> $tempfile || exit 1
sh scripts/bin2h.sh -n font_default_upper_itf font/default-upper-itf.fnt >> $tempfile || exit 1
sh scripts/bin2h.sh -n font_default_upper_alt font/default-upper-alt.fnt >> $tempfile || exit 1
sh scripts/bin2h.sh -n font_half_width font/half-width.fnt >> $tempfile || exit 1
mv $tempfile include/auto/default-font.h
