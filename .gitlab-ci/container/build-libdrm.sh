#!/bin/bash
# shellcheck disable=SC2086 # we want word splitting

# When changing this file, you need to bump the following
# .gitlab-ci/image-tags.yml tags:
# DEBIAN_BUILD_TAG
# DEBIAN_BUILD_MINGW_TAG
# DEBIAN_X86_TEST_ANDROID_TAG
# DEBIAN_X86_TEST_GL_TAG
# DEBIAN_X86_TEST_VK_TAG
# ALPINE_X86_BUILD_TAG
# FEDORA_X86_BUILD_TAG
# KERNEL_ROOTFS_TAG

set -ex

export LIBDRM_VERSION=libdrm-2.4.110

curl -L -O --retry 4 -f --retry-all-errors --retry-delay 60 \
    https://dri.freedesktop.org/libdrm/"$LIBDRM_VERSION".tar.xz
tar -xvf "$LIBDRM_VERSION".tar.xz && rm "$LIBDRM_VERSION".tar.xz
cd "$LIBDRM_VERSION"
meson build -D vc4=false -D freedreno=false -D etnaviv=false $EXTRA_MESON_ARGS
ninja -C build install
cd ..
rm -rf "$LIBDRM_VERSION"
