#!/bin/bash

set -ex

PARALLEL_DEQP_RUNNER_VERSION=d0ea2b69f7d1d5b7e8250db1002a0dc77417221f

git clone https://gitlab.freedesktop.org/mesa/parallel-deqp-runner --single-branch -b master --no-checkout /parallel-deqp-runner
pushd /parallel-deqp-runner
git checkout "$PARALLEL_DEQP_RUNNER_VERSION"
meson . _build
ninja -C _build hang-detection
mkdir -p build/bin
install _build/hang-detection build/bin
strip build/bin/*
find . -not -path './build' -not -path './build/*' -delete
popd
