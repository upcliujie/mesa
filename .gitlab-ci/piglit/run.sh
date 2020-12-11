#!/bin/bash

set -e
set -o xtrace

VERSION=`cat install/VERSION`

rm -rf results
cd /piglit

PIGLIT_OPTIONS=$(echo $PIGLIT_OPTIONS | head -n 1)
set +e
xvfb-run --server-args="-noreset" sh -c \
         "export LD_LIBRARY_PATH=$OLDPWD/install/lib;
         wflinfo --platform glx --api gl --profile core | tee /tmp/version.txt | grep \"Mesa $VERSION\\\$\" &&
         ./piglit run -j${FDO_CI_CONCURRENT:-4} $PIGLIT_OPTIONS $PIGLIT_PROFILES $OLDPWD/results"
retVal=$?
if [ $retVal -ne 0 ]; then
    echo "Found $(cat /tmp/version.txt), expected $VERSION"
fi
set -e

PIGLIT_RESULTS=${PIGLIT_RESULTS:-$PIGLIT_PROFILES}
REFERENCE=$OLDPWD/install/piglit/$PIGLIT_RESULTS.txt
RESULTS=$OLDPWD/results/$PIGLIT_RESULTS.txt

mkdir -p .gitlab-ci/piglit
./piglit summary console $OLDPWD/results | head -n -1 | grep -v ": pass" > $RESULTS

if diff -q $REFERENCE $RESULTS; then
    exit 0
fi

./piglit summary html --exclude-details=pass $OLDPWD/summary $OLDPWD/results

echo Unexpected change in results:
diff -u $REFERENCE $RESULTS
exit 1
