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

#### Browser port in this fork (`web-emscripten`)

The **WebAssembly / in-browser** work in this repository lives on the
[`web-emscripten`](https://github.com/koide-at/schismtracker-emscripten/tree/web-emscripten)
branch of the fork
[**schismtracker-emscripten**](https://github.com/koide-at/schismtracker-emscripten),
which tracks upstream
[**schismtracker/schismtracker**](https://github.com/schismtracker/schismtracker)
(`master`). That branch adds the Emscripten pipeline, the generated runner in
`scripts/build-web.sh`, and related UI/docs so Schism can run in a modern
browser; it is **not** an official upstream release channel.

Some of the browser-port changes were written or refactored with help from
**AI-assisted coding tools** (for example Cursor). Treat this port as
experimental: always compare against upstream sources and run your own tests
before relying on it.

#### Related documentation

| Document | Description |
| -------- | ----------- |
| [docs/building_for_web.md](docs/building_for_web.md) | Web/WASM build steps, status, and troubleshooting (English) |
| [docs/building_for_web_ja.md](docs/building_for_web_ja.md) | Same topic (Japanese) |
| [docs/shortcuts_ja.md](docs/shortcuts_ja.md) | Shortcut reference in Japanese, including Web rebind notes |
| [docs/shortcuts_ja_audit.md](docs/shortcuts_ja_audit.md) | Re-audit of shortcut coverage vs current sources (Japanese) |

The build entry point is [scripts/build-web.sh](scripts/build-web.sh).

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
- Web MIDI input/output bridge (browser permission required)
- configurable app shortcuts from `System Configuration -> Shortcuts -> Configure...`
  (duplicate bindings are rejected and conflict target is shown)

Shortcut note for browser builds:

- `Ctrl+F1` can be reserved by browser/OS in some environments.
- If so, open shortcut settings with `Ctrl+Shift+F1`, then navigate from there.

Quick first-run checklist:

1. Open one module with `Open Module` and confirm tracker playback/UI response.
2. Reload the page and verify the file appears in the stored modules list.
3. Pick `Load Stored` in `Stored Action`, click `Run Action`, and verify load.
4. Try `Rename Stored` and `Delete Stored` from `Stored Action`, then reload to confirm persistence.

## Packaging status

[![Packaging status](https://repology.org/badge/vertical-allrepos/schismtracker.svg)](https://repology.org/project/schismtracker/versions)
