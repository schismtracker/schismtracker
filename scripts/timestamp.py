#!/usr/bin/env python
import struct, sys


"""
Each timestamp field is an eight-byte value, first storing the time the
file was loaded into the editor in DOS date/time format (see
http://msdn.microsoft.com/en-us/library/ms724247(VS.85).aspx for details
of this format), and then the length of time the file was loaded in the
editor, in units of 1/18.2 second. (apparently this is fairly standard
in DOS apps - I don't know)
A new timestamp is written whenever the file is reloaded, but it is
overwritten if the file is re-saved multiple times in the same editing
session.

Thanks to Saga Musix for finally figuring this crazy format out.
"""

for filename in sys.argv[1:]:
        f = open(filename, 'rb')
        if f.read(4) != 'IMPM'.encode('ascii'):
                print("%s: not an IT file" % filename)
                continue
        f.seek(0x20)
        ordnum, insnum, smpnum, patnum = struct.unpack('<4H', f.read(8))
        f.seek(0x2e)
        special, = struct.unpack('<H', f.read(2))
        if not (special & 2):
                print("%s: history flag not set (old IT version?)" % filename)
                continue
        para = []
        if special & 1: # message exists
                f.seek(0x36)
                msglen, msgoff = struct.unpack('<HL', f.read(6))
                if msglen:
                        para.append(msgoff)
        f.seek(0xc0 + ordnum)
        # we'll assume history is invalid if it ends after the first parapointer
        para_count = insnum + smpnum + patnum
        para.extend(struct.unpack('<%sL' % para_count, f.read(4 * para_count)))
        hist, = struct.unpack('<H', f.read(2))
        hist_start = f.tell()
        hist_len = hist_start + 8 * hist
        try:
                para_min = min(filter(None, para))
        except ValueError:
                para_min = hist_len
        # history entry count follows parapointers
        if not hist:
                print("%s: history missing (probably not saved with IT)" % filename)
                continue
        if para_min < hist_len:
                print("%s: history data overlaps parapointers (malformed?)" % filename)
                continue
        histdata = f.read(8 * hist)
        if len(histdata) != 8 * hist:
                print("%s: history malformed (probably not saved by IT)" % filename)
                continue
        f.close()
        print(filename)
        totalticks = 0
        def ticks2hms(ticks):
                secs = ticks / 18.2
                h, m, s = int(secs / 3600), int(secs / 60) % 60, int(secs) % 60
                return ''.join([('%dh' % h if h else ''), ('%dm' % m if h or m else ''), ('%ds' % s)])
        for n in range(hist):
                fatdate, fattime, ticks = struct.unpack('<HHL', histdata[8 * n : 8 * n + 8])
                day = fatdate & 31
                month = (fatdate >> 5) & 15
                year = (fatdate >> 9) + 1980
                second = (fattime & 31) * 2
                minute = (fattime >> 5) & 63
                hour = fattime >> 11
                print('\t%04d-%02d-%02d %02d:%02d:%02d   %s' % (year, month, day, hour, minute, second, ticks2hms(ticks)))
                totalticks += ticks
        print("\t%13d ticks = %s" % (totalticks, ticks2hms(totalticks)))

