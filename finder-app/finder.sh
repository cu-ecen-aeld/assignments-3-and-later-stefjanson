#!/bin/sh
# Search all files in directory for a string

readonly filesdir=$1
readonly searchstr=$2

if [[ $# -ne 2 ]] || [[ ! -d ${filesdir} ]]; then
    echo "finder filesdir searchstr"
	exit 1
fi

countfiles=$(grep -l "${searchstr}" ${filesdir}/* | wc -l)
countmatches=$(grep "${searchstr}" ${filesdir}/* | wc -l)

echo "The number of files are ${countfiles} and the number of matching lines are ${countmatches}" > /tmp/assignment4-result.txt
