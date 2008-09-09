#!/bin/sh
perl scripts/maint-wikiver.pl
scp -C cvs-snapshot.tgz nimh@nimh.org:webshare/schism/.
scp -C schism-0.5.x86.package nimh@nimh.org:webshare/schism/.
scp -C "win32-x86-build/install.exe" nimh@nimh.org:webshare/schism/.
scp -C "macosx-ppc-build/Schism Tracker.dmg" nimh@nimh.org:webshare/schism/.
sh scripts/maint-cvsexport.sh
git-update-server-info
scp .git/info/refs nimh@nimh.org:webshare/schism.git/info/refs
