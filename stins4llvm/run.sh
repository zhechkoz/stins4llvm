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

clang-3.9 -c -emit-llvm ${c} -o "$build$cfile.bc"
exitIfFail $?

opt-3.9 -load "${build}libStateProtectorPass.so" -stateProtect <"${build}${cfile}.bc"> "${build}${cfile}-inst.bc"
exitIfFail $?

llc-3.9 -filetype=obj "${build}${cfile}-inst.bc"
exitIfFail $?

g++ -rdynamic "${build}${cfile}-inst.o" "${build}libcheck.o" -o "${build}${cfile}-rewritten"
exitIfFail $?

