#!/bin/bash

thread_num=1
iteration=10
outputFile=$(pwd)/result.dat

# defaults
[ -z $XSTG_ROOT ] && export XSTG_ROOT=$(cd ../.. && pwd)
[ -z $OCR_INSTALL_ROOT ] && export OCR_INSTALL_ROOT=$XSTG_ROOT/ocr/ocr/install

# Which benchmarks to run?
BENCHMARKS="BinaryTree Hashtable Tempest"

if [ -z $1 ]; then
    echo "Please specify experiment name: [time|op-count]"
elif [ $1 = "time" ]; then
    if [ -e "$outputFile" ]; then
        rm $outputFile
    fi
    touch $outputFile

    echo "offset native" >> $outputFile
#origin version
    printf "\n\n--------------------Offset Based Relptr--------------------"
    for dir in $BENCHMARKS; do
        if [ -d "$dir" ] && [ $dir != "makefiles" ]; then
            printf "\n\n> Running test %s\n\n" $dir
            echo \# $dir offset >> $outputFile
            pushd $dir > /dev/null
            index=0
			make -f Makefile.x86 clean
            while [ $index -lt $iteration ]
            do
                CONFIG_NUM_THREADS=$thread_num NO_DEBUG=yes OCR_TYPE=x86 CFLAGS="-DMEASURE_TIME=1 -DNDEBUG=1" make -f Makefile.x86 run | awk '/elapsed time:/ { print $3 }' >> $outputFile
                index=$((index+1))
            done
            popd > /dev/null
        fi
    done

#native pointer version
    printf "\n\n--------------------Native Pointer Based Relptr--------------------"
    for dir in $BENCHMARKS; do
        if [ -d "$dir" ] && [ $dir != "makefiles" ]; then
            printf "\n\n> Running test %s\n\n" $dir
            echo \# $dir native >> $outputFile
            pushd $dir > /dev/null
            index=0
			make -f Makefile.x86 clean
            while [ $index -lt $iteration ]
            do
                CONFIG_NUM_THREADS=$thread_num NO_DEBUG=yes OCR_TYPE=x86 CFLAGS="-DOCXXR_USE_NATIVE_POINTERS=1 -DMEASURE_TIME=1 -DNDEBUG=1" make -f Makefile.x86 run | awk '/elapsed time:/ { print $3 }' >> $outputFile
                index=$((index+1))
            done
            popd > /dev/null
        fi
    done
    python draw.py
elif [ $1 = "op-count" ]; then
    if [ -e "$outputFile" ]; then
        rm $outputFile
    fi
    touch $outputFile
    iteration=1

#instrument version
    printf "\n\n--------------------Count Operation--------------------"
    for dir in $BENCHMARKS; do
        if [ -d "$dir" ] && [ $dir != "makefiles" ]; then
            printf "\n\n> Running test %s\n\n" $dir
            echo \# $dir>> $outputFile
            pushd $dir > /dev/null
            index=0
            while [ $index -lt $iteration ]
            do
                CONFIG_NUM_THREADS=$thread_num NO_DEBUG=yes OCR_TYPE=x86 CFLAGS="-DINSTRUMENT_POINTER_OP=1 -DNDEBUG=1" make -f Makefile.x86 clean run | awk '/rp|bp/'>> $outputFile
                index=$((index+1))
            done
            popd > /dev/null
        fi
    done

else
    echo "Please specify experiment name: [time|op-count]"
fi


