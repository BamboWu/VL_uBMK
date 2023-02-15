#!/bin/bash

WORKING_DIR=$PWD

wget https://github.com/DynamoRIO/dynamorio/releases/download/release_9.0.1/DynamoRIO-AArch64-Linux-9.0.1.tar.gz
tar xf DynamoRIO-AArch64-Linux-9.0.1.tar.gz
rm DynamoRIO-AArch64-Linux-9.0.1.tar.gz
mkdir DynamoRIO-AArch64-Linux-9.0.1/samples/build
cp ../../src/DynamoRIO/armq_trace.c DynamoRIO-AArch64-Linux-9.0.1/samples/
echo 'add_sample_client(armq_trace  "armq_trace.c;utils.c" "drmgr;drreg;drutil;drx")' >> DynamoRIO-AArch64-Linux-9.0.1/samples/CMakeLists.txt
cd DynamoRIO-AArch64-Linux-9.0.1/samples/build
cmake ../
make -j
