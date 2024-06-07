#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include "pim_math.h"
#include <time.h>
#include <unistd.h>

#include <pthread.h>

static int addr_fd;
struct pl_dma_t *pl_dma;
int totalDevNum;

/* Mutex for mutual exclusive translation */
static pthread_mutex_t lock;

void close_mpim_drv(void) {
	int i;
    struct pl_dma_t *pl_dma_node;
    close(addr_fd);
    if (pl_dma != NULL) {
        for(i = 0; i < totalDevNum; i++) {
            pl_dma_node = &pl_dma[i];
            // pthread_mutex_destroy(&pl_dma_node->lock); 
            if (pl_dma_node->fd != 0) {
                close(pl_dma_node->fd);
                pl_dma_node->fd = 0;
                free(pl_dma_node->pim_isa);
            }
        }
        free(pl_dma);
	}	
}

int init_mpim_drv(void) {
    int i, fd;
    addr_fd = open(PIM_ADDR_DEV, O_RDWR | O_SYNC, 0);

    fd = open(PIM_MEM_DEV, O_RDWR | O_SYNC, 0);
    ioctl(fd, DEV_COUNT, &totalDevNum);
    close(fd);
    printf("totalDevNum : %d\n", totalDevNum);  //TODO: init_pim_drv need to be called only once?

    // pthread_mutex_init(&lock, NULL);
    // static DEFINE_MUTEX(extio_mutex);
    
    if (pl_dma == NULL) {
        pl_dma = (struct pl_dma_t*)malloc(sizeof(struct pl_dma_t)*totalDevNum);

        struct pl_dma_t *pl_dma_node;

        for (i = 0; i < totalDevNum; i++) {
            pl_dma_node = &pl_dma[i];
            snprintf(pl_dma_node->dev_name, sizeof(pl_dma_node->dev_name), "%s%d", PL_DMA_DRV_PREFIX, i);
            
            // printf("CONCATNATED NAME: %s\n", pl_dma_node->dev_name);
            pl_dma_node->fd = open(pl_dma_node->dev_name, O_RDWR| O_SYNC, 0);
            if (pl_dma_node->fd < 0) {
                printf("\033[0;31m Couldn't open device: %s (%d) \033[0m\n", pl_dma_node->dev_name, pl_dma_node->fd);
                return -1;
            }
            pl_dma_node->pim_isa = (pim_isa_t *)malloc(sizeof(pim_isa_t) * 1024*1024); /* 2048 is temporal maximum descriptor */
            pim_args pim_args;
            ioctl(pl_dma_node->fd, DESC_MEM_INFO, &pim_args);
            pl_dma_node->desc_base = pim_args.desc_base;
            pl_dma_node->dram_base = pim_args.dram_base;
            PIM_MATH_LOG("%s: desc_base: %lx, dram_base: %lx \n", __func__, pl_dma_node->desc_base, pl_dma_node->dram_base);
        }
        atexit(close_mpim_drv);
    }
    init_xdma_drv(); //added
    return totalDevNum;
    // return 0;
}
uint64_t mVA2PA(uint64_t va, int fpga_id) { 
    /* Lock translation */
    // pthread_mutex_lock(&lock);

    pim_args pim_args;
    pim_args.va = va;
    if (pl_dma != NULL) { 
        struct pl_dma_t *pl_dma_node;
        pl_dma_node = &pl_dma[fpga_id];  
		// ioctl(pl_dma_node->fd, VA_TO_PA, &pim_args);

        pim_args.dram_base = pl_dma_node->dram_base;
		ioctl(addr_fd, ADDR_TRANS, &pim_args);
#ifdef __x86_64__
        /* In X86, PL memory address MUST BE changed (Refer. Vivado) */
        uint32_t pa_32 = pim_args.pa;
    
        /* Unlock translation */
        // pthread_mutex_unlock(&lock);
        return pa_32; 

#elif defined __aarch64__
        return pim_args.pa;
#endif
	} else {
		printf("\033[0;31m DMA driver not loaded! \033[0m");
		assert(0);
	}
} 

double _get_current_time() {
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return tv.tv_sec + tv.tv_nsec * 1e-9;
}


int internalMemcpy(void *pim_dst, void *pim_src, int size, int fpga_id) {
    if (pl_dma != NULL) {
        struct pl_dma_t *pl_dma_node;
        pl_dma_node = &pl_dma[fpga_id];
        pim_args copy_code;
        copy_code.srcA_va = (uint64_t)pim_src;
        copy_code.dstC_va = (uint64_t)pim_dst;
        copy_code.srcA_size = size;
        // pthread_mutex_lock(&pl_dma_node->lock);
		if (ioctl(pl_dma_node->fd, MEMCPY_PL2PL, &copy_code) < 0) {
            printf("\033[0;31m internal DMA transaction failed! \033[0m\n");
            // pthread_mutex_unlock(&pl_dma_node->lock);
			return -1;
        }
        // pthread_mutex_unlock(&pl_dma_node->lock);
		return 0;
	} else {
		printf("\033[0;31m DMA driver not loaded! \033[0m");
		return -1;
	}
}

int mpim_exec(pim_args *pim_args, int fpga_id) {
    if (pl_dma != NULL) {
        struct pl_dma_t *pl_dma_node;
        pl_dma_node = &pl_dma[fpga_id];
        pim_args->desc_host = pl_dma_node->pim_isa;
    	PIM_MATH_LOG("%s: desc_host:%p, num_desc:%d, last_desc:%x \n", 
    		__func__, pim_args->desc_host, pim_args->desc_idx, pim_args->desc_last);
        // pthread_mutex_lock(&pl_dma_node->lock);
		if (ioctl(pl_dma_node->fd, DMA_START, pim_args) < 0) {
            printf("\033[0;31m DMA transaction failed! \033[0m\n");
            // pthread_mutex_unlock(&pl_dma_node->lock);
			return -1;
        }
        // pthread_mutex_unlock(&pl_dma_node->lock);
		return 0;
	} else {
		printf("\033[0;31m DMA driver not loaded! \033[0m");
		return -1;
	}
}

uint64_t _mVA2PA(struct va_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    return mVA2PA(in.addr, fpga_id_out);
}


int _pim_exec(struct exec_args in) {
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
	return mpim_exec(in.pim_args, fpga_id_out);
}
