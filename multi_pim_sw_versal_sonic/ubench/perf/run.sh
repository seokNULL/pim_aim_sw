#!/bin/bash

ITER=$(seq 0 9)

for i in $ITER
do
    sudo ./perf 4 2048
done

for i in $ITER
do
    sudo ./perf 4 8192
done

for i in $ITER
do
    sudo ./perf 4 32768
done

for i in $ITER
do
    sudo ./perf 4 131072
done

for i in $ITER
do
    sudo ./perf 4 524288
done

