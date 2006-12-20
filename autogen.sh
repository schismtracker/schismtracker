#!/bin/sh
unset GREP
unset GREP_OPTIONS
unset FGREP
unset FGREP_OPTIONS
getver() {
	a="$1"
	shift
	for v in "$@"; do
		if "$a"-"$v" --version 2>/dev/null | fgrep "$v" >/dev/null 2>/dev/null; then
			echo "echo '$a-$v' >&2"
			echo "$a"-"$v"
			return
		fi
	done
	if "$a" --version 2>/dev/null | fgrep "$v" >/dev/null 2>/dev/null; then
		echo "echo '$a-$v' >&2"
		echo "$a"
		return
	fi
	echo "echo '-- require $a-$v' >&2"
	echo "exit 1"
}
(
getver aclocal 1.9
getver automake 1.9
getver autoheader 2.60 2.59
getver autoconf 2.60 2.59
) | exec sh
