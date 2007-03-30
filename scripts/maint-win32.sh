#!/bin/sh
# this is the maintainer script for schism on Win32/x86
mkdir -p win32-x86-build || exit 1
cd win32-x86-build || exit 1
SDL_CONFIG=/usr/bin/i586-mingw32msvc-sdl-config LIBS=-lSDLmain ../configure --host=i586-mingw32msvc --with-sdl-prefix=/usr/i586-mingw32msvc/ --with-windres=i586-mingw32msvc-windres --without-x $@ && make -j3 || exit 1

cp /usr/i586-mingw32msvc/bin/SDL.dll . || exit 1
cp ../COPYING COPYING.txt || exit 1
cp ../README README.txt || exit 1
cp ../NEWS NEWS.txt || exit 1
cp ../ChangeLog ChangeLog.txt || exit 1
cp ../sys/win32/schism.bat . || exit 1
rm -f "Schism Tracker.zip"
zip "Schism Tracker.zip" schismtracker.exe schism.bat SDL.dll COPYING.txt README.txt NEWS.txt ChangeLog.txt || exit 1
