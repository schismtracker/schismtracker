#!/bin/sh
# script to run schism with included libs on linux
# shouldn't™ require bash or any other crap like that.

SCRIPTPATH="$(cd -- "$(dirname "$0")" >/dev/null 2>&1 ; pwd -P)"
env LD_LIBRARY_PATH="$SCRIPTPATH" "$SCRIPTPATH/schismtracker" "$@"
