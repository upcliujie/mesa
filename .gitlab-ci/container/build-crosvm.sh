#!/bin/bash

set -ex

# Pull down repositories that crosvm depends on to cros checkout-like locations.
CROS_ROOT=/
THIRD_PARTY_ROOT=$CROS_ROOT/third_party
mkdir -p $THIRD_PARTY_ROOT
AOSP_EXTERNAL_ROOT=$CROS_ROOT/aosp/external
mkdir -p $AOSP_EXTERNAL_ROOT
PLATFORM2_ROOT=/platform2

PLATFORM2_COMMIT=418aa4f600349859421e494e52d85865e927a85c
git clone --single-branch --no-checkout https://chromium.googlesource.com/chromiumos/platform2 $PLATFORM2_ROOT
pushd $PLATFORM2_ROOT
git checkout $PLATFORM2_COMMIT
popd

# minijail does not exist in upstream linux distros.
MINIJAIL_COMMIT=d01c60272ab9bd2c467f2586cd67137d04eb8938
git clone --single-branch --no-checkout https://android.googlesource.com/platform/external/minijail $AOSP_EXTERNAL_ROOT/minijail
pushd $AOSP_EXTERNAL_ROOT/minijail
git checkout $MINIJAIL_COMMIT
make
cp libminijail.so /usr/lib/x86_64-linux-gnu/
popd

# Pull the cras library for audio access.
ADHD_COMMIT=e20968929240bc9ae6c842f4cbf9135e483b64a2
git clone --single-branch --no-checkout https://chromium.googlesource.com/chromiumos/third_party/adhd $THIRD_PARTY_ROOT/adhd
pushd $THIRD_PARTY_ROOT/adhd
git checkout $ADHD_COMMIT
popd

# Pull vHost (dataplane for virtio backend drivers)
VHOST_COMMIT=7c95b4a2c1e378f7328d8bc0510bbb6998f54581
git clone --single-branch --no-checkout https://chromium.googlesource.com/chromiumos/third_party/rust-vmm/vhost $THIRD_PARTY_ROOT/rust-vmm/vhost
pushd $THIRD_PARTY_ROOT/rust-vmm/vhost
git checkout $VHOST_COMMIT
popd

CROSVM_VERSION=bf37e81e42f768bd833f7ecdcde6fee3a0b576ab
git clone --no-checkout https://gitlab.freedesktop.org/tomeu/crosvm.git /platform/crosvm
pushd /platform/crosvm
git checkout "$CROSVM_VERSION"

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
