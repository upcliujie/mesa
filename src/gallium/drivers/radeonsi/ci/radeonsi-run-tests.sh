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



# piglit
if [[ $PIGLIT_PATH != "x" ]]; then
    if test -f "$prefix/${GPU_NAME}-piglit-quick.csv.gz"; then
        gzip -d -k "$prefix/${GPU_NAME}-piglit-quick.csv.gz" -c > $OUT/${GPU_NAME}-piglit-quick.csv
        BASELINE="--baseline $OUT/${GPU_NAME}-piglit-quick.csv"
    else
        BASELINE=""
    fi
    echo -e "\nRunning piglit tests [$BASELINE]"
    echo "============================================================"
    piglit-runner run --piglit-folder $PIGLIT_PATH \
                      --profile quick \
                      --output $OUT/piglit \
                      --process-isolation \
                      --skips $prefix/skips.csv $BASELINE 2>&1 | tee -a $logfile
fi

DEQP_ARGS="--deqp-surface-width=256 --deqp-surface-height=256 --deqp-gl-config-name=rgba8888d24s8ms0 --deqp-visibility=hidden"

if [[ $GLCTS_PATH != "x" ]]; then
    # glcts
    if test -f "$prefix/${GPU_NAME}-glcts.csv.gz"; then
        gzip -d -k "$prefix/${GPU_NAME}-glcts.csv.gz" -c > $OUT/${GPU_NAME}-glcts.csv
        BASELINE="--baseline $OUT/${GPU_NAME}-glcts.csv"
    else
        BASELINE=""
    fi
    echo -e "\nRunning glcts tests [$BASELINE]"
    echo "============================================================"
    deqp-runner run --deqp $GLCTS_PATH/external/openglcts/modules/glcts \
                    --caselist $GLCTS_PATH/external/openglcts/modules/gl_cts/data/mustpass/gl/khronos_mustpass/4.6.1.x/gl46-master.txt \
                    --output $OUT/glcts \
                    --skips $prefix/skips.csv \
                    $BASELINE \
                    --timeout 1000 -- \
                    $DEQP_ARGS 2>&1 | tee -a $logfile
fi

# deqp
if [[ $DEQP_PATH != "x" ]]; then
    deqptests=("egl" "gles2" "gles3" "gles31")
    for subtest in ${deqptests[@]};
    do
        if test -f "$prefix/${GPU_NAME}-deqp-${subtest}.csv.gz"; then
            gzip -d -k "$prefix/${GPU_NAME}-deqp-${subtest}.csv.gz" -c > $OUT/${GPU_NAME}-deqp-${subtest}.csv
            BASELINE="--baseline $OUT/${GPU_NAME}-deqp-${subtest}.csv"
        else
            BASELINE=""
        fi
        echo -e "\nRunning $subtest tests [$BASELINE]"
        echo "============================================================"
        deqp-runner run --deqp $DEQP_PATH/modules/$subtest/deqp-$subtest \
                        --caselist $DEQP_PATH/android/cts/master/$subtest-master.txt \
                        --output $OUT/$subtest \
                        --skips $prefix/skips.csv \
                        $BASELINE \
                        --timeout 100 -- \
                        $DEQP_ARGS 2>&1 | tee -a $logfile
    done
fi
