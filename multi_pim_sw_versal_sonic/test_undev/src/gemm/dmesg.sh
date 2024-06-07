#!/bin/bash

sudo dmesg -C
rm dmesg_gemm.txt

echo "seq 1" >> dmesg_gemm.txt
sudo ./gemm 1 512 512 
dmesg >> dmesg_gemm.txt
echo "" >> dmesg_gemm.txt
sudo dmesg -C

echo "seq 4" >> dmesg_gemm.txt
sudo ./gemm 4 512 512 
dmesg >> dmesg_gemm.txt
echo "" >> dmesg_gemm.txt
sudo dmesg -C

echo "seq 16" >> dmesg_gemm.txt
sudo ./gemm 16 512 512 
dmesg >> dmesg_gemm.txt
echo "" >> dmesg_gemm.txt
sudo dmesg -C

echo "seq 64" >> dmesg_gemm.txt
sudo ./gemm 64 512 512 
dmesg >> dmesg_gemm.txt
echo "" >> dmesg_gemm.txt
sudo dmesg -C
