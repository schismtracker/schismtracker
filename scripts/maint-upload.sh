#!/bin/sh
perl scripts/maint-wikiver.pl
scp -C cvs-snapshot.tgz nimh@nimh.org:webshare/schism/.
scp -C schism-0.5.x86.package nimh@nimh.org:webshare/schism/.
scp -C "win32-x86-build/install.exe" nimh@nimh.org:webshare/schism/.
scp -C "macosx-ppc-build/Schism Tracker.dmg" nimh@nimh.org:webshare/schism/.
git-update-server-info
sh scripts/maint-cvsexport.sh
scp .git/info/refs nimh@nimh.org:webshare/schism.git/info/refs
scp .git/objects/info/packs nimh@nimh.org:webshare/schism.git/objects/info/packs
