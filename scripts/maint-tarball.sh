#!/bin/sh
cd ..
find schism2/ \
	\! -name 'autom4te.cache' \
	\! -path '*/autom4te.cache/*' \
	\! -path '*/*-build/*' \
	\! -path '*/CVS/*' \
	\! -path '.#*' \
	\! -name 'CVS' \
	\! -name 'Screenshot-*.png' \
	\! -name '*-build' \
	\! -name '.*.sw[opqrstuvwxyz]' \
	\! -name '*.tgz' \
	-print0 \
| cpio -o -0 -H ustar | gzip -9 > schism2/cvs-snapshot.tgz

