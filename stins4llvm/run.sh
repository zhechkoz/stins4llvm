#!/bin/bash

# Tools
OPT=opt-3.9
CLANG=clang-3.9
LINK=llvm-link-3.9
LLC=llc-3.9
CXX=g++

# Common folders
build="build/"

# Pass
rcPass="${build}libStateProtectorPass.so"

# Pass' libraries and files
rcLibrary="${build}libcheck.o"

# Configurations
config=""
resultFile=""

function usage {
    echo "usage: run.sh [-f file ]"
    echo "  -f file containing configuration"
}

function exitIfFail {
    if [ $1 != 0 ]; then
        exit $1
    fi
}

# https://stackoverflow.com/questions/1527049/join-elements-of-an-array
function joinBy { 
    local IFS="$1"; shift; echo "$*"; 
}

function RC {
    ${OPT} -load ${rcPass} \
        "${build}${resultFile}.bc" -stateProtect -ff $config -o "${build}${resultFile}-inst.bc" 
    exitIfFail $?
} 

function getBCFiles {
    local bcFiles
    
    for element in "$@"
    do  
        file=$(basename "$element")
        bcFiles+="${file%.*}.bc "
    done
    
    echo "$bcFiles"
}

# Check if enough arguments supplied to program
if [ $# != 2 ]; then
    usage
    exit 1
fi

# Parce input arguments
while [ "$1" != "" ]; do
    case $1 in
        -f | --file )           shift
                                config=$1
                                ;;
        -h | --help )           usage
                                exit
                                ;;
        * )                     usage
                                exit 1
    esac
    shift
done

# Parse json to get inputs
inputCFiles=($(jq -r '.program[]' $config))
exitIfFail $?
inputCFiles=$(joinBy ' ' "${inputCFiles[@]}")

inputBCFiles=$(getBCFiles $inputCFiles)

resultFile=$(jq -r '.binary' $config)
exitIfFail $?

clangFlags=$(jq -r 'select(.clangFlags != null) .clangFlags' $config) 
exitIfFail $?

# Generate bc files
${CLANG} -c -emit-llvm ${inputCFiles} $clangFlags -O0
exitIfFail $?

${LINK} $inputBCFiles -o "${build}${resultFile}.bc"
exitIfFail $?
rm $inputBCFiles 2> /dev/null

# Protect
RC  

# Generate object
${LLC} -filetype=obj "${build}${resultFile}-inst.bc"
exitIfFail $?

# Link
${CXX} -rdynamic "${build}${resultFile}-inst.o" ${rcLibrary} -o ${build}${resultFile}
exitIfFail $?
