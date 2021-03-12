#!/bin/bash

if [[ $# != 5 ]]; then
    echo "Usage: $0 piglit_folder glcts_folder deqp_folder gpu_name output_folder"
    echo "(use 'x' as piglit/glcts/deqp folder to skip those tests)"
    exit 1
fi

prefix=`dirname $0`
PIGLIT_PATH=$1
GLCTS_PATH=$2
DEQP_PATH=$3
GPU_NAME=$4
OUT=$5

logfile=$OUT/radeonsi-run-tests.log

if test -d $OUT; then
    if test -d $logfile; then
        rm logfile
    fi
else
    mkdir -p $OUT
fi

if ! test -d $OUT/new_baseline; then
    mkdir -p $OUT/new_baseline
fi

BASELINE=""
FLAKES=""

set_baseline_flakes()
{
    if test -f "$prefix/${GPU_NAME}-$1-fail.csv"; then
        BASELINE="--baseline $prefix/${GPU_NAME}-$1-fail.csv"
    else
        BASELINE=""
    fi
    if test -f "$prefix/${GPU_NAME}-$1-flakes.csv"; then
        FLAKES="--flakes $prefix/${GPU_NAME}-$1-flakes.csv"
    else
        FLAKES=""
    fi
}

if [[ $PIGLIT_PATH != "x" ]]; then
    set_baseline_flakes "piglit-quick"

    echo -e "\nRunning piglit tests [$BASELINE]" | tee -a $logfile
    echo "============================================================" | tee -a $logfile
    piglit-runner run --piglit-folder $PIGLIT_PATH \
                      --profile quick \
                      --output $OUT/piglit \
                      --process-isolation \
                      --timeout 300 \
                      --skips $prefix/skips.csv $BASELINE $FLAKES 2>&1 | tee -a $logfile

    cp $OUT/piglit/failures.csv $OUT/new_baseline/${GPU_NAME}-piglit-quick-fail.csv
fi

DEQP_ARGS="--deqp-surface-width=256 --deqp-surface-height=256 --deqp-gl-config-name=rgba8888d24s8ms0 --deqp-visibility=hidden"

if [[ $GLCTS_PATH != "x" ]]; then
    set_baseline_flakes "glcts"

    echo -e "\nRunning glcts tests [$BASELINE]" | tee -a $logfile
    echo "============================================================" | tee -a $logfile
    deqp-runner run --deqp $GLCTS_PATH/external/openglcts/modules/glcts \
                    --caselist $GLCTS_PATH/external/openglcts/modules/gl_cts/data/mustpass/gl/khronos_mustpass/4.6.1.x/gl46-master.txt \
                    --output $OUT/glcts \
                    --skips $prefix/skips.csv \
                    $BASELINE $FLAKES \
                    --timeout 1000 -- \
                    $DEQP_ARGS 2>&1 | tee -a $logfile
    
    cp $OUT/glcts/failures.csv $OUT/new_baseline/${GPU_NAME}-glcts-fail.csv
fi

# deqp
if [[ $DEQP_PATH != "x" ]]; then
    deqptests=("egl" "gles2" "gles3" "gles31")
    for subtest in ${deqptests[@]};
    do
        set_baseline_flakes "deqp-${subtest}"

        echo -e "\nRunning $subtest tests [$BASELINE]" | tee -a $logfile
        echo "============================================================" | tee -a $logfile
        deqp-runner run --deqp $DEQP_PATH/modules/$subtest/deqp-$subtest \
                        --caselist $DEQP_PATH/android/cts/master/$subtest-master.txt \
                        --output $OUT/deqp-$subtest \
                        --skips $prefix/skips.csv \
                        $BASELINE $FLAKES \
                        --timeout 100 -- \
                        $DEQP_ARGS 2>&1 | tee -a $logfile
        
        cp $OUT/deqp-$subtest/failures.csv $OUT/new_baseline/${GPU_NAME}-deqp-$subtest-fail.csv
    done
fi
