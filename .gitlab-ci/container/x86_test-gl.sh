#!/bin/bash

set -e
set -o xtrace

export DEBIAN_FRONTEND=noninteractive

# Ephemeral packages (installed for this script and removed again at the end)
STABLE_EPHEMERAL=" \
      autoconf \
      automake \
      bison \
      ccache \
      clang-10 \
      cmake \
      flex \
      g++ \
      libcap-dev \
      libclang-cpp10-dev \
      libelf-dev \
      libfdt-dev \
      libgbm-dev \
      libgles2-mesa-dev \
      libpcre3-dev \
      libpciaccess-dev \
      libpng-dev \
      libvulkan-dev \
      libwaffle-dev \
      libxcb-keysyms1-dev \
      libxkbcommon-dev \
      libxrender-dev \
      llvm-10-dev \
      make \
      meson \
      ocl-icd-opencl-dev \
      patch \
      pkg-config \
      python3-distutils \
      python3.7-dev \
      wget \
      xz-utils \
      "

apt-get install -y --no-remove \
      $STABLE_EPHEMERAL \
      clinfo \
      inetutils-syslogd \
      iptables \
      libclang-common-10-dev \
      libclang-cpp10 \
      libcap2 \
      libfdt1 \
      libxcb-shm0 \
      ocl-icd-libopencl1 \
      python3-lxml \
      python3-simplejson \
      sysvinit-core


. .gitlab-ci/container/container_pre_build.sh


############### Build kernel

export DEFCONFIG="arch/x86/configs/x86_64_defconfig"
export KERNEL_IMAGE_NAME=bzImage
export KERNEL_ARCH=x86_64
export DEBIAN_ARCH=amd64
export KERNEL_URL="https://gitlab.freedesktop.org/tomeu/linux/-/archive/v5.10-rc2-for-mesa-ci/linux-v5.10-rc2-for-mesa-ci.tar.gz"

. .gitlab-ci/container/build-kernel.sh

############### Build spirv-tools (debian too old)

. .gitlab-ci/container/build-spirv-tools.sh

############### Build libclc

. .gitlab-ci/container/build-libclc.sh

############### Build virglrenderer

. .gitlab-ci/container/build-virglrenderer.sh

############### Build piglit

INCLUDE_OPENCL_TESTS=1 . .gitlab-ci/container/build-piglit.sh

############### Build Rust deps (Crosvm and deqp-runner)

. .gitlab-ci/container/build-rust.sh
. .gitlab-ci/container/build-crosvm.sh
. .gitlab-ci/container/build-deqp-runner.sh
rm -rf /root/.rustup /root/.cargo

############### Build dEQP GL

DEQP_TARGET=surfaceless . .gitlab-ci/container/build-deqp.sh

############### Build apitrace

. .gitlab-ci/container/build-apitrace.sh

############### Build renderdoc

. .gitlab-ci/container/build-renderdoc.sh

############### Build libdrm

. .gitlab-ci/container/build-libdrm.sh

############### Uninstall the build software

ccache --show-stats

apt-get purge -y \
      $STABLE_EPHEMERAL

apt-get autoremove -y --purge
