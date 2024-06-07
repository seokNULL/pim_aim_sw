#!/bin/bash

MB=524288 #1MB/2 -> short size(2Byte) will be multiplied
KB=512    #1KB/2 -> short size(2Byte) will be multiplied
SIZE=$((${1}*$KB))
ITER=$(seq 0 9)
CMD=$(seq 0 5)

for j in $CMD
do
for i in $ITER
do
    sudo ./memcpy 1 ${SIZE} $j
done
done

