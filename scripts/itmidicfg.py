#!/usr/bin/env python
# coding: utf-8
import sys, struct, time, os

"""
No surprises with this format, it's stored exactly the same way as the
embedded MIDI data within IT files, namely as an array of 153 entries of
32 bytes apiece, each terminated with \0, in the same order as presented
in Impulse Tracker's MIDI Output Configuration screen:

        MIDI Start
        MIDI Stop
        MIDI Tick
        Note On
        Note Off
        Change Volume
        Change Pan
        Bank Select
        Program Change
        Macro Setup
                SF0 -> SFF
                Z80 -> ZFF

Every line should be present, for a total expected file size of
(9 + 16 + 128) * 32 = 4896 bytes.
"""


if len(sys.argv) != 2 or sys.argv[1] in ["--help", "-h"]:
        print("usage: %s /path/to/itmidi.cfg >> ~/.schism/config" % sys.argv[0])
        sys.exit(0)

def warning(s):
        sys.stderr.write("%s: %s\n" % (sys.argv[1], s))
def fatal(s):
        warning(s)
        sys.exit(5)

itmidi = open(sys.argv[1], "rb")

def readvalue(k):
        try:
                e = itmidi.read(32)
                if len(e) != 32:
                        fatal("file is truncated, should be 4896 bytes")
                e = e.decode("ascii", "replace").split("\0")[0]
                e.encode("ascii") # test
        except IOError:
                fatal("read error")
        except UnicodeError:
                warning("garbage data encountered for %s" % k.replace("_", " "))
                e = ""
        return e

midiconfig = [(k, readvalue(k)) for k in
        ["start", "stop", "tick", "note_on", "note_off", "set_volume", "set_panning", "set_bank", "set_program"]
        + ["SF%X" % n for n in range(16)]
        + ["Z%02X" % n for n in range(128, 256)]]
try:
        if itmidi.read(1):
                warning("warning: file has trailing data")
except IOError:
        # Don't care anymore.
        pass


print("# MIDI configuration imported from Impulse Tracker on %s" % time.ctime())
print("")
print("[MIDI]")
for (k, v) in midiconfig:
        print("%s=%s" % (k, v))

