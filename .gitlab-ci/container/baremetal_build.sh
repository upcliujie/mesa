#!/bin/bash

set -e
set -o xtrace

# Fetch the arm-built rootfs image and unpack it in our x86 container (saves
# network transfer, disk usage, and runtime on test jobs)

ARTIFACTS_PREFIX="https://${MINIO_HOST}/mesa-lava/"
if wget -q --method=HEAD "${ARTIFACTS_PREFIX}/${FDO_UPSTREAM_REPO}/${DISTRIBUTION_TAG}/${DEBIAN_ARCH}/done"; then
  ARTIFACTS_URL="${ARTIFACTS_PREFIX}/mesa/mesa/${DISTRIBUTION_TAG}/${DEBIAN_ARCH}"
else
  ARTIFACTS_URL="${ARTIFACTS_PREFIX}/${CI_PROJECT_PATH}/${DISTRIBUTION_TAG}/${DEBIAN_ARCH}"
fi

wget $ARTIFACTS_URL -O rootfs.tgz
mkdir -p /rootfs
tar -C /rootfs -zxvf rootfs.tgz
