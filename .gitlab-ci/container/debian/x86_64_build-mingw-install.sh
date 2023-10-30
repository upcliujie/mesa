#!/usr/bin/env bash
# shellcheck disable=SC2206 # we want word splitting

set -e
set -o xtrace

EPHEMERAL=(
)

DEPS=(
    clang-${LLVM_VERSION}
    lld-${LLVM_VERSION}
    zstd
)
apt-get update

apt-get upgrade -y

apt-get install -y --no-remove "${DEPS[@]}" "${EPHEMERAL[@]}"

apt-get clean

if [ "${EPHEMERAL[*]}" != "" ]; then
    apt-get purge -y "${EPHEMERAL[@]}"
fi
