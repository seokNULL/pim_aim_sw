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
// #define  DUMP_BANK0_ONLY
#include <pthread.h>

#define FPGA_ID 0
#define ITER  1
// #define DECOUP
#define NUM_BANK 16
#define REG_SIZE 16

struct timespec start_PIM, end_PIM;
struct timespec start_CPU, end_CPU;
uint64_t diff_PIM;
uint64_t diff_CPU;
int iter;

typedef struct 
{
  /* data */
  int tid;
  pim_args* pim_code;
  short* cpu_buff;
  short* pim_buff;
  int fpga_id;
} ThreadArgs;

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

void pim_align_chunk(const short *src, short *dst, int row_dim, int col_dim) {
    int CHUNK = 256;
    int col_chunk_num = 0;
    if (col_dim % 256 != 0) {
        col_chunk_num = 256 * (col_dim / 256 + 1) / CHUNK;
    } else {
      col_chunk_num = col_dim / CHUNK;
    }
    
    // int col_chunk_num =  512 * (col_dim / 512 + 1) / CHUNK; // CHUNK = 512
    int dest_idx = 0;
    if (col_dim > CHUNK) {
        for (int i = 0; i < col_chunk_num; i++) {
            for (int j = 0; j < row_dim; j++) {
                if (i < col_chunk_num - 1) {
                    for (int k = 0; k < CHUNK; k++) {
                        dst[dest_idx] = (src[(i*CHUNK)+(j*col_dim)+k]);
                        // std::cout << "dest_idx: "<< dest_idx << "\tsrc_index: " << (i*CHUNK)+(j*col_dim)+k << std::endl;
                        dest_idx++;
                    }
                }
                // LAST COL NUM
                else {
                    for (int k = 0; k < CHUNK; k++) {
                        if (i * CHUNK + k < col_dim) {
                            dst[dest_idx] = (src[(i*CHUNK)+(j*col_dim)+k]);
                            // std::cout << "dest_idx: "<< dest_idx << "\tsrc_index: " << (i*CHUNK)+(j*col_dim)+k << std::endl;
                        } else {
                            // printf("ZERO padding");
                            dst[dest_idx] = (0);
                        }
                        dest_idx++;
                    }
                }
            }
        }
        // std::cout << "dest_idx: " << dest_idx << std::endl;
    } else if (col_dim == CHUNK) {
        for (int i = 0; i < row_dim * col_dim; i++) {
            dst[i] = (src[i]);
        }
    } else {
        printf("NOT SUPPORTED, NEED ALIGNMENT");
    }
}

void* matmul_overlap(void* thread_args) {
  ThreadArgs* args = (ThreadArgs*) thread_args;
  int tid = args->tid;
  int fpga_id = args->fpga_id;
  pim_args* pim_info = args->pim_code;
  int size = (pim_info->p_size)*(pim_info->q_size)*sizeof(short);
  short* srcA = (short*)pim_info->srcA_va;
  short* srcB = (short*)pim_info->srcB_va;
  if (tid == 0) {
    usleep(10);
#ifndef DECOUP
    matmul(pim_info, fpga_id); 
#else
    matmul_tiled(pim_info, fpga_id);
#endif 
  }
  else {
    args->cpu_buff = (short *)(mmap(NULL, size, PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    int other_id = fpga_id == 0 ? 1 : 0;
    args->pim_buff = (short *)pim_malloc(size, other_id);
    short* temp = (short*)pim_malloc(size, fpga_id);
    printf("temp PA: %x\n", VA2PA((uint64_t)&temp[0], fpga_id)); 
    // for (int i = 0; i < 10000; i++) datacopy_pim2cpu(args->cpu_buff, (void*)pim_info->srcA_va, size, 0);
    // datacopy_pim2pim((void*)args->pim_buff, (void*)pim_info->srcA_va, size, other_id);
    
      // const size_t block_size = 2048;
      // size_t blocks = size / block_size;
      // size_t remaining = size % block_size;
      // for(int i = 0; i < 10; i ++) {
      //   for(size_t i = 0; i < blocks; i++) {
      //     datacopy_pim2pim((void*)args->pim_buff+i*block_size, (void*)srcA+i*block_size, block_size, other_id);
      //   }
      //   // Copy remaining bytes
      //   if (remaining > 0) {
      //     datacopy_pim2pim((void*)args->pim_buff+ blocks*block_size, (void*)srcA+blocks*block_size, remaining, other_id);
      //   }
      // }
    
    
    for (int i = 0; i < 10; i++) {
      datacopy_pim2pim((void*)args->pim_buff, (void*)srcA, size, other_id);
    //   // datacopy_pim2pim((void*)args->pim_buff, (void*)srcB, size, other_id);
    //   // datacopy_pim2cpu(args->cpu_buff, (void*)pim_info->srcA_va, size, fpga_id);
    //   // datacopy_pim2cpu(args->cpu_buff, temp, size, fpga_id);
    //   // usleep(1);
    //   // datacopy_pim2cpu((void*)args->cpu_buff, (void*)pim_info->srcA_va, size, other_id);
    //   // for (int i = 0; i < 16; i++) args->cpu_buff[i] = srcA[0];
    //   // for (int i = 0; i < (pim_info->p_size)*(pim_info->q_size); i++) args->pim_buff[i] = srcA[i];
    //   // for (int i = 0; i < (pim_info->r_size)*(pim_info->q_size); i++) args->pim_buff[i] = srcB[2048];
    //   // for (int i = 0; i < (pim_info->p_size)*(pim_info->q_size); i++) args->cpu_buff[i] = srcA[i];
    //   // for (int i = 0; i < (pim_info->p_size)*(pim_info->q_size); i++) args->cpu_buff[i] = srcB[i];
    }
    
  }
}

int main(int argc, char *argv[])
{

    if(argc<3)
    {
        printf("Check matrix param p,q,r (pxq) x (qxr) = (pxr)\n");
        exit(1);
    }
    int p_size=atoi(argv[1]);
    int q_size=atoi(argv[2]);
    int r_size=atoi(argv[3]);
    int srcA_size=p_size*q_size;
    int srcB_size=q_size*r_size;
    int dstC_size=p_size*r_size;
    int fpga_id=atoi(argv[4]);
    // const int fpga_id = 0;

    int tmp=0;
    // int fd_dma=0;
    bool is_need_align=false;
    printf("TEST FPGA NUM: %d\n", fpga_id);
    init_pim_drv();

    //For CPU verify
    float *src_A_DRAM = (float *)(mmap(NULL, srcA_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *src_B_DRAM = (float *)(mmap(NULL, srcB_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *dst_C_DRAM = (float *)(mmap(NULL, dstC_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    srand((unsigned int)time(NULL));  // For reset random seed

    void *srcA;
    void *srcB;
    void *dstC;

    uint64_t srcA_va;
    uint64_t srcB_va;
    uint64_t dstC_va;

    pim_args *set_info;
    int size = sizeof(pim_args);
    set_info = (pim_args *)malloc(1024*1024*size);

    short *PL_srcA_buf = (short *)pim_malloc(srcA_size*sizeof(short), fpga_id);
    short *PL_srcB_buf = (short *)pim_malloc(srcB_size*sizeof(short), fpga_id);
    short *PL_dstC_buf = (short *)pim_malloc(dstC_size*sizeof(short), fpga_id);

    // int other_id = fpga_id == 0? 1 : 0;
    // short *Another_PIM_buf = (short *)pim_malloc(dstC_size*sizeof(short), other_id);
  

    if (PL_srcA_buf == NULL) {
        printf("PL srcA call failure.\n");
        return -1;
    }
    if (PL_srcB_buf == NULL) {
        printf("PL srcB call failure.\n");
        return -1;
    }
    if (PL_dstC_buf == NULL) { 
        printf("PL dstC call failure.\n");
        return -1;
    }

    // printf("Complete pim_malloc\n");
    uint64_t A_pa = VA2PA((uint64_t)&PL_srcA_buf[0], fpga_id);    
    uint64_t B_pa = VA2PA((uint64_t)&PL_srcB_buf[0], fpga_id);    
    uint64_t C_pa = VA2PA((uint64_t)&PL_dstC_buf[0], fpga_id);    

    printf("A_pa: %x\n", A_pa);
    printf("B_pa: %x\n", B_pa);
    printf("C_pa: %x\n", C_pa);

    for(size_t i=0; i<dstC_size; i++){
      PL_dstC_buf[i]=1;
      dst_C_DRAM[i] = 0;
    }

    // printf("srcA init\n");
    float temp  = 0.05;
    for(size_t i=0; i<srcA_size; i++){
      // temp  = generate_random();
      // temp  = generate_random()/10;
      // if (i%16 == 0) temp += 1;
      short tmp0 = float_to_short(temp);
      PL_srcA_buf[i] = tmp0;
      src_A_DRAM[i] = short_to_float(tmp0);
    }

    // printf("srcB init (%d)\n", srcB_size);
    float temp1  = 0.03125;
    for(size_t i=0; i<srcB_size; i++){
      // temp1 = generate_random_255()/10;
      temp1  = generate_random();
      // if (i%256 == 0) temp1 += 0.0125;
      // if (i%(16*256) == 0) temp1 = 0.001;
      // float temp1  = 0.02;
      short tmp0 = float_to_short(temp1);
      PL_srcB_buf[i]=tmp0;
      src_B_DRAM[i] = short_to_float(tmp0);
    }    
    
    short *PL_srcB_aligned_buf = (short *)pim_malloc(srcB_size*sizeof(short), fpga_id);

      if (PL_srcB_aligned_buf == NULL) {
        printf("PL srcB call failure.\n");
        return -1;
      }
    // if((r_size%256!=0)&&(r_size>256)){
    if(r_size>256){
      is_need_align = true;
      for(size_t i=0; i<srcB_size; i++){
        PL_srcB_aligned_buf[i]=0;
      }
      pim_align_chunk(PL_srcB_buf, PL_srcB_aligned_buf, q_size, r_size);
    }
  
    //check CPU result
    // printf("check cpu mat mul\n");
    
    mat_mul_CPU(src_A_DRAM, src_B_DRAM, dst_C_DRAM, p_size, q_size, r_size);

#ifndef DECOUP
    set_info->srcA_va   = (uint64_t)&PL_srcA_buf[0];
    set_info->srcB_va   = is_need_align? (uint64_t)&PL_srcB_aligned_buf[0]:(uint64_t)&PL_srcB_buf[0];
    set_info->dstC_va   = (uint64_t)&PL_dstC_buf[0];
    set_info->p_size    = p_size;
    set_info->q_size    = q_size;
    set_info->r_size    = r_size;   
#else
    set_info->srcA_va   = (uint64_t)&PL_srcB_buf[0]; //Must be swapped
    set_info->srcB_va   = (uint64_t)&PL_srcA_buf[0]; //Must be swapped
    set_info->dstC_va   = (uint64_t)&PL_dstC_buf[0]; 
    set_info->p_size    = p_size;
    set_info->q_size    = q_size;
    set_info->r_size    = r_size; 
#endif 
    uint64_t Comp_B_pa = VA2PA(set_info->srcB_va, fpga_id);    
    printf("Comp_B_pa: %x\n", Comp_B_pa);
  
    pthread_t threads[2];
    ThreadArgs thread_args[2];

    for(int i = 0; i < 2; i++) {
      thread_args[i].pim_code = set_info;
      thread_args[i].tid = i;
      thread_args[i].fpga_id = fpga_id;
      pthread_create(&threads[i], NULL, matmul_overlap, (void *)&thread_args[i]);
    }
    for(int i = 0; i < 2; i++) {
      pthread_join(threads[i], NULL);
    }
    // matmul(set_info, fpga_id);
    short *C_ans = (short *)pim_malloc(dstC_size*sizeof(short), fpga_id);
#ifdef DECOUP
  int A_ROW = p_size;
  int A_COL = q_size;
  int B_COL = r_size;
  int A_ROW_PAD, A_COL_PAD, B_COL_PAD;
  A_ROW_PAD = (A_ROW + REG_SIZE - 1) / REG_SIZE * REG_SIZE; 
  A_COL_PAD = (A_COL + REG_SIZE - 1) / REG_SIZE * REG_SIZE;
  B_COL_PAD = (B_COL + NUM_BANK - 1) / NUM_BANK * NUM_BANK;

  int cnt = 0;
//Result layout     
  for (size_t io = 0; io < A_ROW_PAD; io += REG_SIZE) {
    for (size_t j = 0; j < B_COL; j++) {
      for (size_t ii = 0; ii < REG_SIZE; ii++) {
        short tmp = PL_dstC_buf[cnt];
        C_ans[(io + ii) * B_COL + j] = tmp;
        cnt += 1;
        // printf("cnt:%d & dest:%d\n",cnt, (io + ii) * B_COL + j);
        }
        
      }
  }
#endif   

    union converter{
    float f_val;
    unsigned int u_val;
    };
    union converter a;
    union converter b;
    bool fail_flag = false;
    printf("Correctness check!\n\n");
    printf("            CPU       |    PIM\n");
    for(int i=0; i<dstC_size; i++){ 
        a.f_val = dst_C_DRAM[i];

        #ifdef DECOUP
        printf("idx[%4d] 0x%x  |  ", i, a.u_val>>16);
        printf("0x%x ", C_ans[i]);
        printf("|  Diff: %.5f", (a.f_val - short_to_float(C_ans[i])));
        
        if ((a.f_val - short_to_float(C_ans[i])) > 0.035 || (a.f_val - short_to_float(C_ans[i])) < -0.035) {
          fail_flag = true; 
          printf("    <================================");
        }

        if (C_ans[i] == 0) {
          fail_flag = true; 
          printf("    <================================");
        }

        #else
        printf("idx[%4d] 0x%x  |  ", i, a.u_val>>16);
        printf("0x%x ", PL_dstC_buf[i]);
        printf("|  Diff: %.5f\n", (a.f_val - short_to_float(PL_dstC_buf[i])));
        
        if (((a.f_val - short_to_float(PL_dstC_buf[i])) > 0.035 || (a.f_val - short_to_float(PL_dstC_buf[i])) < -0.035) || PL_dstC_buf[i] == 0) {
        // if (PL_dstC_buf[i] == 0) {
          // printf("idx[%4d] 0x%x  |  ", i, a.u_val>>16);
          // printf("0x%x ", PL_dstC_buf[i]);
          // printf("|  Diff: %.5f", (a.f_val - short_to_float(PL_dstC_buf[i])));
          fail_flag = true; 
          // printf("    <================================");
          // printf("\n");
        }
        #endif
    }
    printf("\n");

    // printf("P2P Copy Correctness check!\n\n");
    // for(int i=0; i<srcA_size; i++){ 
    //   printf("idx[%4d] 0x%x  |  ", i, PL_srcA_buf[i]);
    //   printf("0x%x\n", thread_args[1].pim_buff[i]);

    //     if (PL_srcA_buf[i] != thread_args[1].pim_buff[i]) {
    //       fail_flag = true; 
    //       printf("    <================================");
    //     }
    // }

    if (fail_flag) printf("FAILED\n\n");
    else printf("DONE\n");



    pim_free(PL_srcA_buf, fpga_id);
    pim_free(PL_srcB_buf, fpga_id);
    pim_free(PL_dstC_buf, fpga_id);
    pim_free(PL_srcB_aligned_buf, fpga_id);
    return 0;
}



