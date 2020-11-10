#!/bin/bash

set -ex

if [ $DEBIAN_ARCH = arm64 ]; then
    ARCH_PACKAGES="firmware-qcom-media"
elif [ $DEBIAN_ARCH = amd64 ]; then
    # Upstream LLVM package repository
    apt-get -y install --no-install-recommends gnupg ca-certificates
    apt-key add /llvm-snapshot.gpg.key
    echo "deb https://apt.llvm.org/buster/ llvm-toolchain-buster-10 main" >/etc/apt/sources.list.d/llvm10.list
    apt-get update

    ARCH_PACKAGES="libelf1
                   libllvm10
                   libxcb-dri2-0
                   libxcb-dri3-0
                   libxcb-present0
                   libxcb-sync1
                   libxcb-xfixes0
                   libxshmfence1
                   firmware-amd-graphics
                  "
fi

apt-get -y install --no-install-recommends \
    ca-certificates \
    curl \
    git \
    initramfs-tools \
    libpng16-16 \
    strace \
    libsensors5 \
    libexpat1 \
    libx11-6 \
    libx11-xcb1 \
    libxkbcommon0 \
    $ARCH_PACKAGES \
    netcat-openbsd \
    python3 \
    libpython3.7 \
    python3-lxml \
    python3-mako \
    python3-numpy \
    python3-pil \
    python3-pip \
    python3-pytest \
    python3-requests \
    python3-setuptools \
    python3-simplejson
    python3-yaml \
    sntp \
    wget \
    xz-utils

if [ -n "$INCLUDE_VK_CTS" ]; then
    apt-get install -y libvulkan1
fi

passwd root -d
chsh -s /bin/sh

cat > /init <<EOF
#!/bin/sh
export PS1=lava-shell:
exec sh
EOF
chmod +x  /init

mkdir -p /lib/firmware/rtl_nic
wget https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/tree/rtl_nic/rtl8153a-3.fw -O /lib/firmware/rtl_nic/rtl8153a-3.fw

# Needed for ci-fairy, this revision is able to upload files to MinIO
pip3 install git+http://gitlab.freedesktop.org/freedesktop/ci-templates@6f5af7e5574509726c79109e3c147cee95e81366

apt-get purge -y \
    git \
    python3-pip \
    python3-setuptools
