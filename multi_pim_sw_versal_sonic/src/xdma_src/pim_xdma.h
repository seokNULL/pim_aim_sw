#include "../../include/pim.h"
#include "tools/dma_utils.c"

struct xdma_t {
    char devname[50];
    int fd;
};

extern struct xdma_t *xdma_cpu2pim;
extern struct xdma_t *xdma_pim2cpu;