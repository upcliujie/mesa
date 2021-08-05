#!/bin/sh

set -ex

mount -t proc none /proc
mount -t sysfs none /sys
mount -t devtmpfs none /dev || echo possibly already mounted
mkdir -p /dev/pts
mount -t devpts devpts /dev/pts
mount -t tmpfs tmpfs /tmp

. /crosvm-env.sh

# / is ro
export PIGLIT_REPLAY_EXTRA_ARGS="$PIGLIT_REPLAY_EXTRA_ARGS --db-path /tmp/replayer-db"

sleep 4
bash -c 'while :; do
  echo "------------- Guest -------------"
  TERM=xterm top -n1 -b | grep Cpu
  ps -eo pcpu,comm | sort -n | tac | head  -3

  sleep 10
done' &

while [ 1 ] ; do cat /proc/loadavg ; sleep 60; done &

if sh $CROSVM_TEST_SCRIPT; then
    touch /results/success
fi

sleep 5   # Leave some time to get the last output flushed out

poweroff -d -n -f || true

sleep 10   # Just in case init would exit before the kernel shuts down the VM

exit 1
