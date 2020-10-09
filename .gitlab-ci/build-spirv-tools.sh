#!/bin/bash

set -ex

git clone https://github.com/KhronosGroup/SPIRV-Tools
push /SPIRV-Tools
cmake -G Ninja -CMAKE_BUILD_TYPE=Release
ninja
ninja install
popd
