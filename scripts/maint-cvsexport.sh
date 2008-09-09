#!/bin/sh
set -e
export GIT_DIR="$HOME/src/schism/.git"
cd "$HOME/src/schism2"
git-cherry cvshead  | sed -n 's/^+ //p' | xargs -l1 git-cvsexportcommit -c -p -v
git-branch -M cvshead
