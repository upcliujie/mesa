#!/bin/bash

set -e
set -o xtrace

export DEBIAN_FRONTEND=noninteractive

if [[ $arch == "s390x" ]]; then
    LLVM=9
else
    LLVM=11
fi

# Ephemeral packages (installed for this script and removed again at the end)
STABLE_EPHEMERAL=" \
      "
HOST_STABLE_EPHEMERAL=" \
      cmake \
      clang \
      clang-${LLVM} \
      libclang-cpp${LLVM} \
      libncurses-dev \
      libpfm4 \
      libtinfo-dev \
      libyaml-0-2 \
      libz3-dev \
      llvm-${LLVM} \
      llvm-${LLVM}-dev \
      llvm-${LLVM}-runtime \
      llvm-${LLVM}-tools \
      llvm-spirv \
      libllvmspirvlib${LLVM} \
      python3-pygments \
      python3-yaml
      "
dpkg --add-architecture $arch
apt-get update

apt-get install -y --no-remove \
        $HOST_STABLE_EPHEMERAL

# Generate .spv files
. .gitlab-ci/container/build-libclc.sh

# Now remove host version of clang
apt-get remove -y $HOST_STABLE_EPHEMERAL

apt-get install -y --no-remove \
        crossbuild-essential-$arch \
        libelf-dev:$arch \
        libexpat1-dev:$arch \
        libpciaccess-dev:$arch \
        libstdc++6:$arch \
        libvulkan-dev:$arch \
        libx11-dev:$arch \
        libx11-xcb-dev:$arch \
        libxcb-dri2-0-dev:$arch \
        libxcb-dri3-dev:$arch \
        libxcb-glx0-dev:$arch \
        libxcb-present-dev:$arch \
        libxcb-randr0-dev:$arch \
        libxcb-shm0-dev:$arch \
        libxcb-xfixes0-dev:$arch \
        libxdamage-dev:$arch \
        libxext-dev:$arch \
        libxrandr-dev:$arch \
        libxshmfence-dev:$arch \
        libxxf86vm-dev:$arch \
        wget

if [[ $arch != "armhf" ]]; then
    apt-get remove -y glslang-tools spirv-tools

    # llvm-*-tools:$arch conflicts with python3:amd64. Install dependencies only
    # with apt-get, then force-install llvm-*-{dev,tools}:$arch with dpkg to get
    # around this.
    apt-get install -y --no-remove \
            libclang-cpp${LLVM}:$arch \
            libffi-dev:$arch \
            libgcc-s1:$arch \
            libtinfo-dev:$arch \
            libz3-dev:$arch \
            llvm-${LLVM}:$arch \
            libllvmspirvlib-dev:$arch \
            spirv-tools:$arch \
            zlib1g
fi

. .gitlab-ci/container/create-cross-file.sh $arch


. .gitlab-ci/container/container_pre_build.sh


# dependencies where we want a specific version
EXTRA_MESON_ARGS="--cross-file=/cross_file-${arch}.txt -D libdir=lib/$(dpkg-architecture -A $arch -qDEB_TARGET_MULTIARCH)"
. .gitlab-ci/container/build-libdrm.sh

apt-get purge -y \
        $STABLE_EPHEMERAL

. .gitlab-ci/container/container_post_build.sh

# This needs to be done after container_post_build.sh, or apt-get breaks in there
if [[ $arch != "armhf" ]]; then
    apt-get download llvm-${LLVM}-{dev,tools}:$arch libclc-dev:all
    dpkg -i --force-depends llvm-${LLVM}-*_${arch}.deb libclc-dev*_all.deb
    rm llvm-${LLVM}-*_${arch}.deb libclc-dev*_all.deb
fi
