# Building on Windows

_This page was based originally on the COMPILE-WIN32.txt file traditionally provided with
Schism Tracker sources. It has been rewriten with instructions that use newer tools._

## Software needed

To compile on Windows, the following things are needed:

* mingw-gcc
* Python
* SDL headers and libs
* An environment in which to run them, like msys.

If you want proper version information, you'll need git installed and in your path too

## Installing MSYS2 and mingw
These instructions describe how to install msys2, which includes all of the required packages.

### Get MSYS2 and install it
Go to the URL http://msys2.github.io/ and download either the 32bit installer or the 64bit installer.
The 32bit download can run on 32bit windows and the 64bit one requires a 64bit Windows.

64bit executables can be created with either of them.

Once installed, follow these instructions to get up-to-date files. This process is also described in their
web page, so in case of conflict, you might opt to follow their instructions.

Run the MSYS2 shell (a start menu shortcut should have been created) and update the pacman package manager:

    pacman -Sy pacman
	
Close the MSYS2 window. Run it again from the Start menu and update the system:

	pacman -Syu
	
Close the MSYS2 window. Run it again from the Start menu _(note: the update process can, in some cases, break the start menu shortcut - in which case you may need to look in C:\msys64 (or wherever you installed msys) and run msys2\_shell.cmd)_ and update the rest with:

	pacman -Su

### Install the toolchains

Once you have the shell environment ready, it's time to get the compilers. Execute the following command:

	pacman -S mingw-w64-i686-toolchain libtool autoconf automake make

Also, you need the following specific dependency:

	pacman -S mingw-w64-i686-SDL

If you also want to build for 64bits:

	pacman -S mingw-w64-x86_64-toolchain mingw-w64-x86_64-SDL
	
You can search for packages with

	pacman -Ss package descripption


## Compilation

MSYS2 installs three shortcut icons, one to run the msys shell, and two more that setup the
environment to use either the 32bit compiler or the 64bit compiler.

You can also start the 32bit compiler with 

	msys2_shell.cmd -mingw32
	
and the 64bit compiler with

	msys2_shell.cmd -mingw64

### Configure schismtracker to build

Open the 32bit or 64bit shell depending on which version you want to build.
The steps here only need to be done once, or when configure.ac or the installed package versions change.

Go to schismtracker sources root (drive letters are mapped to /x , example C:/ is /c/, D:/ is /d/ ...)

Reconfigure it:

	autoreconf -i

_(note: if you get a "possibly undefined macro: AM\_PATH\_SDL" error, you're probably using the standard msys2 shell - either use the mingw start menu shortcuts, or start msys2_shell.cmd with either -mingw32 or -mingw64 as mentioned above)_

If you're planning to build both 32- and 64-bit binaries, you may wish to create the subfolders
build32 and build64:

	mkdir build32
	mkdir build64

and then follow the rest of the instructions twice, once with the -mingw32 shell in the build32 subdir, and once with the -mingw64 shell in the build64 subdir.

Otherwise just build will do:

	mkdir build
	
Now move into the build subdir and run the configure script:

	cd build	# or build32 or build64 as appropriate
	../configure
	
### Build and rebuild

In order to build and run it, from the appropriate build subdir, run these:

	make
	../schismtracker &

### Compilation problems

The configure script should give hints on what is missing to compile. If you've followed the steps, everything
should already be in the place, but in case it doesn't, see the config.log file, which reports a detailed
output (more than what is seen on the screen) that could help identify what is missing, or which option is not working.


### Debugging

When installing the toolchains, the gdb debugger is also installed. 
Run it from the win32 shell to debug the 32bit exe, or run it from the Win64 shell to debug the 64bit one.


## Prepare the distribution file

To distribute the application, it is important to bundle the correct version of the SDL.dll file with the executable.

For a 32bit build, the file is located in  /msys2_path/mingw32/bin/SDL.dll
For a 64bit build, the file is located in  /msys2_path/mingw64/bin/SDL.dll

The 32bit build also requires the files /msys2_path/mingw64/bin/libgcc_s_dw2-1.dll and /msys2_path/mingw64/bin/libwinpthread-1.dll

If you want to reduce the exe size (removing the debugging information), use the following command:

_(note: you MUST do this from the same shell than you used to build the executable, as the strip tool is architecture-dependent)_

	strip -g schismtracker.exe


## SDL2 notes

The current version of schismtracker uses SDL1, but a fork with SDL2 has been made here https://github.com/davvid/schismtracker/tree/laptop-octave

In order to build that branch, installing the SDL2 packages AND pkg-config is needed

	pacman -S mingw-w64-i686-SDL2 mingw-w64-i686-SDL2_gfx mingw-w64-i686-pkg-config mingw-w64-x86_64-SDL2 mingw-w64-x86_64-SDL2_gfx mingw-w64-x86_64-pkg-config

