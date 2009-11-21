#!/bin/sh
if [ -d CVS ]; then
        cvs2cl -I 'build-version\.h' -f ChangeLog
else
        git log > ChangeLog
fi
cat ChangeLog | ssh nimh@nimh.org 'cat > webshare/schism/CVSChangeLog.txt'
