#!/bin/sh

# bin2h built in lua scripts

if [ "$#" -lt 2 ]; then
        echo >&2 "usage: $0 srcdir script > lua-script.c"
        exit 1
fi

srcdir="$1"
script="$2"
varname="$(basename "$script" | cut -d. -f1)"

sh "$srcdir"/scripts/bin2h.sh -n "$varname"_script "$srcdir"/"$script" || exit 1