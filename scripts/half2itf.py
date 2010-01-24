#!/usr/bin/python
import sys, struct

if sys.version_info[0] > 2:
        read = sys.stdin.buffer.read
        write = sys.stdout.buffer.write
else:
        read = sys.stdin.read
        write = sys.stdout.write

write(struct.pack('<1024H', *[
        (d & 0xf0) | ((d & 0xf) << 12)
        for d in struct.unpack('1024B', read())
]))
write(chr(0x12))
write(chr(0x02))
