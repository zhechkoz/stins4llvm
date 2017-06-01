#!/bin/bash

build="build/"
filename=""
c="" 
cfile=""

function usage() {
    echo "usage: run [-f file ]"
    echo "	-f file containing new line separated functions to protect"
    echo "	-c source file to protect"
}

function exitIfFail() {
	if [ $1 != 0 ]; then
	exit $1
fi
}

if [ "$1" == "" ] || [ $# != 4 ]; then
	usage
	exit 1
fi

while [ "$1" != "" ]; do
    case $1 in
        -f | --file )           shift
                                filename=$1
                                ;;
        -c | --cfile )          shift
                                c=$1
                                ;;
        -h | --help )           usage
                                exit
                                ;;
        * )                     usage
                                exit 1
    esac
    shift
done

# Parce c file
arrC=(${c//// })
cfile=${arrC[${#arrC[@]}-1]}
arrC=(${cfile//./ })
cfile=${arrC[0]}

clang -c -emit-llvm ${c} -o "$build$cfile.bc"
exitIfFail $?

LD_PRELOAD=/usr/lib/x86_64-linux-gnu/libssl.so \
opt -load "${build}libStateProtectorPass.so" -callpath -ff $filename <"${build}${cfile}.bc"> "${build}${cfile}-inst.bc"
exitIfFail $?

llc -filetype=obj "${build}${cfile}-inst.bc"
exitIfFail $?

g++ -rdynamic "${build}${cfile}-inst.o" "${build}libcheck.o" "${build}crypto.o" -o "${build}${cfile}-rewritten" -L/usr/lib/x86_64-linux-gnu/ -lssl -lcrypto
exitIfFail $?

