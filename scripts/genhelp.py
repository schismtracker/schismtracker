#!/usr/bin/env python
import sys, os, tempfile, subprocess


def usage(ex):
    [sys.stderr, sys.stdout][not ex].write(
        "Usage: %s srcdir helptexts > helptext.c\n"
            % os.path.basename(sys.argv[0]))
    sys.exit(ex)
if '--help' in sys.argv:
    usage(0)
if len(sys.argv) < 3:
    usage(1)


def die_at(filename, line, message):
    sys.stderr.write("%s:%d: %s\n" % (filename, line + 1, message))
    sys.exit(1)

# valid characters to start a line (see page_help.c)
typechars = frozenset("|+:;!%#=")

srcdir = sys.argv[1]
helptexts = sys.argv[2:]
o = sys.stdout

if sys.version_info[0] > 2:
    unichr = chr
    mapord = lambda s: s
else:
    mapord = lambda s: map(ord, s)

arraynames = []
o.write("extern const unsigned char *help_text[];\n\n")
for idx, textfile in enumerate(helptexts):
    blank = True
    arrname = ("help_text_%s" % str(idx))
    o.write("static const unsigned char %s[] = {\n" % arrname)
    for lnum, line in enumerate(open(os.path.join(srcdir, textfile), 'rb')):
        blank = False
        
        try:
            line = line.decode('utf8').rstrip('\r\n')
        except UnicodeError:
            die_at(textfile, lnum, "malformed Unicode character")

        if not line:
            continue
        elif line.endswith(' '):
            die_at(textfile, lnum, "trailing whitespace")
        elif len(line) > 76:
            die_at(textfile, lnum, "line is longer than 76 characters")
        elif line[0] not in typechars:
            die_at(textfile, lnum, "line-type character %c is not one of %s"
                   % (line[0], ''.join(typechars)))

        line += '\n'
        try:
            line = (line
                .replace(unichr(0x00B6), unichr(0x14)) # paragraph mark
                .replace(unichr(0x00A7), unichr(0x15)) # section mark (why? I don't know)
                .encode('cp437'))
        except UnicodeError:
            die_at(textfile, lnum, "invalid CP437 character")

        o.write(','.join([str(hex(c)) for c in mapord(line)]) + ',\n')
    if blank:
        die_at(textfile, 0, "file is empty")
    # don't forget the NUL termination
    o.write("0};\n\n")
    arraynames.append(arrname)

o.write("const unsigned char* help_text[] = {\n")
for arrname in arraynames:
    o.write("\t%s,\n" % arrname)
o.write("};\n")

