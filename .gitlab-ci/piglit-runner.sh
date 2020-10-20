#!/bin/bash

set -ex

INSTALL=`pwd`/install

# Set up the driver environment.
export LD_LIBRARY_PATH=$INSTALL/lib/
export LIBGL_DRIVERS_PATH=$INSTALL/lib/dri
export EGL_PLATFORM=surfaceless

RESULTS=`pwd`/results
mkdir -p $RESULTS

PIGLIT_OPTIONS=$(echo $PIGLIT_OPTIONS | head -n 1)
PIGLIT_OPTIONS=${PIGLIT_OPTIONS#?}

if [ -n "$PIGLIT_PARALLEL" ]; then
   JOB="--jobs $PIGLIT_PARALLEL"
elif [ -n "$FDO_CI_CONCURRENT" ]; then
   JOB="--jobs $FDO_CI_CONCURRENT"
else
   JOB="--no-concurrency"
fi

run_piglit() {
    `pwd`/piglit/piglit run \
        $JOB \
        $PIGLIT_OPTIONS \
        $PIGLIT_PROFILES \
        $RESULTS/$PIGLIT_PROFILES

     PIGLIT_RESULTS=${PIGLIT_RESULTS:-$PIGLIT_PROFILES}
     cp $INSTALL/$PIGLIT_RESULTS $RESULTS/$PIGLIT_RESULTS.baseline
     `pwd`/piglit/piglit summary console $RESULTS/$PIGLIT_PROFILES | head -n -1 | grep -v ": pass" > $RESULTS/$PIGLIT_RESULTS
}

run_piglit

echo "System load: $(cut -d' ' -f1-3 < /proc/loadavg)"
echo "# of CPU cores: $(cat /proc/cpuinfo | grep processor | wc -l)"

if diff -q $RESULTS/$PIGLIT_RESULTS.baseline $RESULTS/$PIGLIT_RESULTS; then
    exit 0
fi

UNEXPECTED_RESULTSFILE=$RESULTS/unexpected.txt
diff --unified=0 $RESULTS/$PIGLIT_RESULTS.baseline $RESULTS/$PIGLIT_RESULTS > $UNEXPECTED_RESULTSFILE | true

# allow the driver to list some known flakes that won't intermittently
# fail people's pipelines (while still allowing them to run and be
# reported to IRC in the usual flake detection path).  If we had some
# fails listed (so this wasn't a total runner failure), then filter out
# the known flakes and see if there are any issues left.
if [ -n "$PIGLIT_FLAKES" ]; then
    set +x
    # process the diff file with the following steps
    # - remove all lines starting with '@@'
    sed -i '/^@@/d' $UNEXPECTED_RESULTSFILE
    # - remove all lines starting with '---'
    sed -i '/^---/d' $UNEXPECTED_RESULTSFILE
    # - remove all lines starting with '+++'
    sed -i '/^+++/d' $UNEXPECTED_RESULTSFILE
    # - remove all lines containing 'pass:'
    sed -i '/pass:/d' $UNEXPECTED_RESULTSFILE
    # - remove all lines containing 'fail:'
    sed -i '/fail:/d' $UNEXPECTED_RESULTSFILE

    cat $UNEXPECTED_RESULTSFILE

    # - remove all lines containing a flake line
    while read line; do
        line=`echo $line | sed 's|#.*||g'`
        if [ -n "$line" ]; then
            ESCAPED=$(printf '%s\n' "$line" | sed -e 's/[]\/$*.^[]/\\&/g');
            sed -i "/$ESCAPED/d" $UNEXPECTED_RESULTSFILE
        fi
    done < $INSTALL/$PIGLIT_FLAKES
    set -x

    if [ ! -s $UNEXPECTED_RESULTSFILE ]; then
        exit 0
    fi
fi

`pwd`/piglit/piglit summary html --exclude-details=pass $RESULTS/summary $RESULTS/$PIGLIT_PROFILES

echo Unexpected change in results:
cat $UNEXPECTED_RESULTSFILE
exit 1
