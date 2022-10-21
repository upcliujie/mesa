#!/bin/bash
# shellcheck disable=SC2086 # we want word splitting

set -ex

git config --global user.email "mesa@example.com"
git config --global user.name "Mesa CI"

CROSVM_VERSION=60aa43629ae9be2cc3df37c648ab7e0e5ff2172c
git clone --single-branch -b main --no-checkout https://chromium.googlesource.com/crosvm/crosvm /platform/crosvm
pushd /platform/crosvm
git checkout "$CROSVM_VERSION"
git submodule update --init

VIRGLRENDERER_VERSION=93fc075f9bde7b8603d860dc4dfc786671056d49
rm -rf third_party/virglrenderer
git clone --single-branch -b master --no-checkout https://gitlab.freedesktop.org/virgl/virglrenderer.git third_party/virglrenderer
pushd third_party/virglrenderer
git checkout "$VIRGLRENDERER_VERSION"
meson build/ -Drender-server=true -Drender-server-worker=process -Dvenus-experimental=true -Ddrm-msm-experimental=true $EXTRA_MESON_ARGS
ninja -C build install
popd

RUSTFLAGS="-L native=/usr/local/lib/$GCC_ARCH" cargo install \
  bindgen \
  --version 0.60.1 \
  -j ${FDO_CI_CONCURRENT:-4} \
  --root /usr/local \
  $EXTRA_CARGO_ARGS

RUSTFLAGS="-L native=/usr/local/lib/$GCC_ARCH" cargo install \
  -j ${FDO_CI_CONCURRENT:-4} \
  --locked \
  --features 'default-no-sandbox gpu x virgl_renderer virgl_renderer_next' \
  --path . \
  --root /usr/local \
  $EXTRA_CARGO_ARGS

popd

rm -rf /platform/crosvm
