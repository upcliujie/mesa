#!/usr/bin/env bash

set -ex

PROFILES_TAG="zink-profiles"

git clone -b "$PROFILES_TAG" --single-branch --depth 1 https://github.com/kusma/Vulkan-Profiles.git
pushd Vulkan-Profiles
mkdir build
pushd build
cmake -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr -DUPDATE_DEPS=ON -DBUILD_TESTS=OFF -DBUILD_WERROR=OFF ..
ninja install
popd
rm -rf Vulkan-Profiles
