#!/usr/bin/env bash
# Copyright © David Heidelberg
# SPDX-License-Identifier: MIT

set -ex

CI_COMMON=${CI_PROJECT_DIR}/install/common

CI_RUNNER_TAG="${CI_RUNNER_TAGS//[\"\[\]]/}"
DEVICE_TYPE="${CI_RUNNER_TAG#*:}"

eval $(labgrid-client reserve --prio "${LAVA_JOB_PRIORITY}" --shell board="${DEVICE_TYPE}")
labgrid-client wait

BOARD=$(labgrid-client -p + acquire | cut -d" " -f3-)
echo "Board: ${BOARD:?}"
[[ "$BOARD" =~ ([0-9]+)$ ]] && BOARD_NUMBER="${BASH_REMATCH[1]:?}"

rm -rf results

NFSPATH="/srv/${BOARD}/nfs"

rsync -a --delete "${ROOTFS}/" "${NFSPATH}"

# Make JWT token available as file in the device storage to enable access to S3
cp "${CI_JOB_JWT_FILE}" "${NFSPATH}${CI_JOB_JWT_FILE}"

cp "${CI_COMMON}"/capture-devcoredump.sh "${NFSPATH}/"
cp "${CI_COMMON}"/init-*.sh "${NFSPATH}/"
cp "${CI_COMMON}"/intel-gpu-freq.sh "${NFSPATH}/"
cp "${CI_COMMON}"/kdl.sh "${NFSPATH}/"
cp "${SCRIPTS_DIR}"/setup-test-env.sh "${NFSPATH}/"

rsync -aH --delete "${CI_PROJECT_DIR}"/install/ "${NFSPATH}"/install/

echo "$NFS_SERVER_IP caching-proxy" >> "${NFSPATH}"/etc/hosts

# Prepare env vars for upload.
section_start variables "Variables passed through:"
"${CI_COMMON}"/generate-env.sh | tee "${NFSPATH}/set-job-env-vars.sh"
section_end variables

mkdir -p results/logs/{uboot,linux,test}
touch results/logs/{uboot,linux,test}/console_main
tail -F results/logs/*/console_main &

section_start env_definition "env.yaml definition"
cat << EOF > env.yaml
targets:
  main:
    resources:
      RemotePlace:
        name: ${BOARD}
    drivers:
      TasmotaPowerDriver: {}
      SerialDriver: {}
      UBootDriver:
        prompt: 'u-boot=> '
        init_commands: [
          'setenv serverip 10.0.0.111',
          'setenv ipaddr 10.0.0.$((229 + BOARD_NUMBER))',
          'setenv bootargs debug init=/init root=/dev/nfs ip=:::::eth0:dhcp nfsroot=10.0.0.111:/srv/${BOARD}/nfs,v3,tcp rw',
          'tftp 0x40480000 Image',
          'tftp 0x83000000 ${DEVICE_TYPE}.dtb',
        ]
        boot_command: 'booti 0x40480000 - 0x83000000'

      ShellDriver:
        prompt: 'lava-shell:'
        login_prompt: 'not_used_at_all'
        username: 'something_not_relevant'
      UBootStrategy: {}
EOF

cat env.yaml
section_end env_definition

set +e
python3 install/labgrid/labgrid_submit.py --job-timeout="${JOB_TIMEOUT:-30}"
ret=$?

cp -Rp "${NFSPATH}/results/." results/
set -e

labgrid-client -p + release
labgrid-client cancel-reservation

exit $ret
