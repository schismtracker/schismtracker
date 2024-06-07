# Building on Linux

Since Linux is the primary development platform for Schism Tracker, it's
probably the easiest to compile on, and those familiar with automake-based
projects will find few surprises here. If you just want to use Schism Tracker
on 64-bit Linux, you can also download a pre-built binary from the release page
(just make sure you've installed SDL2).

## Prerequisites

On Ubuntu, run:

    sudo apt update
	sudo apt install build-essential automake autoconf-archive \
                     libsdl2-dev git libtool libflac-dev perl

On Arch Linux:

	sudo pacman -Syu
	sudo pacman -S base-devel git sdl2 alsa-lib libxv libxxf86vm flac perl

Git is not strictly required, but if you don't need it you'll need to download
a tarball manually, and your build won't have a proper version string.

FLAC libraries are optional and only used for FLAC sample loading support.

On other distros, the package names may be different. In particular, note that
`build-essential` includes the packages `gcc` and `make` on Debian-based
systems.

If your distro doesn't come with Python by default, you'll also need that.

## Setting up the source directory

To get and set up the source directory for building:

    git clone https://github.com/schismtracker/schismtracker.git
    cd schismtracker
	autoreconf -i
	mkdir -p build

You can then update your Schism Tracker source directory by going to the
`schismtracker` directory and running:

    git pull

## Building Schism Tracker

From the `schismtracker` directory:

    cd build && ../configure && make

The resulting binary `schismtracker` is completely self-contained and can be
copied anywhere you like on the filesystem.

## Packaging Schism Tracker for Linux systems

The `icons/` directory contains icons that you may find suitable for your
desktop environment. The `sys/fd.org/schism.desktop` can be used to launch
Schism Tracker from a desktop environment, and `sys/fd.org/itf.desktop` can be
used to launch the built-in font-editor.

## ALSA problems

The configure script should autodetect everything on your system, but if you
don't have the ALSA development libraries installed, Schism Tracker won't be
built with ALSA MIDI support, even if your SDL libraries include ALSA digital
output.

## Cross-compiling for Win32

Schism Tracker can be built using the MinGW cross-compiler on a Linux host.
You will also need the [SDL2 MinGW development library][1]. If you unpacked it
into `/usr/i586-mingw32/`, you could use the following to cross-compile Schism
Tracker for Win32:

    mkdir win32-build
    cd build
    env SDL_CONFIG=/usr/i586-mingw32/sdl-config \
        ../configure --{host,target}=i586-mingw32 --without-x
    make

If you want to build an installer using the [Nullsoft Scriptable Install
System][2], copy some files into your build directory:

    cd build
    cp /usr/i586-mingw32/bin/SDL2.dll .
    cp ../COPYING COPYING.txt
    cp ../README README.txt
    cp ../NEWS NEWS.txt
    cp ../sys/win32/schism.nsis .
    cp ../icons/schismres.ico schism.ico

and run the `makensis` application:

    makensis schism.nsis

On Ubuntu, for cross-compiling Win32 binaries, run:

    sudo apt install mingw32 mingw32-binutils mingw32-runtime nsis

On Arch Linux:

    sudo pacman -S mingw-w64-gcc
    yaourt -S mingw-w64-sdl2 nsis

Note: Yaourt isn't strictly necessary, but since `mingw-w64-sdl2` and `nsis`
are AUR packages, you'll have to build them by hand otherwise or use a
different [AUR helper][3]. `mingw-w64-sdl2` may or may not be necessary if
you've manually downloaded the MinGW SDL2 library as mentioned above.

[1]: https://github.com/libsdl-org/SDL/releases
[2]: http://nsis.sourceforge.net/
[3]: https://wiki.archlinux.org/index.php/AUR_helpers
