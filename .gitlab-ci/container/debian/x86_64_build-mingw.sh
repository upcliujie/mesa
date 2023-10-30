#!/usr/bin/env bash

# When changing this file or the files it's called, you need to bump the following
# .gitlab-ci/image-tags.yml tags:
# DEBIAN_BUILD_MINGW_TAG

set -e
set -o xtrace

export

export LLVM_VERSION=15

. .gitlab-ci/container/debian/x86_64_build-mingw-install.sh
. .gitlab-ci/container/x86_64-w64-mingw32-download.sh
. .gitlab-ci/container/x86_64-w64-mingw32-source-deps.sh
