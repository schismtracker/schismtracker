import sys, os, tempfile, subprocess

# this is likely prone to breaking in many entertaining ways,
# but it appears to work with 2.4/2.5 on Linux at least

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
        sys.stderr.write("%s:%d: %s\n" % (filename, line, message))
        sys.exit(1)

# valid characters to start a line (see page_help.c)
typechars = frozenset("|:;!%#=")

srcdir = sys.argv[1]
helptexts = sys.argv[2:]
o = sys.stdout

ctrans = { '\n': '\\n', '"': '\\"', '\\': '\\\\' }
for c in map(chr, range(32, 127)):
        ctrans.setdefault(c, c)

o.write("extern const char *help_text[];\n")
o.write("const char *help_text[] = {")
for idx, textfile in enumerate(sys.argv[2:]):
        blank = True
        for lnum, line in enumerate(open(os.path.join(srcdir, textfile))):
                blank = False
                line = line.rstrip('\r\n')
                if not line:
                        continue
                elif line.endswith(' '):
                        die_at(textfile, lnum, "trailing whitespace")
                elif len(line) > 76:
                        die_at(textfile, lnum, "line is longer than 76 characters")
                elif line[0] not in typechars:
                        die_at(textfile, lnum, "line-type character %c is not one of %s"
                               % (line[0], ''.join(typechars)))
                o.write('\n"' + ''.join([ctrans.get(c, '\\%03o' % ord(c))
                                             for c in line + '\n']) + '"')
        if blank:
                die_at(textfile, 0, "file is empty")
        o.write(",\n")
o.write("};\n")

