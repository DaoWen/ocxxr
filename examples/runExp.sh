#!/bin/bash

thread_num=1

#initialize env
export XSTG_ROOT=$(cd ../.. && pwd)
export OCR_INSTALL_ROOT=$XSTG_ROOT/ocr/ocr/install

#origin version
printf "\n\n--------------------Offset Based Relptr--------------------"
for dir in *; do
    if [ -d "$dir" ] && [ $dir != "makefiles" ]; then
		printf "\n\n> Running test %s\n\n" $dir
	    pushd $dir > /dev/null
		CONFIG_THREAD_NUM=$thread_num NO_DEBUG=yes OCR_TYPE=x86 CFLAGS="-DMEASURE_TIME=1 -DNDEBUG=1" make -f Makefile.x86 clean run | awk '/elased time:/'
		popd > /dev/null
	fi
done

#native pointer version
printf "\n\n--------------------Native Pointer Based Relptr--------------------"
for dir in *; do
    if [ -d "$dir" ] && [ $dir != "makefiles" ]; then
		printf "\n\n> Running test %s\n\n" $dir
	    pushd $dir > /dev/null
		CONFIG_THREAD_NUM=$thread_num NO_DEBUG=yes OCR_TYPE=x86 CFLAGS="-DOCXXR_USE_NATIVE_POINTERS=1 -DMEASURE_TIME=1 -DNDEBUG=1" make -f Makefile.x86 clean run | awk '/elased time:/'
		popd > /dev/null
	fi
done

