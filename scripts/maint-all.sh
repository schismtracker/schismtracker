#!/bin/sh
cvs ci -f -m 'Build' include/auto/build-version.h
sh scripts/maint-macosx.sh $@ &
sh scripts/maint-linux.sh $@ &
sh scripts/maint-win32.sh $@ &
wait
sh scripts/maint-tarball.sh
