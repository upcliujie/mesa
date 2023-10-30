#!/usr/bin/env bash
# shellcheck disable=SC2086 # we want word splitting

set -e
set -o xtrace

. .gitlab-ci/container/setup-wine.sh ~/.wine

wd=$PWD
CMAKE_TOOLCHAIN_MINGW_PATH=$wd/.gitlab-ci/container/x86_64-w64-mingw32-toolchain.cmake
mkdir -p ~/tmp
pushd ~/tmp

# Building DirectX-Headers
git clone https://github.com/microsoft/DirectX-Headers -b v1.711.3-preview --depth 1
mkdir -p DirectX-Headers/build
pushd DirectX-Headers/build
meson .. \
--backend=ninja \
--buildtype=release -Dbuild-test=false \
-Dprefix=/usr/x86_64-w64-mingw32/ \
--cross-file=$wd/.gitlab-ci/container/x86_64-w64-mingw32

ninja install
popd

# Building libva
git clone https://github.com/intel/libva -b 2.17.0 --depth 1
# libva already has a build dir in their repo, use builddir instead
mkdir -p libva/builddir
pushd libva/builddir
meson .. \
--backend=ninja \
--buildtype=release \
-Dprefix=/usr/x86_64-w64-mingw32/ \
--cross-file=$wd/.gitlab-ci/container/x86_64-w64-mingw32

ninja install
popd

export VULKAN_SDK_VERSION=1.3.250.1

# Building SPIRV Tools
git clone -b sdk-$VULKAN_SDK_VERSION --depth=1 \
https://github.com/KhronosGroup/SPIRV-Tools SPIRV-Tools

git clone -b sdk-$VULKAN_SDK_VERSION --depth=1 \
https://github.com/KhronosGroup/SPIRV-Headers SPIRV-Tools/external/spirv-headers

mkdir -p SPIRV-Tools/build
pushd SPIRV-Tools/build
cmake .. \
-DCMAKE_TOOLCHAIN_FILE=$CMAKE_TOOLCHAIN_MINGW_PATH \
-DCMAKE_INSTALL_PREFIX=/usr/x86_64-w64-mingw32/ \
-GNinja -DCMAKE_BUILD_TYPE=Release \
-DCMAKE_CROSSCOMPILING=1 \
-DCMAKE_POLICY_DEFAULT_CMP0091=NEW

ninja install
popd

popd # ~/tmp

# Cleanup ~/tmp
rm -rf ~/tmp
