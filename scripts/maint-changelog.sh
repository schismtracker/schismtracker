#!/bin/sh
if [ -d CVS ]; then
	cvs2cl -I 'build-version\.h' -f CVSChangeLog
else
	git log > CVSChangeLog
fi
cat CVSChangeLog | ssh nimh@nimh.org 'cat > webshare/schism/CVSChangeLog.txt'

