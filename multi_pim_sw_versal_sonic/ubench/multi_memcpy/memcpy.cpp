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
// #define TO_CSV
#define INIT

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
//
int main(int argc, char *argv[])
{

    if(argc<4) {
        printf("Check vector size param p,q (pxq)\n");
        printf("./memcpy p q MODE (0 ~ 9)\n");
        printf("             MODE 0: XDMA) FPGA to FPGA \n");
        printf("             MODE 1: XDMA) FPGA to Host \n");
        printf("             MODE 2: XDMA) Host to FPGA \n");
        printf("             MODE 3: memcopy) FPGA to HOST \n");
        printf("             MODE 4: memcopy) HOST to FPGA \n");
        printf("             MODE 5: memcopy) FPGA to FPGA \n");
        printf("             MODE 6: x86-DMA) HOST to HOST \n");
        printf("             MODE 7: x86-DMA) HOST to FPGA \n");
        printf("             MODE 8: x86-DMA) FPGA to HOST \n");
        printf("             MODE 9: x86-DMA) FPGA to FPGA \n");
        exit(1);
    }
    
    int p_size = atoi(argv[1]);
    int q_size = atoi(argv[2]);
    int r_size = q_size;

    int cmd = atoi(argv[3]);

    int srcA_size = p_size * q_size;
    int srcB_size = p_size * q_size;
    int dstC_size = p_size * q_size;

    int tmp=0;
    int host_dma=0;

    init_pim_drv();
    printf("Memory copy size: %lu B\n", p_size * q_size * sizeof(short));
    
    // int fd = open("/dev/pim_mem0", O_RDWR | O_SYNC, 0);
    short *PL_srcA_buf0 = (short *)pim_malloc(srcA_size*sizeof(short), SRC_FPGA);
    short *PL_dstC_buf0 = (short *)pim_malloc(srcA_size*sizeof(short), SRC_FPGA);
    short *PL_srcA_buf1 = (short *)pim_malloc(dstC_size*sizeof(short), DST_FPGA);
    short *PL_dstC_buf1 = (short *)pim_malloc(dstC_size*sizeof(short), DST_FPGA);
#ifdef INIT
    printf("FPGA srcA init\n");
    for(size_t i=0; i<srcA_size; i++){
      PL_srcA_buf0[i]=0;
      PL_srcA_buf1[i]=0;
      PL_dstC_buf0[i]=0;
      PL_dstC_buf1[i]=0;
    }
    for(size_t i=0; i<srcA_size; i++){
    //   float tmp  = 1.0;
    //   float tmp2  = 2.0;
      float tmp  = generate_random();
      float tmp2  = generate_random();
      short tmp0 = float_to_short(tmp);
      short tmp02 = float_to_short(tmp2);
      PL_srcA_buf0[i] = tmp0;
      PL_srcA_buf1[i] = tmp02;
    }  

    srand((unsigned int)time(NULL));  // For reset random seed
#endif
    // getchar();
    //For CPU verify
    short *PS_srcA_buf = (short *)(mmap(NULL, srcA_size*sizeof(short), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    short *PS_dstC_buf = (short *)(mmap(NULL, dstC_size*sizeof(short), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    if (PS_srcA_buf == MAP_FAILED) {
        printf("PS srcA call failure.\n");
        return -1;
    }
    if (PS_dstC_buf == MAP_FAILED) {
        printf("PS dstC call failure.\n");
        return -1;
    }
    // for(size_t i=0; i<srcA_size; i++){
    //   PS_srcA_buf[i]=0;
    // }
    printf("Complete host malloc \n");
    // getchar();
#ifdef INIT
    for(size_t i=0; i<srcA_size; i++){
      PS_srcA_buf[i]=0;
    //   PS_dstC_buf[i]=0;
    }
    // Input data
    srand((unsigned int)time(NULL));  // For reset random seed
    
    printf("HOST srcA init\n");
    for(size_t i=0; i<srcA_size; i++){
      float tmp  = generate_random();
      short tmp0 = float_to_short(tmp);
    //   short tmp0 = float_to_short(1.0);
      PS_srcA_buf[i] = tmp0;
    }  

    printf("HOST dstC init\n");
    for(size_t i=0; i<dstC_size; i++){
      PS_dstC_buf[i] = 0;
    }
#endif
    bool success = true;
    // printf("Enter to start DMA transaction\n");

    pim_args *set_info;
    int size = sizeof(pim_args);
    set_info = (pim_args *)malloc(size);

    if(cmd==0){
        clock_gettime(CLOCK_MONOTONIC, &start_FPGA2FPGA);

        // int rc = datacopy_pim2pim(PL_dstC_buf0, PL_srcA_buf1, srcA_size*sizeof(short), 0);
        int rc = datacopy_pim2pim(PL_dstC_buf1, PL_srcA_buf0, srcA_size*sizeof(short), 1);
        for(int i; i<10; i++) datacopy_pim2pim(PL_dstC_buf1, PL_srcA_buf0, srcA_size*sizeof(short), 1);
        // for(int i; i<5; i++) datacopy_pim2pim(PL_dstC_buf0, PL_srcA_buf1, srcA_size*sizeof(short), 0);
        if (rc < 0){
            perror("FPGA2FPGA data copy");
            exit(-1);
        }
        
        // for(size_t i = 0; i < srcA_size; i++) {
            // new_pim_input_buffer[i] = copy_buffer[i];
            // PL_dstC_buf1[i] = PL_srcA_buf0[i];
        // }
        clock_gettime(CLOCK_MONOTONIC, &end_FPGA2FPGA);

        diff_FPGA2FPGA = BILLION * (end_FPGA2FPGA.tv_sec - start_FPGA2FPGA.tv_sec) + (end_FPGA2FPGA.tv_nsec - start_FPGA2FPGA.tv_nsec);
        printf("XDMA FPGA --> FPGA execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2FPGA, p_size, q_size);
#ifdef TO_CSV
        std::ofstream outFile("./perf/XDMA_FPGA2FPGA_latency.csv", std::ios::app);
        outFile << diff_FPGA2FPGA << std::endl;
        outFile.close();
#endif
        // printf("XDMA FPGA --> FPGA execution time %llu us %d %d\n", (long long unsigned int) diff_FPGA2FPGA/1000, p_size, q_size);
#ifdef CHECK
        printf("Correctness FPGA0 --> FPGA1 XDMA check!\n\n");
        for(int i=0; i<srcA_size; i++){ 
            // if(PL_dstC_buf0[i] != PL_srcA_buf1[i]) {
            if(PL_srcA_buf0[i] != PL_dstC_buf1[i]) {
                // printf("Error PL_src[%d]=%d PL_dst[%d]=%d \n",i,PL_dstC_buf0[i],i,PL_srcA_buf1[i]);
                printf("Error PL_src[%d]=%d PL_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PL_dstC_buf1[i]);
                success = false;
                break;
            }
            else{
                // printf("idx[%4d]   %hu  |  ", i, PL_dstC_buf0[i]);
                printf("idx[%4d]   %hu  |  ", i, PL_srcA_buf0[i]);
                // printf("  %hu ", PL_srcA_buf1[i]);
                printf("  %hu ", PL_dstC_buf1[i]);
                printf("\n");
            }
        }
        if (!success)
            printf("Correctness FPGA0 --> FPGA1 check failed!\n\n");
        else
            printf("Correctness FPGA0 --> FPGA1 check done!\n\n");
#endif

    } else if(cmd==1){
        clock_gettime(CLOCK_MONOTONIC, &start_FPGA2HOST);
        // for(int i=0; i<dstC_size; i++){ 
        //     printf("Error PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
        // }        
        printf("HOST START ADDR: %llx\n", PS_dstC_buf);
        int rc = datacopy_pim2cpu(PS_dstC_buf, PL_srcA_buf0, srcA_size*sizeof(short), 0);
        // int rc = datacopy_pim2cpu(PS_dstC_buf, PL_srcA_buf1, srcA_size*sizeof(short), 1);
        if (rc < 0){
            perror("FPGA2HOST data copy");
            exit(-1);
        }

        clock_gettime(CLOCK_MONOTONIC, &end_FPGA2HOST);

        diff_FPGA2HOST = BILLION * (end_FPGA2HOST.tv_sec - start_FPGA2HOST.tv_sec) + (end_FPGA2HOST.tv_nsec - start_FPGA2HOST.tv_nsec);
        printf("XDMA FPGA --> HOST execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2HOST, p_size, q_size);
        // printf("XDMA FPGA --> HOST execution time %llu us %d %d\n", (long long unsigned int) diff_FPGA2HOST/1000, p_size, q_size);
#ifdef TO_CSV
        std::ofstream outFile("./perf/XDMA_FPGA2HOST_latency.csv", std::ios::app);
        outFile << diff_FPGA2HOST << std::endl;
        outFile.close();
#endif        
#ifdef CHECK
        printf("Correctness FPGA --> HOST XDMA check!\n\n");
        printf("            FPGA   |    HOST\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PL_srcA_buf0[i] != PS_dstC_buf[i]) {
                printf("Error PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
                success = false;
                break;
            }
            // else{
            //     printf("idx[%4d]   %hu  |  ", i, PL_srcA_buf0[i]);
            //     printf("  %hu ", PS_dstC_buf[i]);
            //     printf("\n");
            // }
        }
        if (!success)
            printf("Correctness FPGA --> HOST check failed!\n\n");
        else
            printf("Correctness FPGA --> HOST check done!\n\n");
#endif
    }
    else if(cmd==2){
        clock_gettime(CLOCK_MONOTONIC, &start_HOST2FPGA);
        // for(int i=0; i<dstC_size; i++){ 
        //     printf("Error PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_dstC_buf0[i],i,PS_dstC_buf[i]);
        // }        
        int rc = datacopy_cpu2pim(PL_dstC_buf0, PS_srcA_buf, srcA_size*sizeof(short), 0);
        // int rc = datacopy_cpu2pim(PL_dstC_buf0, PS_srcA_buf, srcA_size*sizeof(short), SRC_FPGA);
        if (rc < 0){
            perror("HOST2FPGA data copy");
            exit(-1);
        }
    
        clock_gettime(CLOCK_MONOTONIC, &end_HOST2FPGA);

        diff_HOST2FPGA = BILLION * (end_HOST2FPGA.tv_sec - start_HOST2FPGA.tv_sec) + (end_HOST2FPGA.tv_nsec - start_HOST2FPGA.tv_nsec);
        // printf("XDMA HOST --> FPGA execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_HOST2FPGA, p_size, q_size);
        printf("XDMA HOST --> FPGA execution time %llu us %d %d\n", (long long unsigned int) diff_HOST2FPGA/1000, p_size, q_size);
        
#ifdef TO_CSV
        std::ofstream outFile("./perf/XDMA_HOST2FPGA_latency.csv", std::ios::app);
        outFile << diff_HOST2FPGA << std::endl;
        outFile.close();
#endif        
#ifdef CHECK
        printf("Correctness HOST --> FPGA XDMA check!\n\n");
        printf("            HOST   |    FPGA\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PS_srcA_buf[i] != PL_dstC_buf0[i]) {
                printf("idx[%4d]   %hu  |  ", i, PS_srcA_buf[i]);
                printf("  %hu ", PL_dstC_buf0[i]);
                printf("\n");
                success = false;
                break;
            }
            else{
                // printf("idx[%4d]   %hu  |  ", i, PS_srcA_buf[i]);
                // printf("  %hu ", PL_dstC_buf1[i]);
                // printf("\n");
            }
            //printf("Error PS_src[%d]=%d PL_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PL_dstC_buf1[i]);
        }
        if (!success)
            printf("Correctness HOST --> FPGA check failed!\n\n");
        else
            printf("Correctness HOST --> FPGA check done!\n\n");
#endif
    }
    else if(cmd==3){
        clock_gettime(CLOCK_MONOTONIC, &start_FPGA2HOST);
        const char* src = (char*)PL_srcA_buf0;
        char* dst = (char*)PS_dstC_buf;
        size_t size = srcA_size*sizeof(short);
        /* USE MEMCPY FUNCTION WITH FIXED SIZE*/
        // for(size_t i = 0; i < size; i += MEMCPY_UNIT) {
        //     size_t remaining = size - i;
        //     size_t copy_size = (remaining < MEMCPY_UNIT) ? remaining : MEMCPY_UNIT;
        //     memcpy(dst + i, src + i, copy_size);
        // }
        /* USE MEMCPY FUNCTION*/
        // memcpy(dst, src, size);
        /* COPY ELEMENT BY ELEMENT */
        size = srcA_size;
        short* src_short = PL_srcA_buf0;
        short* dst_short = PS_dstC_buf;
        
        while(size--)
            *dst_short++ = *src_short++;

        clock_gettime(CLOCK_MONOTONIC, &end_FPGA2HOST);

        diff_FPGA2HOST = BILLION * (end_FPGA2HOST.tv_sec - start_FPGA2HOST.tv_sec) + (end_FPGA2HOST.tv_nsec - start_FPGA2HOST.tv_nsec);
        printf("MEMCPY FUNC PL --> HOST execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2HOST, p_size, q_size);
#ifdef TO_CSV
        std::ofstream outFile("./perf/MEMCPY_FPGA2HOST_latency.csv", std::ios::app);
        outFile << diff_FPGA2HOST << std::endl;
        outFile.close();
#endif
#ifdef CHECK
        printf("Correctness PL --> HOST check!\n\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PL_srcA_buf0[i] != PS_dstC_buf[i]) {
                printf("Error PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
                success = false;
                break;
            }
            printf("PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
        }
        if (!success)
            printf("Correctness PL --> HOST check failed!\n\n");
        else
            printf("Correctness PL --> HOST check done!\n\n");   
#endif
    }
    else if(cmd==4){
        clock_gettime(CLOCK_MONOTONIC, &start_HOST2FPGA);
        const char* src = (char*)PS_srcA_buf;
        char* dst = (char*)PL_dstC_buf1;
        size_t size = srcA_size*sizeof(short);
        // for(size_t i = 0; i < size; i += MEMCPY_UNIT) {
        //     size_t remaining = size - i;
        //     size_t copy_size = (remaining < MEMCPY_UNIT) ? remaining : MEMCPY_UNIT;
            // memcpy(dst + i, src + i, copy_size);
        // }
        // memcpy(dst, src, size);
        size = srcA_size;
        short* src_short = PS_srcA_buf;
        short* dst_short = PL_dstC_buf1;
        while(size--)
            *dst_short++ = *src_short++;
        clock_gettime(CLOCK_MONOTONIC, &end_HOST2FPGA);

        diff_HOST2FPGA = BILLION * (end_HOST2FPGA.tv_sec - start_HOST2FPGA.tv_sec) + (end_HOST2FPGA.tv_nsec - start_HOST2FPGA.tv_nsec);
        printf("MEM_CPY FUNC HOST --> PL execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_HOST2FPGA, p_size, q_size);
#ifdef TO_CSV
        std::ofstream outFile("./perf/MEMCPY_HOST2FPGA_latency.csv", std::ios::app);
        outFile << diff_HOST2FPGA << std::endl;
        outFile.close();
#endif
#ifdef CHECK
        printf("Correctness HOST --> PL check!\n\n");
        for(int i=0; i<dstC_size; i++){ 

            if(PS_srcA_buf[i] != PL_dstC_buf1[i]) {
                printf("Error PS_src[%d]=%d PL_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PL_dstC_buf1[i]);
                success = false;
                break;
            }
            printf("PS_src[%d]=%d PL_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PL_dstC_buf1[i]);
        }
        if (!success)
            printf("Correctness HOST --> PL check failed!\n\n");
        else
            printf("Correctness HOST --> PL check done!\n\n");
#endif            
    }
    else if(cmd==5){
        clock_gettime(CLOCK_MONOTONIC, &start_FPGA2FPGA);

        char* src = (char*)PL_srcA_buf0;
        char* dst = (char*)PL_dstC_buf1;
        size_t size = srcA_size*sizeof(short);
        // for(size_t i = 0; i < size; i += MEMCPY_UNIT) {
        //     size_t remaining = size - i;
        //     size_t copy_size = (remaining < MEMCPY_UNIT) ? remaining : MEMCPY_UNIT;
        //     memcpy(dst + i, src + i, copy_size);
        // }
        memcpy(dst, src, size);

        clock_gettime(CLOCK_MONOTONIC, &end_FPGA2FPGA);

        diff_FPGA2FPGA = BILLION * (end_FPGA2FPGA.tv_sec - start_FPGA2FPGA.tv_sec) + (end_FPGA2FPGA.tv_nsec - start_FPGA2FPGA.tv_nsec);
        printf("MEM_CPY FUNC FPGA --> FPGA execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2FPGA, p_size, q_size);
#ifdef TO_CSV
        std::ofstream outFile("./perf/MEMCPY_FPGA2FPGA_latency.csv", std::ios::app);
        outFile << diff_FPGA2FPGA << std::endl;
        outFile.close();
#endif
#ifdef CHECK
        printf("Correctness FPGA --> FPGA check!\n\n");
        for(int i=0; i<dstC_size; i++){ 

            if(PL_srcA_buf0[i] != PL_dstC_buf1[i]) {
                printf("Error PL_src[%d]=%d PL_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PL_dstC_buf1[i]);
                success = false;
                break;
            }
            //printf("Error PS_src[%d]=%d PL_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PL_dstC_buf1[i]);
        }
        if (!success)
            printf("Correctness FPGA --> FPGA check failed!\n\n");
        else
            printf("Correctness FPGA --> FPGA check done!\n\n");
#endif            
    } else if(cmd==6){
        set_info->srcA_ptr  = PS_srcA_buf;
        set_info->srcB_ptr  = NULL;
        set_info->dstC_ptr  = PS_dstC_buf;    
        set_info->srcA_va   = (uint64_t)&PS_srcA_buf[0];
        set_info->srcB_va   = 0x0;
        set_info->dstC_va   = (uint64_t)&PS_dstC_buf[0];
        set_info->srcA_size = srcA_size*sizeof(short);
        set_info->srcB_size = 0x0;
        set_info->dstC_size = dstC_size*sizeof(short);
        set_info->length = dstC_size*sizeof(short);

        // set_info->dummy_buf_PA = NULL;

        clock_gettime(CLOCK_MONOTONIC, &start_HOST2HOST);

        //__cache_flush(PS_dstC_buf, dstC_size*sizeof(short));
        
        if (ioctl(host_dma, HOST2HOST, set_info) < 0) {
            printf("ERROR DMA \n");
            return 0;
        }
        clock_gettime(CLOCK_MONOTONIC, &end_HOST2HOST);

        diff_HOST2HOST = BILLION * (end_HOST2HOST.tv_sec - start_HOST2HOST.tv_sec) + (end_HOST2HOST.tv_nsec - start_HOST2HOST.tv_nsec);
        printf("MEM_CPY HOST --> HOST execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_HOST2HOST, p_size, q_size);

        printf("Correctness HOST --> HOST check!\n\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PS_srcA_buf[i] != PS_dstC_buf[i]) {
                printf("Error HOST_src[%d]=%d HOST_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PS_dstC_buf[i]);
                success = false;
                break;
            }
            //printf("Error PS_src[%d]=%d PS_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PS_dstC_buf[i]);
        }
        if (!success)
            printf("Correctness HOST --> HOST check failed!\n\n");
        else
            printf("Correctness HOST --> HOST check done!\n\n");
    } else if(cmd==7){
        set_info->srcA_ptr  = PS_srcA_buf;
        set_info->dstC_ptr  = PL_dstC_buf1;    
        set_info->srcA_va   = (uint64_t)&PS_srcA_buf[0];
        set_info->dstC_va   = (uint64_t)&PL_dstC_buf1[0];
        set_info->srcA_size = srcA_size*sizeof(short);
        set_info->dstC_size = dstC_size*sizeof(short);

        set_info->length = dstC_size*sizeof(short);

        // set_info->dummy_buf_PA = NULL;

        clock_gettime(CLOCK_MONOTONIC, &start_HOST2FPGA);

        //__cache_flush(PS_dstC_buf, dstC_size*sizeof(short));
        
        if (ioctl(host_dma, HOST2FPGA, set_info) < 0) {
            printf("ERROR DMA \n");
            return 0;
        }
        clock_gettime(CLOCK_MONOTONIC, &end_HOST2FPGA);

        diff_HOST2FPGA = BILLION * (end_HOST2FPGA.tv_sec - start_HOST2FPGA.tv_sec) + (end_HOST2FPGA.tv_nsec - start_HOST2FPGA.tv_nsec);
        printf("MEM_CPY HOST --> PL execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_HOST2FPGA, p_size, q_size);

        printf("Correctness HOST --> PL check!\n\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PS_srcA_buf[i] != PL_dstC_buf1[i]) {
                printf("Error HOST_src[%d]=%d HOST_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PL_dstC_buf1[i]);
                success = false;
                break;
            }
            //printf("Error PS_src[%d]=%d PS_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PL_dstC_buf1[i]);
        }
        if (!success)
            printf("Correctness HOST --> PL check failed!\n\n");
        else
            printf("Correctness HOST --> PL check done!\n\n");
    } else if(cmd==8){
        set_info->srcA_ptr  = PL_srcA_buf0;
        set_info->dstC_ptr  = PS_dstC_buf;    
        set_info->srcA_va   = (uint64_t)&PL_srcA_buf0[0];
        set_info->dstC_va   = (uint64_t)&PS_dstC_buf[0];
        set_info->srcA_size = srcA_size*sizeof(short);
        set_info->dstC_size = dstC_size*sizeof(short);

        set_info->length = dstC_size*sizeof(short);

        // set_info->dummy_buf_PA = NULL;

        clock_gettime(CLOCK_MONOTONIC, &start_FPGA2HOST);

        //__cache_flush(PS_dstC_buf, dstC_size*sizeof(short));
        if (ioctl(host_dma, FPGA2HOST, set_info) < 0) {
            printf("ERROR DMA \n");
            return 0;
        }
        clock_gettime(CLOCK_MONOTONIC, &end_FPGA2HOST);

        diff_FPGA2HOST = BILLION * (end_FPGA2HOST.tv_sec - start_FPGA2HOST.tv_sec) + (end_FPGA2HOST.tv_nsec - start_FPGA2HOST.tv_nsec);
        printf("MEM_CPY PL --> HOST execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2HOST, p_size, q_size);

        printf("Correctness PL --> HOST check!\n\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PL_srcA_buf0[i] != PS_dstC_buf[i]) {
                printf("Error PL_src[%d]=%d HOST_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
                success = false;
                break;
            }
            //printf("Error PS_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PL_dstC_buf1[i]);
        }
        if (!success)
            printf("Correctness PL --> HOST check failed!\n\n");
        else
            printf("Correctness PL --> HOST check done!\n\n");
    } else if(cmd==9){
        set_info->srcA_ptr  = PL_srcA_buf0;
        set_info->dstC_ptr  = PL_dstC_buf1;    
        set_info->srcA_va   = (uint64_t)&PL_srcA_buf0[0];
        set_info->dstC_va   = (uint64_t)&PL_dstC_buf1[0];
        set_info->srcA_size = srcA_size*sizeof(short);
        set_info->dstC_size = dstC_size*sizeof(short);

        set_info->length = dstC_size*sizeof(short);

        // set_info->dummy_buf_PA = NULL;

        clock_gettime(CLOCK_MONOTONIC, &start_FPGA2FPGA);

        //__cache_flush(PL_dstC_buf1, dstC_size*sizeof(short));
        
        if (ioctl(host_dma, FPGA2FPGA, set_info) < 0) {
            printf("ERROR DMA \n");
            return 0;
        }
        clock_gettime(CLOCK_MONOTONIC, &end_FPGA2FPGA);

        diff_FPGA2FPGA = BILLION * (end_FPGA2FPGA.tv_sec - start_FPGA2FPGA.tv_sec) + (end_FPGA2FPGA.tv_nsec - start_FPGA2FPGA.tv_nsec);
        printf("MEM_CPY FPGA --> FPGA execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2FPGA, p_size, q_size);

        printf("Correctness FPGA --> FPGA check!\n\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PL_srcA_buf0[i] != PL_dstC_buf1[i]) {
                printf("Error HOST_src[%d]=%d HOST_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PL_dstC_buf1[i]);
                success = false;
                break;
            }
            //printf("Error PS_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PL_dstC_buf1[i]);
        }
        if (!success)
            printf("Correctness FPGA --> FPGA check failed!\n\n");
        else
            printf("Correctness FPGA --> FPGA check done!\n\n");
    }
    else if(cmd==10){
        clock_gettime(CLOCK_MONOTONIC, &start_FPGA2FPGA);

        char* src = (char*)PS_srcA_buf;
        char* dst = (char*)PS_dstC_buf;
        size_t size = srcA_size*sizeof(short);
        // for(size_t i = 0; i < size; i += MEMCPY_UNIT) {
        //     size_t remaining = size - i;
        //     size_t copy_size = (remaining < MEMCPY_UNIT) ? remaining : MEMCPY_UNIT;
        //     memcpy(dst + i, src + i, copy_size);
        // }
        memcpy(dst, src, size);

        clock_gettime(CLOCK_MONOTONIC, &end_FPGA2FPGA);

        diff_FPGA2FPGA = BILLION * (end_FPGA2FPGA.tv_sec - start_FPGA2FPGA.tv_sec) + (end_FPGA2FPGA.tv_nsec - start_FPGA2FPGA.tv_nsec);
        printf("MEM_CPY FUNC HOST --> HOST execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2FPGA, p_size, q_size);
#ifdef TO_CSV
        std::ofstream outFile("./perf/MEMCPY_FPGA2FPGA_latency.csv", std::ios::app);
        outFile << diff_FPGA2FPGA << std::endl;
        outFile.close();
#endif
#ifdef CHECK
        printf("Correctness HOST --> HOST check!\n\n");
        for(int i=0; i<dstC_size; i++){ 

            if(PL_srcA_buf0[i] != PL_dstC_buf1[i]) {
                printf("Error PL_src[%d]=%d PL_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PL_dstC_buf1[i]);
                success = false;
                break;
            }
            //printf("Error PS_src[%d]=%d PL_dst[%d]=%d \n",i,PS_srcA_buf[i],i,PL_dstC_buf1[i]);
        }
        if (!success)
            printf("Correctness FPGA --> FPGA check failed!\n\n");
        else
            printf("Correctness FPGA --> FPGA check done!\n\n");
#endif            
    } else if (cmd==11){
        internalMemcpy(PL_dstC_buf0, PL_srcA_buf0, srcA_size*sizeof(short), 0);
        printf("Correctness PL --> PL check!\n\n");
        for(int i=0; i<srcA_size; i++){ 
            if(PL_srcA_buf0[i] != PL_dstC_buf0[i]) {
                printf("Error PL_src[%d]=%d PL_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PL_dstC_buf0[i]);
                success = false;
                // break;
            }
            else{
                printf("PL_src[%d]=%d PL_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PL_dstC_buf0[i]);
            }
        }
        if (!success)
            printf("Correctness PL --> PL check failed!\n\n");
        else
            printf("Correctness PL --> PL check done!\n\n");
    }
    
    else{
        printf("CMD ERROR!\n");
    }
    return 0;
}



