#ifndef _MULTI_DEV_DRV_
#define _MULTI_DEV_DRV_

#include <linux/list.h>
#include <linux/pci.h>
#include <linux/cdev.h>

#include "multi_dev_lib_kern.h"


extern void init_list(void);
extern void dev_insert(struct pci_dev* dev, int fpga_id);
extern struct pci_dev* find_dev(int fpga_id);
extern void destroy_mem_list(void);
extern void destroy_dma_list(void);

#endif