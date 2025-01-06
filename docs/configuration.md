# Configuration

Schism Tracker saves its configuration in a plain-text,
[INI-style](http://en.wikipedia.org/wiki/INI_file) file named `config`. Under
normal circumstances it should theoretically not be necessary to deal with this
file directly. However, there are some options that are not configurable from
within Schism Tracker for one reason or another. In these cases, any plain text
editor should suffice.

The location of this file is dependent on the OS:

- **Windows**
  - `%APPDATA%\Schism Tracker`
- **Mac OS X**
  - `~/Library/Application Support/Schism Tracker`
- **AmigaOS 4**
  - `PROGDIR:`
- **Linux/Unix**
  - `$HOME/.schism/`
- **Wii**
  - Same directory as `boot.elf` (e.g. `sd:/apps/schismtracker`)

Aside from `config`, you may also add a `fonts` subdirectory for custom font
files. By default, `font.cfg` is automatically loaded on startup, and other
`*.itf` files are listed by the built-in font editor (Shift-F12). See [the
links page](https://github.com/schismtracker/schismtracker/wiki/Links) for some
resources on getting fonts for Schism Tracker.

## Potentially useful "hidden" config options

To enable any of these, find the `[section]` in the config file, look for the
`key=`, and change the value. If the key doesn't exist, simply add it.

#### Video

    [Video]
    lazy_redraw=1
    width=640
    height=400
    want_fixed=0
    want_fixed_width=3200
    want_fixed_height=2400

`lazy_redraw` slows down the framerate when the program isn't focused. This
used to be kind of useful when the GUI rendering sucked, and maybe it still is
if you're stuck with a painfully slow video card and software rendering, and
you also want to have a huge window that isn't active.

`width` and `height` are the initial dimensions to use for the window, and the
dimensions to return to when toggling fullscreen off.

If `want_fixed` is set to 1, Schism will be displayed with a constant width and height regardless of the window size. Those values are retrieved from `want_fixed_width` and `want_fixed_height` which correspond to a 4:3 aspect ratio by default.

#### Backups

    [General]
    make_backups=1
    numbered_backups=1

When overwriting a `filename.it`, copy the existing file to `filename.it~`.
With numbered_backups, write to `filename.it.1~`, `filename.it.2~`, etc.

#### Key repeat

    [General]
    key_repeat_delay=125
    key_repeat_rate=25

Alter the key repeat. "Delay" is how long before keys begin to repeat, "rate"
is how long between repeated keystrokes. (Both are in milliseconds.) Above are
Storlek's settings, which are very fast but convenient for speed tracking.

The *default* repeat delay and rate come from your operating system, so you
only need to set this if you like having a different rate for Schism Tracker
than you do for the rest of your system.

#### Alternate font

    [General]
    font=notch.itf

Load some other font besides `font.cfg` at startup. This option doesn't really
have much of a point, because the file listed is limited to those within the
`fonts` directory, and it's just easier to open the font editor, browse fonts,
and save to `font.cfg`.

#### DJ mode

    [General]
    stop_on_load=0

If zero, loading a song when another one is playing will start playing the new
song after it is loaded.

#### File browser

    [Directories]
    module_pattern=*.it\073 *.xm\073 *.s3m\073 *.mtm\073 *.669\073 *.mod

Changes what files are presented in the load/save module lists. Use * for all
files. For annoying compatibility reasons, semicolons are rewritten as `\x3b`
or `\073` when saving.

This was formerly named `filename_pattern`; Schism Tracker ignores the old
value and comments it out when saving to work around bugs in older versions.

    sort_with=strcaseverscmp

Alter the sort order. Possible values are `strcmp` (case-sensitive),
`strcasecmp` (case-insensitive), `strverscmp` (case-sensitive, but handles
numbers smartly e.g. `5.it` will be listed above `10.it`), and
`strcaseverscmp` (case-insensitive and handles numbers smartly).

#### Keyjazz

    [Pattern Editor]
    keyjazz_noteoff=1
    keyjazz_write_noteoff=0
    keyjazz_repeat=0
	keyjazz_capslock=0

If `keyjazz_noteoff` is 1, letting go of a key in the pattern editor will cause
a note-off. If using this, you might also want to consider setting
`keyjazz_repeat` to 0 in order to avoid inserting multiple notes when holding
down keys.

If `keyjazz_write_noteoff` is 1, letting go of a key in the pattern editor will
also write a note off *if* playback tracing (Ctrl+F) is enabled.

If `keyjazz_capslock` is 1, keyjazz will be enabled if Caps Lock is toggled, not if
the key is pressed. This is particularly useful for macOS users where SDL doesn't
send proper key events for the Caps Lock key, see issue #385.

#### Pattern editor behavior tweaks

    [Pattern Editor]
    mask_copy_search_mode=1
    invert_home_end=1

When `mask_copy_search_mode` is set to 1, pressing Enter on a row with no
instrument number will search backward in the channel for an instrument and
switch to that one.

`invert_home_end` changes the order of the Home and End keys to make the cursor
move to the first or last row within the channel before moving to the first or
last channel. FT2 users might want to enable this.

#### Key modifiers

    [General]
    meta_is_ctrl=1
    altgr_is_alt=1

These alter how modifier keys are interpreted. Mac OS X users in particular
might appreciate `meta_is_ctrl`, which allows using the Command/Apple key as
the Ctrl modifier within Schism Tracker. `altgr_is_alt` works similarly.

#### Audio output

    [Audio]
    buffer_size=256
    driver=dsp:/dev/dsp1

These settings define the audio buffer size, and which audio device Schism
Tracker uses. (The other settings in the `[Audio]` section are configurable
from Shift-F1.) `buffer_size` should be a power of two and defines the number
of samples in the mixing buffer. Smaller values result in less audio latency
but could cause buffer underruns and skipping.

`driver` is parsed identically to the `--audio-driver` switch on the command
line. If you're using Alsa on Linux and want to use you can set
`driver=alsa:dmix` to get Schism Tracker to play with other programs. (However,
Alsa completely ignores the latency with dmix so it might cause massive delays
between pressing a note and hearing it, which is why Schism Tracker requests a
"real" device by default.) If neither the `driver` nor `--audio-driver` is set,
the `SDL_AUDIODRIVER`, `AUDIODEV` and `SDL_PATH_DSP` environment variables can
be used to configure Schism's audio output.

    [Diskwriter]
    rate=96000
    bits=16
    channels=2

This defines the sample format used by the disk writer – for exporting to
.wav/.aiff *and* internal pattern-to-sample rendering.

## Hook functions

Schism Tracker can run custom scripts on startup, exit, and upon completion of
the disk writer. These are stored in the configuration directory, and are named
`startup-hook`, `exit-hook`, and `diskwriter-hook` respectively. (On Windows,
append `.bat` to the filenames.) Hooks are useful for making various
adjustments to the system – adjusting the system volume, remapping the
keyboard, etc. The disk writer hook can be used to do additional
post-processing, converting, etc. (Note: on the Wii, hooks are not processed
since there is no underlying OS or command interpreter to run them.)

#### Example

For users with non-US keyboards, some keys may not work properly. This can be
worked around by switching temporarily to a US keyboard layout on startup, and
resetting the keyboard on exit. To define hooks to accomplish this:

    cat >~/.schism/startup-hook <<EOF
    #!/bin/sh
    setxkbmap us
    EOF
    cat >~/.schism/exit-hook <<EOF
    #!/bin/sh
    setxkbmap fi
    EOF
    chmod +x ~/.schism/*-hook

This is for a Finnish keyboard; replace the `fi` with the appropriate [ISO
3166](http://www.wikipedia.org/wiki/ISO%203166-1%20alpha-2) country code or
other keyboard mapping. See `/etc/X11/xkb/symbols` for a list of available
keyboard layouts.
