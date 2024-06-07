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
#include <pthread.h>

#define CHECK
// #define TO_CSV
#define INIT
#define MAX_PIM 2

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
 
    short *PL_srcA_buf0 = (short *)pim_malloc(srcA_size*sizeof(short), SRC_FPGA);
    short *PL_dstC_buf0 = (short *)pim_malloc(srcA_size*sizeof(short), SRC_FPGA);
    short *PL_srcA_buf1 = (short *)pim_malloc(dstC_size*sizeof(short), DST_FPGA);
    short *PL_dstC_buf1 = (short *)pim_malloc(dstC_size*sizeof(short), DST_FPGA);
    short *PL_dstT_buf0 = (short *)pim_malloc(dstC_size*sizeof(short), SRC_FPGA);

    printf("VA) PL_srcA_buf0: %llx\n", PL_srcA_buf0);
    printf("VA) PL_dstC_buf0: %llx\n", PL_dstC_buf0);
    printf("VA) PL_srcA_buf1: %llx\n", PL_srcA_buf1);
    printf("VA) PL_dstC_buf1: %llx\n", PL_dstC_buf1);
    printf("PA) PL_srcA_buf0: %llx\n", VA2PA(((uint64_t)&PL_srcA_buf0[0]), SRC_FPGA));
    printf("PA) PL_dstC_buf0: %llx\n", VA2PA(((uint64_t)&PL_dstC_buf0[0]), SRC_FPGA));
    printf("PA) PL_srcA_buf1: %llx\n", VA2PA(((uint64_t)&PL_srcA_buf1[0]), DST_FPGA));
    printf("PA) PL_dstC_buf1: %llx\n", VA2PA(((uint64_t)&PL_dstC_buf1[0]), DST_FPGA));
    // if ((PL_srcA_buf0 == MAP_FAILED) | (PL_dstC_buf1 == MAP_FAILED)) {
    //     printf("FPGA call failure.\n");
    //     return -1;
    // }

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
    printf("Complete host malloc \n");

#ifdef INIT
    printf("FPGA srcA init\n");
    // for(size_t i=0; i<srcA_size; i++){
    //   PL_srcA_buf0[i]=0;
    //   PL_srcA_buf1[i]=0;
    //   PL_dstC_buf0[i]=0;
    //   PL_dstC_buf1[i]=0;
    // }
    srand((unsigned int)time(NULL));  // For reset random seed
    for(size_t i=0; i<srcA_size; i++){
    //   float tmp  = 1.0;
    //   float tmp2  = 2.0;
      float tmp  = generate_random();
      float tmp2  = generate_random();
      float tmp3  = generate_random();
      short tmp0 = float_to_short(tmp);
      short tmp02 = float_to_short(tmp2);
      short tmp03 = float_to_short(tmp3);
      PL_srcA_buf0[i] = tmp0;
      PL_srcA_buf1[i] = tmp02;
      PS_srcA_buf[i] = tmp03;
      PL_dstC_buf0[i] = tmp03;
    }  
#endif

    bool success = true;
    // printf("Enter to start DMA transaction\n");

    pim_args *set_info;
    int size = sizeof(pim_args);
    set_info = (pim_args *)malloc(size);

    if(cmd==0){
//         clock_gettime(CLOCK_MONOTONIC, &start_FPGA2FPGA);

//         int rc = datacopy_pim2pim(PL_dstC_buf0, PL_srcA_buf1, srcA_size*sizeof(short), 0);
//         // int rc = datacopy_pim2pim(PL_dstC_buf1, PL_srcA_buf0, srcA_size*sizeof(short), 1);
//         if (rc < 0){
//             perror("FPGA2FPGA data copy");
//             exit(-1);
//         }
        
//         clock_gettime(CLOCK_MONOTONIC, &end_FPGA2FPGA);

//         diff_FPGA2FPGA = BILLION * (end_FPGA2FPGA.tv_sec - start_FPGA2FPGA.tv_sec) + (end_FPGA2FPGA.tv_nsec - start_FPGA2FPGA.tv_nsec);
//         printf("XDMA FPGA --> FPGA execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2FPGA, p_size, q_size);
// #ifdef TO_CSV
//         std::ofstream outFile("./perf/XDMA_FPGA2FPGA_latency.csv", std::ios::app);
//         outFile << diff_FPGA2FPGA << std::endl;
//         outFile.close();
// #endif
//         // printf("XDMA FPGA --> FPGA execution time %llu us %d %d\n", (long long unsigned int) diff_FPGA2FPGA/1000, p_size, q_size);
// #ifdef CHECK
//         printf("Correctness FPGA0 --> FPGA1 XDMA check!\n\n");
//         for(int i=0; i<dstC_size; i++){ 
//             if(PL_srcA_buf1[i] != PL_dstC_buf0[i]) {
//                 printf("Error PL_src[%d]=%d PL_dst[%d]=%d \n",i,PL_srcA_buf1[i],i,PL_dstC_buf0[i]);
//                 success = false;
//                 break;
//             }
//             else{
//                 // printf("idx[%4d]   %hu  |  ", i, PL_srcA_buf1[i]);
//                 // printf("  %hu ", PL_dstC_buf0[i]);
//                 // printf("\n");
//             }
//         }
//         if (!success)
//             printf("Correctness FPGA0 --> FPGA1 check failed!\n\n");
//         else
//             printf("Correctness FPGA0 --> FPGA1 check done!\n\n");
// #endif
//     } else if(cmd==1){
//         clock_gettime(CLOCK_MONOTONIC, &start_FPGA2HOST);
//         // for(int i=0; i<dstC_size; i++){ 
//         //     printf("Error PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
//         // }        
//         // int rc = datacopy_pim2cpu(PS_dstC_buf, PL_srcA_buf1, srcA_size*sizeof(short), SRC_FPGA);
//         int rc = datacopy_pim2cpu(PS_dstC_buf, PL_srcA_buf0, srcA_size*sizeof(short), SRC_FPGA);
//         if (rc < 0){
//             perror("FPGA2HOST data copy");
//             exit(-1);
//         }

//         clock_gettime(CLOCK_MONOTONIC, &end_FPGA2HOST);

//         diff_FPGA2HOST = BILLION * (end_FPGA2HOST.tv_sec - start_FPGA2HOST.tv_sec) + (end_FPGA2HOST.tv_nsec - start_FPGA2HOST.tv_nsec);
//         printf("XDMA FPGA --> HOST execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2HOST, p_size, q_size);
//         // printf("XDMA FPGA --> HOST execution time %llu us %d %d\n", (long long unsigned int) diff_FPGA2HOST/1000, p_size, q_size);
       
// #ifdef CHECK
//         printf("Correctness FPGA --> HOST XDMA check!\n\n");
//         printf("            FPGA   |    HOST\n");
//         for(int i=0; i<dstC_size; i++){ 
//             if(PL_srcA_buf0[i] != PS_dstC_buf[i]) {
//                 printf("Error PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
//                 success = false;
//                 // break;
//             }
//             // else{
//             //     printf("idx[%4d]   %hu  |  ", i, PL_srcA_buf0[i]);
//             //     printf("  %hu ", PS_dstC_buf[i]);
//             //     printf("\n");
//             // }
//         }
//         if (!success)
//             printf("Correctness FPGA --> HOST check failed!\n\n");
//         else
//             printf("Correctness FPGA --> HOST check done!\n\n");
// #endif
    }  else if(cmd==1){
        clock_gettime(CLOCK_MONOTONIC, &start_FPGA2HOST);
        // for(int i=0; i<dstC_size; i++){ 
        //     printf("Error PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
        // }        
        // int rc = datacopy_pim2cpu(PS_dstC_buf, PL_srcA_buf1, srcA_size*sizeof(short), SRC_FPGA);
        printf("HOST START ADDR: %llx\n", PS_dstC_buf);
        int rc = datacopy_pim2cpu(PS_dstC_buf, PL_srcA_buf0, srcA_size*sizeof(short), 0);
        if (rc < 0){
            perror("FPGA2HOST data copy");
            exit(-1);
        }

        clock_gettime(CLOCK_MONOTONIC, &end_FPGA2HOST);

        diff_FPGA2HOST = BILLION * (end_FPGA2HOST.tv_sec - start_FPGA2HOST.tv_sec) + (end_FPGA2HOST.tv_nsec - start_FPGA2HOST.tv_nsec);
        printf("XDMA FPGA --> HOST execution time %llu nanoseconds %d %d\n", (long long unsigned int) diff_FPGA2HOST, p_size, q_size);
        // printf("XDMA FPGA --> HOST execution time %llu us %d %d\n", (long long unsigned int) diff_FPGA2HOST/1000, p_size, q_size);

#ifdef CHECK
        printf("Correctness FPGA --> HOST XDMA check!\n\n");
        printf("            FPGA   |    HOST\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PL_srcA_buf0[i] != PS_dstC_buf[i]) {
                printf("Error PL_src[%d]=%d PS_dst[%d]=%d \n",i,PL_srcA_buf0[i],i,PS_dstC_buf[i]);
                success = false;
            }
        }
        if (!success)
            printf("Correctness FPGA --> HOST check failed!\n\n");
        else
            printf("Correctness FPGA --> HOST check done!\n\n");
#endif
    } else if(cmd==2){

        clock_gettime(CLOCK_MONOTONIC, &start_HOST2FPGA);

        int rc = datacopy_cpu2pim(PL_dstC_buf1, PS_srcA_buf, srcA_size*sizeof(short), 1);
        if (rc < 0){
            perror("HOST2FPGA data copy");
            exit(-1);
        }
    
        clock_gettime(CLOCK_MONOTONIC, &end_HOST2FPGA);

        diff_HOST2FPGA = BILLION * (end_HOST2FPGA.tv_sec - start_HOST2FPGA.tv_sec) + (end_HOST2FPGA.tv_nsec - start_HOST2FPGA.tv_nsec);
        printf("XDMA HOST --> FPGA execution time %llu us %d %d\n", (long long unsigned int) diff_HOST2FPGA/1000, p_size, q_size);
     
#ifdef CHECK
        printf("Correctness HOST --> FPGA XDMA check!\n\n");
        printf("            HOST   |    FPGA\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PS_srcA_buf[i] != PL_dstC_buf1[i]) {
                printf("idx[%4d]   %hu  |  ", i, PS_srcA_buf[i]);
                printf("  %hu ", PL_dstC_buf1[i]);
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
    } else if(cmd==3){
        
        short** pim_array = (short**)malloc(sizeof(short*)*MAX_PIM);
        pim_array[0] = PL_dstC_buf0;
        pim_array[1] = PL_dstC_buf1;
        clock_gettime(CLOCK_MONOTONIC, &start_HOST2FPGA);
        hostBcast(pim_array, PS_srcA_buf, srcA_size*sizeof(short));
        clock_gettime(CLOCK_MONOTONIC, &end_HOST2FPGA); 

        diff_HOST2FPGA = BILLION * (end_HOST2FPGA.tv_sec - start_HOST2FPGA.tv_sec) + (end_HOST2FPGA.tv_nsec - start_HOST2FPGA.tv_nsec);
        printf("XDMA HOST --> FPGA Broadcasting time %llu us %d %d\n", (long long unsigned int) diff_HOST2FPGA/1000, p_size, q_size);
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
        }
        for(int i=0; i<dstC_size; i++){ 
            if(PS_srcA_buf[i] != PL_dstC_buf1[i]) {
                printf("idx[%4d]   %hu  |  ", i, PS_srcA_buf[i]);
                printf("  %hu ", PL_dstC_buf1[i]);
                printf("\n");
                success = false;
                break;
            }
        }
        if (!success)
            printf("Correctness HOST --> FPGA check failed!\n\n");
        else
            printf("Correctness HOST --> FPGA check done!\n\n");
#endif
    } else if(cmd==4){
        
        short** pim_arr = (short**)malloc(sizeof(short*)*MAX_PIM);
        pim_arr[0] = PL_dstC_buf0;
        pim_arr[1] = PL_dstC_buf1;
        
        clock_gettime(CLOCK_MONOTONIC, &start_HOST2FPGA);
        pimBcast(pim_arr, PL_srcA_buf0, 0, srcA_size*sizeof(short));
        clock_gettime(CLOCK_MONOTONIC, &end_HOST2FPGA); 

        diff_HOST2FPGA = BILLION * (end_HOST2FPGA.tv_sec - start_HOST2FPGA.tv_sec) + (end_HOST2FPGA.tv_nsec - start_HOST2FPGA.tv_nsec);
        printf("XDMA FPGA --> FPGA Broadcasting time %llu us %d %d\n", (long long unsigned int) diff_HOST2FPGA/1000, p_size, q_size);
    
#ifdef CHECK     
        printf("Correctness FPGA --> FPGA XDMA check!\n\n");
        printf("            FPGA   |    FPGA\n");
        for(int i=0; i<dstC_size/2; i++){ 
            if(PL_dstC_buf1[i] != PL_srcA_buf0[i]) {
                printf("idx[%4d]   %hu  |  ", i, PL_dstC_buf1[i]);
                printf("  %hu ", PL_srcA_buf0[i]);
                printf("\n");
                success = false;
                break;
            }
        }
        if (!success)
            printf("Correctness FPGA --> FPGA check failed!\n\n");
        else
            printf("Correctness FPGA --> FPGA check done!\n\n");
#endif
    } else if(cmd==5){
        
        short** pim_dsts = (short**)malloc(sizeof(short*)*MAX_PIM);
        pim_dsts[0] = PL_dstC_buf0;
        pim_dsts[1] = PL_dstC_buf1;
        
        clock_gettime(CLOCK_MONOTONIC, &start_HOST2FPGA);
        pimScatter(pim_dsts, PL_srcA_buf0, 0, srcA_size*sizeof(short));
        clock_gettime(CLOCK_MONOTONIC, &end_HOST2FPGA); 

        diff_HOST2FPGA = BILLION * (end_HOST2FPGA.tv_sec - start_HOST2FPGA.tv_sec) + (end_HOST2FPGA.tv_nsec - start_HOST2FPGA.tv_nsec);
        printf("XDMA FPGA --> FPGA Broadcasting time %llu us %d %d\n", (long long unsigned int) diff_HOST2FPGA/1000, p_size, q_size);
    
#ifdef CHECK     
        printf("Correctness FPGA --> FPGA XDMA check!\n\n");
        printf("            FPGA   |    FPGA\n");
        for(int i=0; i<dstC_size/2; i++){ 
            int half_i = i+dstC_size/2;     // temporarily hard coding
            if(PL_dstC_buf1[i] != PL_srcA_buf0[half_i]) {
                printf("idx[%4d]   %hu  |  ", i, PL_dstC_buf1[i]);
                printf("  %hu ", PL_srcA_buf0[half_i]);
                printf("\n");
                success = false;
                break;
            }
            if(PL_dstC_buf0[i] != PL_srcA_buf0[i]) {
                printf("idx[%4d]   %hu  |  ", i, PL_dstC_buf0[i]);
                printf("  %hu ", PL_srcA_buf0[i]);
                printf("\n");
                success = false;
                break;
            }
        }
        if (!success)
            printf("Correctness FPGA --> FPGA check failed!\n\n");
        else
            printf("Correctness FPGA --> FPGA check done!\n\n");
#endif
    } else if(cmd==6){
        
        short** pim_arr = (short**)malloc(sizeof(short*)*MAX_PIM);
        pim_arr[0] = PL_dstC_buf0;
        pim_arr[1] = PL_dstC_buf1;
        
        pimScatter(pim_arr, PL_srcA_buf0, 0, srcA_size*sizeof(short));
        pimGather(PL_dstT_buf0, pim_arr, 0, srcA_size*sizeof(short));

    
#ifdef CHECK     
        printf("Correctness FPGA --> FPGA XDMA check!\n\n");
        printf("            FPGA   |    FPGA\n");
        for(int i=0; i<dstC_size; i++){ 
            if(PL_srcA_buf0[i] != PL_dstT_buf0[i]) {
                printf("idx[%4d]   %hu  |  ", i, PL_srcA_buf0[i]);
                printf("  %hu ", PL_dstT_buf0[i]);
                printf("\n");
                success = false;
                break;
            }
        }
        if (!success)
            printf("Correctness FPGA --> FPGA check failed!\n\n");
        else
            printf("Correctness FPGA --> FPGA check done!\n\n");
#endif
    }
    else{
        printf("CMD ERROR!\n");
    }
    return 0;
}


