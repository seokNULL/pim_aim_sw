#!/bin/bash

cd ../xdma/
sudo make clean; sudo make install
# sudo make clean; sudo make install DEBUG=1
cd ../tools/
sudo make clean; sudo make
cd ../tests
sudo ./load_driver.sh 0

# sudo rmmod ../xdma/xdma.ko
# sudo insmod ../xdma/xdma.ko interrupt_mode=0