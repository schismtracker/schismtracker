#!/usr/bin/python
# This converts a palette string from the packed ASCII format used
# in the runtime config into a C structure suitable for inclusion
# in palettes.h.

import string, sys
table = '.0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ_abcdefghijklmnopqrstuvwxyz'
if len(sys.argv) != 2:
        print "usage: %s <encoded palette string>"
        raise SystemExit, 1
for n in xrange(16):
        print "/* %2d */ {%s}," % (n, string.join(["%2d" % table.index(c)
                for c in sys.argv[1][3 * n : 3 * n + 3]], ', '))
