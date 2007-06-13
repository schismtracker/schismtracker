#!/bin/sh
# this is the maintainer script for schism on Win32/x86
mkdir -p win32-x86-build || exit 1
cd win32-x86-build || exit 1
make -j3 || exit 1

cp /usr/i586-mingw32msvc/bin/SDL.dll . || exit 1
cp ../COPYING COPYING.txt || exit 1
cp ../sys/win32/schism.bat . || exit 1
zip "Schism Tracker.zip" schismtracker.exe schism.bat SDL.dll COPYING.txt || exit 1
