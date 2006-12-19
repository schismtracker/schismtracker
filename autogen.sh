#!/bin/sh
set -e
# I haven't tried all versions, but I know that these work
# on macosx I got them from fink
# they were available on my FC4 machine
# they're probably available to you.
if aclocal --version | fgrep 1.9; then aclocal ; else aclocal-1.9; fi
if automake --version | fgrep 1.9; then automake --add-missing; else automake-1.9 --add-missing; fi
if autoheader --version | fgrep 2.59; then autoheader ; else autoheader-2.59; fi
if autoconf --version | fgrep 2.59; then autoconf ; else autoconf-2.59; fi
