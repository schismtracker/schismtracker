#!/bin/sh
# this is the maintainer script for schism on Linux/x86
mkdir -p linux-x86-build || exit 1
cd linux-x86-build || exit 1
../configure $@ && make -j3 || exit 1
make
