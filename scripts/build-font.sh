#!/bin/sh

# make default (builtin) font

if [ "$#" -lt 2 ]; then
        echo >&2 "usage: $0 srcdir fonts > default-font.c"
        exit 1
fi

srcdir="$1"
shift

for file in "$@"; do
        ident=font_`basename "$srcdir"/"$file" .fnt | tr /- __`
        sh "$srcdir"/scripts/bin2h.sh -n "$ident" "$srcdir"/"$file" || exit 1
done

