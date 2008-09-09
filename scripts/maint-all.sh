#!/bin/sh
sh scripts/build-version.sh
sh scripts/maint-macosx.sh $@ &
sh scripts/maint-linux.sh $@ &
sh scripts/maint-win32.sh $@ &
wait
sh scripts/maint-tarball.sh
sh scripts/maint-changelog.sh
