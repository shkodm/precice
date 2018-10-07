#!/bin/sh

# check pull request

# check change log
last_commit=$(git log | head -1 | awk '{ print $2 }')
lines_changed=$(git diff-tree --numstat --no-commit-id $last_commit | awk '{ sum += $1 + $2 ; } END {print sum;}')
# check changelog for ids
git diff-tree --numstat --no-commit-id $last_commit
echo $lines_changed
if [ $lines_changed -gt 100 ]; then
  a=$(git diff-tree --numstat --no-commit-id $last_commit | grep 'CHANGELOG.md')
  if [ -z $a ]; then
    echo "Changelog was not modified, even though number of lines changed exceeded 100"
  fi
fi

# other

