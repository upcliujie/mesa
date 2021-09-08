#!/bin/sh

set -e

export DEQP_TEMP_DIR=$1

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev || echo possibly already mounted
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /tmp

. $DEQP_TEMP_DIR/crosvm-env.sh

cd $PWD

set +e
sh $DEQP_TEMP_DIR/crosvm-script.sh
echo $? > $DEQP_TEMP_DIR/exit_code
set -e

sleep 1   # Leave some time to get the last output flushed out

poweroff -d -n -f || true

sleep 1   # Just in case init would exit before the kernel shuts down the VM
