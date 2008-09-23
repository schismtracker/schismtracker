#!/bin/sh
HERE=`pwd`
HERE=`basename "$HERE"`
# this is my maintainer script for macosx builds
# I use fink for SDL and stuff, and I don't have (regular) access to XCode
mm=`cat "$HOME/.my-macosx"`
(cd ..; rsync \
 --cvs-exclude --exclude=.git --exclude="*-build" --exclude="schism-*.x86.package" --exclude="*.tgz" \
 -v -z -r -e ssh "$HERE" "$mm:.")

ssh "$mm" 'cd "'"$HERE"'"; PATH="/sw/bin:$PATH"; export PATH; sh scripts/fink-macosx.sh'" $@" || exit 1

mkdir -p macosx-ppc-build
rsync -v -z -r -e ssh "$mm:$HERE/macosx-ppc-build/./" macosx-ppc-build/.


