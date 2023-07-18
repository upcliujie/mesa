#!/usr/bin/env bash

arch=riscv64

export DEBIAN_FRONTEND=noninteractive

# needed for the RISC-V support, snapshot before bookworm release
{
  # disable backports and security updates
  find /etc/apt -type f -name '*.list' -exec sed -i 's/\(^deb.*-backports.*\)/#\1/; s/\(^deb.*-updates.*\)/#\1/; s/\(^deb.*-proposed.*\)/#\1/; s/\(^deb.*-security.*\)/#\1/' {} +
  apt-get update
  # do not check validity of older snapshot repositories
  echo "Acquire::Check-Valid-Until false;" | tee -a /etc/apt/apt.conf.d/10-nocheckvalid
  add-apt-repository -y "deb [arch=${arch}, trusted=yes] https://snapshot.debian.org/archive/debian-ports/20230614T205357Z/ unstable main"
  # Duplicated because of: https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1041012
  add-apt-repository -y "deb [arch=${arch}, trusted=yes] https://snapshot.debian.org/archive/debian-ports/20230614T205357Z/ unstable main"
}

# hack missing crossbuild-essential-riscv64 package
# see https://bugs.debian.org/cgi-bin/bugreport.cgi?bug=1022540
{
  apt-get install -y gcc-riscv64-linux-gnu g++-riscv64-linux-gnu
  curl -L -O http://mirrors.kernel.org/ubuntu/pool/universe/b/build-essential/crossbuild-essential-riscv64_12.8ubuntu1.1_all.deb
  dpkg -i crossbuild-essential-riscv64_12.8ubuntu1.1_all.deb
  rm crossbuild-essential-riscv64_12.8ubuntu1.1_all.deb
}

. .gitlab-ci/container/cross_build.sh
