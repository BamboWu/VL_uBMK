#!/bin/bash

ARCH=`uname -m`
if [ "aarch64" == $ARCH ]
then
    gcc -o trypmu ../src/pmuenable/trypmu.c
    ./trypmu
    TRYPMU=$?
    rm -f trypmu
    exit $TRYPMU
else
    exit 1
fi
