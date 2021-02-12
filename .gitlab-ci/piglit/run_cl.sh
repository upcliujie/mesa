#!/bin/bash

set -e
set -o xtrace

VERSION=`cat install/VERSION`

rm -rf results
cd /piglit

export OCL_ICD_VENDORS=$OLDPWD/install/etc/OpenCL/vendors/

PIGLIT_OPTIONS=$(echo $PIGLIT_OPTIONS | head -n 1)
DRIVER_NAME="${PIGLIT_RESULTS%%-*}"
set +e
unset DISPLAY
export LD_LIBRARY_PATH=$OLDPWD/install/lib
clinfo
./piglit run -c -j${FDO_CI_CONCURRENT:-4} $PIGLIT_OPTIONS $PIGLIT_PROFILES $OLDPWD/results
retVal=$?
if [ $retVal -ne 0 ]; then
    echo "Found $(cat /tmp/version.txt), expected $VERSION"
fi
set -e

PIGLIT_RESULTS=${PIGLIT_RESULTS:-$PIGLIT_PROFILES}
mkdir -p ci-expects/$DRIVER_NAME
cp $OLDPWD/install/piglit/$PIGLIT_RESULTS.txt "ci-expects/$DRIVER_NAME/$PIGLIT_RESULTS.txt.baseline"
./piglit summary console $OLDPWD/results | head -n -1 | grep -v ": pass" >ci-expects/$DRIVER_NAME/$PIGLIT_RESULTS.txt

if diff -q ci-expects/$DRIVER_NAME/$PIGLIT_RESULTS.txt{.baseline,}; then
    exit 0
fi

./piglit summary html --exclude-details=pass $OLDPWD/summary $OLDPWD/results

echo Unexpected change in results:
diff -u ci-expects/$DRIVER_NAME/$PIGLIT_RESULTS.txt{.baseline,}
exit 1
