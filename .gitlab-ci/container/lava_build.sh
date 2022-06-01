#!/bin/bash

set -e
set -o xtrace

# Load variables and functions to use minio fdo instance as a artifact
# repository for build components and rootfs images
. .gitlab-ci/container/minio-utils.sh
export DEBIAN_FRONTEND=noninteractive


build_apitrace() {
    . .gitlab-ci/container/build-apitrace.sh
    mv /apitrace/build ${TMP_ROOTFS_DIR}/apitrace
    rm -Rf /apitrace
}

build_deqp_runner() {
    ############### Build dEQP runner
    . .gitlab-ci/container/build-deqp-runner.sh
    mkdir -p ${TMP_ROOTFS_DIR}/usr/bin
    mv /usr/local/bin/*-runner ${TMP_ROOTFS_DIR}/usr/bin/.
}

build_deqp() {
    ############### Build dEQP
    DEQP_TARGET=surfaceless . .gitlab-ci/container/build-deqp.sh
    mv /deqp ${TMP_ROOTFS_DIR}
}

build_skqp() {
    ############### Build SKQP
    if [[ "$DEBIAN_ARCH" = "arm64" ]] \
    || [[ "$DEBIAN_ARCH" = "amd64" ]]; then
        . .gitlab-ci/container/build-skqp.sh
        mv /skqp ${TMP_ROOTFS_DIR}
    fi
}

build_piglit() {
    ############### Build piglit
    PIGLIT_OPTS="-DPIGLIT_BUILD_DMA_BUF_TESTS=ON" . .gitlab-ci/container/build-piglit.sh
    mv /piglit ${TMP_ROOTFS_DIR}
}

build_libva_tests() {
    ############### Build libva tests
    if [[ "$DEBIAN_ARCH" = "amd64" ]]; then
        . .gitlab-ci/container/build-va-tools.sh
        mkdir -p ${TMP_ROOTFS_DIR}/usr/bin
        mv /va/bin/* ${TMP_ROOTFS_DIR}/usr/bin/
    fi
}

build_crosvm() {
    ############### Build Crosvm
    if [[ ${DEBIAN_ARCH} = "amd64" ]]; then
        . .gitlab-ci/container/build-crosvm.sh
        mkdir -p ${TMP_ROOTFS_DIR}/usr/bin
        mkdir -p ${TMP_ROOTFS_DIR}/usr/lib/"${GCC_ARCH}"
        mv /usr/local/bin/crosvm ${TMP_ROOTFS_DIR}/usr/bin/
        mv /usr/local/lib/$GCC_ARCH/libvirglrenderer.* ${TMP_ROOTFS_DIR}/usr/lib/$GCC_ARCH/
    fi
}

build_libdrm() {
    ############### Build libdrm
    EXTRA_MESON_ARGS+=" -D prefix=/libdrm"
    . .gitlab-ci/container/build-libdrm.sh
    mkdir -p ${TMP_ROOTFS_DIR}/usr/lib/"$GCC_ARCH"
    find /libdrm/ -name lib\*\.so\* | xargs cp -t ${TMP_ROOTFS_DIR}/usr/lib/$GCC_ARCH/.
    mkdir -p ${TMP_ROOTFS_DIR}/libdrm/
    cp -Rp /libdrm/share ${TMP_ROOTFS_DIR}/libdrm/share
    rm -rf /libdrm
}

build_kernel() {
    ############### Build kernel
    # Installing part is inside the script
    . .gitlab-ci/container/build-kernel.sh
}

# If remote files are up-to-date, skip rebuilding them
# check_minio "${FDO_UPSTREAM_REPO}"
check_minio

. .gitlab-ci/container/container_pre_build.sh

# Install rust, which we'll be using for deqp-runner.  It will be cleaned up at the end.
. .gitlab-ci/container/build-rust.sh

if [[ "$DEBIAN_ARCH" = "arm64" ]]; then
    GCC_ARCH="aarch64-linux-gnu"
    KERNEL_ARCH="arm64"
    SKQP_ARCH="arm64"
    DEFCONFIG="arch/arm64/configs/defconfig"
    DEVICE_TREES="arch/arm64/boot/dts/rockchip/rk3399-gru-kevin.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/amlogic/meson-gxl-s805x-libretech-ac.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/allwinner/sun50i-h6-pine-h64.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/amlogic/meson-gxm-khadas-vim2.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/qcom/apq8016-sbc.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/qcom/apq8096-db820c.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/amlogic/meson-g12b-a311d-khadas-vim3.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/mediatek/mt8183-kukui-jacuzzi-juniper-sku16.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/nvidia/tegra210-p3450-0000.dtb"
    DEVICE_TREES+=" arch/arm64/boot/dts/qcom/sc7180-trogdor-lazor-limozeen-nots.dtb"
    KERNEL_IMAGE_NAME="Image"

elif [[ "$DEBIAN_ARCH" = "armhf" ]]; then
    GCC_ARCH="arm-linux-gnueabihf"
    KERNEL_ARCH="arm"
    SKQP_ARCH="arm"
    DEFCONFIG="arch/arm/configs/multi_v7_defconfig"
    DEVICE_TREES="arch/arm/boot/dts/rk3288-veyron-jaq.dtb"
    DEVICE_TREES+=" arch/arm/boot/dts/sun8i-h3-libretech-all-h3-cc.dtb"
    DEVICE_TREES+=" arch/arm/boot/dts/imx6q-cubox-i.dtb"
    KERNEL_IMAGE_NAME="zImage"
    . .gitlab-ci/container/create-cross-file.sh armhf
else
    GCC_ARCH="x86_64-linux-gnu"
    KERNEL_ARCH="x86_64"
    SKQP_ARCH="x64"
    DEFCONFIG="arch/x86/configs/x86_64_defconfig"
    DEVICE_TREES=""
    KERNEL_IMAGE_NAME="bzImage"
    ARCH_PACKAGES="libasound2-dev libcap-dev libfdt-dev libva-dev wayland-protocols"
fi

# Determine if we're in a cross build.
if [[ -e /cross_file-$DEBIAN_ARCH.txt ]]; then
    EXTRA_MESON_ARGS="--cross-file /cross_file-$DEBIAN_ARCH.txt"
    EXTRA_CMAKE_ARGS="-DCMAKE_TOOLCHAIN_FILE=/toolchain-$DEBIAN_ARCH.cmake"

    if [ $DEBIAN_ARCH = arm64 ]; then
        RUST_TARGET="aarch64-unknown-linux-gnu"
    elif [ $DEBIAN_ARCH = armhf ]; then
        RUST_TARGET="armv7-unknown-linux-gnueabihf"
    fi
    rustup target add $RUST_TARGET
    export EXTRA_CARGO_ARGS="--target $RUST_TARGET"

    export ARCH=${KERNEL_ARCH}
    export CROSS_COMPILE="${GCC_ARCH}-"
fi

apt-get update
apt-get install -y --no-remove \
                   ${ARCH_PACKAGES} \
                   automake \
                   bc \
                   clang \
                   cmake \
                   debootstrap \
                   git \
                   glslang-tools \
                   libdrm-dev \
                   libegl1-mesa-dev \
                   libxext-dev \
                   libfontconfig-dev \
                   libgbm-dev \
                   libgl-dev \
                   libgles2-mesa-dev \
                   libglu1-mesa-dev \
                   libglx-dev \
                   libpng-dev \
                   libssl-dev \
                   libudev-dev \
                   libvulkan-dev \
                   libwaffle-dev \
                   libwayland-dev \
                   libx11-xcb-dev \
                   libxcb-dri2-0-dev \
                   libxkbcommon-dev \
                   ninja-build \
                   patch \
                   python-is-python3 \
                   python3-distutils \
                   python3-mako \
                   python3-numpy \
                   python3-serial \
                   unzip \
                   wget


if [[ "$DEBIAN_ARCH" = "armhf" ]]; then
    apt-get install -y --no-remove \
                       libegl1-mesa-dev:armhf \
                       libelf-dev:armhf \
                       libgbm-dev:armhf \
                       libgles2-mesa-dev:armhf \
                       libpng-dev:armhf \
                       libudev-dev:armhf \
                       libvulkan-dev:armhf \
                       libwaffle-dev:armhf \
                       libwayland-dev:armhf \
                       libx11-xcb-dev:armhf \
                       libxkbcommon-dev:armhf
fi


############### Building
STRIP_CMD="${GCC_ARCH}-strip"
mkdir -p /lava-files/rootfs-${DEBIAN_ARCH}/usr/lib/$GCC_ARCH

DEPS="apitrace deqp_runner deqp skqp piglit libva_tests crosvm"
for dep in $DEPS
do
    artifact_repo_sync "$dep"
done

artifact_repo_sync kernel /lava-files

############### Build local stuff for use by igt and kernel testing, which
############### will reuse most of our container build process from a specific
############### hash of the Mesa tree.
if [[ -e ".gitlab-ci/local/build-rootfs.sh" ]]; then
    . .gitlab-ci/local/build-rootfs.sh
fi


############### Delete rust, since the tests won't be compiling anything.
rm -rf /root/.cargo
rm -rf /root/.rustup

############### Create rootfs
set +e
if ! ${DEBOOTSTRAP_CMD} \
     --variant=minbase \
     --arch=${DEBIAN_ARCH} \
     --components main,contrib,non-free \
     bullseye \
     /lava-files/rootfs-${DEBIAN_ARCH}/ \
     http://deb.debian.org/debian; then
    cat /lava-files/rootfs-${DEBIAN_ARCH}/debootstrap/debootstrap.log
    exit 1
fi
set -e

cp .gitlab-ci/container/create-rootfs.sh /lava-files/rootfs-${DEBIAN_ARCH}/.
${CHROOT_CMD} /lava-files/rootfs-${DEBIAN_ARCH} sh /create-rootfs.sh
rm /lava-files/rootfs-${DEBIAN_ARCH}/create-rootfs.sh


############### Install the built libdrm
# Dependencies pulled during the creation of the rootfs may overwrite
# the built libdrm. Hence, we add it after the rootfs has been already
# created.
artifact_repo_sync libdrm


if [ ${DEBIAN_ARCH} = arm64 ]; then
    # Make a gzipped copy of the Image for db410c.
    gzip -k /lava-files/Image
    KERNEL_IMAGE_NAME+=" Image.gz"
fi

du -ah /lava-files/rootfs-${DEBIAN_ARCH} | sort -h | tail -100
pushd /lava-files/rootfs-${DEBIAN_ARCH}
  tar czf /lava-files/"${ROOTFS_FILE}" .
popd

. .gitlab-ci/container/container_post_build.sh

############### Upload the files!
ci-fairy minio login --token-file "${CI_JOB_JWT_FILE}"
FILES_TO_UPLOAD="lava-rootfs.tgz \
                 $KERNEL_IMAGE_NAME"

if [[ -n $DEVICE_TREES ]]; then
    FILES_TO_UPLOAD="$FILES_TO_UPLOAD $(basename -a $DEVICE_TREES)"
fi

for f in $FILES_TO_UPLOAD; do
    ci-fairy minio cp /lava-files/$f \
             minio://"${MINIO_DIR}"/$f
done

touch /lava-files/done
ci-fairy minio cp /lava-files/done minio://"${MINIO_DIR}"/done
