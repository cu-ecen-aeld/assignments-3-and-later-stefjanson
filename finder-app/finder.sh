#!/bin/bash
# Search all files in directory for a string

readonly filesdir=$1
readonly searchstr=$2

if [[ $# -ne 2 ]] || [[ ! -d ${filesdir} ]]; then
    echo "./finder filesdir searchstr"
	exit 1
fi

X=$(grep --files-with-matches "${searchstr}" ${filesdir}/* | wc -l)
Y=$(grep "${searchstr}" ${filesdir}/* | wc -l)

echo "The number of files are ${X} and the number of matching lines are ${Y}"
