# Accessibility

This fork of Schism Tracker includes experimental support for self-voicing style accessibility via [SRAL](https://github.com/m1maker/SRAL), a cross-platform sscreen reader/speech engine abstraction library.

Although the accessibility functions are somewhat rough, all the expected basics should be already working. Read below for more details.

# Why?

Here are some motivating factors that led to this in no particular order:

* Total absence of accessible cross-platform trackers.
Currently the only tracker in the world with complete accessibility support is the excellent [OpenMPT](https://www.openmpt.org), but it's usable only under Windows.
* The idea of adding an accessibility layer to a completely custom-drawn application.
* I always wanted to experience the feel of truly old-school DOS trackers like Scream Tracker 3 or Impulse Tracker in an accessible way,
but this was obviously not possible.
* I just wanted to try coding in something more low-level than Python or C#.

## How to use this?
