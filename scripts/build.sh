#!/bin/sh
autoreconf -i
mkdir -p build
cd build
sh ../configure
make