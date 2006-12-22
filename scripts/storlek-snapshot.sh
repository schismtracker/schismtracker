#!/bin/sh
cd ..

tmpf="/tmp/schism-cvs-snapshot.tar.bz2"
tar -jcf "$tmpf" -X - <<EOF || exit 1
autom4te.cache
CVS
.??*
*~
*.tar
*.gz
*.zip
*.bz2
*.o
schismtracker
RANDOM_NOTES
EOF

scp "$tmpf" pair:public_html/schism/dl/schism-cvs-snapshot.tar.bz2
