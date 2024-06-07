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

#define FPGA_ID 0
#define ITER  1
// #define DECOUP

struct timespec start_PIM, end_PIM;
struct timespec start_CPU, end_CPU;
uint64_t diff_PIM;
uint64_t diff_CPU;
int iter;

// #ifdef DECOUP

// void matmul_decoupled_pim_initialize(float *A, float *B, short *pim_A, short *pim_B, size_t M, size_t K, size_t N) {
//   int p_size = M;
//   int q_size = K;
//   int r_size = N;
//   if(p_size <16){
//     perror("Tiling factor of decoupled-PIM is p_size>16");
//   }

//   int A_ROW = p_size;
//   int A_COL = q_size;
//   int B_COL = r_size;
//   int A_ROW_PAD, A_COL_PAD, B_COL_PAD;
//   A_ROW_PAD = (A_ROW + REG_SIZE - 1) / REG_SIZE * REG_SIZE;
//   A_COL_PAD = (A_COL + REG_SIZE - 1) / REG_SIZE * REG_SIZE;
//   B_COL_PAD = (B_COL + NUM_BANK - 1) / NUM_BANK * NUM_BANK;
//   int cnt = 0;
//   size_t idx = 0;

//  //Input matrix A tiling
//     for (size_t io = 0; io < A_ROW_PAD; io += REG_SIZE) {
//       for (size_t k = 0; k < A_COL_PAD; k++) {
//         for (size_t ii = 0; ii < REG_SIZE; ii++) {
//             float tmp =A[(io + ii) * A_COL + k];
//             short tmp0 = float_to_short(tmp);
//               if (io + ii < A_ROW && k < A_COL) {
//                 pim_A[idx] = tmp0;
//               }
//               else {
//                 pim_A[idx] = 0;
//               }
//             cnt += 1;
//             idx += 1;
//         }
//       }
//     }
// //Weight matrix B tiling
//     cnt = 0;
//     for (size_t jo = 0; jo < B_COL_PAD; jo += NUM_BANK) {
//       for (size_t ko = 0; ko < A_COL_PAD; ko += NUM_BANK) {
//         for (size_t ji = 0; ji < REG_SIZE; ji++) {
//           for (size_t ki = 0; ki < REG_SIZE; ki++) {
//             float tmp = B[(ko + ki) * B_COL + (jo + ji)];
//             short tmp0 = float_to_short(tmp);
//               if (ko + ki < A_COL && jo + ji < B_COL) {
//                   pim_B[cnt] = tmp0;
//               }
//               else {
//                   pim_B[cnt] = 0;
//               }
//             cnt += 1;
//           }
//         }
//       }
//     }
// }
// #endif

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
  
  // Naive CPU matrix multiplication(ubench)
  // int M = p_size;
  // int K = q_size;
  // int N = r_size;
  // for (int i = 0; i < M; ++i) {
  //   for (int k = 0; k < K; ++k) {
  //     for (int j = 0; j < N; ++j) {
  //       dst_C_DRAM[i * N + j] += src_A_DRAM[i * K + k] * src_B_DRAM[k * N + j];
  //     }
  //   }
  // }
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
    // printf("A_va:%llx \n", &PL_srcA_buf[0]);
    // printf("B_va:%llx \n", &PL_srcB_buf[0]);
    // printf("C_va:%llx \n", &PL_dstC_buf[0]);

    printf("A_pa:%llx \n", A_pa);
    printf("B_pa:%llx \n", B_pa);
    printf("C_pa:%llx \n", C_pa);
    // getchar();

    //zeroing
    for(size_t i=0; i<srcA_size; i++){
      PL_srcA_buf[i]=0;
    }
    for(size_t i=0; i<srcB_size; i++){
      PL_srcB_buf[i]=0;
    }
    for(size_t i=0; i<dstC_size; i++){
      PL_dstC_buf[i]=0;
    }

    // printf("srcA init\n");
    float temp0 = 0.05;
      // short tmp0 = float_to_short(temp0);
    for(size_t i=0; i<srcA_size; i++){
      temp0  = generate_random()/10;
      // temp0  = 0.05; // testbench
      // if (i%16 == 0) temp0 += 0.001;
      short tmp0 = float_to_short(temp0);
      PL_srcA_buf[i] = tmp0;
      src_A_DRAM[i] = temp0;
      // src_A_DRAM[i] = short_to_float(tmp0);
    }

    // printf("srcB init (%d)\n", srcB_size);
    float temp1  = 0.03125; // testbench
    for(size_t i=0; i<srcB_size; i++){
      float temp1  = generate_random()/10;
      // float temp1  = generate_random_255();
    // float temp1  = 0.03125; // testbench
      // temp1  += 1;
      short tmp0 = float_to_short(temp1);
      PL_srcB_buf[i]=tmp0;
      src_B_DRAM[i] = temp1;
      // src_B_DRAM[i] = short_to_float(tmp0);
    }    
    
    // printf("dstC init\n");
    for(size_t i=0; i<dstC_size; i++){
      PL_dstC_buf[i] = 0;
      dst_C_DRAM[i] = 0;
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
    
    uint64_t dur_CPU = 0;
    for(int i = 0; i < ITER; i++) {
      clock_gettime(CLOCK_MONOTONIC, &start_CPU);
      mat_mul_CPU(src_A_DRAM, src_B_DRAM, dst_C_DRAM, p_size, q_size, r_size);
      clock_gettime(CLOCK_MONOTONIC, &end_CPU);  
      diff_CPU = BILLION * (end_CPU.tv_sec - start_CPU.tv_sec) + (end_CPU.tv_nsec - start_CPU.tv_nsec);
      dur_CPU += diff_CPU;
      printf("CPU matmul latency %llu ns at iter #%d\t%llu\n", (long long unsigned int) diff_CPU, i, (long long unsigned int) diff_CPU);
    }
    printf("CPU average matmul latency %llu ns %d %d %d\t%llu\n", (long long unsigned int) dur_CPU/ITER, p_size, q_size, r_size, (long long unsigned int) dur_CPU/ITER);

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
    
    // getchar();
    uint64_t dur_PIM = 0;
    for(int i = 0; i < ITER; i++) {
      clock_gettime(CLOCK_MONOTONIC, &start_PIM);
#ifndef DECOUP
      matmul(set_info, fpga_id);  
#else
      matmul_tiled(set_info, fpga_id);
#endif 
      clock_gettime(CLOCK_MONOTONIC, &end_PIM);  
      diff_PIM = BILLION * (end_PIM.tv_sec - start_PIM.tv_sec) + (end_PIM.tv_nsec - start_PIM.tv_nsec);
      dur_PIM += diff_PIM;
      printf("PIM matmul latency %llu ns at iter #%d\t%llu\n", (long long unsigned int) diff_PIM, i, (long long unsigned int) diff_PIM);
    }
    printf("PIM average matmul latency %llu ns %d %d %d\t%llu\n", (long long unsigned int) dur_PIM/ITER, p_size, q_size, r_size, (long long unsigned int) dur_PIM/ITER);

    
    union converter{
    float f_val;
    unsigned int u_val;
    };
    union converter a;
    union converter b;


    printf("Correctness check!\n\n");
    printf("            CPU       |    PIM\n");
    for(int i=0; i<dstC_size; i++){ 
        a.f_val = dst_C_DRAM[i];

        #ifdef DUMP_BANK0_ONLY
        if ((i%256)==0){ // Only Bank 0
          for(int j=0; j<16; j++){
            a.f_val = dst_C_DRAM[i+j];
            printf("idx[%4d] 0x%x  |  ", i, a.u_val);
            printf("0x%x ", PL_dstC_buf[i+j]);
            printf("\n");
          }
        }
        #else
        printf("idx[%4d] 0x%x  |  ", i, a.u_val >> 16);
        printf("0x%x ", PL_dstC_buf[i]);
        printf("\n");
        #endif
        //if (i == 32) // Only Bank 0
        //    break;        
    }

    printf("\n");
    pim_free(PL_srcA_buf, fpga_id);
    pim_free(PL_srcB_buf, fpga_id);
    pim_free(PL_dstC_buf, fpga_id);
    pim_free(PL_srcB_aligned_buf, fpga_id);
    return 0;
}



