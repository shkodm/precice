#!/bin/bash

# Checks pull request


set -e
set -x

# Chechking change log
# we merform git diff between commit ids of current branch and the one from PR
lines_changed=$(git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --numstat | awk '{ sum+= $1 + $2 ; } END { print sum; }')
if [ $lines_changed -gt 100 ]; then
  git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --numstat | grep 'CHANGELOG.md'
  if [ "$?" -eq 1 ]; then
    BOT_MSG+="It seems, like you  forgot to update CHANGELOG.md"
    echo "It seems like you forgot to update CHANGELOG.md"
    exit 1
  fi
fi
