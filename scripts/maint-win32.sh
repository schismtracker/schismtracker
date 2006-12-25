#!/bin/sh
# this is the maintainer script for schism on Win32/x86
mkdir -p win32-x86-build || exit 1
cd win32-x86-build || exit 1
SDL_CONFIG=/usr/local/bin/i386-mingw32-sdl-config LIBS=-lSDLmain ../configure --host=i386-mingw32 --with-sdl-prefix=/usr/local/i386-mingw32/ --with-windres=i386-mingw32-windres --without-x && make -j3 || exit 1

cp /usr/local/i386-mingw32/bin/SDL.dll . || exit 1
cp ../COPYING COPYING.txt || exit 1
cp ../sys/win32/schism.bat . || exit 1
rm -f "Schism Tracker.zip"
zip "Schism Tracker.zip" schismtracker.exe schism.bat SDL.dll COPYING.txt || exit 1
