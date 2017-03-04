#!/bin/bash

set -e

# defaults
[ -z $XSTG_ROOT ] && export XSTG_ROOT=$(cd ../.. && pwd)
[ -z $OCR_INSTALL_ROOT ] && export OCR_INSTALL_ROOT=$XSTG_ROOT/ocr/ocr/install

run_benchmarks() {
    # arguments
    local label=$1
    local flags=$2
    local awk_cmd=$3
    local iterations=${4}
    # constants
    local thread_num=1
    # go!
    echo "$label" >> $OUTPUT_FILE
    for dir in $BENCHMARKS; do
        if [ -d "$dir" ] && [ $dir != "makefiles" ]; then
            printf "\n\n> Running test %s\n\n" $dir
            echo "# $dir $label" >> $OUTPUT_FILE
            pushd $dir > /dev/null
            make -f Makefile.x86 clean
            for ((index = 0; index < iterations; index++)); do
                CONFIG_NUM_THREADS=$thread_num NO_DEBUG=yes OCR_TYPE=x86 CFLAGS="-DNDEBUG=1 $flags" \
                    make -f Makefile.x86 run | awk "$awk_cmd" >> $OUTPUT_FILE
            done
            popd > /dev/null
        fi
    done
}

export EXPERIMENT_TYPE="$1"

# Which benchmarks to run?
if [ -z "$2" ]; then
    export BENCHMARKS="BinaryTree Hashtable Tempest Lulesh"
else
    export BENCHMARKS="$2"
fi
for d in $BENCHMARKS; do
    if ! [ -d $d ]; then
        echo "Experiment directory not found: $d"
        exit 1
    fi
done

export OUTPUT_FILE=$PWD/result.dat

if [ "$EXPERIMENT_TYPE" = "time" ]; then
    rm -f $OUTPUT_FILE
    echo "offset native" >> $OUTPUT_FILE
    BASE_FLAGS="-DMEASURE_TIME=1"
    ITERS=10
    AWK_CMD='/elapsed time:/ { print $3 }'
    # original version
    printf "\n\n--------------------Position Independent Pointers--------------------\n"
    run_benchmarks "position independent" "$BASE_FLAGS" "$AWK_CMD" $ITERS
    # native pointer version
    printf "\n\n--------------------Native Pointers--------------------\n"
    run_benchmarks "native" "-DOCXXR_USE_NATIVE_POINTERS=1 $BASE_FLAGS" "$AWK_CMD" $ITERS
    # plot
    python draw.py
elif [ "$EXPERIMENT_TYPE" = "op-count" ]; then
    rm -f $OUTPUT_FILE
    BASE_FLAGS="-DINSTRUMENT_POINTER_OP=1"
    ITERS=1
    AWK_CMD='/^rp|^bp/'
    # instrumented version
    printf "\n\n--------------------Count Operations--------------------\n"
    run_benchmarks "op count" "$BASE_FLAGS" "$AWK_CMD" $ITERS
else
    echo "Please specify experiment name: [time|op-count]"
fi


