#!/bin/bash

sudo dmesg -C
rm dmesg_matmul.txt

echo "seq 1" >> dmesg_matmul.txt
sudo ./matmul 1 512 512 
dmesg >> dmesg_matmul.txt
echo "" >> dmesg_matmul.txt
sudo dmesg -C

echo "seq 4" >> dmesg_matmul.txt
sudo ./matmul 4 512 512 
dmesg >> dmesg_matmul.txt
echo "" >> dmesg_matmul.txt
sudo dmesg -C

echo "seq 16" >> dmesg_matmul.txt
sudo ./matmul 16 512 512 
dmesg >> dmesg_matmul.txt
echo "" >> dmesg_matmul.txt
sudo dmesg -C

echo "seq 64" >> dmesg_matmul.txt
sudo ./matmul 64 512 512 
dmesg >> dmesg_matmul.txt
echo "" >> dmesg_matmul.txt
sudo dmesg -C
