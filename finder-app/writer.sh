#!/bin/bash
# Write string to a supplied file

readonly writefile=$1
readonly writestr=$2
readonly filedir=$(dirname ${writefile})

if [[ $# -ne 2 ]]; then
    echo "./writer.sh writefile writestr"
	exit 1
fi

if [[ ! -d ${filedir} ]]; then
    mkdir -p ${filedir}
fi

echo "${writestr}" > ${writefile} || { echo "File could not be created"; exit 1; }
