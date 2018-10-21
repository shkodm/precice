#!/bin/bash

# Checks pull request

set -x

# generic bot message start
BOT_MSG="Thank you for your contribution.\n"


# Checking change log
# we merform git diff between commit ids of current branch and the one from PR
pr_invalid=0
lines_changed=$(git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --numstat | awk '{ sum+= $1 + $2 ; } END { print sum; }')
if [ $lines_changed -gt 100 ]; then
  git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --numstat | grep 'CHANGELOG.md'
  if [ "$?" -eq 1 ]; then
    pr_invalid=1
    BOT_MSG+="\nSome suggestions for your pull request\n* It seems, like you  forgot to update CHANGELOG.md"
  fi
fi


# Check formatting
not_formatted=""
files=$( git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --name-only )
if [ -n "$files" ]; then
  for file in "$files"; do
    clang-format -style=file -output-replacements-xml $file  | grep -c "<replacement " > /dev/null
    if [ "$?" -eq 0 ]; then
      not_formatted+="\n    * \`$file\` "
      pr_invalid=1
    fi
  done
fi

if [ -n "$not_formatted" ]; then
  BOT_MSG+="\n* Your code formatting did not follow our clang-format style in following files:"
  BOT_MSG+="$not_formatted"
fi

# send message to github if we failed
if [[ "$pr_invalid" -eq 1 ]]; then
  curl -s -H "Authorization: token $TRAVIS_ACCESS_TOKEN" -X POST -d "{\"body\": \"$BOT_MSG\"}" "https://api.github.com/repos/${TRAVIS_REPO_SLUG}/issues/${TRAVIS_PULL_REQUEST}/comments"
  exit 1
fi
