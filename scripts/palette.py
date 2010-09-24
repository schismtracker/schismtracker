#!/usr/bin/python
# This converts a palette string from the packed ASCII format used
# in the runtime config into a C structure suitable for inclusion
# in palettes.h.

import sys
table = '.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz'
if len(sys.argv) != 2:
        print("usage: %s <encoded palette string>" % sys.argv[0])
        sys.exit(1)
for n in range(16):
        print("/* %2d */ {%s}," % (n, ', '.join([
                "%2d" % table.index(c)
                for c in sys.argv[1][3 * n : 3 * n + 3]
        ])))

