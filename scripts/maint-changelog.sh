#!/bin/sh
cvs2cl -I 'build-version\.h' -f CVSChangeLog
#cat CVSChangeLog ~/import/schism/CVSChangeLog \
#	| ssh nimh@nimh.org 'cat > webshare/schism/CVSChangeLog.txt'
cat CVSChangeLog | ssh nimh@nimh.org 'cat > webshare/schism/CVSChangeLog.txt'

