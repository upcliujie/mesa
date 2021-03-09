#!/bin/sh

set -e

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev || echo possibly already mounted
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /tmp

. `dirname "$0"`/crosvm-env.sh

sleep 2

# / is ro
export PIGLIT_REPLAY_EXTRA_ARGS="$PIGLIT_REPLAY_EXTRA_ARGS --db-path /tmp/replayer-db"

sleep 2

if sh $CROSVM_TEST_SCRIPT; then
    echo 'mesa: pass';
else
    echo 'mesa: fail';
fi

sleep 2

poweroff -d -n -f || true

sleep 10   # Just in case init would exit before the kernel shuts down the VM

exit 1
