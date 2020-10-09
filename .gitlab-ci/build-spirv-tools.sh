#!/bin/bash

set -ex

git clone --depth 1 https://github.com/KhronosGroup/SPIRV-Tools /SPIRV-Tools
pushd /SPIRV-Tools
pushd external
git clone --depth 1 https://github.com/KhronosGroup/SPIRV-Headers
popd
cmake -G Ninja -CMAKE_BUILD_TYPE=Release
ninja
ninja install
popd
