#include "multi_dev_drv.h"

void init_list(void){
    INIT_LIST_HEAD(&dev_list);
}

void dev_insert(struct pci_dev* dev, int fpga_id){
    struct dev_node_t *new_dev;
    new_dev = (struct dev_node_t*)kmalloc(sizeof(struct dev_node_t), GFP_KERNEL);
    new_dev->pdev = dev;
    new_dev->fpga_id = fpga_id;
    // dev_list.next->_totalDev = fpga_id + 1;
    
    list_add(&new_dev->list, &dev_list);

    printk("========= Device List Inserted =========");
    printk("Device id: %d\n", new_dev->fpga_id);
    printk("Device address: %s\n", dev_name(&new_dev->pdev->dev));
    printk("========================================");
}

struct pci_dev* find_dev(int num){
    struct list_head *ptr;
    struct dev_node_t *node = NULL;

    list_for_each(ptr, &dev_list){
        node = list_entry(ptr, struct dev_node_t, list); 
        if(node->fpga_id == num) return node->pdev;
    }
    printk(KERN_ERR "PIM_MEM] Failed to find FPGA device %d", num);
    return NULL;
}

void destroy_mem_list(void){
    struct list_head *ptr;
    struct dev_node_t *node = NULL;

    list_for_each(ptr, &dev_list){
        node = list_entry(ptr, struct dev_node_t, list); 

        pci_disable_device(node->pdev);
        if (node->pim_class) {
            device_destroy(node->pim_class, node->pim_dev);
            cdev_del(&node->pim_cdev);
            unregister_chrdev(MAJOR(node->pim_dev), node->pim_mem_name);
            unregister_chrdev_region(node->pim_dev, 1);
        }

        if (node->pim_mem_reg) {
            iounmap(node->pim_mem_reg);
            // pci_iounmap(node->pdev, node->pim_mem_reg);
        }
    }
}

void destroy_dma_list(void){
    struct list_head *ptr;
    struct dev_node_t *node = NULL;

    list_for_each(ptr, &dev_list){
        node = list_entry(ptr, struct dev_node_t, list);
        struct cdma_dev_t *_cdma_dev;
        _cdma_dev = node->cdma_dev;

#ifdef INTR_ENABLE
        free_irq(_cdma_dev->irq, _cdma_dev);
#endif
        if (_cdma_dev->dma_ctl_reg != NULL)
            iounmap(_cdma_dev->dma_ctl_reg);
        if (_cdma_dev->config_reg != NULL)
            iounmap(_cdma_dev->config_reg);
        if (_cdma_dev->desc_pim != NULL)
            iounmap(_cdma_dev->desc_pim);    
        if (_cdma_dev->config_mem != NULL)
            iounmap(_cdma_dev->config_mem);    
        if (_cdma_dev->pci_ctl_reg != NULL)
            iounmap(_cdma_dev->pci_ctl_reg);
        /* PL-DMA is commonly used in x86 and ARM */
        if (node->pl_class != NULL) {
            device_destroy(node->pl_class, node->pl_dev_t);
            cdev_del(&node->pl_cdev);
            unregister_chrdev(MAJOR(node->pl_dev_t), node->pl_drv_name);
            unregister_chrdev_region(node->pl_dev_t, 1);
        }
    }
}

// destroy_cdma_list