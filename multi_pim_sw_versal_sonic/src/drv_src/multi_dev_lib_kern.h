#ifndef _MULTI_DEV_KERN_LIB_
#define _MULTI_DEV_KERN_LIB_

#include "../../include/pim.h"
// #include "pim_mem_lib_user.h"
#include "dma_lib_kern.h"

struct dev_node_t {
    struct list_head list;
    struct pci_dev *pdev;
    int fpga_id;
    dev_t pim_dev;
    struct cdev pim_cdev;
    struct class *pim_class;
    struct device *pim_device;
    char pim_mem_name[10];

    struct cdma_dev_t *cdma_dev;
    dev_t pl_dev_t;
    struct cdev pl_cdev;
    struct class *pl_class;
    struct device *pl_device;
    char pl_drv_name[10];

    resource_size_t mem_start;
    resource_size_t mem_len;
    uint32_t *pim_mem_reg;

    int _totalDev;
}; 

extern int totalDev;
extern struct list_head dev_list; // head of dev_list

#endif