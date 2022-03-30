#!/bin/bash

set -ex

git clone https://github.com/ValveSoftware/Fossilize.git
cd Fossilize
git checkout 72088685d90bc814d14aad5505354ffa8a642789
git config --global url."https://github.com/".insteadOf "git://github.com/"
git submodule update --init
mkdir build
cd build
cmake -S .. -B . -G Ninja -DCMAKE_BUILD_TYPE=Release
ninja -C . install
cd ../..
rm -rf Fossilize
