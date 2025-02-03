#!/bin/sh
# script to run schism with included libs on linux
# shouldn't™ require bash or any other crap like that.

# This is standard among GNU systems
PREFIX="/usr/local"

SCRIPTPATH="$(cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P)"

install "$SCRIPTPATH/schismtracker" "$PREFIX/bin/schismtracker"
for i in FLAC.so.8 utf8proc.so.2.3.2 ogg.so.0; do
	install "$SCRIPTPATH/lib$i" "$PREFIX/lib/lib$i"
done
install "$SCRIPTPATH/schism.desktop" "$PREFIX/share/applications/schism.desktop"
install "$SCRIPTPATH/schismtracker.1" "$PREFIX/share/man/man1/schismtracker.1"
install "$SCRIPTPATH/schism-icon-128.png" "$PREFIX/share/pixmaps/schism-icon-128.png"
