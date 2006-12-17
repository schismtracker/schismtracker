#!/bin/sh
# this script is used by the maintainer script for macosx builds
# assumes we're doing things the finkish way

mkdir -p macosx-ppc-build || exit 1
cd macosx-ppc-build || exit 1
../configure --without-x --disable-sdltest CC=gcc-3.3 CXX=g++-3.3 || exit 1
make || exit 1

# transfer template
mkdir -p "root/Schism Tracker.app"
cp -R "../sys/macosx/Schism Tracker.app/"* "root/Schism Tracker.app/."
find "root/Schism Tracker.app" -path '*/CVS/*' -or -path '*/CVS' -print0 | xargs -0 rm -rf

mkdir -p "root/Schism Tracker.app/Contents/MacOS" || exit 1

cp schism "root/Schism Tracker.app/Contents/MacOS/schism" || exit 1
cp /sw/lib/libSDL-1.2.0.dylib "root/Schism Tracker.app/Contents/MacOS/sdl.dylib" || exit 1
install_name_tool -change /sw/lib/libSDL-1.2.0.dylib @executable_path/sdl.dylib "root/Schism Tracker.app/Contents/MacOS/schism" || exit 1

# okay, next!
cp ../COPYING root/COPYING.txt || exit 1

cd root
hdiutil create -srcfolder . "../Schism Tracker.dmg" -ov -volname 'Schism Tracker CVS'
