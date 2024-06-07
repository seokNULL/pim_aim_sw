#include "pim_mem.h"

static struct pim_mem_t *pim_mem;

void *multi_mmap_drv(int size, int fpga_id)
{
    struct pim_mem_t *pim_mem_node;
    pim_mem_node = &pim_mem[fpga_id];
    void *ptr = pim_mem_node->base + pim_mem_node->total_size;

    pim_mem_node->total_size += size;
    if (pim_mem_node->total_size > pim_mem_node->pim_mem_size) {
    	printf("PIM] OOM \n");
 		assert(0);
    } 
    PIM_MEM_LOG("MMAP - ptr:%p (Size: %d) \n", ptr, size);
    return ptr;
}

void __init_mpim_drv(void) {
    int i;         
    atexit(__release_mpim_drv);
    pim_mem = (struct pim_mem_t*)malloc(sizeof(struct pim_mem_t)*totalDevNum);

    pim_mem_info pim_info;
    pim_info.addr = 0;
    pim_info.size = 0;
    struct pim_mem_t *pim_mem_node;

    for (i = 0; i < totalDevNum; i++) {
        pim_mem_node = &pim_mem[i];
        snprintf(pim_mem_node->dev_name, sizeof(pim_mem_node->dev_name), "%s%d", PIM_MEM_DEV_PREFIX, i);
        
        pim_mem_node->fd = open(pim_mem_node->dev_name, O_RDWR, 0);
        ioctl(pim_mem_node->fd, PIM_MEM_INFO, &pim_info);
        if (pim_mem_node->fd < 0) {
            printf("couldn't open device: %s (%d) \n", pim_mem_node->dev_name, pim_mem_node->fd);
            assert(0);
        } 
        if ((pim_info.addr == 0) || (pim_info.size == 0)) {
            printf("Error PIM driver\n");
            assert(0);
        }
        
        PIM_MEM_LOG("INIT PIM drv (PID: %d)\n", getpid());
        PIM_MEM_LOG("PIM memory addr: %lx\n", pim_info.addr);
        PIM_MEM_LOG("PIM memory size: %lx\n", pim_info.size);
        pim_mem_node->pim_mem_size = pim_info.size;
        PIM_MEM_LOG("PIM start address: %lx\n", pim_info.addr);
        pim_mem_node->base = mmap(0, pim_info.size, PROT_READ | PROT_WRITE, MAP_SHARED, pim_mem_node->fd, pim_info.addr);
        PIM_MEM_LOG("PIM mmap address: %lx\n", pim_mem_node->base);
        pim_mem_node->total_size = 0;
    }
}

void __release_mpim_drv(void) {
    int i;
    struct pim_mem_t *pim_mem_node;

    for (i = 0; i < totalDevNum; i++) {
        pim_mem_node = &pim_mem[i];
        PIM_MEM_LOG("Release PIM driver (PID: %d) \n", getpid());
        if (pim_mem_node->fd > 0) {
            close(pim_mem_node->fd);
            pim_mem_node->fd = -1;
            PIM_MEM_LOG("\t Close PIM driver \n");
        }
        if (pim_mem_node->base != NULL) {
            munmap(pim_mem_node->base, pim_mem_node->pim_mem_size);
            pim_mem_node->base = NULL;
            PIM_MEM_LOG("\t Unmap PIM memory \n");
        }	
    }
}
