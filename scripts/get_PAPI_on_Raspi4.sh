#!/bin/bash

WORKING_DIR=$PWD
PAPI_ROOT=${PAPI_ROOT:-$PWD}
if [ 0 -lt $# ]
then
    PAPI_ROOT=$1
fi

echo "PAPI_ROOT=$PAPI_ROOT"
echo "(export PAPI_ROOT or pass a path to the script to overwrite this)"

git clone https://bitbucket.org/icl/papi.git $PAPI_ROOT
cd $PAPI_ROOT
git checkout papi-6-0-0-1-t
git config user.name BamboWu
git config user.email qw2699@utexas.edu
git am $WORKING_DIR/../src/papi.ca72.patch
cd $PAPI_ROOT/src
./configure
make -j
