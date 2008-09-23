#!/bin/sh
# this is the maintainer script for schism on Win32/x86
mkdir -p win32-x86-build || exit 1
cd win32-x86-build || exit 1
make -j3 || exit 1

cp /usr/i586-mingw32msvc/bin/SDL.dll . || exit 1
cp ../COPYING COPYING.txt || exit 1
cp ../README README.txt || exit 1
cp ../NEWS NEWS.txt || exit 1
cp ../ChangeLog ChangeLog.txt || exit 1
cp ../sys/win32/schism.nsis . || exit 1
cp ../icons/schismres.ico schism.ico || exit 1
makensis schism.nsis || exit 1

