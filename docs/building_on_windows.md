# Building on Windows

The easiest way to use Schism Tracker on Windows is to download one of the 
pre-built binaries. However, if you want to build it yourself, it isn't too
tricky.

These instructions assume you are using 64-bit Windows to build a 64-bit
Schism executable.

## Installing the tools

We will be using an environment called MSYS2, which provides all the packages we
need in one place.

If you want correct version information to show, you must also 
have [git](https://git-scm.com/) installed and in your PATH.

### Get MSYS2 and install it

Go to the URL http://msys2.github.io/ and download the 64-bit installer.

Once installed, follow these instructions to get up-to-date files. This process
is also described in their web page, so in case of conflict, you might opt to
follow their instructions.

Run the MSYS2 shell (a start menu shortcut should have been created) and update
the pacman package manager:

	pacman -Sy pacman
	
Follow the onscreen options and choose "yes" where prompted. 
Close the MSYS2 window. Run it again from the Start menu and update the system:

	pacman -Syu
	
Again follow the instructions, chose "yes" where prompted, and close the MSYS2 window. 

Run it again from the Start menu _(note: the update
process can, in some cases, break the start menu shortcut - in which case you
may need to look in C:\msys64 (or wherever you installed MSYS2) and run
msys2\_shell.cmd)_ and update the rest with:

	pacman -Su

### Install the toolchains

Once you have the shell environment ready, it's time to get the compilers.
Execute the following command:

	pacman -S mingw-w64-x86_64-toolchain libtool autoconf automake make perl

If asked to "enter a selection", hit Enter to go with the default.

Also, you need the following specific dependency:

	pacman -S mingw-w64-x86_64-SDL2

For FLAC sample loading, you'll also need the following dependency:

	pacman -S mingw-w64-x86_64-flac
	
Once you have installed these packages, close all your MSYS2 windows 
before continuing with the instructions.

## Compilation

MSYS2 installs three shortcut icons, one to run the MSYS2 shell, and two more
that setup the environment to use either the 32bit compiler or the 64-bit
compiler. We will be using the one called "MSYS2 MINGW64" throughout.

If you've lost the shortcuts, you can also start the 64bit compiler with

	msys2_shell.cmd -mingw64

### Configure schismtracker to build

Open the 64-bit shell.

Download the Schismtracker sources (or clone the repo) and navigate to the
schismtracker-master folder (the one that contains README.md) using `cd`

Drive letters are mapped to /x , example C:/
is /c/, D:/ is /d/ ..., and so on. For example:

	cd /c/Users/YourUserName/Downloads/schismtracker-master/

Reconfigure it:

	autoreconf -i

_(note: if you get a "possibly undefined macro: AM\_PATH\_SDL" error, you're
probably using the standard msys2 shell - either use the mingw start menu
shortcuts, or start `msys2_shell.cmd` with `-mingw64` as mentioned above)_

Make a folder to build the binary in:

	mkdir build
	
Now move into the build subdir and run the configure script:

	cd build
	../configure
	
### Build and rebuild

In order to build Schism, from the build folder, run:

	make
	
You should now have an executable in the build folder that you can run from
Windows Explorer, or with 
	
	./schismtracker.exe

After the first time, you can usually build Schism again without having to run 
`autoreconf` or `../configure` again, but if you run into problems, follow the
steps from "Configure schismtracker to build" onwards again.

### Compilation problems

The configure script should give hints on what is missing to compile. If you've
followed the steps, everything should already be in the right place, but in case 
it doesn't work, see the config.log file, which reports a detailed output (more 
than what is seen on the screen) that could help identify what is missing, or which
option is not working.

### Debugging

When installing the toolchains, the gdb debugger is also installed. You can run this
from the MSYS2 MINGW64 shell if you need to debug Schism.

## Preparing for distribution or sharing to other machines

To distribute the application, it is important to bundle the correct version of
the SDL.dll file with the executable. For a 64bit build, the file is located in 
`/msys2_path/mingw64/bin/SDL.dll`

If you want to reduce the exe size (removing the debugging information), use
the following command from MSYS2 MINGW64:

	strip -g schismtracker.exe
