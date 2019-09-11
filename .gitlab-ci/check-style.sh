#!/usr/bin/env bash
set -eu -o pipefail

error=false
error() {
  echo "$*"
  error=true
}

# https://docs.gitlab.com/ce/ci/variables/predefined_variables.html
mr=origin/$CI_MERGE_REQUEST_SOURCE_BRANCH_NAME
target=$CI_MERGE_REQUEST_TARGET_BRANCH_NAME
git remote add mesa "$CI_MERGE_REQUEST_PROJECT_URL"
git fetch mesa $target

# Minimum hash length to uniquely identify commits
min_chars=$(git rev-list --abbrev=4 --abbrev-commit --all | while read -r line; do echo ${#line}; done | sort -n | tail -1)

git rev-list --reverse mesa/$target..$mr | while read commit_hash
do
  echo "Checking commit $(git show --no-patch --pretty='%h ("%s")')"

  git show --no-patch --pretty=%b $commit_hash | grep -iE '^fixes:' | while read line
  do
    fixes_hash=$(cut -d: -f2 <<< "$line" | sed -e 's/^[[:space:]]*//' | cut -d' ' -f1)

    # the commit identified exists
    if ! git show $fixes_hash &>/dev/null
    then
      error "Commit $commit_hash contains an invalid Fixes: hash: $fixes_hash"
    fi

    # hash is long enough to uniquely identify a commit
    if [ ${#fixes_hash} -lt $min_chars ]
    then
      error "Commit $commit_hash contains a Fixes: hash of less than $min_chars chars: $fixes_hash"
    fi

    #TODO: the commit hash is followed by some text
    # (should be the commit title, but it might be reformatted or split into
    # multiple lines, so we can't really check more)

    #TODO: check that the code style is the one we want
    #format_patch=$(git show "$commit_hash" -- | clang-format-diff -p1)
    #if [ -n "$format_patch" ]
    #then
    #  error "$format_patch"
    #fi

  done
done

if $error
then
  exit 1
fi
