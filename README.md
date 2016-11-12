# Schism Tracker

Schism Tracker is a free and open-source reimplementation of [Impulse
Tracker](https://github.com/schismtracker/schismtracker/wiki/Impulse-Tracker),
a program used to create high quality music without the requirements of
specialized, expensive equipment, and with a unique "finger feel" that is
difficult to replicate in part. The player is based on a highly modified
version of the [Modplug](https://openmpt.org/legacy_software) engine, with a
number of bugfixes and changes to [improve IT
playback](https://github.com/schismtracker/schismtracker/wiki/Player-abuse-tests).

Where Impulse Tracker was limited to i386-based systems running MS-DOS, Schism
Tracker runs on almost any platform that [SDL](http://www.libsdl.org/)
supports, and has been successfully built for Linux, Mac OS X, Windows,
FreeBSD, AmigaOS, BeOS, and even [the
Wii](http://www.wiibrew.org/wiki/Schism_Tracker). Schism will most likely build
on *any* architecture supported by GCC4 (e.g. alpha, m68k, arm, etc.) but it
will probably not be as well-optimized on many systems.

See [the wiki](https://github.com/schismtracker/schismtracker/wiki) for more
information.

![screenshot](http://schismtracker.org/screenie.png)

## Download

The latest stable builds for Windows and Linux are available from [the releases
page](https://github.com/schismtracker/schismtracker/releases). An
OSX build can be installed via Homebrew with `brew install schismtracker`. Older builds for other platforms can be found
on [the wiki](https://github.com/schismtracker/schismtracker/wiki).

## Compilation

See the
[docs/](https://github.com/schismtracker/schismtracker/tree/master/docs) folder
for platform-specific instructions.

## Build Status

|                                | **Status**                |
| -----------------------------: | :------------------------ |
|                     **GitHub** | [![GitHub release][1]][2] |
|     **Travis** (Linux & macOS) | [![Master][3]][4]         |
|         **AppVeyor** (Windows) | [![Master][5]][6]         |

[1]: https://img.shields.io/github/release/schismtracker/schismtracker.svg
[2]: https://github.com/schismtracker/schismtracker/releases/latest
[3]: https://travis-ci.org/schismtracker/schismtracker.svg?branch=master
[4]: https://travis-ci.org/schismtracker/schismtracker
[5]: https://img.shields.io/appveyor/ci/jangler/schismtracker/master.svg
[6]: https://ci.appveyor.com/project/jangler/schismtracker/branch/master
