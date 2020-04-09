#!/bin/sh

commit_mr() {
    git show --no-patch --pretty=format:"%(trailers:only,unfold)" $1 | \
        grep -oP "Part-of: \K.*"
}

commit_week() {
    git show --no-patch --pretty=format:"%cd" --date=format-local:"%V" $1
}

first_commit_from_different_mr() {
    local current_commit_id=$1
    local last_commit_id=""
    local num_iters=0

    while
        last_commit_id=$current_commit_id
        current_commit_id=$(git rev-parse $current_commit_id^)
        num_iters=$((num_iters + 1))
        [ "$(commit_mr $last_commit_id)" = "$(commit_mr $current_commit_id)" ] && \
            [ "$num_iters" -lt 100 ]
    do
        :
    done

    echo $current_commit_id
}

# Only store images for pushes to master
if [ "$GITLAB_USER_LOGIN" != "marge-bot" ]; then
    exit 1
fi

commit_id=$CI_COMMIT_SHA

if [ -z "$commit_id" ]; then
    echo "[tracie-should-store-images] Error: empty \$CI_COMMIT_SHA env. variable"
    exit 1
fi

prev_push_commit_id=$(first_commit_from_different_mr $commit_id)

# Store images if traces.yml changed in this push
if git diff-tree --no-commit-id --name-only -r $prev_push_commit_id..$commit_id | grep -q traces.yml; then
    echo "[tracie-should-store-images] Detected traces.yml update in this push"
    exit 0
fi

# Store images once a week
if [ "$(commit_week $prev_push_commit_id)" -lt "$(commit_week $commit_id)" ]; then
    echo "[tracie-should-store-images] Detected week change since last push"
    exit 0
fi

exit 1
