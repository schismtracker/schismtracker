#!/bin/sh
# this is the maintainer script for schism on Linux/x86
EXTRA_ARGS="$@"
export EXTRA_ARGS
exec makeinstaller sys/fd.org/autopackage.apspec
