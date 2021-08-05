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
  # Get the first line with aggregate of all CPUs 
  cpu_now=($(head -n1 /proc/stat)) 
  # Get all columns but skip the first (which is the "cpu" string) 
  cpu_sum="${cpu_now[@]:1}" 
  # Replace the column seperator (space) with + 
  cpu_sum=$((${cpu_sum// /+})) 
  # Get the delta between two reads 
  cpu_delta=$((cpu_sum - cpu_last_sum)) 
  # Get the idle time Delta 
  cpu_idle=$((cpu_now[4]- cpu_last[4])) 
  # Calc time spent working 
  cpu_used=$((cpu_delta - cpu_idle)) 
  # Calc percentage 
  cpu_usage=$((100 * cpu_used / cpu_delta)) 
  
  # Keep this as last for our next read 
  cpu_last=("${cpu_now[@]}") 
  cpu_last_sum=$cpu_sum 
  
  echo "Guest CPU usage at $cpu_usage%" 
  ps aux | sort -nrk 3,3 | head -n 4 | tr -s " " | cut -d " " -f 3,11-
  echo "------------------------------"
  
  # Wait a second before the next read 
  sleep 10
done' &

while [ 1 ] ; do cat /proc/loadavg ; sleep 60; done &

if sh $CROSVM_TEST_SCRIPT; then
    touch /results/success
fi

poweroff -d -n -f || true

sleep 10   # Just in case init would exit before the kernel shuts down the VM

exit 1
