#!/bin/bash

# Checks pull request

set -x

# generic bot message start
BOT_MSG="Thank you for your contribution"


# Checking change log
# we merform git diff between commit ids of current branch and the one from PR
changelog_missing=0
lines_changed=$(git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --numstat | awk '{ sum+= $1 + $2 ; } END { print sum; }')
if [ $lines_changed -gt 100 ]; then
  git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --numstat | grep 'CHANGELOG.md'
  if [ "$?" -eq 1 ]; then
    changelog_missing=1
    BOT_MSG+="It seems, like you  forgot to update CHANGELOG.md"
  fi
fi

if [[ "$changelog_missing" -eq 1 ]]; then
  curl -s -H "Authorization: token $TRAVIS_ACCESS_TOKEN" -X POST -d "{\"body\": \"$BOT_MSG\"}" "https://api.github.com/repos/${TRAVIS_REPO_SLUG}/issues/${TRAVIS_PULL_REQUEST}/comments"
  exit 1
fi
