#!/bin/sh
# I haven't tried all versions, but I know that these work
# on macosx I got them from fink
# they were available on my FC4 machine
# they're probably available to you.

# added hackery for darwinports current versions /storlek

unset GREP
unset GREP_OPTIONS
unset FGREP
unset FGREP_OPTIONS

getver() {
	a="$1"
	shift
	for v in "$@"; do
		if "$a" --version | fgrep "$v" >/dev/null; then
			echo "$a"
			echo "'$a' => $a version $v" 1>&2
			return
		fi
	done
	for v in "$@"; do
		if "$a"-"$v" --version | fgrep "$v" >/dev/null; then
			echo "$a"-"$v"
			echo "'$a"-"$v' => $a version $v" 1>&2
			return
		fi
	done
}

aclocal=`getver aclocal 1.9`
automake=`getver automake 1.9`
autoheader=`getver autoheader 2.60 2.59`
autoconf=`getver autoconf 2.60 2.59`
$aclocal && $autoheader && $automake -a && $autoconf
