# Accessibility

This fork of Schism Tracker includes experimental support for self-voicing style accessibility via [SRAL](https://github.com/m1maker/SRAL), a cross-platform screen reader/speech engine abstraction library.

Although the accessibility implementation is quite rough, all the expected basics should be already working. Read below for more details.

# Why?

Here are some motivating factors that led to this in no particular order:

* Total absence of accessible cross-platform trackers.
Currently the only tracker in the world with complete accessibility support is the excellent [OpenMPT](https://www.openmpt.org), but it's usable only under Windows.
* The fascinating idea of adding an accessibility layer to a completely custom-drawn application.
* I always wanted to experience the feel of truly old-school DOS trackers like Scream Tracker 3 or Impulse Tracker in an accessible way,
but this was obviously not possible.
* I just wanted to try coding in something more low-level than Python or C#.

## Building

Except for the SRAL library dependency, everything should be the same as in vanilla Schism Tracker.

The SRAL library should be easy to build and install with CMake.

Note: SRAL is written in ++. It also requires Speech Dispatcher and libx11 to build under *NIX systems.

After installing SRAL proceed with building the tracker as described in the original Schism Tracker documentation.

## Usage
### Accessibility Stuff

Run the tracker and hit Ctrl-Alt-\ (Backslash) to enable accessibility announcements.

This is actually a toggle that can be used to disable/re-enable them at any time.

#### Additional shortcuts

The accessibility mode doesn't alter the interface in any way, so you can freely use the native help system to get acquainted with the application.

However, some things could not be made usable without adding a few shortcuts. Here is the complete list:

* Ctrl-Alt-\ toggles the accessibility mode. Its state is saved in the configuration file.
* Cursor keys can be used to read the contents of some screens which lack the real cursor, e.g. help, message log, song message in view mode etc.
As usual, Up/Down read by line, Left and Right read by character,
PageUp/PageDown skip several lines,
Home/End read first/last line.
* Ctrl-Alt-Arrows/Home/End/Space can be used to read the entire contents of the application's window.
This can be useful to get some status info, like song name, current pattern/order, sample format etc.
Ctrl-Alt-Space reads the current line.
Virtual cursor position follows keyboard focus.

#### Pattern Editor Tips

The accessibility text reported when moving between rows/channels currently consists of the following parts.

* Current column value, i.e. note/instrument/volume/effect, if any
* Row number
* Channel number
* Column name

Words like "row" or "channel" before the respective numbers are omitted for more efficient navigation. However, I might add an ability to customize the format of this string in the future.

Also, the current row highlight toggle (Ctrl-H) is now overloaded.
In addition to its main function, it changes the accessibility description format when enabled.
The value of the current column is replaced by the entire content of the current cell. This can be useful as a way of having a quick look around patterns.

### Additional Tweaks

I have also included a few tweaks which are not directly related to accessibility but, in my opinion, make the screen reader user experience more pleasant.

They are all optional and can be disabled in the configuration file by setting them to 0.

* keyjazz_play_row makes the tracker play the entire row when entering notes while the song isn't playing.
* play_row_when_navigating does the same but when navigating between the rows.

Additionally, some standard hidden options are enabled by default.
These currently include keyjazz_noteoff and keyjazz_capslock.

Oh, these things make Schism Tracker behave much more like OpenMPT when it comes to note preview.

## Bugs and limitations

Although most things should already work as expected, there are still a few oddities and internal limitations worth to be noted about.

* The Ctrl-Alt-... screen reading cursor cannot be used to read some things, like half-width characters in the pattern editor.
It's meant only to be an aid for reading otherwise unreachable info, not an advanced exploration mode.
* The accessibility of the info page is very basic. Currently only the channel numbers are reported when navigating with arrow keys.
As most of the information on this page is very dynamic, I don't know if there's any point in presenting more details to accessibility users in this case.
* Currently, accessibility support cannot be disabled at compile-time. This makes it impossible to build this fork for platforms like Nintendo Wii. I certainly need to fix this up.
* Font editor is completely inaccessible and fixing it is not my top priority.
Its utility for screen reader users is quite limited, and most importantly, it uses its own very weird focus handler instead of the standard one.
* Some accessibility announcements can still be missing in a few places. They are gradually being added.
