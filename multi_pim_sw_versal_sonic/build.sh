#!/bin/bash

if [ $# -eq 0 ]; then
	echo "command usage: ./insmod.sh MODE"
	echo ""	
	echo "			   MODE=0 -> Only insmod"
	echo "			   MODE=1 -> Only build"
	echo "			   MODE=2 -> Only make install"
	echo "			   MODE=3 -> Build and insmod"
	echo "			   MODE=4 -> Clean build and insmod and make install PIM module only"
	echo "			   MODE=5 -> Clean build and insmod and make install PIM/XDMA modules"
	exit 0
else
	if [[ "$1" -eq 0 ]]; then
		sudo rmmod src/drv_src/pim_drv.ko
		sudo insmod src/drv_src/pim_drv.ko
	elif [[ "$1" -eq 1 ]]; then
		make all
	elif [[ "$1" -eq 2 ]]; then
		sudo make install
	elif [[ "$1" -eq 3 ]]; then
		sudo dmesg -C
		make clean;
		make && sudo rmmod src/drv_src/pim_drv.ko
		sudo insmod src/drv_src/pim_drv.ko 		
	elif [[ "$1" -eq 4 ]]; then
		make clean; make && sudo make install
		sudo rmmod src/drv_src/pim_drv.ko
		sudo insmod src/drv_src/pim_drv.ko 		
	elif [[ "$1" -eq 5 ]]; then
		make clean; make && sudo make install
		sudo rmmod src/drv_src/pim_drv.ko
		sudo rmmod src/xdma_src/xdma/xdma.ko
		make -C src/xdma_src/xdma clean
		make -C src/xdma_src/tools clean
		make -C src/xdma_src/xdma
		make -C src/xdma_src/tools
		sudo insmod src/xdma_src/xdma/xdma.ko interrupt_mode=0
		sudo insmod src/drv_src/pim_drv.ko 	
	else
		echo "command usage: ./insmod.sh MODE"
		echo ""
		echo "			   MODE=0 -> Only insmod"
		echo "			   MODE=1 -> Only build PIM Driver and MEM"
		echo "			   MODE=2 -> Only make install"
		echo "			   MODE=3 -> Build and insmod"
		echo "			   MODE=4 -> Clean build and insmod and make install PIM module only"
		echo "			   MODE=5 -> Clean build and insmod and make install PIM/XDMA modules"
		exit 0		
	fi
fi

# Check to see if the xdma devices were recognized
echo ""
# cat /proc/devices | grep xdma > /dev/null
# returnVal=$?
# echo "returnVal: $returnVal"
# if [ $returnVal == 0 ]; then
# 	# Installed devices were recognized.
# 	echo "The Kernel module installed correctly and the xdma devices were recognized."
# else
# 	# No devices were installed.
# 	echo "Error: The Kernel module installed correctly, but no devices were recognized."
# 	echo " FAILED"
# 	exit 1
# fi

echo "DONE"