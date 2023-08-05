#!/usr/bin/env bash

set -e
set -o xtrace

apt-get update
apt-get install -y --no-remove \
        zstd \
        g++-mingw-w64-i686 \
        g++-mingw-w64-x86-64

. .gitlab-ci/container/x86_64-w64-mingw32-download.sh
. .gitlab-ci/container/x86_64-w64-mingw32-source-deps.sh
