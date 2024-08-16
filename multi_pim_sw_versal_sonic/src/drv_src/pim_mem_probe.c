#include <linux/module.h>
#include <linux/if_arp.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kref.h>
#include <linux/smp.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>
#include <linux/kallsyms.h>
#include <linux/mmzone.h>
#include <asm/page.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/device.h>
#include <linux/ioport.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>
#ifdef MODVERSIONS
    #include <linux/modversions.h>
#endif
#include <asm/io.h>
#include <linux/pci.h>
#include <linux/async_tx.h>
#include <linux/async.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/highmem.h>
#include <linux/delay.h>
#include <linux/spinlock.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include "pim_mem_lib_kern.h"
#include "pim_mem_lib_user.h"
#include "dma_lib_kern.h"

#include "multi_dev_drv.h"
#include "multi_dev_lib_kern.h"
// #include "multi_dev_lib_user.h"

#ifdef __x86_64__
#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
#include <asm/pat.h> 
#endif
#include <asm/set_memory.h>
#endif //__x86_64__

// #define TEST_ONE_DEV

int totalDev;	
struct list_head dev_list;

static dev_t pim_dev;
static struct class *pim_class = NULL;
static struct device *pim_device = NULL;

static DEFINE_MUTEX(pim_mem_lock);

static int dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static int fops_open(struct inode *inode, struct file *filp)
{
    struct dev_node_t *node;
    node = container_of(inode->i_cdev, struct dev_node_t, pim_cdev);
    filp->private_data = node;
    // totalDev = node->_totalDev;
    // printk("DEVICE NAME: %s\n", node->pim_mem_name);
    // printk("TOTAL DEV NUM: %d\n", totalDev);
    return 0;
}
static int fops_release(struct inode *inode, struct file *filp)
{
    //mutex_unlock(&pim_mem_lock);    
    return 0;
}
static int fops_mmap(struct file *filp, struct vm_area_struct *vma)
{
    vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);    

    if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot))
       return -EAGAIN;
    
    return 0;
}
static long fops_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    struct dev_node_t *pim_node = (struct dev_node_t*)filp->private_data;
    // printk("PRIVATE DATA CHECK - FILE NAME: %s\n", pim_node->pim_mem_name);

    void *__user user_ptr;
    user_ptr = (void __user *) arg;
    //printk("PIM_MEM] ioctl cmd: %d ", cmd);
    switch (cmd)
    {
        case PIM_MEM_INFO:
        {
            pim_mem_info pim_info;
            if (copy_from_user(&pim_info, user_ptr, sizeof(pim_mem_info))) {
                return -EFAULT;
            }
            //printk("PIM_MEM size :%llu ", mem_len);
            pim_info.addr = pim_node->mem_start;
            pim_info.size = pim_node->mem_len;
#ifdef P2P
            pim_info.kaddr = pim_node->pim_mem_reg;
#endif
            if (copy_to_user(user_ptr, &pim_info, sizeof(pim_mem_info))) {
                return -EFAULT;
            }
            break;
        }
        case DEV_COUNT:
        {   
            int totalDev_out;
            if (copy_from_user(&totalDev_out, user_ptr, sizeof(int))) {
                return -EFAULT;
            }            
            totalDev_out = totalDev;
            if (copy_to_user(user_ptr, &totalDev_out, sizeof(int))) {
                return -EFAULT;
            }
            break;
        }
        default:
        {
            printk(KERN_ERR "Invalid IOCTL\n");
            break;
        }
    }
    return 0;
}
static struct file_operations pim_mem_fops = {
    .open = fops_open,
    .release = fops_release,
    .mmap = fops_mmap,
    .unlocked_ioctl   = fops_ioctl,        
};


int register_pim_mem(void) {
    struct list_head *ptr;
    struct dev_node_t *node = NULL;

    pim_class = class_create(THIS_MODULE, PIM_MEM_NAME);
    if (!pim_class) {
        printk("%s: calss_create() failed", __func__);
        goto out_unalloc_region;
    }   
    printk("class creation done... char device registering...\n");

    list_for_each(ptr, &dev_list){
        node = list_entry(ptr, struct dev_node_t, list);
        snprintf(node->pim_mem_name, sizeof(node->pim_mem_name), "%s%d", PIM_MEM_NAME_PREFIX, node->fpga_id);

        if (alloc_chrdev_region(&pim_dev, 0, 1, node->pim_mem_name) < 0) {
            printk("%s: could not allocate major number for mmap\n", __func__);
            goto out_vfree;
        }   
        pim_class->dev_uevent = dev_uevent;
        pim_device = device_create(pim_class, NULL, pim_dev, NULL, node->pim_mem_name); // device name
        if (!pim_device) {
            printk("%s: device_create() failed", __func__);
            goto out_unalloc_region;
        }   
        cdev_init(&node->pim_cdev, &pim_mem_fops);
        if (cdev_add(&node->pim_cdev, pim_dev, 1) < 0) {
            printk("%s: could not allocate chrdev for mmap\n", __func__);
            goto out_unalloc_region;
        }   
        node->pim_dev = pim_dev;
        node->pim_class = pim_class;
        node->pim_device = pim_device;
    }    
    return 0;


  out_unalloc_region:
    unregister_chrdev_region(pim_dev, 1); 
  out_vfree:
    return -1;

}

#ifdef __x86_64__

#ifdef TEST_ONE_DEV
static int pciem_find_device_recur(struct pci_bus *pci_bus)
{
    struct pci_bus *child_bus;
    struct pci_dev *dev;
    list_for_each_entry(dev, &pci_bus->devices, bus_list) {
        /* 0x10ee is vendor ID for Xilinx */
        if(dev->vendor==0x10ee){
            dev_insert(dev, totalDev);
            totalDev++;
            printk("Device found: #%d\n", totalDev);
            printk("Test: #%d\n", totalDev);
            return -1;
        }
    }
    list_for_each_entry(child_bus, &pci_bus->children, node) {
        if (pciem_find_device_recur(child_bus) < 0) return -1;
    }
    return 0;
}
#else
static struct pci_dev * pciem_find_device_recur(struct pci_bus *pci_bus)
{
    struct pci_bus *child_bus;
    struct pci_dev *dev;
    list_for_each_entry(dev, &pci_bus->devices, bus_list) {
        /* 0x10ee is vendor ID for Xilinx */
        if(dev->vendor==0x10ee){
            dev_insert(dev, totalDev);
            totalDev++;
            printk("Device found: #%d\n", totalDev);
            printk("Test: #%d\n", totalDev);
        }
    }
    list_for_each_entry(child_bus, &pci_bus->children, node) {
        pciem_find_device_recur(child_bus);
    }
    return NULL;
}
#endif
static struct pci_dev * pciem_find_device(void)     // need to optimize referencing XDMA driver code
{
    totalDev = 0;
    // struct pci_dev *pci_root0 = pci_get_domain_bus_and_slot(0, 14, 0); //
    // struct pci_dev *pci_root0 = pci_get_domain_bus_and_slot(0, 15, 0); //
    struct pci_dev *pci_root0 = pci_get_domain_bus_and_slot(0, 0, 0); //
    struct pci_bus *pci_bus0 = pci_root0->bus;
    pciem_find_device_recur(pci_bus0);
    // struct pci_dev *pci_root0 = pci_get_domain_bus_and_slot(0, 41, 0); //29
    // struct pci_bus *pci_bus0 = pci_root0->bus;
    // pciem_find_device_recur(pci_bus0);

    // struct pci_dev *pci_root1 = pci_get_domain_bus_and_slot(0, 60, 0); //3c
    // struct pci_bus *pci_bus1 = pci_root1->bus;
    // pciem_find_device_recur(pci_bus1);

    // struct pci_dev *pci_root2 = pci_get_domain_bus_and_slot(0, 170, 0); //aa
    // struct pci_bus *pci_bus2 = pci_root2->bus;
    // pciem_find_device_recur(pci_bus2);

    // struct pci_dev *pci_root3 = pci_get_domain_bus_and_slot(0, 188, 0); //BC
    // struct pci_bus *pci_bus3 = pci_root3->bus;
    // pciem_find_device_recur(pci_bus3);

    return NULL;
}

int probe_pim_mem_x86(void)
{
    // list initialization
    init_list();
    pciem_find_device();
    struct list_head *ptr;
    struct dev_node_t *node = NULL;
    resource_size_t mem_start;
    resource_size_t mem_len;
    // volatile uint32_t *pim_mem_reg;
    resource_size_t pim_mem_reg;
    
    list_for_each(ptr, &dev_list){
        int err;
        node = list_entry(ptr, struct dev_node_t, list); 
        struct pci_dev *pciem_dev = NULL;
        pciem_dev = node->pdev;

        printk("RECOGNIZED dev %s\n", dev_name(&pciem_dev->dev));

        if (pciem_dev == NULL) {
            printk(KERN_ERR "PIM_MEM] Failed to find PL DMA");
            return -1;
        }
        err = pcim_enable_device(pciem_dev);
        if (err) {
            printk(KERN_ERR "PIM_MEM] Failed to enable PL DMA");
            return -1;
        }

        /* 
        BAR0 : FPGA DRAM
        BAR1 : DMA control register
        BAR2 : Out port for AXI_PCIE
        BAR3 : AXI_PCIE control register
        */
        printk("---------------------------");
        printk("PIM_MEM_%d] PIM DMA/PCIe control driver", node->fpga_id);
        
        printk("PIM_MEM_%d] BAR #%d - DRAM    : 0x%llx - 0x%llx (LEN: %llx) \n", node->fpga_id, MEMORY_NUM, pci_resource_start(pciem_dev, MEMORY_NUM) + RESERVED,  pci_resource_end(pciem_dev, MEMORY_NUM), pci_resource_len(pciem_dev, MEMORY_NUM) - RESERVED);
        printk("PIM_MEM_%d] BAR #%d - DMA     : 0x%llx - 0x%llx (LEN: %llx) \n", node->fpga_id, DMA_CONF_NUM, pci_resource_start(pciem_dev, DMA_CONF_NUM),  pci_resource_end(pciem_dev, DMA_CONF_NUM), pci_resource_len(pciem_dev, DMA_CONF_NUM));
        printk("PIM_MEM_%d] BAR #%d - PCIE_OUT: 0x%llx - 0x%llx (LEN: %llx) \n", node->fpga_id, PCI_CONF_NUM, pci_resource_start(pciem_dev, PCI_CONF_NUM),  pci_resource_end(pciem_dev, PCI_CONF_NUM), pci_resource_len(pciem_dev, PCI_CONF_NUM));

        
        
        //printk("PIM_MEM] BAR #%d - PCIE_CTL: %llx - %llx (LEN: %llx) \n", 3, pci_resource_start(pciem_dev, 3),  pci_resource_end(pciem_dev, 3), pci_resource_len(pciem_dev, 3));
        //printk("BAR #%d - DESC.   : %llx - %llx \n", 4, pciem_dev->resource[4].start, pci_resource_len(pciem_dev, 4) + pciem_dev->resource[4].start - 1);

        /* PCIe DMA setting */
        pci_set_master(pciem_dev);
        err = pci_set_dma_mask(pciem_dev, DMA_BIT_MASK(64));
        if (err) {
            printk(KERN_ERR "Error set dma mask ");
            err = pci_set_dma_mask(pciem_dev, DMA_BIT_MASK(32));
        }

        err = pci_set_consistent_dma_mask(pciem_dev, DMA_BIT_MASK(64));
        if (err) {
            printk(KERN_ERR "Error set consistent dma mask ");
            err = pci_set_consistent_dma_mask(pciem_dev, DMA_BIT_MASK(32));
        }
        if(pcie_relaxed_ordering_enabled(pciem_dev)){
        printk("PIM_MEM] Relaxed Ordering enable\n");
        // Clear Relaxed Ordering bit
        pcie_capability_clear_word(pciem_dev, PCI_EXP_DEVCTL, PCI_EXP_DEVCTL_RELAX_EN);
        printk("PIM_MEM] Complete relaxed Ordering disable\n");    
        }
        else{
        printk("PIM_MEM] Relaxed Ordering disable\n");
        }
        /* FPGA BAR */
        mem_start = pci_resource_start(pciem_dev, MEMORY_NUM) + RESERVED;
        mem_len = pci_resource_len(pciem_dev, MEMORY_NUM) - RESERVED;
        // pim_mem_reg = ioremap_nocache(mem_start, mem_len);
        pim_mem_reg = ioremap_cache(mem_start, mem_len);

        printk("PIM_MEM_PROBE - pim_mem_reg: %p\n", pim_mem_reg);
        printk("PIM_MEM_PROBE - pim_mem_reg: %llx\n", pim_mem_reg);
        printk("PIM_MEM_PROBE - virt_to_bus pim_mem_reg: %llx\n", virt_to_bus(pim_mem_reg));

        if (!pim_mem_reg) {
            // printk(KERN_ERR "PIM_MEM] ioremap error (%llx, %llx)", mem_start, mem_len);
            printk(KERN_ERR "PIM_MEM] pci iomap error (%llx, %llx)", mem_start, mem_len);
            return -1;
        }
        // else{
        //     printk("pci iomap success\n");
        //     // pci_iounmap(pciem_dev, pim_mem_reg);
        //     // return -1;
        // }
        set_memory_uc((unsigned long)pim_mem_reg, (mem_len/PAGE_SIZE));

        node->mem_start = mem_start;
        node->mem_len = mem_len;
        node->pim_mem_reg = pim_mem_reg;
    }

    return 0;
}

#elif defined __aarch64__
int probe_pim_mem_arm(struct platform_device *pdev)
{
    struct device_node *np; 
    struct resource res;
    int ret;
    struct device *dev = &pdev->dev;

    np = of_parse_phandle(dev->of_node, PIM_MEM_PLATFORM, 0);
    if (!np) {
        printk(KERN_ERR "No %s specified \n", PIM_MEM_PLATFORM);
        goto out;
    }
    ret = of_address_to_resource(np, 0, &res);
    if (ret) {
        printk(KERN_ERR "No memory address assigned to the region");
        goto out;
    }
    /* FPGA BAR */
    mem_start = res.start;
    mem_len = resource_size(&res);

    printk("PIM_MEM] PIM memory address: 0x%llx - 0x%llx (0x%llx)", mem_start, res.end, res.start);
    printk("PIM_MEM]            length : 0x%llx", mem_len);

    pim_mem_reg=ioremap(mem_start, mem_len);
    if (!pim_mem_reg) {
        printk(KERN_ERR "PIM_MEM] ioremap error (%llx, %llx)", mem_start, mem_len);
        return -1;
    }
    return 0;
out:
    return -EFAULT; 
}
#endif // __aarch64__


void remove_pim_mem(void)
{
#ifdef __x86_64__
    destroy_mem_list();
    if (pim_class) {
        class_destroy(pim_class);
    }
#else
    if (pim_class) {
        device_destroy(pim_class, pim_dev);
        class_destroy(pim_class);
        cdev_del(&pim_cdev);
        unregister_chrdev(MAJOR(pim_dev), PIM_MEM_NAME);
        unregister_chrdev_region(pim_dev, 1);
    }
    if (pim_mem_reg) {
        iounmap(pim_mem_reg);
    }
#endif
    printk("PIM_MEM] Removed PIM-MEM driver\n");
}

MODULE_DESCRIPTION("PIM memory driver");
MODULE_AUTHOR("KU-Leewj");
MODULE_LICENSE("GPL");