#!/bin/bash

# Exit on any error
set -e

# Check for SDL2
if ! pkg-config --exists sdl2; then
    echo "Error: SDL2 not found. Please install it:"
    echo "brew install sdl2"
    exit 1
fi

# Get SDL2 paths from pkg-config
SDL2_CFLAGS=$(pkg-config --cflags sdl2)
SDL2_LIBS=$(pkg-config --libs sdl2)

mkdir -p build

cd build

../configure \
    --with-sdl2=yes \
    --enable-sdl2-linking \
    SDL2_CFLAGS="$SDL2_CFLAGS" \
    SDL2_LIBS="$SDL2_LIBS" || {
        echo "Configure failed! Check config.log for details"
        exit 1
    }

# Use all CPU cores for faster build
make -j$(sysctl -n hw.ncpu) || {
    echo "Build failed!"
    exit 1
}
