#!/bin/bash

rm result_gemm.txt
echo "seq 1" >> result_gemm.txt
sudo ./gemm 1 512 512 >> result_gemm.txt
echo "" >> result_gemm.txt
echo "seq 4" >> result_gemm.txt
sudo ./gemm 4 512 512 >> result_gemm.txt
echo "" >> result_gemm.txt
echo "seq 16" >> result_gemm.txt
sudo ./gemm 16 512 512 >> result_gemm.txt
echo "" >> result_gemm.txt
echo "seq 64" >> result_gemm.txt
sudo ./gemm 64 512 512 >> result_gemm.txt
echo "" >> result_gemm.txt