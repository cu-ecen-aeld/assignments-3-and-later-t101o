#!/bin/bash

if [ $# -lt 2 ]; then
    if [ -z $1 ]; then
	echo "writefile was not specified"
	exit 1
    fi

    if [ -z $2 ]; then
	echo "writestr was not specified"
	exit 1
    fi
fi

DIRECTORY=$(dirname $1)

if [ ! -d $1 ]; then
    mkdir -p $DIRECTORY || (echo "directory could not be created" && exit 1)
fi

touch $1 || (echo "file could not be created" && exit 1)

echo $2 > $1 || (echo "could not write to file" && exit 1)
