#!/bin/bash

set -ex

git clone --single-branch -b for-mesa-ci https://gitlab.freedesktop.org/tomeu/crosvm.git /platform/crosvm
pushd /platform/crosvm
git submodule update --init

RUSTFLAGS='-L native=/usr/local/lib' cargo install \
  bindgen \
  -j ${FDO_CI_CONCURRENT:-4} \
  --root /usr/local \
  $EXTRA_CARGO_ARGS

RUSTFLAGS='-L native=/usr/local/lib' cargo install \
  -j ${FDO_CI_CONCURRENT:-4} \
  --locked \
  --features 'default-no-sandbox gpu x virgl_renderer virgl_renderer_next' \
  --path . \
  --root /usr/local \
  $EXTRA_CARGO_ARGS

popd

rm -rf $PLATFORM2_ROOT $AOSP_EXTERNAL_ROOT/minijail $THIRD_PARTY_ROOT/adhd $THIRD_PARTY_ROOT/rust-vmm /platform/crosvm
