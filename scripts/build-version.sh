#!/bin/sh
if [ -d CVS ]; then
	cvs ci -f -m 'Build' include/auto/build-version.h
else
	cd include/auto || exit 1
	exec > build-version.h.tmp || exit 1
	echo "/* this file must be checked in before each proper build."
	echo "*/"
	date +'#define BUILD_VERSION "$Date: %Y/%m/%d %H:%M:%S $"' 
	mv build-version.h.tmp build-version.h
fi
