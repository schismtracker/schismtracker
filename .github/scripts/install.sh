#!/bin/sh
# script to run schism with included libs on linux
# shouldn't™ require bash or any other crap like that.

# By default, we install to /opt. This is because /usr/local/lib is not
# included in the actual ld paths on many distributions. However, we make
# due by installing a "fake" stub script that sets up LD_LIBRARY_PATH
# with the proper variables into /usr/local/bin; this behavior can be
# disabled, if desired.
PREFIX="/opt/schism"
INSTALL_USR_LOCAL_STUB=yes

if test "x$1" = "x--help"; then
	printf "usage: $0 [--help] [--no-usr-stub] [--prefix <prefix> (default: /opt/schism)>]"
	exit
fi

# --no-usr-stub installs a stub script to /usr/local/bin
# that sets up an environment to run the real binary
if test "x$1" = "x--no-usr-stub"; then
	INSTALL_USR_LOCAL_STUB=no
	shift
fi

if test "x$1" = "x--prefix"; then
	PREFIX="$2"
	shift
	shift
fi

SCRIPTPATH="$(cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P)"

# mkdir -p is POSIX.
mkdir -p "$PREFIX/bin" "$PREFIX/lib" "$PREFIX/share/applications" "$PREFIX/share/man/man1" "$PREFIX/share/pixmaps"

cp "$SCRIPTPATH/schismtracker" "$PREFIX/bin/schismtracker"
for i in FLAC.so.8 utf8proc.so.2.3.2 ogg.so.0; do
	cp "$SCRIPTPATH/lib$i" "$PREFIX/lib/lib$i"
done
cp "$SCRIPTPATH/schism.desktop" "$PREFIX/share/applications/schism.desktop"
cp "$SCRIPTPATH/schismtracker.1" "$PREFIX/share/man/man1/schismtracker.1"
cp "$SCRIPTPATH/schism-icon-128.png" "$PREFIX/share/pixmaps/schism-icon-128.png"

if test "x$INSTALL_USR_LOCAL_STUB" = "xyes" && ! test "x$PREFIX" = "x/usr/local"; then
	cat<<EOF > "/usr/local/bin/schismtracker"
#!/bin/sh

env LD_LIBRARY_PATH="$PREFIX/lib" "$PREFIX/bin/schismtracker"
EOF
	# don't forget!
	chmod +x "/usr/local/bin/schismtracker"
fi
