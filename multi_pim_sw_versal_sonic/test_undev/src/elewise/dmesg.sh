#!/bin/bash

sudo dmesg -C
rm dmesg_add.txt

echo "Add" >> dmesg_add.txt
echo "256x256" >> dmesg_add.txt
sudo ./elewise 256 256 0 
dmesg >> dmesg_add.txt
echo "" >> dmesg_add.txt
sudo dmesg -C

echo "512x512" >> dmesg_add.txt
sudo ./elewise 512 512 0 
dmesg >> dmesg_add.txt
echo "" >> dmesg_add.txt
sudo dmesg -C

echo "512x512" >> dmesg_add.txt
sudo ./elewise 1024 1024 0 
dmesg >> dmesg_add.txt
echo "" >> dmesg_add.txt
sudo dmesg -C

echo "2048x2048" >> dmesg_add.txt
sudo ./elewise 2048 2048 0 
dmesg >> dmesg_add.txt
echo "" >> dmesg_add.txt
sudo dmesg -C

# rm dmesg_mul.txt

# echo "Mul" >> dmesg_mul.txt
# echo "256x256" >> dmesg_mul.txt
# sudo ./elewise 256 256 1 
# dmesg >> dmesg_mul.txt
# echo "" >> dmesg_mul.txt
# sudo dmesg -C

# echo "512x512" >> dmesg_mul.txt
# sudo ./elewise 512 512 1 
# dmesg >> dmesg_mul.txt
# echo "" >> dmesg_mul.txt
# sudo dmesg -C

# echo "512x512" >> dmesg_mul.txt
# sudo ./elewise 1024 1024 1 
# dmesg >> dmesg_mul.txt
# echo "" >> dmesg_mul.txt
# sudo dmesg -C

# echo "2048x2048" >> dmesg_mul.txt
# sudo ./elewise 2048 2048 1 
# dmesg >> dmesg_mul.txt
# echo "" >> dmesg_mul.txt
# sudo dmesg -C
