#include <stdio.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <time.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h> 
#include <stdint.h>

#include <sys/ioctl.h>
#include <stdbool.h>

#include <pim.h>

#include "convert_numeric.h"

#include "gpu_memcpy.h"

#include <iostream>
#include <fstream>

#define SRC_FPGA 0
#define DST_FPGA 1
#define MILLION 1000000L
#define MEMCPY_UNIT 1024

struct timespec start_HOST2HOST, end_HOST2HOST;
struct timespec start_HOST2FPGA, end_HOST2FPGA;
struct timespec start_FPGA2HOST, end_FPGA2HOST;
struct timespec start_FPGA2FPGA, end_FPGA2FPGA;


uint64_t diff_HOST2HOST;
uint64_t diff_HOST2FPGA;
uint64_t diff_FPGA2HOST;
uint64_t diff_FPGA2FPGA;


int iter;

int main(int argc, char *argv[])
{

    if(argc<3) {
        printf("Check vector size param p,q (pxq)\n");
        printf("./memcpy p q MODE (0 ~ 9)\n");
        printf("             MODE 0: XDMA) FPGA to FPGA \n");
        exit(1);
    }

    int p_size = atoi(argv[1]);
    int q_size = atoi(argv[2]);
    int r_size = q_size;

    int srcA_size = p_size * q_size;
    int srcB_size = p_size * q_size;
    int dstC_size = p_size * q_size;

    init_pim_drv();
    printf("Memory copy size: %lu B\n", p_size * q_size * sizeof(short));

    short *FPGA_src = (short *)pim_malloc(srcA_size*sizeof(short), SRC_FPGA);
    short *FPGA_dst = (short *)pim_malloc(srcA_size*sizeof(short), SRC_FPGA);
    printf("FPGA_src: %llx\n", VA2PA(((uint64_t)&FPGA_src[0]), SRC_FPGA));
    printf("FPGA_dst: %llx\n", VA2PA(((uint64_t)&FPGA_dst[0]), SRC_FPGA));
    if ((FPGA_src == MAP_FAILED) | (FPGA_dst == MAP_FAILED)) {
        printf("FPGA call failure.\n");
        return -1;
    }
    
    printf("FPGA srcA init\n");
    for(size_t i=0; i<srcA_size; i++){
      FPGA_src[i]=0;
      FPGA_dst[i]=0;
    }
    for(size_t i=0; i<srcA_size; i++){
      float tmp  = generate_random();
      short tmp0 = float_to_short(tmp);
      FPGA_src[i] = tmp0;
    }  

    srand((unsigned int)time(NULL));  // For reset random seed
    // getchar();
    //For CPU verify
    short *Host_src = (short *)(mmap(NULL, srcA_size*sizeof(short), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    short *Host_dst = (short *)(mmap(NULL, dstC_size*sizeof(short), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    printf("Complete host malloc \n");

    for(size_t i=0; i<dstC_size; i++){
      Host_dst[i]=0;
    }
    // Input data
    srand((unsigned int)time(NULL));  // For reset random seed
    
    printf("HOST srcA init\n");
    for(size_t i=0; i<srcA_size; i++){
      float tmp  = generate_random();
      short tmp0 = float_to_short(tmp);
    //   short tmp0 = float_to_short(1.0);
      Host_src[i] = tmp0;
    }  

    bool success = true;

    for(int i = 0; i < dstC_size; ++i) printf("Host_src[%d]: %d\n", i, Host_src[i]);
    for(int i = 0; i < dstC_size; ++i) printf("FPGA_src[%d]: %d\n", i, FPGA_src[i]);

    int fd = open("/dev/pim_mem0", O_RDWR | O_SYNC, 0);
    // int fd = open("/dev/null", O_RDWR | O_SYNC, 0);
    short* io_test = (short*)mmap(0, srcA_size*sizeof(short), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0x64000000);

    // short* GPU_dst = memcpy_host2gpu(Host_src, srcA_size*sizeof(short)); // GPU addr를 argu로 받아야됨
    // short* GPU_dst = memcpy_host2gpu(FPGA_src, srcA_size*sizeof(short)); // GPU addr를 argu로 받아야됨
    short* GPU_dst = memcpy_host2gpu(io_test, srcA_size*sizeof(short)); // GPU addr를 argu로 받아야됨
    // memcpy_gpu2host(Host_dst, GPU_dst, srcA_size*sizeof(short));
    // memcpy_gpu2host(FPGA_dst, GPU_dst, srcA_size*sizeof(short));
    // for(int i = 0; i < dstC_size; ++i) printf("FPGA_dst[%d]: %d\n", i, FPGA_dst[i]);

    return 0;
}



