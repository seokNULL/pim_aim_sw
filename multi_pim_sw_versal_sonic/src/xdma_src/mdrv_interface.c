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
#include <time.h> //


#include "../drv_src/multi_dev_lib_user.h"
#include "pim_xdma.h"

struct xdma_t *xdma_cpu2pim;
struct xdma_t *xdma_pim2cpu;
// int totalDevNum;

double get_current_time() {
  struct timespec tv;
  clock_gettime(CLOCK_MONOTONIC, &tv);
  return tv.tv_sec + tv.tv_nsec * 1e-9;
}


void close_xdma_drv(void) {
	int i;
    if (xdma_cpu2pim != NULL || xdma_pim2cpu != NULL) {
        for(i = 0; i < totalDevNum; i++) {
            close(xdma_cpu2pim[i].fd);
            close(xdma_pim2cpu[i].fd);
        }
        free(xdma_cpu2pim);
        free(xdma_pim2cpu);
	}	
}

int init_xdma_drv(void) {
    int i;
    if (xdma_cpu2pim == NULL || xdma_pim2cpu == NULL) {
        xdma_cpu2pim = (struct xdma_t*)malloc(sizeof(struct xdma_t)*totalDevNum);
        xdma_pim2cpu = (struct xdma_t*)malloc(sizeof(struct xdma_t)*totalDevNum);
        printf("TOTAL FPGA NUM: %d\n", totalDevNum);
        for (i = 0; i < totalDevNum; i++) {
            snprintf(xdma_cpu2pim[i].devname, sizeof(xdma_cpu2pim[i].devname), "%s%d%s", XDMA_PREFIX, i, XDMA_CPU2PIM);
            xdma_cpu2pim[i].fd = open(xdma_cpu2pim[i].devname, O_RDWR|O_SYNC, 0);
            if (xdma_cpu2pim[i].fd < 0) {
                perror("XDMA drvier open");
                exit(-1);
            }
            snprintf(xdma_pim2cpu[i].devname, sizeof(xdma_pim2cpu[i].devname), "%s%d%s", XDMA_PREFIX, i, XDMA_PIM2CPU);
            xdma_pim2cpu[i].fd = open(xdma_pim2cpu[i].devname, O_RDWR|O_SYNC, 0);
            if (xdma_pim2cpu[i].fd < 0) {
                perror("XDMA drvier open");
                exit(-1);
            }
        }
        atexit(close_xdma_drv);
    }
    return 0;
}

uint64_t va_to_axi(uint64_t va, int fpga_id) {
    return 0x20800000000 | VA2PA(va, fpga_id); 
}

int datacopy_cpu2pim(void *pim_dst, const void *cpu_src, int size, int fpga_id) {
    uint64_t buffer = (uint64_t)cpu_src;
    uint64_t pim_axi_pa = va_to_axi((uint64_t)pim_dst, fpga_id);

    int rc = write_from_buffer(xdma_cpu2pim[fpga_id].devname, xdma_cpu2pim[fpga_id].fd, (char*)buffer, (uint64_t)size, pim_axi_pa);

    if (rc < 0){
        perror("datacopy_cpu2pim error");
        exit(-1);
    }
    if (rc < size) {
        printf("#%d: underflow %d/%d.\n",
            fpga_id, rc, size);
        exit(-1);
    }
    return rc;
}

int datacopy_pim2cpu(void *cpu_dst, const void *pim_src, int size, int fpga_id) {
    uint64_t buffer = (uint64_t)cpu_dst;
    uint64_t pim_axi_pa = va_to_axi((uint64_t)pim_src, fpga_id);

    int rc = read_to_buffer(xdma_pim2cpu[fpga_id].devname, xdma_pim2cpu[fpga_id].fd, (char*)buffer, (uint64_t)size, pim_axi_pa);
    if (rc < 0){
        perror("datacopy_pim2cpu error");
        exit(-1);
    }
    if (rc < size) {
        printf("#%d: underflow %d/%d.\n",
            fpga_id, rc, size);
        exit(-1);
    }
    return rc;
}

int datacopy_pim2pim(void *pim_dst, const void *pim_src, int size, int dst_id) {
    uint64_t buffer = (uint64_t)pim_src;
    // uint64_t dst_axi_pa = 0x20800000000 | VA2PA((uint64_t)&pim_dst[0]);
    uint64_t dst_axi_pa = va_to_axi((uint64_t)pim_dst, dst_id);
        
    int rc = p2p_write_from_buffer(xdma_cpu2pim[dst_id].devname, xdma_cpu2pim[dst_id].fd, (char*)buffer, (uint64_t)size, dst_axi_pa);
    if (rc < 0){
        perror("datacopy_pim2pim error"); //TODO: implement additional channel for exclusive p2p usage
        exit(-1);
    }
    if (rc < size) {
        printf("#%d: underflow %d/%d.\n",
            dst_id, rc, size);
        exit(-1);
    }
    return rc;
}