#!/bin/bash

rm result_elewise.txt

echo "Add" >> result_elewise.txt
echo "256x256" >> result_elewise.txt
sudo ./elewise 256 256 0 >> result_elewise.txt
echo "" >> result_elewise.txt
echo "512x512" >> result_elewise.txt
sudo ./elewise 512 512 0 >> result_elewise.txt
echo "" >> result_elewise.txt
echo "1024x1024" >> result_elewise.txt
sudo ./elewise 1024 1024 0 >> result_elewise.txt
echo "" >> result_elewise.txt
echo "2048x2048" >> result_elewise.txt
sudo ./elewise 2048 2048 0 >> result_elewise.txt
echo "" >> result_elewise.txt

# echo "Mul" >> result_elewise.txt
# echo "256x256" >> result_elewise.txt
# sudo ./elewise 256 256 1 >> result_elewise.txt
# echo "" >> result_elewise.txt
# echo "512x512" >> result_elewise.txt
# sudo ./elewise 512 512 1 >> result_elewise.txt
# echo "" >> result_elewise.txt
# echo "1024x1024" >> result_elewise.txt
# sudo ./elewise 1024 1024 1 >> result_elewise.txt
# echo "" >> result_elewise.txt
# echo "2048x2048" >> result_elewise.txt
# sudo ./elewise 2048 2048 1 >> result_elewise.txt
# echo "" >> result_elewise.txt

# echo "Sub" >> result_elewise.txt
# echo "256x256" >> result_elewise.txt
# sudo ./elewise 256 256 2 >> result_elewise.txt
# echo "" >> result_elewise.txt
# echo "512x512" >> result_elewise.txt
# sudo ./elewise 512 512 2 >> result_elewise.txt
# echo "" >> result_elewise.txt
# echo "1024x1024" >> result_elewise.txt
# sudo ./elewise 1024 1024 2 >> result_elewise.txt
# echo "" >> result_elewise.txt
# echo "2048x2048" >> result_elewise.txt
# sudo ./elewise 2048 2048 2 >> result_elewise.txt
# echo "" >> result_elewise.txt