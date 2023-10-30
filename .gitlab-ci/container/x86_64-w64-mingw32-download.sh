#!/usr/bin/env bash
# shellcheck disable=SC2086 # we want word splitting

set -e
set -o xtrace

# TODO: Fixup Clang header
# Revert after https://github.com/llvm/llvm-project/issues/64467 fixed
BMIINTRIN_PATH=$(find /usr/lib/ | grep "/llvm[^/]*/lib/clang/[^/]*/include/bmiintrin.h")
sed  -i 's/#define _tzcnt_u32(a)     (__tzcnt_u32((a)))/#define _tzcnt_u32 __tzcnt_u32/' "$BMIINTRIN_PATH"

# Under newly install msys2, use pacman to find the list of packages
# to download, then update MINGW_PACKET_LIST
: <<'COMMENT'
pacman -S -p \
mingw-w64-x86_64-clang \
mingw-w64-x86_64-libclc \
mingw-w64-x86_64-libelf \
mingw-w64-x86_64-llvm \
mingw-w64-x86_64-pkgconf \
mingw-w64-x86_64-spirv-llvm-translator \
mingw-w64-x86_64-vulkan-loader \
COMMENT

mkdir ~/tmp
pushd ~/tmp
MINGW_PACKET_LIST="
mingw-w64-x86_64-libffi-3.4.4-1-any.pkg.tar.zst
mingw-w64-x86_64-libwinpthread-git-11.0.0.r107.gd367cc9d7-1-any.pkg.tar.zst
mingw-w64-x86_64-gcc-libs-13.2.0-1-any.pkg.tar.zst
mingw-w64-x86_64-zlib-1.2.13-3-any.pkg.tar.zst
mingw-w64-x86_64-libiconv-1.17-3-any.pkg.tar.zst
mingw-w64-x86_64-expat-2.5.0-1-any.pkg.tar.zst
mingw-w64-x86_64-gettext-0.21.1-2-any.pkg.tar.zst
mingw-w64-x86_64-xz-5.4.4-1-any.pkg.tar.zst
mingw-w64-x86_64-libxml2-2.11.4-2-any.pkg.tar.zst
mingw-w64-x86_64-zstd-1.5.5-1-any.pkg.tar.zst
mingw-w64-x86_64-llvm-libs-16.0.5-3-any.pkg.tar.zst
mingw-w64-x86_64-llvm-16.0.5-3-any.pkg.tar.zst
mingw-w64-x86_64-binutils-2.41-1-any.pkg.tar.zst
mingw-w64-x86_64-headers-git-11.0.0.r107.gd367cc9d7-1-any.pkg.tar.zst
mingw-w64-x86_64-crt-git-11.0.0.r107.gd367cc9d7-1-any.pkg.tar.zst
mingw-w64-x86_64-gmp-6.3.0-1-any.pkg.tar.zst
mingw-w64-x86_64-isl-0.26-1-any.pkg.tar.zst
mingw-w64-x86_64-mpfr-4.2.0.p12-1-any.pkg.tar.zst
mingw-w64-x86_64-mpc-1.3.1-1-any.pkg.tar.zst
mingw-w64-x86_64-windows-default-manifest-6.4-4-any.pkg.tar.zst
mingw-w64-x86_64-winpthreads-git-11.0.0.r107.gd367cc9d7-1-any.pkg.tar.zst
mingw-w64-x86_64-gcc-13.2.0-1-any.pkg.tar.zst
mingw-w64-x86_64-clang-16.0.5-3-any.pkg.tar.zst
mingw-w64-x86_64-libclc-16.0.5-1-any.pkg.tar.zst
mingw-w64-x86_64-libelf-0.8.13-7-any.pkg.tar.zst
mingw-w64-x86_64-pkgconf-1~1.9.5-1-any.pkg.tar.zst
mingw-w64-x86_64-spirv-tools-1~1.3.250.0-1-any.pkg.tar.zst
mingw-w64-x86_64-spirv-llvm-translator-16.0.0-2-any.pkg.tar.zst
mingw-w64-x86_64-vulkan-headers-1.3.256-1-any.pkg.tar.zst
mingw-w64-x86_64-vulkan-loader-1.3.256-1-any.pkg.tar.zst
"

mkdir -p /usr/x86_64-w64-mingw32/bin
mkdir -p /usr/msys2
ln -s -T /usr/x86_64-w64-mingw32 /usr/msys2/mingw64

for i in $MINGW_PACKET_LIST
do
    curl -L -s --retry 5 -f --retry-all-errors --retry-delay 30 \
        -O "https://mirror.msys2.org/mingw/mingw64/$i"
    tar xf $i --strip-components=1 -C /usr/x86_64-w64-mingw32/
done
popd
rm -rf ~/tmp

if [ "$LLVM_VERSION" != "" ]; then
    export CLANG_CC_EXECUTABLE=clang-$LLVM_VERSION
    export CLANG_CXX_EXECUTABLE=clang++-$LLVM_VERSION
    export LLD_EXECUTABLE=ld.lld-$LLVM_VERSION
else
    export CLANG_CC_EXECUTABLE=clang
    export CLANG_CXX_EXECUTABLE=clang++
    export LLD_EXECUTABLE=ld.lld
fi

cat >/usr/x86_64-w64-mingw32/bin/clang <<EOF
#!/bin/sh
$CLANG_CC_EXECUTABLE --target=x86_64-pc-windows-gnu --sysroot=/usr/x86_64-w64-mingw32/ -femulated-tls -fuse-ld=/usr/x86_64-w64-mingw32/bin/ld \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/clang

cat >/usr/x86_64-w64-mingw32/bin/clang++ <<EOF
#!/bin/sh
$CLANG_CXX_EXECUTABLE --target=x86_64-pc-windows-gnu --sysroot=/usr/x86_64-w64-mingw32/ -femulated-tls -fuse-ld=/usr/x86_64-w64-mingw32/bin/ld \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/clang++

# Debian's pkgconf wrapers for mingw are broken, and there's no sign that
# they're going to be fixed, so we'll just have to fix it ourselves
# https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=930492
cat >/usr/x86_64-w64-mingw32/bin/pkgconf <<EOF
#!/bin/bash
set -o pipefail
wine pkgconf \$@ | sed -e "s,Z:/,/,gi"
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/pkgconf

# The output of `wine llvm-config --system-libs --cxxflags mcdisassembler`
# containes absolute path like '-IZ:'
# The sed is used to replace `-IZ:/usr/x86_64-w64-mingw32/include`
# to `-I/usr/x86_64-w64-mingw32/include`
cat >/usr/x86_64-w64-mingw32/bin/llvm-config <<EOF
#!/bin/bash
set -o pipefail
wine llvm-config \$@ | sed -e "s,Z:/,/,gi"
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/llvm-config

cat >/usr/x86_64-w64-mingw32/bin/llvm-as <<EOF
#!/bin/sh
wine llvm-as \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/llvm-as

cat >/usr/x86_64-w64-mingw32/bin/llvm-link <<EOF
#!/bin/sh
wine llvm-link \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/llvm-link

cat >/usr/x86_64-w64-mingw32/bin/opt <<EOF
#!/bin/sh
wine opt \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/opt

cat >/usr/x86_64-w64-mingw32/bin/llvm-spirv <<EOF
#!/bin/sh
wine llvm-spirv \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/llvm-spirv

cat >/usr/x86_64-w64-mingw32/bin/ar <<EOF
#!/bin/sh
wine ar \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/ar

cat >/usr/x86_64-w64-mingw32/bin/strip <<EOF
#!/bin/sh
wine strip \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/strip

cat >/usr/x86_64-w64-mingw32/bin/windres <<EOF
#!/bin/sh
wine windres \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/windres

cat >/usr/x86_64-w64-mingw32/bin/ld <<EOF
#!/bin/sh
$LLD_EXECUTABLE -lpthread \$@
EOF
chmod +x /usr/x86_64-w64-mingw32/bin/ld
