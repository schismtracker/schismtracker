# Compiling Schism Tracker on Windows

_This page was adapted from COMPILE-WIN32.txt traditionally provided with Schism Tracker sources. Any additional notes are in italics, like this one_

## Software needed

To compile on Windows, the following things are needed:

* mingw-gcc
* Python
* SDL headers and libs
* An environment in which to run them, like msys (It could also be cygwin. Use its setup program, and get sdl sources to compile them in this case).

## Installing needed software

_note: follow the instructions in either the x86/Win32 or the x64/Win64 section, but not both!_

### Installing mingw and msys (x86/Win32) ***

They've recently created an installer that maintains packages, that works nicely:
[http://sourceforge.net/projects/mingw/files/Installer/mingw-get-setup.exe/download](http://sourceforge.net/projects/mingw/files/Installer/mingw-get-setup.exe/download)
That installer can also install msys if told to do so.

if installing msys, then run `(installdir)\msys\1.0\postinstall\pi.sh` to set up where mingw is located.

Get Python from [http://www.python.org](http://www.python.org) and install it. Get version 2 for now.

Add Python to msys PATH. You can do it in two ways:
* Add the path to Windows PATH Environment variable (msys will add it automatically to msys's environment.
  You will need to relaunch it after the change)
* OR modify the msys file `/etc/profile` and modify the export PATH adding your python path. (like `export PATH=/c/python27:$PATH` )

For mingw x86, there is a precompiled libs and headers package that can be downloaded
from [http://www.libsdl.org/](http://www.libsdl.org/) .
At the time of this writing, there's the file: SDL-devel-1.2.15-mingw32.tar.gz (Mingw32).

You can unpack this file into mingw by copying individually the folders
bin, include, lib and share into C:/MinGW (if that's the name you used). The rest of the package
is not needed, as it contains examples, and other documentation, some of it used to build from
source, which is not needed with this package.

Also, you will need to modify the file `sdl-config` to change the "prefix" path.
Like this:
`prefix=/mingw`

### Installing mingw and msys (x64/Win64)

_note: To build for 64-bit, you will have to compile your own SDL libraries. To build them with DirectX support (Schism doesn't run very well on Windows without DirectX support) may require Visual Studio. If you don't have Visual Studio you may be better off using 32-bit and the precompiled SDL libraries. Alternatively, something that has been known to work is to compile SDL without DirectX support and then replace the built SDL.dll with an SDL.dll from somewhere else (where it is known to have DirectX support)._

Since mingw's installer only installs an x86 platform, you might opt to install winBuilds:
[http://win-builds.org/](http://win-builds.org/)

Follow the instructions from the x86 section to set up msys and python.

For x64, there aren't precompiled SDL libs, so you have to get the sources to compile them
from [http://www.libsdl.org/](http://www.libsdl.org/) .
At the time of this writing, there's the file: SDL-1.2.15.tar.gz

Unpack it somewhere (like `C:/msys/opt/SDL`).

Get Microsoft's DirectX SDK from [http://www.microsoft.com/en-us/download/details.aspx?id=6812](http://www.microsoft.com/en-us/download/details.aspx?id=6812)
and install it.

Now, you need msys/mingw to know about your directx includes and libs. This is a little tricky
and the best solution I found was to make symbolic links (`ln -s`) to the directories as follows:

Let's say that the Direct X SDK is installed in `C:\Program Files (x86)\Microsoft DirectX SDK (June 2010)`

1. Run msys, and go to `/mingw/include`
1. type `ln -s C:/Program\ Files\ \(x86\)/Microsoft\ DirectX\ SDK\ \(June\ 2010\)/Include dxinclude`
1. go to `/mingw/lib`
1. type `ln -s C:/Program\ Files\ \(x86\)/Microsoft\ DirectX\ SDK\ \(June\ 2010\)/Lib dxlib`

I also had to copy the file `C:\Program Files (x86)\Microsoft Visual Studio 9.0\VC\include\sal.h` to `/mingw/include`.
This file's header says: "sal.h - markers for documenting the semantics of APIs". It only
has some defines, so it might be safe to just put an empty file (not tried _[note: tried, doesn't work]_). Else, it might be obtained from Visual Studio.

Now, go to where you copied the SDL SDK (`example: /opt/SDL/`) , and do `./configure CPPFLAGS=-I/mingw/include/dxinclude LIBS=-L/mingw/lib/dxlib`

Now look at the output of configure and see if it says something like:

    checking ddraw.h usability
    result: yes
    checking dsound.h usability
    result: yes

(This is extracted from config.log. the output in the screen is a bit different)

If it says no, open the config.log file, locate the lines and see which test fail and why.

If the `./configure` executes successfully and you have ddraw _(note: you probably don't - ddraw.h isn't actually supplied with the DirectX SDK! It may be supplied with Visual Studio)_ and dsound, then continue with

    make
    make install

## Compilation

Run msys (`C:/msys/1.0/msys.bat`), go to schismtracker sources (hint: drive letters are mapped to /x , example
C:/ is /c/, D:/ is /d/ ...)

If configure does not exist, (you will need autoconf and automake _[note: you can install them through the mingw GUI package manager - they are part of a package called "autotools"]_) execute:

    autoreconf

Alternatively, you can execute each individual command:

    aclocal
    autoconf
    autoheader
    automake --add-missing

If you get a warning that AM_PATH_SDL is missing, you should check where the sdl.m4 _(note: the folder where you unpacked SDL, probably. `grep -r "sdl.m4" /` for it)_ is, and use the -I parameter like:

    aclocal -I/usr/local/share/aclocal
    autoconf
    autoheader
    automake --add-missing

Once `./configure` exists, run:

    mkdir build
    cd build
    ../configure
    make

And you should find a `schismtracker.exe` in the `build/` directory.

## Debugging

Msys comes (if installed) with a 32bit gdb version. you can use it to debug the 32bit version of Schismtracker.

For the 64bit version, you can get it from
[http://sourceforge.net/projects/mingw-w64/files/External%20binary%20packages%20%28Win64%20hosted%29/gdb/](http://sourceforge.net/projects/mingw-w64/files/External%20binary%20packages%20%28Win64%20hosted%29/gdb/)
I got a newer version from here : [http://www.equation.com/servlet/equation.cmd?fa=gdb](http://www.equation.com/servlet/equation.cmd?fa=gdb)
I named the file gdb64 so that it didn't get mistaken for the other.

