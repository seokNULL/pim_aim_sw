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

#include <pthread.h>

#define MAX_PIM 2


void *pcclCopy_cpu2pim(void *copy_args) {
    struct args* args = (struct args*) copy_args;
    short** pim_dsts = args->pim_dsts;
    short* cpu_src = args->cpu_src;
    int size = args->size;
    int fpga_id = args->fpga_id;
    // printf("============ Thread info ============\n");
    // printf("tid : %d\n", fpga_id);
    // printf("pim_dst[%d] : %llx\n", fpga_id, pim_dsts[fpga_id]);
    // printf("cpu_src[%d] : %llx\n", fpga_id, cpu_src);
    // printf("size : %d\n", size);
    datacopy_cpu2pim(pim_dsts[fpga_id], cpu_src, size, fpga_id);
}

void *_thread_bcast(void *copy_args) {
    struct args* args = (struct args*) copy_args;
    short** pim_dsts = args->pim_dsts;
    int size = args->size;
    int src_id = args->src_id;
    int fpga_id = args->fpga_id;
    short* pim_src = args->pim_src;
    if (fpga_id == src_id) {
        for(int i = 0; i < size/sizeof(short); i++) {
            pim_dsts[src_id][i] = pim_src[i];
        }
        pthread_exit(0);
    }

    // printf("============ Thread info ============\n");
    // printf("tid : %d\n", fpga_id);
    // printf("pim_dst[%d] : %llx\n", fpga_id, pim_arr[fpga_id]);
    // printf("cpu_src[%d] : %llx\n", fpga_id, pim_arr[src_id]);
    // printf("size : %d\n", size);
    
    datacopy_pim2pim(pim_dsts[fpga_id], pim_src, size, fpga_id);
    // datacopy_pim2pim(pim_arr[fpga_id], pim_arr[src_id], size, fpga_id);
}

void *_thread_scatter(void *copy_args) {
    struct args* args = (struct args*) copy_args;
    short** pim_dsts = args->pim_dsts;
    short* pim_src = args->pim_src;
    int size = args->size;
    int src_id = args->src_id;
    int fpga_id = args->fpga_id;
    int each_size = size/MAX_PIM;
    if (fpga_id == src_id) {
        // short* buff = (short*)malloc(each_size);
        // datacopy_pim2cpu(buff, pim_src, each_size, fpga_id);
        // datacopy_cpu2pim(pim_dsts[src_id], buff, each_size, fpga_id);
        // free(buff);
        for(int i = 0; i < each_size/sizeof(short); i++) {
            pim_dsts[src_id][i] = pim_src[i];
        }
        pthread_exit(0);  
    } 

    // printf("============ Thread info ============\n");
    // printf("tid : %d\n", fpga_id);
    // printf("size : %d\n", size);
    short* src_offset = (short*)((void*)pim_src + each_size*fpga_id);
    
    datacopy_pim2pim(pim_dsts[fpga_id], src_offset, each_size, fpga_id);
    // datacopy_pim2pim(pim_arr[fpga_id], pim_arr[src_id], size, fpga_id);
}

void *_thread_gather(void *copy_args) {
    struct args* args = (struct args*) copy_args;
    short** pim_srcs = args->pim_srcs;
    short* pim_dst = args->pim_dst;
    int size = args->size;
    int src_id = args->src_id;
    int fpga_id = args->fpga_id;
    int each_size = size/MAX_PIM;
    if (fpga_id == src_id) {
        // short* buff = (short*)malloc(each_size);
        // datacopy_pim2cpu(buff, pim_srcs[src_id], each_size, fpga_id);
        // datacopy_cpu2pim(pim_dst, buff, each_size, fpga_id);
        // free(buff);
        for(int i = 0; i < each_size/2; i++) {
            pim_dst[i] = pim_srcs[src_id][i];
            // printf("pim_dst[%d]: %hu\n", i, pim_dst[i]);
        }
        pthread_exit(0);  
    } 

    // printf("============ Thread info ============\n");
    // printf("tid : %d\n", fpga_id);
    // printf("size : %d\n", size);
    short* dst_offset = (short*)((void*)pim_dst + each_size*fpga_id);
    
    datacopy_pim2pim(dst_offset, pim_srcs[fpga_id], each_size, src_id);
    // datacopy_pim2pim(pim_arr[fpga_id], pim_arr[src_id], size, fpga_id);
}
/* pim_src: Broadcast의 destination PIM 주소들로 구성된 array */
void hostBcast(short** pim_dsts, short* cpu_src, int size) {
    int rc;
    struct args copy_args[MAX_PIM];
    // struct args *copy_args[M] = (struct args*)malloc(sizeof(struct args));
    pthread_t threads[MAX_PIM];
    
    
    for(int i = 0; i < MAX_PIM; i++) {
        copy_args[i].pim_dsts = pim_dsts;
        copy_args[i].cpu_src = cpu_src;
        copy_args[i].size = size;
        copy_args[i].fpga_id = i;
        rc = pthread_create(&threads[i], NULL, pcclCopy_cpu2pim, (void *)&copy_args[i]);
        // rc = pthread_create(&threads[i], NULL, pcclCopy_cpu2pim, (void *)copy_args);
        if (rc) {
            printf("Error:unable to create thread, %d", rc);
            exit(-1);
        }
    }
    for(int i = 0; i < MAX_PIM; i++) {
        pthread_join(threads[i], NULL);
    }

}

void pimBcast(short** pim_dsts, short* pim_src, int src_id, int size) {
    int rc;
    struct args copy_args[MAX_PIM];
    // struct args *copy_args[M] = (struct args*)malloc(sizeof(struct args));
    pthread_t threads[MAX_PIM];
    
    
    for(int i = 0; i < MAX_PIM; i++) {
        copy_args[i].pim_dsts = pim_dsts;
        copy_args[i].pim_src = pim_src;
        copy_args[i].size = size;
        copy_args[i].src_id = src_id;
        copy_args[i].fpga_id = i;
        rc = pthread_create(&threads[i], NULL, _thread_bcast, (void *)&copy_args[i]);
        // rc = pthread_create(&threads[i], NULL, pcclCopy_cpu2pim, (void *)copy_args);
        if (rc) {
            printf("Error:unable to create thread, %d", rc);
            exit(-1);
        }
    }
    for(int i = 0; i < MAX_PIM; i++) {
        pthread_join(threads[i], NULL);
    }
}

void pimScatter(short** pim_dsts, short* pim_src, int src_id, int size) {
    int rc;
    struct args copy_args[MAX_PIM];
    pthread_t threads[MAX_PIM];
    
    
    for(int i = 0; i < MAX_PIM; i++) {
        copy_args[i].pim_dsts = pim_dsts;
        copy_args[i].pim_src = pim_src;
        copy_args[i].size = size;
        copy_args[i].src_id = src_id;
        copy_args[i].fpga_id = i;
        rc = pthread_create(&threads[i], NULL, _thread_scatter, (void *)&copy_args[i]);
        // rc = pthread_create(&threads[i], NULL, pcclCopy_cpu2pim, (void *)copy_args);
        if (rc) {
            printf("Error:unable to create thread, %d", rc);
            exit(-1);
        }
    }
    for(int i = 0; i < MAX_PIM; i++) {
        pthread_join(threads[i], NULL);
    }
}

void pimGather(short* pim_dst, short** pim_srcs, int src_id, int size) {
    int rc;
    struct args pccl_args[MAX_PIM];
    pthread_t threads[MAX_PIM];
    
    
    for(int i = 0; i < MAX_PIM; i++) {
        pccl_args[i].pim_srcs = pim_srcs;
        pccl_args[i].pim_dst = pim_dst;
        pccl_args[i].size = size;
        pccl_args[i].src_id = src_id;
        pccl_args[i].fpga_id = i;
        rc = pthread_create(&threads[i], NULL, _thread_gather, (void *)&pccl_args[i]);
        // rc = pthread_create(&threads[i], NULL, pcclCopy_cpu2pim, (void *)copy_args);
        if (rc) {
            printf("Error:unable to create thread, %d", rc);
            exit(-1);
        }
    }
    for(int i = 0; i < MAX_PIM; i++) {
        pthread_join(threads[i], NULL);
    }
}
