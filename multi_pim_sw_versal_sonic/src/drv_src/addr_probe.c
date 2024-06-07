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
#include "addr_lib_user.h"

#ifdef __x86_64__
#if KERNEL_VERSION(5, 15, 0) > LINUX_VERSION_CODE
#include <asm/pat.h> 
#endif
#include <asm/set_memory.h>
#endif //__x86_64__

static dev_t addr_dev;
static struct cdev addr_cdev;
static struct class *addr_class = NULL;
static struct device *addr_device = NULL;

// static DEFINE_MUTEX(pim_mem_lock);

static int dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
    add_uevent_var(env, "DEVMODE=%#o", 0666);
    return 0;
}

static u64 _va_to_pa(u64 vaddr)
{
    pgd_t *pgd;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *pte;
    unsigned long paddr = 0;
    unsigned long page_addr = 0;
    unsigned long page_offset = 0;
    struct mm_struct *mm = current->mm;
    struct vm_area_struct *vma_t = find_vma(current->mm, vaddr);

    /* Page table walk */    
    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd)) {
        printk("not mapped in pgd %llx\n", vaddr);
        return -1;
    }
    pud = pud_offset((p4d_t *)pgd, vaddr);
    if (pud_none(*pud)) {
        printk("not mapped in pud %llx\n", vaddr);
        return -1;
    }
    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd)) {
        printk("not mapped in pmd %llx\n", vaddr);
        pmd = pmd_offset(pud, vaddr);
        if (pmd_none(*pmd)){
            printk("Error-Not mapped in pmd %llx\n", vaddr);
            return -1;
        }
    }
    pte = pte_offset_kernel(pmd, vaddr);
    // For result mmap region (No touched region)
    if (pte_none(*pte)) {
        printk("Error-Not mapped in PTE (%lx) %llx\n",vma_t->vm_flags, vaddr);
        return -1;
    }
    /* Page frame physical address mechanism | offset */
    page_addr = pte_val(*pte) & PAGE_MASK;
    page_offset = vaddr & ~PAGE_MASK;
    paddr = page_addr | page_offset;
    // printk("va_to_pa: %llx\n", paddr);
    return paddr;
}

static int addr_open(struct inode *inode, struct file *filp)
{
    return 0;
}
static int addr_release(struct inode *inode, struct file *filp)
{
    //mutex_unlock(&pim_mem_lock);    
    return 0;
}
static int addr_mmap(struct file *filp, struct vm_area_struct *vma)
{
    // vma->vm_page_prot = pgprot_noncached(vma->vm_page_prot);    

    // if (io_remap_pfn_range(vma, vma->vm_start, vma->vm_pgoff, vma->vm_end - vma->vm_start, vma->vm_page_prot))
    //    return -EAGAIN;
    return 0;
}
static long addr_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{

    void *__user user_ptr;
    user_ptr = (void __user *) arg;
    pim_args addr_info;
    switch (cmd)
    {
        case ADDR_TRANS:
        {
            if (copy_from_user(&addr_info, user_ptr, sizeof(pim_args))) {
                return -EFAULT;
            }
            //printk("PIM_MEM size :%llu ", mem_len);
            uint32_t pa = _va_to_pa(addr_info.va) - addr_info.dram_base;
            addr_info.pa = pa;

            if (copy_to_user(user_ptr, &addr_info, sizeof(pim_args))) {
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
static struct file_operations addr_fops = {
    .open             = addr_open,
    .release          = addr_release,
    .mmap             = addr_mmap,
    .unlocked_ioctl   = addr_ioctl,        
};


int register_addr_drv(void) {
    if (alloc_chrdev_region(&addr_dev, 0, 1, PIM_ADDR_NAME) < 0) {
        printk("%s: could not allocate major number for mmap\n", __func__);
        goto out_vfree;
    }   
    addr_class = class_create(THIS_MODULE, PIM_ADDR_NAME);
    if (!addr_class) {
        printk("%s: calss_create() failed", __func__);
        goto out_unalloc_region;
    }   
    addr_class->dev_uevent = dev_uevent;
    addr_device = device_create(addr_class, NULL, addr_dev, NULL, PIM_ADDR_NAME); // device name
    if (!addr_device) {
        printk("%s: device_create() failed", __func__);
        goto out_unalloc_region;
    }   
    cdev_init(&addr_cdev, &addr_fops);
    if (cdev_add(&addr_cdev, addr_dev, 1) < 0) {
        printk("%s: could not allocate chrdev for mmap\n", __func__);
        goto out_unalloc_region;
    }   
    return 0;

  out_unalloc_region:
    unregister_chrdev_region(addr_dev, 1); 
  out_vfree:
    return -1;

}

void remove_pim_addr(void)
{
    if (addr_class) {
        device_destroy(addr_class, addr_dev);
        class_destroy(addr_class);
        cdev_del(&addr_cdev);
        unregister_chrdev(MAJOR(addr_dev), PIM_ADDR_NAME);
        unregister_chrdev_region(addr_dev, 1);
    }
    printk("PIM_MEM] Removed PIM-ADDR driver\n");
}

MODULE_DESCRIPTION("PIM address translation driver");
MODULE_AUTHOR("KU-COMMIT");
MODULE_LICENSE("GPL");
