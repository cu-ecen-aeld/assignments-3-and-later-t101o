#!/bin/bash

if [ $# -lt 2 ]; then
    if [ -z $1 ]; then
	echo "filesdir was not specified"
	exit 1
    fi

    if [ -z $2 ]; then
	echo "searchstr was not specified"
	exit 1
    fi
fi

if [ ! -d $1 ]; then
    echo "filesdir is not a directory"
    exit 1
fi

FILE_COUNT=$(find $1 | wc -l)
FILE_COUNT=$(( $FILE_COUNT - 1 ))
SEARCH_COUNT=$(cd $1 && grep -R $2 | wc -l)

echo "The number of files are ${FILE_COUNT} and the number of matching lines are ${SEARCH_COUNT}"
