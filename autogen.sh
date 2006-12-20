#!/bin/bash
set -e
# I haven't tried all versions, but I know that these work
# on macosx I got them from fink
# they were available on my FC4 machine
# they're probably available to you.

# added hackery for darwinports current versions /storlek

unset GREP

function getver() {
	a="$1"
	shift
	for v in "$@"; do
		if "$a" --version | fgrep "$v"; then
			echo $a
			return
		elif "$a"-"$v" --version | fgrep "$v"; then
			echo "$a"-"$v"
			return 0
		fi
	done
	return 1
}

aclocal=`getver aclocal 1.9`			|| exit 1
automake=`getver automake 1.9`			|| exit 1
autoheader=`getver autoheader 2.60 2.59`	|| exit 1
autoconf=`getver autoconf 2.60 2.59`		|| exit 1

$aclocal && $automake -a && $autoheader && $autoconf
