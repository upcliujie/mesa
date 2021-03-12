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

# piglit
if [[ $PIGLIT_PATH != "x" ]]; then
    if test -f "$prefix/piglit-radeonsi-quick-expected-${GPU_NAME}.csv.gz"; then
        gzip -d -k "$prefix/piglit-radeonsi-quick-expected-${GPU_NAME}.csv.gz" -c > $OUT/piglit-radeonsi-quick-expected-${GPU_NAME}.csv
        BASELINE="--baseline $OUT/piglit-radeonsi-quick-expected-${GPU_NAME}.csv"
    else
        BASELINE=""
    fi
    piglit-runner run --piglit-folder $PIGLIT_PATH \
                      --profile quick \
                      --output $OUT/piglit \
                      --skips $prefix/piglit-deqp-radeonsi-skips.csv $BASELINE
fi

DEQP_ARGS="--deqp-surface-width=256 --deqp-surface-height=256 --deqp-gl-config-name=rgba8888d24s8ms0 --deqp-visibility=hidden"

if [[ $GLCTS_PATH != "x" ]]; then
    # glcts
    if test -f "$prefix/glcts-radeonsi-${GPU_NAME}.csv.gz"; then
        gzip -d -k "$prefix/glcts-radeonsi-${GPU_NAME}.csv.gz" -c > $OUT/glcts-radeonsi-${GPU_NAME}.csv
        BASELINE="--baseline $OUT/glcts-radeonsi-${GPU_NAME}.csv"
    else
        BASELINE=""
    fi
    deqp-runner run --deqp $GLCTS_PATH/external/openglcts/modules/glcts \
                    --caselist $GLCTS_PATH/external/openglcts/modules/gl_cts/data/mustpass/gl/khronos_mustpass/4.6.1.x/gl46-master.txt \
                    --output $OUT/glcts \
                    --skips $prefix/piglit-deqp-radeonsi-skips.csv \
                    $BASELINE \
                    --timeout 1000 -- \
                    $DEQP_ARGS
fi

# deqp
if [[ $DEQP_PATH != "x" ]]; then
    deqptests=("egl" "gles2" "gles3" "gles31")
    for subtest in ${deqptests[@]};
    do
        if test -f "$prefix/deqp-${subtest}-radeonsi-${GPU_NAME}.csv.gz"; then
            gzip -d -k "$prefix/deqp-${subtest}-radeonsi-${GPU_NAME}.csv.gz" -c > $OUT/deqp-${subtest}-radeonsi-${GPU_NAME}.csv
            BASELINE="--baseline $OUT/deqp-${subtest}-radeonsi-${GPU_NAME}.csv"
        else
            BASELINE=""
        fi
        deqp-runner run --deqp $DEQP_PATH/modules/$subtest/deqp-$subtest \
                        --caselist $DEQP_PATH/android/cts/master/$subtest-master.txt \
                        --output $OUT/$subtest \
                        --skips $prefix/piglit-deqp-radeonsi-skips.csv \
                        $BASELINE \
                        --timeout 100 -- \
                        $DEQP_ARGS
    done
fi
