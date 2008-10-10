#!/bin/sh
# this will turn your schism tarball into junk
# don't run it.
rm -f aclocal.m4  config.guess config.sub configure depcomp install-sh
rm -rf autom4te.cache "Makefile.in" missing
rm -rf win32-x86-build linux-x86-build macosx-ppc-build schism-1.0.x86.package
