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

#include <iostream>
#include <fstream>
#define CHECK
#define TO_CSV
// #define INIT

#define FPGA_ID 0
#define MILLION 1000000L
#define MEMCPY_UNIT 1024

// struct timespec start_HOST2HOST, end_HOST2HOST;
// struct timespec start_HOST2FPGA, end_HOST2FPGA;
// struct timespec start_FPGA2HOST, end_FPGA2HOST;
// struct timespec start_FPGA2FPGA, end_FPGA2FPGA;
struct timespec start, end;


// uint64_t diff_HOST2HOST;
// uint64_t diff_HOST2FPGA;
// uint64_t diff_FPGA2HOST;
// uint64_t diff_FPGA2FPGA;
uint64_t diff;

// insertion sort
void insertionSort(short arr[], int n)
{
    int i, key, j;
    for (i = 1; i < n; i++) {
        key = arr[i];
        j = i - 1;
         // Move elements of arr[0..i-1],
        // that are greater than key, 
        // to one position ahead of their
        // current position
        while (j >= 0 && arr[j] > key) {
            arr[j + 1] = arr[j];
            j = j - 1;
        }
        arr[j + 1] = key;
    }
}
// A utility function to print an array
// of size n
void printArray(short arr[], int n)
{
    int i;
    for (i = 0; i < n; i++)
        std::cout << arr[i] << " ";
    std::cout << std::endl;
}
// matrix multiplication
void mat_mul_CPU(float *src_A_DRAM, float * src_B_DRAM, float * dst_C_DRAM, int p_size, int q_size, int r_size)
{
  union converter{
  float f_val;
  unsigned int u_val;
  };
  union converter a;
  union converter b;
  union converter c;
  union converter d;

  for(size_t c_r=0; c_r < p_size; c_r++)
  {
    for(size_t c_c=0; c_c<r_size; c_c++)
    {
      float tmp=0.0f;
      float mul_tmp=0.0f;
      for(size_t k=0; k<q_size; k++)
      {
        unsigned residual=0;
        if(q_size>32) residual= (q_size-32)*16;

        unsigned row=((k/32)*512)+(c_r*512);
        unsigned col=k%32;

        mul_tmp = (src_A_DRAM[ k + c_r*q_size ] * src_B_DRAM[(k*r_size) + c_c]);

        tmp+=(src_A_DRAM[ k + c_r*q_size ] * src_B_DRAM[(k*r_size) + c_c]);

        a.f_val = src_A_DRAM[ k + c_r*q_size ];
        b.f_val = src_B_DRAM[(k*r_size) + c_c];
        c.f_val = mul_tmp;
        d.f_val = tmp;

        //if(c_c==0) printf("idx[%lu] a(%x) x b(%x) = c(%x) || acc=%x \n", k, a.u_val, b.u_val, c.u_val, d.u_val);
      }
      dst_C_DRAM[c_c+c_r*r_size]=tmp;
    }
  }
}

int main(int argc, char *argv[])
{

    if(argc<3) {
        printf("Check vector size param p,q (pxq)\n");
        printf("./memcpy p q MODE (0 ~ 9)\n");
        printf("             MODE 0: XDMA) FPGA to FPGA \n");
        exit(1);
    }
    
    int flush_size = 20*1024*1024;
    int buffer_size = atoi(argv[2]);
    int matmul_size = 32;
    int cmd = atoi(argv[1]);

    init_pim_drv();

    srand((unsigned int)time(NULL));  // For reset random seed

    short *host_buffer = (short *)(mmap(NULL, buffer_size*sizeof(short), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    double *flush_buffer = (double *)(mmap(NULL, buffer_size*sizeof(double), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    short *fpga_buffer = (short *)pim_malloc(buffer_size*sizeof(short), FPGA_ID);

    // matmul oprands
    float *host_buffer_srcA = (float *)(mmap(NULL, buffer_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *host_buffer_srcB = (float *)(mmap(NULL, buffer_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *host_buffer_dst  = (float *)(mmap(NULL, buffer_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *fpga_buffer_srcA = (float *)pim_malloc(buffer_size*sizeof(float), FPGA_ID);
    float *fpga_buffer_srcB = (float *)pim_malloc(buffer_size*sizeof(float), FPGA_ID);
    float *fpga_buffer_dst  = (float *)pim_malloc(buffer_size*sizeof(float), FPGA_ID);


    // initialization
    for(size_t i=0; i<buffer_size; i++){
      float tmp  = generate_random();
      short tmp0 = float_to_short(tmp);
      host_buffer[i] = tmp0;
      fpga_buffer[i] = tmp0;
    }
    for(size_t i=0; i<buffer_size; i++){
      float tmp  = generate_random();
      short tmp0 = float_to_short(tmp);
      host_buffer_srcA[i] = tmp0;
      host_buffer_srcB[i] = tmp0;
      host_buffer_dst[i] = tmp0;
      fpga_buffer_srcA[i] = tmp0;
      fpga_buffer_srcB[i] = tmp0;
      fpga_buffer_dst[i] = tmp0;
    }
    // for(size_t i=0; i<flush_size; i++) {
    //     flush_buffer[i] = i;
    // }
    // /* Cache flush */
    // double flush_temp;
    // for(size_t i=0; i<flush_size; i++) {
    //     flush_temp = flush_buffer[i];
    // }
    // /****/
    bool success = true;  
    pim_args *set_info;
    int size = sizeof(pim_args);
    set_info = (pim_args *)malloc(size);

    /* 1. Test execution time of R/W 1000 times */
    if(cmd==0){
        short temp_buffer;
        clock_gettime(CLOCK_MONOTONIC, &start);
        for(int i = 0; i < 1000; i++) {
            host_buffer[i] += 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);

        diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        printf("CPU R/W 1000 times total execution time %llu nanoseconds (2 Bytes per access)\n", (long long unsigned int) diff);
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        for(int i = 0; i < 1000; i++) {
            fpga_buffer[i] += 1;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);

        diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        printf("PIM R/W 1000 times total execution time %llu nanoseconds (2 Bytes per access)\n", (long long unsigned int) diff);
    }   
    /* 2. Sorting */
    else if(cmd==1) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        insertionSort(host_buffer, buffer_size);
        clock_gettime(CLOCK_MONOTONIC, &end);
        diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        printf("CPU sorting total execution time %llu nanoseconds (insertion sort of 1024 elements)\n", (long long unsigned int) diff);

        clock_gettime(CLOCK_MONOTONIC, &start);
        insertionSort(fpga_buffer, buffer_size);
        clock_gettime(CLOCK_MONOTONIC, &end);
        diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        printf("PIM sorting total execution time %llu nanoseconds (insertion sort of 1024 elements)\n", (long long unsigned int) diff);


        //printArray(host_buffer, buffer_size);
    }
    /* 3. Matmul */
    else if(cmd==2) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        mat_mul_CPU(host_buffer_srcA, host_buffer_srcB, host_buffer_dst, matmul_size, matmul_size, matmul_size);
        clock_gettime(CLOCK_MONOTONIC, &end);
        diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        printf("CPU matrix multiplication total execution time %llu nanoseconds (32x32x32 matmul)\n", (long long unsigned int) diff);

        // clock_gettime(CLOCK_MONOTONIC, &start);
        // mat_mul_CPU(fpga_buffer_srcA, fpga_buffer_srcB, fpga_buffer_dst, matmul_size, matmul_size, matmul_size);
        // clock_gettime(CLOCK_MONOTONIC, &end);
        // diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        // printf("PIM matrix multiplication total execution time %llu nanoseconds (32x32x32 matmul)\n", (long long unsigned int) diff);
    }
    /* 4. Memory copy */
    else if(cmd==3) {
        clock_gettime(CLOCK_MONOTONIC, &start);
        for(int i=0; i<buffer_size; i++) {
            host_buffer_dst[i] = host_buffer[i];
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        printf("CPU memcpy total execution time %llu nanoseconds (%d bytes)\n", (long long unsigned int) diff, buffer_size*sizeof(short));
        
        clock_gettime(CLOCK_MONOTONIC, &start);
        for(int i=0; i<buffer_size; i++) {
            host_buffer_dst[i] = fpga_buffer[i];
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        printf("PIM memcpy total execution time %llu nanoseconds (%d bytes)\n", (long long unsigned int) diff, buffer_size*sizeof(short));
    }
    /* 5. FPGA to FPGA data copy with CDMA */
    else if(cmd==4) {
        set_info->srcA_va   = (uint64_t)&fpga_buffer_srcA[0];
        set_info->dstC_va   = (uint64_t)&fpga_buffer_dst[0];
        set_info->srcA_size = buffer_size*sizeof(short);
        int pl_dma = 0;
        if ((pl_dma=open(PL_DMA_DRV, O_RDWR|O_SYNC)) < 0) {
            perror("CDMA drvier open");
            exit(-1);
        }
        clock_gettime(CLOCK_MONOTONIC, &start);
        if (ioctl(pl_dma, MEMCPY_PL2PL, set_info) < 0) {
            printf("ERROR DMA \n");
            return 0;
        }
        clock_gettime(CLOCK_MONOTONIC, &end);
        diff = BILLION * (end.tv_sec - start.tv_sec) + (end.tv_nsec - start.tv_nsec);
        printf("PIM CDMA single tx total execution time %llu nanoseconds (%d bytes)\n", (long long unsigned int) diff, buffer_size*sizeof(short));
    }
    /**/
    else{
        printf("CMD ERROR!\n");
    }
    return 0;
}



