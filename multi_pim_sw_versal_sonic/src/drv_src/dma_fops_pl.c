#include <linux/module.h>
#include <linux/init.h>
#include <linux/mmzone.h>
//#include <asm/mmzone.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/if_arp.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/kref.h>
#include <linux/kallsyms.h>
#include <asm/pgtable.h>
//#include <asm/pat.h>
#include <asm/tlbflush.h>
#include <linux/mm.h>
#include <linux/types.h>
#include <linux/stat.h>
#include <linux/fcntl.h>
#include <asm/unistd.h>
#include <linux/kernel.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/unistd.h>
#include <linux/errno.h>
#include <asm/uaccess.h>
#include <linux/cdev.h>
#include <linux/mutex.h>

#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <linux/iopoll.h>

#include <linux/delay.h>
#include <asm/delay.h>
#include "../../include/pim.h"
#include "dma_lib_kern.h"
#include "multi_dev_lib_kern.h"
#include "multi_dev_lib_user.h"

// #include <asm/barrier.h>
// #include <asm/delay.h>

#ifdef PERF_TIME
struct timespec64 start_internal_tx, end_internal_tx;
uint64_t diff_internal_tx;
static uint64_t internal_tx_time = 0;

struct timespec64 start_dma_tx, end_dma_tx;
uint64_t diff_dma_tx;
static uint64_t tx_time = 0;

struct timespec64 start_dma_init, end_dma_init;
uint64_t diff_dma_init;
static uint64_t dma_init_time = 0;

struct timespec64 start_desc_copy, end_desc_copy;
uint64_t diff_desc_copy;
static uint64_t desc_copy_time = 0;
#endif

#ifdef x86_PIM_PERF
struct timespec64 start_pim, end_pim;
uint64_t diff_pim;
static uint64_t pim_time = 0;
#endif

static DEFINE_MUTEX(dma_lock);

#ifndef INTR_ENABLE
int __attribute__((optimize("O0"))) wait_sg(u32 desc_idx, struct cdma_dev_t *cdma_dev)
{
    int j=0;
    u32 status, errors;
    for (j = 0; j < CDMA_MAX_POLLING; j++) {
        status = dma_ctrl_read(CDMA_REG_SR, cdma_dev);
        if (status & CDMA_REG_SR_ALL_ERR_MASK) {
            printk(KERN_ERR " PL_DMA] ERROR DMA - SR: %x", status);
            printk(KERN_ERR " PL_DMA] ERROR DMA - OPCODE: %x", cdma_dev->opcode);
            errors = status & CDMA_REG_SR_ALL_ERR_MASK;
            dma_ctrl_write(CDMA_REG_SR, errors & CDMA_REG_SR_ERR_RECOVER_MASK, cdma_dev);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET, cdma_dev);
            return -EDMA_TX;
        }
        if ((dma_desc_read(desc_idx, CDMA_DESC_STATUS, cdma_dev) & CDMA_REG_DESC_TX_COMPL) == CDMA_REG_DESC_TX_COMPL){
            // get time end
            // #ifdef PERF_TIME
            // PERF_END(start_internal_tx, end_internal_tx, internal_tx_time);
            // #endif

            dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ, cdma_dev);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET, cdma_dev); //DMA RES, ET
            return SUCCESS;
        }
    }
    printk(KERN_ERR " PL_DMA] DMA polling count reaches zero !! \n");    
    return -EDMA_TX;
}

int __attribute__((optimize("O0"))) wait_simple(struct cdma_dev_t *cdma_dev)
{
    int j=0;
    u32 status, errors;
    for (j = 0; j < CDMA_MAX_POLLING; j++) {
        status = dma_ctrl_read(CDMA_REG_SR, cdma_dev);
        if (status & CDMA_REG_SR_ALL_ERR_MASK) {
            printk(KERN_ERR " PL_DMA] ERROR DMA - SR: %x", status);
            errors = status & CDMA_REG_SR_ALL_ERR_MASK;            
            dma_ctrl_write(CDMA_REG_SR, errors & CDMA_REG_SR_ERR_RECOVER_MASK, cdma_dev);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET, cdma_dev); //DMA RES, ET
            return -EDMA_TX;
        }        
        if ((status & CDMA_REG_SR_TX_COMPL) == CDMA_REG_SR_TX_COMPL){
            // get time end
            #ifdef x86_PIM_PERF
            x86_PERF_END(start_pim, end_pim, pim_time);
            #endif
            PL_DMA_LOG("Success single transaction");
            dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ, cdma_dev);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET, cdma_dev); //DMA RES, ET
            return SUCCESS;
        }
        // printk(KERN_ERR " PL_DMA] ERROR LOOP - SR: %x, %d, %d", status,j,CDMA_MAX_POLLING);
        // if(j%100000 == 0){
        //     printk(KERN_ERR " PL_DMA] ERROR DMA - SR: %x, %d, %d", status,j,CDMA_MAX_POLLING);
        // }
    }
    dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET, cdma_dev);  
    printk(KERN_ERR " PL_DMA] ERROR DMA - SR: %x", status);
    printk(KERN_ERR " PL_DMA] DMA polling count reaches zero !! \n");
    return -EDMA_TX;
}
#endif

#ifdef INTR_ENABLE
// static struct completion cdma_dev->compl;

irqreturn_t pl_dma_irq_handler(int irq, void *data)
{ 
    struct cdma_dev_t *cdma_dev = (struct cdma_dev_t*) data;
    u32 status, errors;
    // get time end
    #ifdef x86_PIM_PERF
    x86_PERF_END(start_pim, end_pim, pim_time);
    #endif
    PL_DMA_LOG("------------------------------------- \n");
    PL_DMA_LOG("Interrupt occured ! | SR: %x | CR: %x \n", dma_ctrl_read(CDMA_REG_SR, cdma_dev), dma_ctrl_read(CDMA_REG_CR, cdma_dev));
    status = dma_ctrl_read(CDMA_REG_SR, cdma_dev);
    if (status & CDMA_REG_SR_ERR_IRQ) {
        printk("ERR IRQ - SR: %x, OPCODE: %x", status, cdma_dev->opcode);
        if (status & CDMA_REG_SR_SG_DEC_ERR)
            printk(KERN_ERR " PL_DMA] DMA SG-Decoding error ");
        if (status & CDMA_REG_SR_SG_SLV_ERR)
            printk(KERN_ERR " PL_DMA] DMA SG-Slave error ");
        if (status & CDMA_REG_SR_DMA_DEC_ERR)
            printk(KERN_ERR " PL_DMA] DMA Decoding error ");
        if (status & CDMA_REG_SR_DMA_SLAVE_ERR)
            printk(KERN_ERR " PL_DMA] DMA Slave error ");
        if (status & CDMA_REG_SR_DMA_INT_ERR)
            printk(KERN_ERR " PL_DMA] DMA Internal error ");
        errors = status & CDMA_REG_SR_ALL_ERR_MASK;
        dma_ctrl_write(CDMA_REG_SR, errors & CDMA_REG_SR_ERR_RECOVER_MASK, cdma_dev);
        dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET, cdma_dev);
        cdma_dev->err = -EDMA_INTR;
        PL_DMA_LOG("------------------------------------- \n");
        goto done;
    } 
    if (status & CDMA_REG_SR_DLY_CNT_IRQ) {
        /*
         * Device takes too long to do the transfer when user requires
         * responsiveness.
         */
        printk(KERN_ERR " PL_DMA] DMA Inter-packet latency too long\n");
        cdma_dev->err = -EDMA_INTR;
        dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ | CDMA_REG_SR_DLY_CNT_IRQ | CDMA_REG_SR_ERR_IRQ, cdma_dev);
        PL_DMA_LOG("------------------------------------- \n");        
        goto done;
    }
    PL_DMA_LOG("IRQ NUM : %d", cdma_dev->irq);
    PL_DMA_LOG("COMPLETE - SR: %x, OPCODE: %x", status, cdma_dev->opcode);
    cdma_dev->err = 0;
    dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ | CDMA_REG_SR_DLY_CNT_IRQ | CDMA_REG_SR_ERR_IRQ, cdma_dev);
    dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET, cdma_dev); //DMA RES, ET
    PL_DMA_LOG("------------------------------------- \n");
    complete(&cdma_dev->compl);
done:
    return IRQ_HANDLED;
}
#endif


int dma_single_tx(u32 src_low, u32 src_high, u32 dst_low, u32 dst_high, u32 copy_len, struct cdma_dev_t *cdma_dev)
{
    int ret=0;
    PL_DMA_LOG("SRC_L: %x", src_low);
    PL_DMA_LOG("SRC_H: %x", src_high);
    PL_DMA_LOG("DST_L: %x", dst_low);
    PL_DMA_LOG("DST_H: %x", dst_high);
    PL_DMA_LOG("SIZE: %x", copy_len)
    dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ, cdma_dev);
    // PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR, cdma_dev));
    // PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR, cdma_dev));
    dma_ctrl_write(CDMA_REG_SA_L, src_low, cdma_dev);
    dma_ctrl_write(CDMA_REG_SA_H, src_high, cdma_dev);
    dma_ctrl_write(CDMA_REG_DA_L, dst_low, cdma_dev);
    dma_ctrl_write(CDMA_REG_DA_H, dst_high, cdma_dev);
    dma_ctrl_write(CDMA_REG_BYTETRANS, copy_len, cdma_dev);

#ifdef INTR_ENABLE
    dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_ALL_IRQ_MASK | CDMA_REG_CR_DELAY_MASK | (0x1 << CDMA_COALESCE_SHIFT), cdma_dev);
    // get time start -- when single tx starts?
    // #ifdef x86_PIM_PERF
    // x86_PERF_START(start_pim);
    // #endif

    PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR, cdma_dev));
    PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR, cdma_dev));
    init_completion(&cdma_dev->compl);
    ret = wait_for_completion_timeout(&cdma_dev->compl, msecs_to_jiffies(WAIT_INTR));
    if (ret == 0) {
        printk(KERN_ERR " PL_DMA] DMA Timeout !! \n");
        return -EDMA_TX;
    }
    if (cdma_dev->err < 0) {
        printk(" PL_DMA] DMA Error !! \n");
        return -EDMA_INTR;
    }
#else
    // get time start -- when single tx starts?
    // #ifdef x86_PIM_PERF
    // x86_PERF_START(start_pim);
    // #endif
    ret = wait_simple(cdma_dev);
    if (ret < 0)
        return -EDMA_TX;
#endif
    return SUCCESS;
}

int __attribute__((optimize("O0")))dma_sg_tx(u32 desc_idx, u32 last_desc, struct cdma_dev_t *cdma_dev)
{
    int ret;
#ifdef INTR_ENABLE
    int num_intr;
    if (desc_idx < CDMA_MAX_COALESCE) {
        num_intr = desc_idx << CDMA_COALESCE_SHIFT;
        dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_SG_EN | CDMA_REG_CR_ALL_IRQ_MASK | CDMA_REG_CR_DELAY_MASK | num_intr, cdma_dev);
        dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ, cdma_dev);
        PL_DMA_LOG("COALESCE: %x (%d) \n", num_intr, desc_idx);
        PL_DMA_LOG("1) :DESC_ADDR: (%llx - %x) \n", cdma_dev->desc_mem_base, last_desc-0x40);
        PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR, cdma_dev));
        PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR, cdma_dev));
        // pim_conf_write(CONF_OFFSET_PROF_START, 0x0, cdma_dev);   // uncomment because clr/pim_signal are already used for pim activation
        dma_ctrl_write(CDMA_CURDESC_PNTR_L, cdma_dev->desc_mem_base, cdma_dev);
        dma_ctrl_write(CDMA_CURDESC_PNTR_H, HIGH_ADDR, cdma_dev);
        wmb();
        dma_ctrl_write(CDMA_TAILDESC_PNTR_L, last_desc-0x40, cdma_dev);
        dma_ctrl_write(CDMA_TAILDESC_PNTR_H, HIGH_ADDR, cdma_dev);
        // get time start (need in else statement)
        // #ifdef x86_PIM_PERF
        // x86_PERF_START(start_pim);
        // #endif

        init_completion(&cdma_dev->compl);
        ret = wait_for_completion_timeout(&cdma_dev->compl, msecs_to_jiffies(WAIT_INTR));
        if (ret == 0) {
            printk(" PL_DMA] DMA Timeout (%d)!! \n", desc_idx);
            return -EDMA_TX;
        }
        if (cdma_dev->err < 0) {
            printk(KERN_ERR " PL_DMA] DMA Error !!\n");
            return -EDMA_INTR;
        }        
        // pim_conf_write(CONF_OFFSET_PROF_START, 0x0, cdma_dev); // uncomment because clr/pim_signal are already used for pim activation
    } else {
        int chunk_desc, remain_desc, k, offset;
        u32 start_desc, end_desc;
        chunk_desc = (desc_idx / CDMA_MAX_COALESCE);
        remain_desc = (desc_idx % CDMA_MAX_COALESCE);
        for (k = 0; k < chunk_desc; k++) {
            num_intr = CDMA_MAX_COALESCE << CDMA_COALESCE_SHIFT;
            offset = k * CDMA_MAX_COALESCE * 0x40;
            start_desc = cdma_dev->desc_mem_base + offset;
            end_desc = start_desc + ((CDMA_MAX_COALESCE - 1) * 0x40);
            PL_DMA_LOG("%d - %d) DESC_ADDR: (%x - %x) \n", k, chunk_desc, start_desc, end_desc);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_SG_EN | CDMA_REG_CR_ALL_IRQ_MASK | CDMA_REG_CR_DELAY_MASK | num_intr, cdma_dev);
            dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ, cdma_dev);
            PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR, cdma_dev));
            PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR, cdma_dev));
            // pim_conf_write(CONF_OFFSET_PROF_START, 0x0, cdma_dev);
            dma_ctrl_write(CDMA_CURDESC_PNTR_L, start_desc, cdma_dev);
            dma_ctrl_write(CDMA_CURDESC_PNTR_H, HIGH_ADDR, cdma_dev);
            wmb();
            dma_ctrl_write(CDMA_TAILDESC_PNTR_L, end_desc, cdma_dev);
            dma_ctrl_write(CDMA_TAILDESC_PNTR_H, HIGH_ADDR, cdma_dev);
            // get time start
            // #ifdef x86_PIM_PERF
            // x86_PERF_START(start_pim);
            // #endif

            init_completion(&cdma_dev->compl);
            ret = wait_for_completion_timeout(&cdma_dev->compl, msecs_to_jiffies(WAIT_INTR));
            if (ret == 0) {
                printk(KERN_ERR " PL_DMA] DMA Timeout !! (%d - %d)\n", k, chunk_desc);
                return -EDMA_TX;
            }
            if (cdma_dev->err < 0) {
                printk(KERN_ERR " PL_DMA] DMA Error !! (%d - %d)\n", k, chunk_desc);
                return -EDMA_INTR;
            }
            // pim_conf_write(CONF_OFFSET_PROF_START, 0x0, cdma_dev);
        }
        if (remain_desc) {
            num_intr = remain_desc << CDMA_COALESCE_SHIFT;
            start_desc = end_desc + 0x40;
            end_desc = last_desc-0x40;
            PL_DMA_LOG("%d) DESC_ADDR: (%x - %x) \n", remain_desc, start_desc, end_desc);
            dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_SG_EN | CDMA_REG_CR_ALL_IRQ_MASK | CDMA_REG_CR_DELAY_MASK | num_intr, cdma_dev);
            dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ, cdma_dev);
            PL_DMA_LOG("CR: %x \n", dma_ctrl_read(CDMA_REG_CR, cdma_dev));
            PL_DMA_LOG("SR: %x \n", dma_ctrl_read(CDMA_REG_SR, cdma_dev));
            // pim_conf_write(CONF_OFFSET_PROF_START, 0x0, cdma_dev);
            dma_ctrl_write(CDMA_CURDESC_PNTR_L, start_desc, cdma_dev);
            dma_ctrl_write(CDMA_CURDESC_PNTR_H, HIGH_ADDR, cdma_dev);
            wmb();
            dma_ctrl_write(CDMA_TAILDESC_PNTR_L, end_desc, cdma_dev);
            dma_ctrl_write(CDMA_TAILDESC_PNTR_H, HIGH_ADDR, cdma_dev);
            // get time start -- when single tx starts?
            // #ifdef x86_PIM_PERF
            // x86_PERF_START(start_pim);
            // #endif

            init_completion(&cdma_dev->compl);
            ret = wait_for_completion_timeout(&cdma_dev->compl, msecs_to_jiffies(WAIT_INTR));
            if (ret == 0) {
                printk(KERN_ERR " PL_DMA] DMA Timeout !! (%d - %d)\n", k, remain_desc);
                return -EDMA_TX;
            }
            if (cdma_dev->err < 0) {
                printk(KERN_ERR " PL_DMA] DMA Error !! (%d - %d)\n", k, chunk_desc);
                return -EDMA_INTR;
            }
            // pim_conf_write(CONF_OFFSET_PROF_START, 0x0, cdma_dev);
        }
    }
    // pim_conf_write(CONF_OFFSET_AIM_WORKING, 0x1, cdma_dev); // uncomment because clr/pim_signal are already used for pim activation
    return ret;
#else
    dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_SG_EN, cdma_dev);
    dma_ctrl_write(CDMA_REG_SR, CDMA_REG_SR_IOCIRQ, cdma_dev);
    dma_ctrl_write(CDMA_CURDESC_PNTR_L, cdma_dev->desc_mem_base, cdma_dev);
    dma_ctrl_write(CDMA_CURDESC_PNTR_H, HIGH_ADDR, cdma_dev);
    dma_ctrl_write(CDMA_TAILDESC_PNTR_L, last_desc-0x40, cdma_dev);
    dma_ctrl_write(CDMA_TAILDESC_PNTR_H, HIGH_ADDR, cdma_dev);
    wmb(); // RELEASE
    // #ifdef PERF_TIME
    // PERF_START(start_internal_tx);
    // #endif
    ret=wait_sg(desc_idx, cdma_dev);
    return ret;
#endif    
}
/* For file operations */
// static DEFINE_MUTEX(extio_mutex);
long pl_dma_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
    pim_args pim_args;
    void *__user arg_ptr;
    u64 src_pa, dst_pa;
    u32 val;
    int err;
    void *dmabuf;
    dma_addr_t dma_handle;
    uint32_t *clr_signal;
    uint32_t *pim_signal;

    struct dev_node_t *dev_node = (struct dev_node_t *)filp->private_data;
    struct cdma_dev_t *cdma_dev = dev_node->cdma_dev;

    // mutex_lock(&extio_mutex);
    clr_signal = (void *)cdma_dev->config_reg + CONF_OFFSET_HPC_CLR;
    pim_signal = (void *)cdma_dev->config_reg + CONF_OFFSET_AIM_WORKING;
    arg_ptr = (void __user *) arg;
    if (copy_from_user((void *)&pim_args, (void __user *)arg, sizeof(pim_args))) {
        printk(KERN_ERR " PL_DMA] Failed copy pim_args");
        return -EIOCTL;
    }
    
    cdma_dev->opcode = cmd;
    dma_ctrl_write(CDMA_REG_CR, CDMA_REG_CR_RESET, cdma_dev);
    switch (cmd)
    {
        case DMA_START:  /* Notify DMA start */
        {   
            #ifdef PERF_TIME
            dma_init_time    = 0;    
            desc_copy_time   = 0;     
            tx_time          = 0;
            // internal_tx_time = 0;
            PERF_START(start_dma_init);
            #endif
          
            /* Wait for DMA idle until loop count reaches zero or timeout */
            err = readl_poll_timeout((void *)(cdma_dev->dma_ctl_reg) + CDMA_REG_SR, val, (val & CDMA_REG_SR_IDLE), 0, CDMA_MAX_POLLING);
            if (err) {
                printk(KERN_ERR " PL_DMA] DMA in used \n");
                // mutex_unlock(&extio_mutex);
                return -EIOCTL;
            }

            #ifdef PERF_TIME
            PERF_END(start_dma_init, end_dma_init, dma_init_time);
            #endif


            #ifdef PERF_TIME
            PERF_START(start_desc_copy);
            #endif

            
            /* 
             * Copy PISA-DMA instructions from system memory to descriptor memory 
             * pisa structure size is 64B (= descriptor granularity)
             */
            if (copy_from_user((void *)cdma_dev->desc_pim, (void *__user)pim_args.desc_host, (pim_args.desc_idx + 1) * 0x40)) {
                printk(KERN_ERR " PL_DMA] ERROR copy_from_user descriptor");
                // mutex_unlock(&extio_mutex);
                return -EIOCTL;
            }
            #ifdef PERF_TIME
            PERF_END(start_desc_copy, end_desc_copy, desc_copy_time);
            #endif

            #ifdef PERF_TIME
            PERF_START(start_dma_tx);
            #endif

            clr_signal[0] = 0;
            pim_signal[0] = 0;
            /* Start DMA transaction */
            if (dma_sg_tx(pim_args.desc_idx, pim_args.desc_last, cdma_dev) < 0) {
                printk(KERN_ERR " PL_DMA] Error transactions");
                // mutex_unlock(&extio_mutex);
                return -EDMA_TX;
            }
            clr_signal[0] = 0;
            pim_signal[0] = 0;
            
            // #ifdef x86_PIM_PERF
            // printk("PROF] PIM EXEC: %llu ns\n", (long long unsigned int) pim_time);         
            // #endif

            #ifdef PERF_TIME
            PERF_END(start_dma_tx, end_dma_tx, tx_time);
            printk("PROF] DMA WAIT: %llu ns\n", (long long unsigned int) dma_init_time);            
            printk("PROF] DESC COPY: %llu ns\n", (long long unsigned int) desc_copy_time);            
            printk("PROF] DMA TX: %llu ns\n", (long long unsigned int) tx_time);            
            // printk("PROF] DMA TX (us): \t%llu\n", (long long unsigned int) tx_time/1000);
            // printk("PROF] internal DMA TX (us): \t%llu\n", (long long unsigned int) internal_tx_time/1000);
            #endif

            break;   
        }
        case DESC_MEM_INFO: /* Get descriptor memory information */
        {
            PL_DMA_LOG("Descriptor base addr: %llx", cdma_dev->desc_mem_base);
            pim_args.desc_base = cdma_dev->desc_mem_base;
            pim_args.dram_base = cdma_dev->dram_base;
            if (copy_to_user(arg_ptr, &pim_args, sizeof(pim_args))) {
                // mutex_unlock(&extio_mutex);
                return -EFAULT;
            }
            break;
        }
        case VA_TO_PA: /* Virtual to physical address translation for DMA transaction */
        {
            // pim_args.pa = va_to_pa(pim_args.va); // axi address starts from dram_base
            pim_args.pa = va_to_pa(pim_args.va) - cdma_dev->dram_base; // axi address starts from dram_base
            PL_DMA_LOG("DRAM BASE: %llx \n", cdma_dev->dram_base);
            PL_DMA_LOG("VA_TO_PA: %llx -> %llx \n", pim_args.va, pim_args.pa);
            if (copy_to_user(arg_ptr, &pim_args, sizeof(pim_args))) {
                // mutex_unlock(&extio_mutex);
                return -EFAULT;
            }
            break;            
        }
        case MEMCPY_PL2PL: /* Memory copy using DMA engine */
        {
            PL_DMA_LOG("Memory copy PL -> PL");
            src_pa = va_to_pa(pim_args.srcA_va) - cdma_dev->dram_base; // axi address starts from dram_base;
            dst_pa = va_to_pa(pim_args.dstC_va) - cdma_dev->dram_base; // axi address starts from dram_base;
            PL_DMA_LOG("src addr: %llx -> %llx ", pim_args.srcA_va, src_pa);
            PL_DMA_LOG("dst addr: %llx -> %llx ", pim_args.dstC_va, dst_pa);
            
            #ifdef PERF_TIME  
            tx_time          = 0;
            #endif
            
            #ifdef PERF_TIME
            PERF_START(start_dma_tx);
            #endif

            if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, HIGH_ADDR, pim_args.srcA_size, cdma_dev) < 0) {
                // mutex_unlock(&extio_mutex);
                return -EDMA_TX;
            }
            
            #ifdef PERF_TIME
            PERF_END(start_dma_tx, end_dma_tx, tx_time);
            // printk("PROF] DMA WAIT: %llu ns\n", (long long unsigned int) dma_init_time);            
            // printk("PROF] DESC COPY: %llu ns\n", (long long unsigned int) desc_copy_time);            
            printk("PROF] DMA TX: %llu ns %d bytes \n", (long long unsigned int) tx_time, pim_args.srcA_size);            
            #endif

            break;
        }
        case MEMCPY_PL2PS:
        {
            u32 copy_len, num_chunk, i;
            PL_DMA_LOG("Memory copy PL -> PS");
            copy_len = (pim_args.srcA_size > CPY_CHUNK_SIZE) ? CPY_CHUNK_SIZE : \
                                                      pim_args.srcA_size;
            num_chunk = (pim_args.srcA_size % CPY_CHUNK_SIZE) ? (pim_args.srcA_size / CPY_CHUNK_SIZE) + 1 : \
                                                      (pim_args.srcA_size / CPY_CHUNK_SIZE);
#ifdef __x86_64__
            dmabuf=dma_alloc_coherent(NULL, copy_len, &dma_handle, GFP_KERNEL);
#elif defined __aarch64__
            /* In the ARM platform, PS DMA buffer is used as dmabuf */
            dmabuf = ps_dma_t->kern_addr;
            dma_handle = ps_dma_t->bus_addr;
#endif
            if (dmabuf == NULL) {
                printk(KERN_ERR "DMA buffer is not allocated");
                // mutex_unlock(&extio_mutex);
                return -EFAULT;
            }
            PL_DMA_LOG("copy_len: %d \n", copy_len);
            PL_DMA_LOG("num_chunk: %d \n", num_chunk);
            for (i = 0; i < num_chunk; i++) {
                src_pa = va_to_pa(pim_args.srcA_va + (i * CPY_CHUNK_SIZE));
                dst_pa = dma_handle;
                // addr_hi = dst_pa >> 0x20;
                u64 test;
                test = 0x1234567812345678;
                printk("dst_pa: %llx\n", dst_pa);
                printk("test: %llx\n", test);
                // printk("dst_hi: %x\n", addr_hi);

#ifdef __x86_64__                
                /* pci control register not operated */
                // addr_hi = dst_pa >> 0x20;
                // addr_lo = dst_pa & 0xFFFFFFFF;
                // pci_ctrl_write(AXIBAR2PCIEBAR_0U, dst_pa >> 0x20);   
                // pci_ctrl_write(AXIBAR2PCIEBAR_0L, dst_pa & 0xFFFFFFFF);
                src_pa = src_pa & AXI_MASK;
                src_pa = src_pa + AXIBAR;
                // dst_pa = dst_pa & AXI_MASK;
                // dst_pa = dst_pa + AXIBAR;
                /* In x86 platform, high address of dst_pa is always 0 because dst_pa is 32 bits */
                // if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, HIGH_ADDR, copy_len) < 0)
                if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, 0x80, copy_len, cdma_dev) < 0) {
                    // mutex_unlock(&extio_mutex);
                    return -EDMA_TX;                
                }
#elif defined __aarch64__
                //if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, dst_pa>>0x20, copy_len) < 0)
                if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, dst_pa>>0x20, copy_len) < 0) {
                    // mutex_unlock(&extio_mutex);
                    return -EDMA_TX;
                }
#endif
                if (copy_to_user(pim_args.dstC_ptr + (i * CPY_CHUNK_SIZE), dmabuf, copy_len)) {
                    printk(KERN_ERR " PL_DMA] Error copy_to_user in PL2PS");
                    // mutex_unlock(&extio_mutex);
                    return -EFAULT;
                }
            }
#ifdef __x86_64__
            dma_free_coherent(NULL, pim_args.srcA_size, dmabuf, dma_handle);
#endif
            break;
        }
        case MEMCPY_PS2PL:
        {
            u32 copy_len, num_chunk, i;
            PL_DMA_LOG("Memory copy PS -> PL");
            copy_len = (pim_args.srcA_size > CPY_CHUNK_SIZE) ? CPY_CHUNK_SIZE : \
                                                      pim_args.srcA_size;
            num_chunk = (pim_args.srcA_size % CPY_CHUNK_SIZE) ? (pim_args.srcA_size / CPY_CHUNK_SIZE) + 1 : \
                                                      (pim_args.srcA_size / CPY_CHUNK_SIZE);

#ifdef __x86_64__
            dmabuf=dma_alloc_coherent(NULL, copy_len, &dma_handle, GFP_KERNEL);
#elif defined __aarch64__
            /* In the ARM platform, PS DMA buffer is used as dmabuf */
            dmabuf = ps_dma_t->kern_addr;
            dma_handle = ps_dma_t->bus_addr;
#endif
            if (dmabuf == NULL) {
                printk(KERN_ERR "DMA buffer is not allocated");
                // mutex_unlock(&extio_mutex);
                return -EFAULT;
            }
            PL_DMA_LOG("copy_len: %d \n", copy_len);
            PL_DMA_LOG("num_chunk: %d \n", num_chunk);

            for (i = 0; i < num_chunk; i++) {
                if (copy_from_user(dmabuf, pim_args.srcA_ptr + (i * CPY_CHUNK_SIZE), copy_len)) {
                    // mutex_unlock(&extio_mutex);
                    return -EFAULT;
                }
                src_pa = dma_handle;
                printk("src_pa: %llx\n", src_pa);
                dst_pa = va_to_pa(pim_args.dstC_va);
#ifdef __x86_64__
                /* pci control register not operated */
                //addr_hi = src_pa >> 0x20;
                //addr_lo = src_pa & 0xFFFFFFFF;
                // pci_ctrl_write(AXIBAR2PCIEBAR_0U, addr_hi);
                //pci_ctrl_write(AXIBAR2PCIEBAR_0L, addr_lo);
                // src_pa = src_pa & AXI_MASK; // TODO: AXI_MASK verification
                // src_pa = src_pa + AXIBAR;
                dst_pa = dst_pa & AXI_MASK;
                dst_pa = dst_pa + AXIBAR;
                /* In x86 platform, high address of src_pa is always 0 because src_pa is 32 bits */
                // if (dma_single_tx(src_pa, HIGH_ADDR, dst_pa, HIGH_ADDR, copy_len) < 0)
                if (dma_single_tx(src_pa, 0x80, dst_pa, HIGH_ADDR, copy_len, cdma_dev) < 0) {
                    // mutex_unlock(&extio_mutex);
                    return -EDMA_TX;
                }

#elif defined __aarch64__
                if (dma_single_tx(src_pa, src_pa>>0x20, dst_pa, HIGH_ADDR, copy_len) < 0) {
                    // mutex_unlock(&extio_mutex);
                    return -EDMA_TX;
                }
#endif
            }
#ifdef __x86_64__
            dma_free_coherent(NULL, pim_args.srcA_size, dmabuf, dma_handle);
#endif
            break;
        }
        default :
            printk(KERN_ERR "Invalid IOCTL:%d\n", cmd);
            break;
    }
    // mutex_unlock(&extio_mutex);
    return SUCCESS;
}

int pl_dma_open(struct inode *inode, struct file *file)
{
    struct dev_node_t *node;
    node = container_of(inode->i_cdev, struct dev_node_t, pl_cdev);
    file->private_data = node;
    // printk("PL_DMA_OPEN - CDMA_DEV->DMA_CTL_REG: %p\n", node->cdma_dev->dma_ctl_reg);
    // totalDev = node->_totalDev; // Is this instruction required?
    //int ret;
    //ret = mutex_trylock(&dma_lock);
    //if (ret == 0) {
    //    printk(KERN_ERR "DMA is in use\n");
    //    return -EFAULT;
    //}
    return SUCCESS;
}

int pl_dma_release(struct inode *inode, struct file *file)
{
    //dma_ctrl_write(CDMA_REG_CR,  CDMA_REG_CR_RESET, cdma_dev); //DMA RESET
    //wmb();
    //mutex_unlock(&dma_lock);
    return SUCCESS;
}

int pl_dma_mmap(struct file *filp, struct vm_area_struct *vma)
{
    return SUCCESS;
}

ssize_t pl_dma_read(struct file *f, char *buf, size_t nbytes, loff_t *ppos)
{
    return SUCCESS;
}

MODULE_DESCRIPTION("PL-DMA file operations");
MODULE_AUTHOR("KU-Leewj");
MODULE_LICENSE("GPL");
