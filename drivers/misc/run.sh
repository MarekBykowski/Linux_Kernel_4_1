#!/bin/bash

dmesg -C
MOD=axxia_smccall
if lsmod | grep -q $MOD; then
    : #/sbin/rmmod $MOD.ko
else
    /sbin/insmod $MOD.ko
fi

#echo N > /sys/module/$MOD/parameters/all_cpus
#cat /proc/axxia_smccall

echo Y > /sys/module/$MOD/parameters/all_cpus
cat /proc/axxia_smccall

