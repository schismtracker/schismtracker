#!/bin/sh
exec > include/auto/build-version.h.tmp || exit 1
echo "/* auto-generated timestamp */"
date +'#define BUILD_VERSION "$Date: %Y/%m/%d %H:%M:%S $"'
mv -f include/auto/build-version.h.tmp include/auto/build-version.h
