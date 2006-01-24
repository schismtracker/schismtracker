#!/bin/sh
# this is my maintainer script for macosx builds
# I use fink for SDL and stuff, and I don't have (regular) access to XCode
mm=`cat "$HOME/.my-macosx"`
(cd ..; rsync \
	--cvs-exclude --exclude="*-build" --exclude="schism-*.x86.package" \
	-v -z -r -e ssh schism "$mm:.")

ssh "$mm" 'cd schism; PATH="/sw/bin:$PATH"; export PATH; sh scripts/fink-macosx.sh' || exit 1

mkdir -p macosx-ppc-build
rsync -v -z -r -e ssh "$mm:schism/macosx-ppc-build/./" macosx-ppc-build/.


