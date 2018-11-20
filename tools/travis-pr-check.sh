#!/bin/bash

# Checks pull request for code changes and writes a comment on github

set -x

# generic bot message start
bot_msg="Thank you for your contribution.\n"
# limit of lines after which you need to change CHANGELOG
lines_for_changelong=100


# Checking change log
# we merform git diff between commit ids of current branch and the one from the PR
pr_invalid=0
lines_changed=$(git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --numstat | awk '{ sum+= $1 + $2 ; } END { print sum; }')
if [ $lines_changed -gt $lines_for_changelong ]; then
  git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --numstat | grep 'CHANGELOG.md'
  if [ "$?" -eq 1 ]; then
    pr_invalid=1
    bot_msg+="\nSome suggestions for your pull request:\n* It seems, like you  forgot to update \`CHANGELOG.md\`"
  fi
fi


# Check formatting
not_formatted='   '
# get diff from this commit and filter filtypes that are needed
files=$( git log | sed -n '2p' | awk '{print $2, $3}' | xargs git diff --name-only | grep '.cpp\|.hpp' | xargs )
if [ -n "$files" ]; then
  for file in $files; do
    clang-format -style=file -output-replacements-xml $file  | grep -c "<replacement " > /dev/null
    if [ "$?" -eq 0 ]; then
      not_formatted+="\n    * \`$file\` "
      pr_invalid=1
    fi
  done
fi

if [ -n "$not_formatted" ]; then
  bot_msg+="\n* Your code formatting did not follow our clang-format style in following files: "
  bot_msg+="$not_formatted"
fi

# send message to github if we failed
if [[ "$pr_invalid" -eq 1 ]]; then
  echo "$bot_msg"
  curl -s -H "Authorization: token $TRAVIS_ACCESS_TOKEN" -X POST -d "{\"body\": \"$bot_msg\"}" "https://api.github.com/repos/${TRAVIS_REPO_SLUG}/issues/${TRAVIS_PULL_REQUEST}/comments"
  exit 1
fi
