#!/bin/bash

set -ex

git clone https://github.com/ValveSoftware/Fossilize.git
cd Fossilize
git checkout fa96c938bafb26dc0794942a16eb49f91acb1543
git submodule update --init
mkdir build
cd build
cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C . install
cd ../..
rm -rf Fossilize
