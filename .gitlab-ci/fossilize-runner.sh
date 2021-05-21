#!/bin/sh

set -ex

if [ -z "$VK_DRIVER" ]; then
   echo 'VK_DRIVER must be to something like "radeon" or "intel" for the test run'
   exit 1
fi

# Set up the driver environment.
export LD_LIBRARY_PATH=/install/lib/
export VK_ICD_FILENAMES=/install/share/vulkan/icd.d/"$VK_DRIVER"_icd.x86_64.json

# To store Fossilize logs on failure.
RESULTS=`pwd`/results
mkdir -p results

"/install/fossils/fossils.sh" "/install/fossils.yml" "$RESULTS"
