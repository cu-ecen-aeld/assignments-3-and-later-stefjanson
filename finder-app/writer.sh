#!/bin/bash

readonly writefile=$1
readonly writestr=$2
readonly filedir=$(dirname ${writefile})

if [[ $# -ne 2 ]]; then
	exit 1
fi

if [[ ! -d ${filedir} ]]; then
    mkdir -p ${filedir}
fi

echo "${writestr}" > ${writefile} || { echo "File could not be created"; exit 1; }