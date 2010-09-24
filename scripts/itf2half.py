#!/usr/bin/python
import sys, struct

if sys.version_info[0] > 2:
        read = sys.stdin.buffer.read
        write = sys.stdout.buffer.write
else:
        read = sys.stdin.read
        write = sys.stdout.write

write(struct.pack('1024B', *[
        (d & 0xf0) | ((d & 0xf000) >> 12)
        for d in struct.unpack('<1024H', read(2048))
]))
