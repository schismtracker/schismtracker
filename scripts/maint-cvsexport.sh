#!/bin/sh
set -e
wd=`pwd`
export GIT_DIR="$HOME/src/schism/.git"
if [ "x$1" = "x-force" ]; then
        cd "$HOME/src/schism2"
        cvs up -Pd
        git-ls-files -z | xargs -i -0 cp "$wd/{}" "./{}"
        cvs ci -m 'Force cvs/git merge'
else
        cd "$HOME/src/schism2"
        git-cherry cvshead  | sed -n 's/^+ //p' | xargs -l1 git-cvsexportcommit -c -p -v
fi
git-branch --no-track -f cvshead
git-update-server-info
