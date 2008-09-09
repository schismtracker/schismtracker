#!/bin/sh
if [ -d CVS ]; then
	cvs ci -f -m 'Build' include/auto/build-version.h
else
	exec > include/auto/build-version.h.tmp || exit 1
	echo "/* this file must be checked in before each proper build."
	echo "*/"
	date -r .git/refs/heads/master \
		+'#define BUILD_VERSION "$Date: %Y/%m/%d %H:%M:%S $"' 
	mv include/auto/build-version.h.tmp include/auto/build-version.h
fi
