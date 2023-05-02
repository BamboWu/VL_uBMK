#!/bin/bash

for i in $(seq 0 $(echo "$(nproc) - 1" | bc));
do
    cpufreq-set -c ${i} -g $1
done
