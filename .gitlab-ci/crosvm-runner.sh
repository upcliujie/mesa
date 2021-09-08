#!/bin/sh

set -e

# This script can be called concurrently, pass arguments and env in a per-instance tmp dir
export DEQP_TEMP_DIR=`mktemp -d /tmp.XXXXXXXXXX`

# The dEQP binary needs to run from the directory it's in
if [ -z "${1##*"deqp"*}" ]; then
  PWD=`dirname $1`
fi

export -p > $DEQP_TEMP_DIR/crosvm-env.sh

CROSVM_KERNEL_ARGS="root=my_root rw rootfstype=virtiofs loglevel=3 init=$CI_PROJECT_DIR/install/crosvm-init.sh ip=192.168.30.2::192.168.30.1:255.255.255.0:crosvm:eth0 -- $DEQP_TEMP_DIR"

echo $@ > $DEQP_TEMP_DIR/crosvm-script.sh

unset DISPLAY
unset XDG_RUNTIME_DIR

# We aren't testing LLVMPipe here, so we don't need to validate NIR on the host
export NIR_VALIDATE=0

export LIBGL_ALWAYS_SOFTWARE="true"
export GALLIUM_DRIVER="$CROSVM_GALLIUM_DRIVER"

# One-time setup
(
  flock 9
  if [ -z "`pidof syslogd`" ]; then
      /usr/sbin/iptables-legacy  -t nat -A POSTROUTING -o eth0 -j MASQUERADE
      echo 1 > /proc/sys/net/ipv4/ip_forward

      syslogd > /dev/null &  # Crosvm requires syslogd
      sleep 1
  fi
) 9>/var/lock/crosvm_lock

crosvm run \
  --gpu "$CROSVM_GPU_ARGS" \
  -m 4096 \
  -c 1 \
  --disable-sandbox \
  --shared-dir /:my_root:type=fs:writeback=true:timeout=60:cache=always \
  --host_ip=192.168.30.1 --netmask=255.255.255.0 --mac "AA:BB:CC:00:00:12" \
  -p "$CROSVM_KERNEL_ARGS" \
  /lava-files/bzImage 2>&1 | LC_ALL=C tr -dc '\0-\177'

test -f $CI_PROJECT_DIR/results/success
