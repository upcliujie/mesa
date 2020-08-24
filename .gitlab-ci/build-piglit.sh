#!/bin/bash

set -ex

if [ -n "$INCLUDE_OPENCL_TESTS" ]; then
    PIGLIT_OPTS="-DPIGLIT_BUILD_CL_TESTS=ON"
fi

git clone https://gitlab.freedesktop.org/mesa/piglit.git --single-branch --no-checkout /piglit
pushd /piglit
git checkout 5d3fbc00e32293d0b7029b986bfb99fa891c9bae
patch -p1 <$OLDPWD/.gitlab-ci/piglit/disable-vs_in.diff
cmake -S . -B . -G Ninja -DCMAKE_BUILD_TYPE=Release $PIGLIT_OPTS
ninja $PIGLIT_BUILD_TARGETS
find -name .git -o -name '*ninja*' -o -iname '*cmake*' -o -name '*.[chao]' | xargs rm -rf
rm -rf target_api
if [ "x$PIGLIT_BUILD_TARGETS" = "xpiglit_replayer" ]; then
    find ! -regex "^\.$" \
         ! -regex "^\.\/piglit.*" \
         ! -regex "^\.\/framework.*" \
         ! -regex "^\.\/bin\/replayer\.py" \
         ! -regex "^\.\/tests\/replay\.py" 2>/dev/null | xargs rm -rf
fi
popd
