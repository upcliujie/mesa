#!/bin/bash

set -e
set -o xtrace

VERSION=`cat install/VERSION`

rm -rf results
cd /piglit

export OCL_ICD_VENDORS=$OLDPWD/install/etc/OpenCL/vendors/

set +e
unset DISPLAY
export LD_LIBRARY_PATH=$OLDPWD/install/lib
clinfo

if [ "x$PIGLIT_PROFILES" != "xreplay" ] && [ "$PIGLIT_PROFILES" = "${PIGLIT_PROFILES% *}" ]; then
    USE_CASELIST=1
fi

if [ -n "$USE_CASELIST" ]; then
    ./piglit print-cmd $PIGLIT_TESTS $PIGLIT_PROFILES --format "{name}" > /tmp/case-list.txt

    PIGLIT_TESTS="--test-list /tmp/case-list.txt"

    # If the caselist is too long to run in a reasonable amount of time, let
    # the job specify what fraction (1/n) of the caselist we should run.
    # Note: N~M is a gnu sed extension to match every nth line (first line is
    # #1).
    if [ -n "$PIGLIT_FRACTION" ]; then
        sed -ni 1~$PIGLIT_FRACTION"p" /tmp/case-list.txt
    fi

    # If the job is parallel at the gitlab job level, take the corresponding
    # fraction of the caselist.
    if [ -n "$CI_NODE_INDEX" ]; then
        sed -ni $CI_NODE_INDEX~$CI_NODE_TOTAL"p" /tmp/case-list.txt
    fi
else
    if [ -n "$PIGLIT_FRACTION" ]; then
        echo "Can't use PIGLIT_FRACTION with replay profile or multiple profiles"
        exit 1
    fi

    if [ -n "$CI_NODE_INDEX" ]; then
        echo "Can't parallelize piglit with replay profile or multiple profiles"
        exit 1
    fi
fi

./piglit run -c -j${FDO_CI_CONCURRENT:-4} $PIGLIT_OPTIONS $PIGLIT_TESTS $PIGLIT_PROFILES $OLDPWD/results
retVal=$?
if [ $retVal -ne 0 ]; then
    echo "Found $(cat /tmp/version.txt), expected $VERSION"
fi
set -e

PIGLIT_RESULTS=${PIGLIT_RESULTS:-$PIGLIT_PROFILES}
mkdir -p .gitlab-ci/piglit
./piglit summary console $OLDPWD/results | head -n -1 | grep -v ": pass" > .gitlab-ci/piglit/$PIGLIT_RESULTS.txt

if [ -n "$USE_CASELIST" ]; then
    # Just filter the expected results based on the tests that were actually
    # executed, and switch to the version with no summary
    cat .gitlab-ci/piglit/$PIGLIT_RESULTS.txt | head -n -16 | tee .gitlab-ci/piglit/$PIGLIT_RESULTS.txt.new \
         | rev | cut -f2- -d: | rev | sed "s/$/:/g" > /tmp/executed.txt
    grep -F -f /tmp/executed.txt $OLDPWD/install/piglit/$PIGLIT_RESULTS.txt \
         > .gitlab-ci/piglit/$PIGLIT_RESULTS.txt.baseline || true
else
    cp $OLDPWD/install/piglit/$PIGLIT_RESULTS.txt .gitlab-ci/piglit/$PIGLIT_RESULTS.txt.baseline
    cp .gitlab-ci/piglit/$PIGLIT_RESULTS.txt .gitlab-ci/piglit/$PIGLIT_RESULTS.txt.new
fi

if diff -q .gitlab-ci/piglit/$PIGLIT_RESULTS.txt.{baseline,new}; then
    exit 0
fi

./piglit summary html --exclude-details=pass $OLDPWD/summary $OLDPWD/results

echo Unexpected change in results:
diff -u .gitlab-ci/piglit/$PIGLIT_RESULTS.txt.{baseline,new}
exit 1
