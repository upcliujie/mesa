#!/bin/bash

set -ex

if [ -n "$INCLUDE_OPENCL_TESTS" ]; then
    PIGLIT_OPTS="-DPIGLIT_BUILD_CL_TESTS=ON"
fi

git clone https://gitlab.freedesktop.org/tanty/piglit.git -b tanty/cmake-include-after-project --single-branch --no-checkout /piglit
pushd /piglit
git checkout ab8308747fecc12f9cceaf3a2031d1b6ca54df60
patch -p1 <$OLDPWD/.gitlab-ci/piglit/disable-vs_in.diff
cmake -S . -B . -G Ninja -DCMAKE_BUILD_TYPE=Release $PIGLIT_OPTS $EXTRA_CMAKE_ARGS
ninja $PIGLIT_BUILD_TARGETS
find -name .git -o -name '*ninja*' -o -iname '*cmake*' -o -name '*.[chao]' | xargs rm -rf
rm -rf target_api
popd
