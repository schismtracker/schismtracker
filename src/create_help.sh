#!/bin/sh

# create_help.sh <bin2h> <outfile> <srcdir> <infiles>
# (infiles should be in srcdir)
BIN2H="$1"
OUTFILE="$2"
SRCDIR="$3"
shift 3

if [ -d "$TMPDIR" ]; then
        tempfile="$TMPDIR"
elif [ -d "$TMP" ]; then
	tempfile="$TMP"
else
	tempfile=/tmp
fi
tempfile="$tempfile/create_help.$$"

(for f in $@; do
	cat "$SRCDIR/$f" || exit 1
	head -c1 /dev/zero
done) > "$tempfile"

rm -f "$OUTFILE"
bzip2 -9 < "$tempfile" | \
	"$BIN2H" -n COMPRESSED_HELP_TEXT - "$OUTFILE" || exit 1
echo "#define UNCOMPRESSED_HELP_TEXT_SIZE" `wc -c < "$tempfile"` >> \
	"$OUTFILE" || exit 1

rm -f "$tempfile" || exit 1
