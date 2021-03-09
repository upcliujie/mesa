#!/bin/sh

set -ex

INSTALL_DIR=`pwd`/install

export LD_LIBRARY_PATH=$INSTALL_DIR/lib/
export EGL_PLATFORM=surfaceless

env > $INSTALL_DIR/crosvm-env.sh

mkdir -p /results
mount -t tmpfs tmpfs /results

mkdir -p /piglit/.gitlab-ci/piglit
mount -t tmpfs tmpfs /piglit/.gitlab-ci/piglit

unset DISPLAY
unset XDG_RUNTIME_DIR

/usr/sbin/iptables-legacy  -t nat -A POSTROUTING -o eth0 -j MASQUERADE
echo 1 > /proc/sys/net/ipv4/ip_forward 

# Crosvm wants this
syslogd -n &> /dev/null

crosvm run \
  --gpu gles=false,backend=3d,egl=true,surfaceless=true \
  -m 4096 \
  -c 4 \
  --disable-sandbox \
  --shared-dir /:my_root:type=fs:writeback=true:timeout=60:cache=always \
  --host_ip=192.168.30.1 --netmask=255.255.255.0 --mac "AA:BB:CC:00:00:12" \
  -p "root=my_root rw rootfstype=virtiofs loglevel=3 init=$INSTALL_DIR/crosvm-init.sh ip=192.168.30.2::192.168.30.1:255.255.255.0:crosvm:eth0" \
  /lava-files/bzImage

