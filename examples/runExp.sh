#!/bin/bash

set -e


# defaults
[ -z $XSTG_ROOT ] && export XSTG_ROOT=$(cd ../.. && pwd)
[ -z $OCR_INSTALL_ROOT ] && export OCR_INSTALL_ROOT=$XSTG_ROOT/ocr/ocr/install
[ -z $OCXXR_LIB_ROOT ] && export OCXXR_LIB_ROOT=$XSTG_ROOT/ocxxr
run_benchmarks() {
    # arguments
    local label=$1
    local flags=$2
    local awk_cmd=$3
    local iterations=${4}
    # constants
    local thread_num=1
    # go!
    #echo "$label" >> $OUTPUT_FILE
    export CONFIG_NUM_THREADS=$thread_num
    export OCR_TYPE=x86
    export V=1 # verbose OCR make
    export NO_DEBUG=yes
    export CFLAGS="-O3 -DNDEBUG=1 $flags"
    export CC=clang
    export CXX=clang++
    for dir in $BENCHMARKS; do
        if [ -d "$dir" ] && [ $dir != "makefiles" ]; then
            printf "\n\n> Running test %s\n\n" $dir
            echo "# $dir $label" | tee -a $LOG_FILE >> $OUTPUT_FILE
            pushd $dir > /dev/null
            make -f Makefile.x86 clean install | tee -a $LOG_FILE
            for ((index = 0; index < iterations; index++)); do
                make -f Makefile.x86 run | tee -a $LOG_FILE | awk "$awk_cmd" >> $OUTPUT_FILE
                printf "."
            done
            printf "\n"
            popd > /dev/null
        fi
    done
}

export EXPERIMENT_TYPE="$1"

# Which benchmarks to run?
if [ -z "$2" ]; then
   # export BENCHMARKS="BinaryTree Hashtable Tempest Lulesh UTS"
    export BENCHMARKS="BinaryTree Hashtable Tempest UTS"
else
    export BENCHMARKS="$2"
fi
for d in $BENCHMARKS; do
    if ! [ -d $d ]; then
        echo "Experiment directory not found: $d"
        exit 1
    fi
done

export NOW=$(date +"%F-%T")
export OUTPUT_FILE="$PWD/result-$NOW.dat"
export LOG_FILE=$PWD/result-$NOW.log
export GRAPH_FILE=$(echo `expr "$OUTPUT_FILE" : '\(.\+\.\)'`pdf)
export GRAPH_LINK=$PWD/latest.pdf
export DATA_LINK=$PWD/latest.dat
export LOG_LINK=$PWD/latest.log

if [ "$EXPERIMENT_TYPE" = "time" ]; then
    rm -f $OUTPUT_FILE $LOG_FILE
    echo "native position_independent sanity_check" >> $OUTPUT_FILE
    BASE_FLAGS="-DMEASURE_TIME=1"
    ITERS=10
    AWK_CMD='/elapsed time:/ { print $3 }'
    # native pointer version
    printf "\n\n--------------------Native Pointers--------------------\n" | tee -a $LOG_FILE
    run_benchmarks "native" "-DOCXXR_USE_NATIVE_POINTERS=1 $BASE_FLAGS" "$AWK_CMD" $ITERS
    # original version
    printf "\n\n--------------------Position Independent Pointers--------------------\n" | tee -a $LOG_FILE
    run_benchmarks "position_independent" "$BASE_FLAGS" "$AWK_CMD" $ITERS
    # sanity check version
    printf "\n\n--------------------Sanity Check--------------------\n" | tee -a $LOG_FILE
    run_benchmarks "sanity_check" "-DSANITY_CHECK=1 $BASE_FLAGS" "$AWK_CMD" $ITERS
    # plot
    python draw.py $OUTPUT_FILE
    #soft link
    rm -f $GRAPH_LINK
    rm -f $DATA_LINK
    rm -f $LOG_LINK
    ln -s $GRAPH_FILE $GRAPH_LINK
    ln -s $OUTPUT_FILE $DATA_LINK
    ln -s $LOG_FILE $LOG_LINK
elif [ "$EXPERIMENT_TYPE" = "op-count" ]; then
    rm -f $OUTPUT_FILE
    BASE_FLAGS="-DINSTRUMENT_POINTER_OP=1"
    ITERS=1
    AWK_CMD='/^rp|^bp/'
    # instrumented version
    printf "\n\n--------------------Count Operations--------------------\n"
    run_benchmarks "op count" "$BASE_FLAGS" "$AWK_CMD" $ITERS
    #plot
    python draw2.py $OUTPUT_FILE
    #soft link
    rm -f $GRAPH_LINK
    rm -f $DATA_LINK
    rm -f $LOG_LINK
    ln -s $GRAPH_FILE $GRAPH_LINK
    ln -s $OUTPUT_FILE $DATA_LINK
    ln -s $LOG_FILE $LOG_LINK
elif [ "$EXPERIMENT_TYPE" = "bt-variants" ]; then
    rm -f $OUTPUT_FILE
    BENCHMARKS="BinaryTree"
    BASE_FLAGS="-DMEASURE_TIME=1"
    ITERS=10
    AWK_CMD='/elapsed time:/ { print $3 }'
    echo "native relative based based_db" >> $OUTPUT_FILE
    # native pointer version
    printf "\n\n--------------------Native Pointers--------------------\n"
    run_benchmarks "native" "-DBT_ARG_PTR_TYPE=RelPtr -DOCXXR_USE_NATIVE_POINTERS=1 $BASE_FLAGS" "$AWK_CMD" $ITERS
    # relative pointer version
    printf "\n\n--------------------Relative Pointers--------------------\n"
    run_benchmarks "relative" "$BASE_FLAGS" "$AWK_CMD" $ITERS
    # based pointer version
    printf "\n\n--------------------Based Pointers--------------------\n"
    run_benchmarks "based" "-DBT_PTR_TYPE=BasedPtr $BASE_FLAGS" "$AWK_CMD" $ITERS
    # based db pointer version
    printf "\n\n--------------------Based Db Pointers--------------------\n"
    run_benchmarks "based_db" "-DBT_PTR_TYPE=BasedDbPtr $BASE_FLAGS" "$AWK_CMD" $ITERS
    #plot
    python draw3.py $OUTPUT_FILE
    #soft link
    rm -f $GRAPH_LINK
    rm -f $DATA_LINK
    rm -f $LOG_LINK
    ln -s $GRAPH_FILE $GRAPH_LINK
    ln -s $OUTPUT_FILE $DATA_LINK
    ln -s $LOG_FILE $LOG_LINK
else
    echo "Please specify experiment name: [time|op-count|bt-variants]"
fi


