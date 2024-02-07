#!/bin/sh
# Write string to a file

readonly writefile=$1
readonly writestr=$2
readonly filedir=$(dirname ${writefile})

if [[ $# -ne 2 ]]; then
    echo "./writer.sh writefile writestr"
	exit 1
fi

if [[ ! -d ${filedir} ]]; then
    mkdir -p "${filedir}"  || { echo "Directory could not be created"; exit 1; }
fi

echo "${writestr}" > "${writefile}" || { echo "File could not be created"; exit 1; }
