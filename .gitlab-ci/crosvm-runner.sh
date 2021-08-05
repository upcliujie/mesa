#!/bin/sh

set -x

ln -sf $CI_PROJECT_DIR/install /install

export LD_LIBRARY_PATH=$CI_PROJECT_DIR/install/lib/
export EGL_PLATFORM=surfaceless

export -p > /crosvm-env.sh
export GALLIUM_DRIVER="$CROSVM_GALLIUM_DRIVER"
export GALLIVM_PERF="nopt"
export LIBGL_ALWAYS_SOFTWARE="true"

CROSVM_KERNEL_ARGS="root=my_root rw rootfstype=virtiofs loglevel=3 init=$CI_PROJECT_DIR/install/crosvm-init.sh ip=192.168.30.2::192.168.30.1:255.255.255.0:crosvm:eth0"

# Temporary results dir because from the guest we cannot write to /
mkdir -p /results
mount -t tmpfs tmpfs /results

mkdir -p /piglit/.gitlab-ci/piglit
mount -t tmpfs tmpfs /piglit/.gitlab-ci/piglit

unset DISPLAY
unset XDG_RUNTIME_DIR

/usr/sbin/iptables-legacy  -t nat -A POSTROUTING -o eth0 -j MASQUERADE
echo 1 > /proc/sys/net/ipv4/ip_forward

# Crosvm wants this
syslogd > /dev/null

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
  
  echo "CPU usage at $cpu_usage%" 
  ps aux | sort -nrk 3,3 | head -n 10 | tr -s ' ' | cut -d ' ' -f 3,11
  
  # Wait a second before the next read 
  sleep 10
done' &

while [ 1 ] ; do cat /proc/loadavg ; sleep 60; done &

crosvm run \
  --gpu "$CROSVM_GPU_ARGS" \
  -m 4096 \
  -c $FDO_CI_CONCURRENT \
  --disable-sandbox \
  --shared-dir /:my_root:type=fs:writeback=true:timeout=60:cache=always \
  --host_ip=192.168.30.1 --netmask=255.255.255.0 --mac "AA:BB:CC:00:00:12" \
  -p "$CROSVM_KERNEL_ARGS" \
  /lava-files/bzImage

mkdir -p $CI_PROJECT_DIR/results
mv /results/* $CI_PROJECT_DIR/results/.

test -f $CI_PROJECT_DIR/results/success
