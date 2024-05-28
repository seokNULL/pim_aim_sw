#!/bin/bash

function setup_dev() {
	total_dev="$(ls /sys/bus/pci/drivers/qdma-pf/ | grep 0000)"
	total_dev_cnt="$(ls /sys/bus/pci/drivers/qdma-pf/ | grep 0000 | wc -l)"
	for i in $(seq 1 ${total_dev_cnt});
	do
		pf_add="$(ls /sys/bus/pci/drivers/qdma-pf/ | grep 0000 | sed -n ${i}p)"
		echo "$pf_add"
		pci_bus=${pf_add:5:2}
		pci_device=${pf_add:8:2}
		pci_func=${pf_add:11:1}
		echo "sudo chmod 666 /sys/bus/pci/devices/0000\:${pci_bus}\:${pci_device}.${pci_func}/qdma/qmax"
		sudo chmod 666 /sys/bus/pci/devices/0000\:${pci_bus}\:${pci_device}.${pci_func}/qdma/qmax
		echo "sudo echo 256 > /sys/bus/pci/devices/0000\:${pci_bus}\:${pci_device}.${pci_func}/qdma/qmax"
		sudo echo 256 > /sys/bus/pci/devices/0000\:${pci_bus}\:${pci_device}.${pci_func}/qdma/qmax
	done
}

QDMA_NAME=bin/qdma-pf.ko
NEWTON_DEV_PATH=/dev/newton1
NEWTON_MOD_NAME=newton1
killall newton_se
if [ -c "$NEWTON_DEV_PATH" ] ; then
    echo "${NEWTON_MOD_NAME}: rmmod ${NEWTON_MOD_NAME}"
    sudo rmmod -f ${NEWTON_MOD_NAME}
fi
make clean
make
echo "rmmod qdma-pf"
sudo rmmod qdma-pf
echo "insmod ${QDMA_NAME}"
sudo insmod ${QDMA_NAME}

setup_dev
