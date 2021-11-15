# Building on Linux

Since Linux is the primary development platform for Schism Tracker, it's
probably the easiest to compile on, and those familiar with automake-based
projects will find few surprises here.

## Prerequisites

You'll need [autoconf](http://www.gnu.org/software/autoconf/),
[automake](http://www.gnu.org/software/automake/), [gcc](http://gcc.gnu.org/),
[make](http://www.gnu.org/software/make/), [Python](https://www.python.org/)
and [LibSDL](http://www.libsdl.org/) *at a minimum*. Additionally,
[Git](https://git-scm.com/) is strongly recommended. If all you're planning on
doing is building it once, you can just as easily grab the source tarball from
the repository and build from that, but having Git installed makes it easier to
keep up-to-date, help with debugging, and (if you're so inclined) development.

See below for distro-specific instructions on how to get everything installed
in order to build Schism Tracker.

To get the source:

    git clone https://github.com/schismtracker/schismtracker.git
    cd schismtracker && autoreconf -i

You can then update your schismtracker source directory by going to this
schismtracker directory and running:

    git pull

## Building Schism Tracker

To build Schism Tracker, you should set up a build-directory and compile from
there. From the schismtracker directory:

    mkdir -p build && cd build && ../configure && make

The resulting binary `schismtracker` is completely self-contained and can be
copied anywhere you like on the filesystem.

You can specify custom compiler flags, e.g. to optimize schismtracker
stronly, system-dependently:

    make clean  # recompiling needed after changing compiler setting
    ../configure --enable-extra-opt
    make -j $(nproc || sysctl -n hw.ncpu || echo 2)

The -j flag passed to make enables compilation on multiple threads.
For debugging, and other settings, see `../configure --help`.

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

See below for information on what packages you should install for your
distribution in order to build a full-featured Schism Tracker binary.

## Cross-compiling Win32

Schism Tracker can be built using the mingw32 cross-compiler on a Linux host.
You will also need the [SDL MINGW32 development
library](http://libsdl.org/download-1.2.php). If you unpacked it into
`/usr/i586-mingw32/`, you could use the following to cross-compile Schism
Tracker for Win32:

    mkdir win32-build
    cd build
    env SDL_CONFIG=/usr/i586-mingw32/sdl-config \
        ../configure --{host,target}=i586-mingw32 --without-x
    make

If you want to build an installer using the [nullsoft scriptable install
system](http://nsis.sourceforge.net/), copy some files into your build
directory:

    cd build
    cp /usr/i586-mingw32/bin/SDL.dll .
    cp ../COPYING COPYING.txt
    cp ../README README.txt
    cp ../NEWS NEWS.txt
    cp ../sys/win32/schism.nsis .
    cp ../icons/schismres.ico schism.ico

and run the makensis application:

    makensis schism.nsis

## Distribution-specific instructions

Getting the prerequisites covered is fairly straightforward in most Linux
distributions.

#### Ubuntu / Debian

    apt-get install build-essential automake autoconf autoconf-archive \
                    libx11-dev libxext-dev libxv-dev libxxf86misc-dev \
                    libxxf86vm-dev libsdl1.2-dev libasound2-dev git \
                    libtool

Additionally, for cross-compiling win32 binaries:

    apt-get install mingw32 mingw32-binutils mingw32-runtime nsis

#### Arch Linux

    pacman -S base-devel git sdl alsa-lib libxv libxxf86vm

For cross-compiling win32 binaries:

    pacman -S mingw-w64-gcc
    yaourt -S mingw-w64-sdl nsis

Note: yaourt isn't strictly necessary, but since `mingw-w64-sdl` and `nsis` are
AUR packages, you'll have to build them by hand otherwise or use a different
[AUR helper](https://wiki.archlinux.org/index.php/AUR_helpers).
