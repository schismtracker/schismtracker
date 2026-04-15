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
Tracker runs on almost any platform that [SDL 2](https://www.libsdl.org/index.php) 
supports. Currently builds are provided for Linux, Mac OS X, and Windows. Most 
development is currently done on 64-bit Linux. Schism will most likely build on
_any_ architecture supported by GCC4 (e.g. alpha, m68k, arm, etc.) but it will 
probably not be as well-optimized on many systems.

See [the wiki](https://github.com/schismtracker/schismtracker/wiki) for more
information.

![screenshot](http://schismtracker.org/screenie.png)

## Download

The latest stable builds for Windows, macOS, and Linux are available from [the
releases page](https://github.com/schismtracker/schismtracker/releases). Builds
can also be installed from some distro repositories on Linux, but these
versions may not have the latest bug fixes and enhancements. Older builds for
other platforms can be found on
[the wiki](https://github.com/schismtracker/schismtracker/wiki). Installing via
Homebrew on macOS is no longer recommended, as the formula for Schism Tracker
is not supported or maintained by anyone directly involved in the project.

## Compilation

See the
[docs/](https://github.com/schismtracker/schismtracker/tree/master/docs) folder
for platform-specific instructions.

### WebAssembly (browser) build

A web build scaffold is available via Emscripten.

```sh
source /path/to/emsdk/emsdk_env.sh
./scripts/build-web.sh
```

Then run the generated files with any static server:

```sh
cd build-web/dist
python3 -m http.server 8080
```

Open <http://localhost:8080>.

The generated web runner supports:

- module open/save from the browser UI
- persistent storage via IndexedDB (`/persistent/modules`)
- stored module management via `Stored Action` + `Run Action`
  (`Load Stored`, `Refresh List`, `Rename Stored`, `Delete Stored`)
- display scale switching (`Fit Window`, `x1`, `x2`, `x3`)

Quick first-run checklist:

1. Open one module with `Open Module` and confirm tracker playback/UI response.
2. Reload the page and verify the file appears in the stored modules list.
3. Pick `Load Stored` in `Stored Action`, click `Run Action`, and verify load.
4. Try `Rename Stored` and `Delete Stored` from `Stored Action`, then reload to confirm persistence.

For full details, see `docs/building_for_web.md`.

## Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/schismtracker.svg)](https://repology.org/project/schismtracker/versions)
