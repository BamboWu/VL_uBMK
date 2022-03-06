#!/bin/bash

proc_name=$(basename $1)

rm -f /tmp/$proc_name.{stdout,stderr,VmPeak}
$@ > /tmp/$proc_name.stdout 2>/tmp/$proc_name.stderr &
PID=$!
while [ -f /proc/$PID/status ]
do
    grep ^VmPeak /proc/$PID/status >> /tmp/$proc_name.VmPeak 2>/dev/null
done

tick=$(awk '/ticks/{ print $1 }' /tmp/$proc_name.stdout)
vmpeak=$(tail -n 2 /tmp/$proc_name.VmPeak | head -n 1 | awk '{ print $2 }')
echo "$tick,$vmpeak"
