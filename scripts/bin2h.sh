#!/bin/sh
#
# this is a bin2h-like program that is useful even when cross compiling.
#
# it requires:
#       unix-like "od" tool
#       unix-like "sed" tool
#       unix-like "fgrep" tool
#
# it's designed to be as portable as possible and not necessarily depend on
# gnu-style versions of these tools

name=""
type="unsigned const char"
if [ "X$1" = "X-n" ]; then
        name="$2"
        shift
        shift
fi
if [ "X$1" = "X-t" ]; then
        type="$2"
        shift
        shift
fi

if [ "X$name" = "X" ]; then
        echo "Usage: $0 -n name [-t type] files... > output.h"
fi

if [ "X$OD" = "X" ]; then
        OD="od"
fi
if [ "X$SED" = "X" ]; then
        SED="sed"
fi

length="$(cat "$@" | wc -c | tr -d ' ')"

echo "#include <stddef.h>"

case "$type" in
        *static*) ;;
        *)
                echo "extern $type $name""[];"
                echo "extern size_t $name""_size;"
                ;;
esac
echo "$type $name""[] = {"
$OD -b -v "$@" \
| $SED -e 's/^[^ ][^ ]*[ ]*//' \
| $SED -e "s/\([0-9]\{3\}\)/'\\\\\1',/g" \
| $SED -e '$ s/,$//'
echo "};"

echo "size_t $name""_size = $length;"
