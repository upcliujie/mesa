#!/bin/sh

# Very early init, used to make sure devices and network are set up and
# reachable.

set -ex

cd /

mount -t proc none /proc
mount -t sysfs none /sys
mount -t debugfs none /sys/kernel/debug
mount -t devtmpfs none /dev || echo possibly already mounted
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
mkdir /dev/shm
mount -t tmpfs -o noexec,nodev,nosuid tmpfs /dev/shm
mount -t tmpfs tmpfs /tmp

# Use FreeDesktop.org's DNS resolver, so that any change we need to make is
# instantly propagated.
# Note: any domain not under freedesktop.org will fail to resolve.
echo "nameserver 131.252.210.177" > /etc/resolv.conf

[ -z "$NFS_SERVER_IP" ] || echo "$NFS_SERVER_IP caching-proxy" >> /etc/hosts

# Set the time so we can validate certificates before we fetch anything;
# however as not all DUTs have network, make this non-fatal.
for _ in 1 2 3; do sntp -sS pool.ntp.org && break || sleep 2; done || true
