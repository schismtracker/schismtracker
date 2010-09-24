#!/usr/bin/env python
# coding: utf-8
import sys, struct, time, os

"""
Here is the general layout of Impulse Tracker's configuration file (IT.CFG).
It's quite a bit messier than the .IT format, probably because it wasn't at
all intended for people to be tinkering with. Impulse Tracker has practically
no error checking when loading the configuration, so it's very easy to cause
a crash or hang just by changing values around in a hex editor.



        0   1   2   3   4   5   6   7   8   9   A   B   C   D   E   F
      ┌───────────────────────────────────────────────────────────────┐
0000: │ Directories: module, sample, instrument (70 chars each)       │
      ├───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┤

      ├───┴───┼───┼───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┤
00D0: │.......│Kbd│ Palette settings (48 bytes, 3 per color)          │
      ├───┬───┼───┼───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┤

      ├───┴───┴───┼───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┴───┤
0100: │...........│ Info page view setup (6 * 8 bytes, see below)     │
      ├───┬───┬───┼───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┤

      ├───┴───┴───┼───┴───┼───┴───┼───┴───┼───┼───┼───┼───┼───┴───┼───┤
0130: │...........│NumView│  ???  │NormTrk│Min│Maj│Msk│Div│TrkCols│TVS│
      ├───────────┴───────┴───────┴───────┴───┴───┴───┴───┴───────┴───┤
0140: │ Track view scheme (2 * 100 bytes)                             │
      ├───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┬───┤

      ├───┴───┴───┴───┴───┴───┴───┼───┼───┼───┼───┼───┼───┴───┼───┼───┘
0200: │...........................│VCT│Lnk│PF1│Amp│C-5│FastVol│PF2│
      └───────────────────────────┴───┴───┴───┴───┴───┴───────┴───┘

      Each directory name is 70 characters although only 64 are used.
      The first \0 byte terminates the name (unlike some other parts of
      the interface).

      The palette is stored in the obvious fashion, three bytes per color
      going down the columns, for the red/green/blue component of each of
      the 16 colors. Each component value is in the range 0-63.


          Kbd:  Keyboard type (ignored by IT 2.04+)
                        0 = United States
                        1 = United Kingdom
                        2 = Sweden/Finland
                        3 = Spain
                        4 = Portugal
                        5 = Netherlands
                        6 = Italy
                        7 = Germany
                        8 = France

      NumView:  Number of views active on info page. Even though the config
                file supports six views, IT only allows creating five.
                (However, adding a sixth with a hex editor doesn't cause any
                evident problems)

      NormTrk:  Number of tracks in "normal" (13-column, ST3-style) view in
                the pattern editor.

      Min/Maj:  Pattern row hilight information.

         Mask:  Pattern edit copy mask:
                        Bit 0: Instrument
                        Bit 1: Volume
                        Bit 2: Effect

          Div:  Whether to draw the divisions between channels.
                Values other than 0 or 1 will make the track divisions
                "stuck", i.e. Alt-H will not disable them.

      TrkCols:  How many columns to draw the box around the track view.
                This also adjusts the positioning of the "normal" channels.
                If zero, no track view is drawn.
                This should not have a value of 1.

          VCT:  View-channel cursor tracking (Ctrl-T)
                Values other than 0 or 1 will make the cursor tracking
                "stuck", i.e. Ctrl-T will not disable it.

         Link:  Link/Split effect column (zero means split)

          PF1:  Bit 0. On = Centralise cursor in pattern editor
                Bit 1. On = Highlight current row
                Bit 2. On = Fast volume mode (Ctrl-J) enabled
                Bit 3. On = MIDI: Tick Quantize
                Bit 4. On =       Base Program 1
                Bit 5. On =       Record Note Off
                Bit 6. On =       Record Velocity
                Bit 7. On =       Record Aftertouch

          Amp:  MIDI volume amplification (percentage).

          C-5:  MIDI C-5 note. Default value is 60, i.e. C-5.

      FastVol:  Fast volume percentage. Although this is a 16-bit value, the
                fast volume dialog only changes the low byte, so if the high
                byte is nonzero, the fast volume settings will be "stuck".

          PF2:  Bit 0. On = Show default volumes in normal pattern view.
                Bit 1. On = MIDI: Cut Note Off


Track view scheme.

        The track view settings are stored as 100 (!) two-byte pairs of
        channel and view type. Channel number is first, numbered from 0;
        0FFh is the end mark.
        Values for the view type:
                0 = 13-column, nnn ii vv eee
                1 = 10-column, nnniivveee
                2 = 7-column, nnnivee (half-width inst / vol / effect value)
                3 = 3-column, nnn/_ii/_vv/eee
                4 = 2-column, nn/ii/vv/ee (half-width effect values)



Info Page settings (starting at 0103h):

        3   4   5   6   7   8   9   A   B   C   D   E   F   0   1   2
      ┌───────┬───┬───┬───────┬───────╥───────┬───┬───┬───────┬───────┐
xxxx: │ Type  │ ? │Row│Height │MemOff ║ Type  │ ? │Row│Height │MemOff │
      └───────┴───┴───┴───────┴───────╨───────┴───┴───┴───────┴───────┘

         Type:  Window "type" of respective info page window
                        0 = Sample names
                        1 = 5-channel pattern view
                        2 = 8-channel pattern view
                        3 = 10-channel pattern view
                        4 = 24-channel pattern view
                        5 = 36-channel pattern view
                        6 = 64-channel pattern view
                        7 = Global volume / active channel count
                        8 = Note dots
                        9 = Technical details

          Row:  Row number on screen to draw the view.
                Numbering starts at 0, and counts downward from the top of
                the screen. The first info page view should be on row 12.

       Height:  How many rows the view uses. IT disallows sizing windows
                smaller than 4 rows, but the hard minimum is 3.
                (Setting most views smaller than that *will* cause a crash.)

       MemOff:  VGA memory offset of beginning of first row.
                Should be equal to 2 * 80 * row.



File format changes:

        IT <1.06 - original (?) data size is 521 bytes.
                (Note: only tested with 1.05; information on other versions
                would be appreciated!)

        IT 1.06 - 522 bytes
                First flag byte (PF1) added.

        IT 2.04 - 522 bytes
                Keyboard type byte no longer used due to the introduction
                of KEYBOARD.CFG.

        IT 2.05 - 522 bytes
                24-channel track view added, resulting in renumbering of
                the 36-channel view, global volume, and technical data.

        IT 2.06 - 526 bytes
                Amp, C-5, and FastVol bytes added.

        IT 2.11 - 526 bytes
                Info page settings adjusted slightly to use an extra row at
                the bottom of the screen (just increased the 'height' value
                for the last visible window by one).

        IT 2.12 - 527 bytes
                Second flag byte (PF2) added. Also, 64-channel and note dots
                views introduced on the info page, thus causing the global
                volume and technical data to be renumbered again.

        IT 2.14p5 - 1337 bytes
                No apparent changes to the file format aside from the size.
                I can't see a purpose for the extra bytes, aside from leet...
                at least, changing those values seems to have no effect on
                the operation of the tracker.

        Note that no automatic configuration adjustments are made in any IT
        version when loading a configuration file written by an older version
        of the tracker (as there's no real version information in the file).



And there you have it. If you have any questions, comments, anomalous IT.CFG
files, or a decidedly ancient version of Impulse Tracker, send an e-mail to
<storlek@rigelseven.com> or find me on IRC (/msg Storlek on Freenode, and
usually also SynIRC and EFnet).

        -- Storlek - http://rigelseven.com/ - http://schismtracker.org/
"""

def warning(s):
        sys.stderr.write(s + "\n")

def getnth(n):
        if n > 3 and n < 21:
                return "%dth" % n
        return {1: "%dst", 2: "%dnd", 3: "%drd"}.get(n % 10, "%dth") % n

def pluralize(n, s, pl=None):
        return "%g %s" % (n, [pl or s + "s", s][n == 1])

def asciiz(s):
        try: s = s.decode("cp437", "replace")
        except: pass
        return s.split("\0")[0]


if len(sys.argv) != 2 or sys.argv[1] in ["--help", "-h"]:
        print("usage: %s /path/to/it.cfg > ~/.schism/config" % sys.argv[0])
        sys.exit(0)

itcfg = open(sys.argv[1], "rb")
try:
        itcfg.seek(0, 2) # EOF
        itcfg_size = itcfg.tell()
        if itcfg_size < 527:
                warning("file is too small -- resave with IT 2.12+")
                sys.exit(1)
        itcfg.seek(0) # back to start
except IOError:
        warning("%s: failed to seek, what kind of non-file is this?" % sys.argv[1])
        sys.exit(1)



dir_modules, dir_samples, dir_instruments = map(asciiz, struct.unpack("70s 70s 70s", itcfg.read(210)))

# Not used by IT 2.04+
it203_keyboard = min(struct.unpack("B", itcfg.read(1))[0], 9)

palette = "".join([".0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz"[b & 0x3f]
                for b in struct.unpack("48B", itcfg.read(48))])


# Info page nastiness.

info_window_types = [
        "samples",
        "track5",
        "track8",
        "track10",
        "track12",
        "track18",
        "track24", # added in 2.05
        "track36",
        "track64", # added in 2.12
        "global",
        "dots", # added in 2.12
        "tech",
]

infopage_data = [itcfg.read(8) for n in range(6)]
num_views, unknown = struct.unpack("<HH", itcfg.read(4))
if num_views == 0:
        num_views = 1
elif num_views > 6:
        warning("info page: too many views, will probably crash IT")
        num_views = 1

nextrow = 12 # row that the next view should start on
infopage_layout = []

for n, data in enumerate(infopage_data[:num_views]):
        nth = getnth(n + 1)
        # XXX what's this unknown byte? always seems to be 0, but changing it seems not to affect anything.
        wtype, unknown, firstrow, height, offset = struct.unpack("<HBBHH", data)

        # "offset" is the position in view memory where the data should be written (but not the box)
        # This should be at the start of the row, i.e. row * 80 characters/row * 2 bytes/character
        if offset != 160 * firstrow:
                warning("info page: %s view has strange VGA offset (will look scrambled or crash)" % nth)
        if n == 0 and firstrow != nextrow:
                warning("info page: %s view is on row %d (should be row %d)" % (nth, firstrow, nextrow))
        elif firstrow < nextrow:
                warning("info page: %s view overlaps previous by %s"
                        % (nth, pluralize(nextrow - firstrow, "row")))
        elif firstrow > nextrow:
                warning("info page: %s view followed by %s"
                        % (nth, pluralize(firstrow - nextrow, "empty row")))
        if height < 3:
                warning("info page: %s view is %s too short (will cause crash/hang)"
                        % (nth, pluralize(3 - height, "row")))
        elif firstrow + height > 50:
                warning("info page: %s view is %s too tall (might cause crash)"
                        % (nth, pluralize(firstrow + height - 50, "row")))
        if wtype > 11:
                warning("info page: %s view has unknown window type %d (will cause crash)"
                        % (nth, wtype))
                wtype = 0 # something sane

        nextrow = firstrow + height

        # Work around a Schism Tracker oddity: the saved height of the first view is off by one.
        # I'm sure there was a good reason for this, but I have no idea what it might have been.
        if n == 0:
                height -= 1
        infopage_layout.append("%s %d" % (info_window_types[wtype], height))

# IT 2.11 added an extra row to the info page
if nextrow == 49:
        warning("info page: extra row at bottom of screen (old IT version?)")
elif nextrow < 50:
        warning("info page: %d extra rows at bottom of screen (corrupt config?)" % 50 - nextrow)
elif nextrow > 50:
        warning("info page: data extends %s beyond end of screen (corrupt config?)"
                % pluralize(nextrow - 50, "row"))

if num_views == 6:
        # as far as I can tell, this requires a hex editor, but it works fine once enabled
        # (handled here rather than above to allow error-checking the rest of the settings)
        warning("info page: six views visible, omghax")
infopage_layout = " ".join(infopage_layout[:num_views])


# Pattern editor settings

normal_view, rhmin, rhmaj, edit_copy_mask, draw_divisions, track_cols = struct.unpack("<H4BH", itcfg.read(8))

edit_copy_mask = 1 | ((edit_copy_mask & 7) << 1) # blah

if draw_divisions not in [0, 1]:
        warning("pattern editor: weird track-divisions value %d; Alt-H won't work right" % draw_divisions)
draw_divisions = bool(draw_divisions)

# track_cols should be 0 (no track view drawn) or 2+ (indicates width of box around track view channels)
if track_cols == 1:
        warning("pattern editor: track view has width of 1 (will freeze IT)")
        # I guess none?
        track_cols = 0
track_view_visible = bool(track_cols)

# I'm not quite sure about this math, haven't tested it completely
if track_cols + 14 * normal_view > 78:
        warning("pattern editor: track setup is too wide, display will look trashed")


track_view_scheme = []
end_reached = False
prevchannel = -1

for n, (channel, scheme) in [struct.unpack("BB", itcfg.read(2)) for n in range(100)]:
        nth = getnth(n + 1)
        if channel == 0xff:
                # End marker.
                break
        elif scheme > 4:
                warning("pattern editor: %s view uses out-of-range scheme %d, will crash IT" % (nth, scheme))
                break

        if scheme > 3:
                # adjust up for Schism's added 6 column / 12 channel view
                scheme += 1
        if channel > 63:
                warning("pattern editor: %s track view shows channel %d (weird but harmless)"
                        % (nth, channel + 1))
        elif prevchannel + 1 != channel:
                warning("pattern editor: tracks not in sequential order -- Schism Tracker can't do this")
        track_view_scheme.append(scheme)
        prevchannel = channel

# might be nice to check each track's width and compare with the width of the box

if track_view_visible and not track_view_scheme:
        warning("pattern editor: track view setup was blank... strange!")
        track_view_visible = False
if normal_view and track_view_visible:
        # Split view -- channel data is displayed on both left and right of row numbers
        warning("pattern editor: split track view unimplemented in Schism Tracker")
elif not track_view_visible:
        # Normal (5-channel) view, not affected by alt-h etc.
        # Handle this by building a 5-channel track view instead (blah)
        draw_divisions = True
        track_view_scheme = []

track_view_scheme = "".join(["abcdefghijklmnopqrstuvwxyz"[v] for v in track_view_scheme])

(view_tracking, link_effect_column, pflag1, midi_amplification,
        midi_c5note, fast_volume_percent, pflag2
) = struct.unpack("<5BHB", itcfg.read(8))

if view_tracking not in [0, 1]:
        warning("pattern editor: weird view tracking value %d; Ctrl-T won't work right" % view_tracking)
if view_tracking and normal_view > 0:
        warning("pattern editor: view tracking unimplemented in Schism Tracker")
# link/split is not quite as weird -- neither button will appear to be "pressed",
# but it still operates fine (acts like 'link') and selecting either mode fixes it

if fast_volume_percent > 255:
        warning("pattern editor: fast volume percent has high byte set, Alt-J will be broken")

view_tracking = bool(view_tracking)
link_effect_column = bool(link_effect_column)
centralise_cursor = bool(pflag1 & 1)
highlight_current_row = bool(pflag1 & 2)
fast_volume_mode = bool(pflag1 & 4)
midi_tick_quantize = bool(pflag1 & 8)
midi_base_program_1 = bool(pflag1 & 16)
midi_record_note_off = bool(pflag1 & 32)
midi_record_velocity = bool(pflag1 & 64)
midi_record_aftertouch = bool(pflag1 & 128)
show_default_volumes = bool(pflag2 & 1)
midi_cut_note_off = bool(pflag2 & 2)


# I hate how Schism Tracker saves MIDI settings...
midi_flags = 0
if midi_tick_quantize: midi_flags |= 1
if midi_base_program_1: midi_flags |= 2
if midi_record_note_off: midi_flags |= 4
if midi_record_velocity: midi_flags |= 8
if midi_record_aftertouch: midi_flags |= 16
if midi_cut_note_off: midi_flags |= 32



# finally! dump the config

print("# Configuration imported from Impulse Tracker on %s" % time.ctime())
if it203_keyboard != 0:
        print("# Note: keyboard set to %s (IT <=2.03)" % [
                "United States", "United Kingdom", "Sweden/Finland", "Spain",
                "Portugal", "Netherlands", "Italy", "Germany", "France", "unknown"
        ][it203_keyboard])
print("")

print("[Directories]")
print("modules=%s" % dir_modules.replace("\\", "\\\\"))
print("samples=%s" % dir_samples.replace("\\", "\\\\"))
print("instruments=%s" % dir_instruments.replace("\\", "\\\\"))
print("sort_with=strcasecmp")
print("")
print("[General]")
print("classic_mode=1") # ;)
print("palette_cur=%s" % palette)
print("")
print("[Pattern Editor]")
print("link_effect_column=%d" % link_effect_column)
print("draw_divisions=%d" % draw_divisions)
print("centralise_cursor=%d" % centralise_cursor)
print("highlight_current_row=%d" % highlight_current_row)
print("show_default_volumes=%d" % show_default_volumes)
print("edit_copy_mask=%d" % edit_copy_mask)
print("fast_volume_percent=%d" % fast_volume_percent)
print("fast_volume_mode=%d" % fast_volume_mode)
print("track_view_scheme=%s" % track_view_scheme)
print("highlight_major=%d" % rhmaj)
print("highlight_minor=%d" % rhmin)
print("")
print("[MIDI]")
print("flags=%d" % midi_flags)
print("amplification=%d" % midi_amplification)
print("c5note=%d" % midi_c5note)
print("pitch_depth=0")

