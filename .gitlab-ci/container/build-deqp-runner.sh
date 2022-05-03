#!/bin/sh

set -ex

# FIXME: Drop this when the following MR is merged and released:
# https://gitlab.freedesktop.org/anholt/deqp-runner/-/merge_requests/39
DEQP_RUNNER_GIT_URL=https://gitlab.freedesktop.org/cristicc/deqp-runner.git
DEQP_RUNNER_GIT_REV=6d642bd4545efbb2fbdcc993ffba4cfd3ec3abb2

if [ -n "${DEQP_RUNNER_GIT_TAG}${DEQP_RUNNER_GIT_REV}" ]; then
    # Build and install from source
    DEQP_RUNNER_CARGO_ARGS="--git ${DEQP_RUNNER_GIT_URL:-https://gitlab.freedesktop.org/anholt/deqp-runner.git}"

    if [ -n "${DEQP_RUNNER_GIT_TAG}" ]; then
        DEQP_RUNNER_CARGO_ARGS="--tag ${DEQP_RUNNER_GIT_TAG} ${DEQP_RUNNER_CARGO_ARGS}"
    else
        DEQP_RUNNER_CARGO_ARGS="--rev ${DEQP_RUNNER_GIT_REV} ${DEQP_RUNNER_CARGO_ARGS}"
    fi

    DEQP_RUNNER_CARGO_ARGS="${DEQP_RUNNER_CARGO_ARGS} ${EXTRA_CARGO_ARGS}"
else
    # Install from package registry
    DEQP_RUNNER_CARGO_ARGS="--version 0.13.1 ${EXTRA_CARGO_ARGS} -- deqp-runner"
fi

cargo install --locked  \
    -j ${FDO_CI_CONCURRENT:-4} \
    --root /usr/local \
    ${DEQP_RUNNER_CARGO_ARGS}
