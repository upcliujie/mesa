#!/bin/bash

set -e
set -o xtrace

############### Install packages for building
apt-get install -y ca-certificates
sed -i -e 's/http:\/\/deb/https:\/\/deb/g' /etc/apt/sources.list
echo 'deb https://deb.debian.org/debian buster main' >/etc/apt/sources.list.d/buster.list
apt-get update

apt-get install -y --no-remove \
        abootimg \
        bc \
        bison \
        bzip2 \
        ccache \
        cmake \
        cpio \
        debootstrap \
        fastboot \
        flex \
        g++ \
        git \
        meson \
        netcat \
        nginx-full \
        pkg-config \
        procps \
        python-is-python3 \
        python3-distutils \
        python3-minimal \
        python3-serial \
        rsync \
        telnet \
        u-boot-tools \
        unzip

# Not available in bullseye anymore
apt-get install -y --no-remove -t buster \
        android-sdk-ext4-utils

# setup nginx
sed -i '/gzip_/ s/#\ //g' /etc/nginx/nginx.conf
cp .gitlab-ci/bare-metal/nginx-default-site  /etc/nginx/sites-enabled/default

. .gitlab-ci/container/container_post_build.sh
