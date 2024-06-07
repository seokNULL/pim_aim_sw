#!/bin/bash

rm result_matmul.txt
echo "seq 1" >> result_matmul.txt
sudo ./matmul 1 512 512 >> result_matmul.txt
echo "" >> result_matmul.txt
echo "seq 4" >> result_matmul.txt
sudo ./matmul 4 512 512 >> result_matmul.txt
echo "" >> result_matmul.txt
echo "seq 16" >> result_matmul.txt
sudo ./matmul 16 512 512 >> result_matmul.txt
echo "" >> result_matmul.txt
echo "seq 64" >> result_matmul.txt
sudo ./matmul 64 512 512 >> result_matmul.txt
echo "" >> result_matmul.txt