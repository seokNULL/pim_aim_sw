#include <getopt.h>
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
#include "matmul.h"
#include "util.h"

static bool print_matrix = false;
static bool validation = false;
static size_t device = 0;
static size_t M = 16;
static size_t N = 256;
static size_t K = 256;
static size_t num_iterations = 1;

static char matmul_type_string[6][64] = {
    "CPU (sequential)", "PIM (Silent-PIM)", "PIM (Decoupled w/ tiling)",
};

static void print_help(const char *prog_name) {
  printf("Usage: %s [-pvh] [-n num_iterations] device M N K\n", prog_name);
  printf("Options:\n");
  printf("     -p : print matrix. (default: off)\n");
  printf("     -v : validate matmul. (default: off)\n");
  printf("     -h : print this page.\n");
  printf("     -n : number of iterations (default: 1)\n");
  printf("      device : type of matrix multiplication (default: 0)\n");
  printf("            0 : CPU (sequential)\n");
  printf("            1 : PIM (Silent-PIM)\n");
  printf("            2 : PIM (Decoupled w/ tiling)\n");
  printf("      M : number of rows of matrix A and C. (default: 16)\n");
  printf("      N : number of columns of matrix B and C. (default: 256)\n");
  printf(
      "      K : number of columns of matrix A and rows of B. (default: 256)\n");
}

static void parse_opt(int argc, char **argv) {
  int c;
  while ((c = getopt(argc, argv, "pvht:n:m:")) != -1) {
    switch (c) {
      case 'p': print_matrix = true; break;
      case 'v': validation = true; break;
      case 'n': num_iterations = atoi(optarg); break;
      case 'h':
      default: print_help(argv[0]); exit(0);
    }
  }
  for (int i = optind, j = 0; i < argc; ++i, ++j) {
    switch (j) {
      case 0: device = (size_t) atoi(argv[i]); break;
      case 1: M = (size_t) atoi(argv[i]); break;
      case 2: N = (size_t) atoi(argv[i]); break;
      case 3: K = (size_t) atoi(argv[i]); break;
      default: break;
    }
  }

  printf("============= Matrix Multiplication Benchmark =============\n");
  printf("- Matmul Type: %s\n", matmul_type_string[device]);
  printf("- Problem size: M = %lu, N = %lu, K = %lu\n", M, N, K);
  printf("- A = (%lu) x (%lu), B = (%lu) x (%lu), C = (%lu) x (%lu)\n", M, K, K, N, M, N);
  printf("- Number of iterations: %lu\n", num_iterations);
  printf("- Print matrix: %s\n", print_matrix ? "on" : "off");
  printf("- Validation: %s\n", validation ? "on" : "off");
}

void matmul_cpu(float *A, float *B, float *C, size_t M, size_t N, size_t K) {
  // Naive CPU matrix multiplication
  for (int i = 0; i < M; ++i) {
    for (int k = 0; k < K; ++k) {
      for (int j = 0; j < N; ++j) {
        C[i * N + j] += A[i * K + k] * B[k * N + j];
      }
    }
  }
}

void matmul_mpim
(short *A0, short *B0, short *C0, short *A1, short *B1, short *C1,
 pim_args *pim_code0, pim_args *pim_code1,
 size_t M, size_t N, size_t K, 
 int fpga_id0, int fpga_id1) 
{
  pim_code0->srcA_va = (uint64_t)&A0[0];
  pim_code0->srcB_va = (uint64_t)&B0[0];
  pim_code0->dstC_va = (uint64_t)&C0[0];
  pim_code0->p_size  = M;
  pim_code0->q_size  = K;
  pim_code0->r_size  = N;

  pim_code1->srcA_va = (uint64_t)&A1[0];
  pim_code1->srcB_va = (uint64_t)&B1[0];
  pim_code1->dstC_va = (uint64_t)&C1[0];
  pim_code1->p_size  = M;
  pim_code1->q_size  = K;
  pim_code1->r_size  = N;
    
  matmul(pim_code0, fpga_id0);
  matmul(pim_code1, fpga_id1);
}

void matmul_pim(short *A, short *B, short *C, pim_args *pim_code, size_t M, size_t N, size_t K, int fpga_id) {
  pim_code->srcA_va = (uint64_t)&A[0];
  pim_code->srcB_va = (uint64_t)&B[0];
  pim_code->dstC_va = (uint64_t)&C[0];
  pim_code->p_size  = M;
  pim_code->q_size  = K;
  pim_code->r_size  = N;  
  matmul(pim_code, fpga_id);
}

void matmul_decoupled_pim(short *A, short *B, short *C, pim_args *pim_code, size_t M, size_t N, size_t K){
  int A_ROW_PAD;
  int num_of_reg = 16;
  A_ROW_PAD = (M + num_of_reg - 1) / num_of_reg * num_of_reg; // 
  pim_code->srcA_va   = (uint64_t)&B[0]; //Must be swapped
  pim_code->srcB_va   = (uint64_t)&A[0]; //Must be swapped
  pim_code->dstC_va   = (uint64_t)&C[0]; 
  pim_code->p_size    = A_ROW_PAD;
  pim_code->q_size    = K;
  pim_code->r_size    = N; 
  matmul_tiled(pim_code);
}

int main(int argc, char **argv) {
  parse_opt(argc, argv);
  fflush(stdout);

  /* Allocate and initialize matrices on CPU and PIM*/
    int p_size = M;
    int q_size = K;
    int r_size = N;

    int srcA_size = p_size * q_size;
    int srcB_size = q_size * r_size;
    int dstC_size = p_size * r_size;

    if ((init_pim_drv()) < 0) {
        perror("PIM DMA drvier open");
        exit(-1);
    }

    float *srcA = (float *)(mmap(NULL, srcA_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *srcB = (float *)(mmap(NULL, srcB_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *dstC = (float *)(mmap(NULL, dstC_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *C_ans = (float *)(mmap(NULL, dstC_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    short *srcA_pim0 = (short *)pim_malloc(srcA_size*sizeof(short)/2, 0);
    short *srcB_pim0 = (short *)pim_malloc(srcB_size*sizeof(short), 0);
    short *dstC_pim0 = (short *)pim_malloc(dstC_size*sizeof(short)/2, 0);

    short *srcA_pim1 = (short *)pim_malloc(srcA_size*sizeof(short)/2, 1);
    short *srcB_pim1 = (short *)pim_malloc(srcB_size*sizeof(short), 1);
    short *dstC_pim1 = (short *)pim_malloc(dstC_size*sizeof(short)/2, 1);

    pim_args *pim_code0, *pim_code1;
    int size = sizeof(pim_args);
    pim_code0 = (pim_args *)malloc(1024*1024*size);
    pim_code1 = (pim_args *)malloc(1024*1024*size);

    if (srcA_pim0 == MAP_FAILED | srcA_pim1 == MAP_FAILED) {
        printf("PL srcA call failure.\n");
        return -1;
    }
    if (srcB_pim0 == MAP_FAILED | srcB_pim1 == MAP_FAILED) {
        printf("PL srcB call failure.\n");
        return -1;
    }
    if (dstC_pim0 == MAP_FAILED | srcB_pim1 == MAP_FAILED) {
        printf("PL dstC call failure.\n");
        return -1;
    }
    zero_mat(srcA, p_size, q_size);
    zero_mat(srcB, p_size, q_size);
    zero_mat(dstC, p_size, q_size);
    
    zero_mat(srcA_pim0, p_size/2, q_size);
    zero_mat(srcB_pim0, q_size, r_size);
    zero_mat(dstC_pim0, p_size/2, r_size);

    zero_mat(srcA_pim1, p_size/2, q_size);
    zero_mat(srcB_pim1, q_size, r_size);
    zero_mat(dstC_pim1, p_size/2, r_size);
    
    srand((unsigned int)time(NULL));  // For reset random seed
    rand_mat(srcA, p_size, q_size);
    rand_mat(srcB, q_size, r_size);

    //for validation
    zero_mat(C_ans, p_size, q_size);
    matmul_cpu(srcA, srcB, C_ans, p_size, r_size, q_size);
    
  /* Initialize Matrix Multiplication */
    copy_mat_input_each(srcA, srcA_pim0, srcA_pim1, p_size, q_size);
    copy_mat_input(srcB, srcB_pim0, q_size, r_size);
    copy_mat_input(srcB, srcB_pim1, q_size, r_size);

    switch (device) {
      case 0: break;
      case 1: 
        srcB_pim0 = matmul_pim_initialize(srcB_pim0, q_size, r_size, 1);
        srcB_pim1 = matmul_pim_initialize(srcB_pim1, q_size, r_size, 1); break;
      case 2: matmul_decoupled_pim_initialize(srcA, srcB, srcA_pim, srcB_pim, p_size, q_size, r_size); break;
    }


  /* Run matrix multiplication for num_iterations */
  printf("\n--------------------- Run Benchmark -----------------------\n");

  double elapsed_time_sum = 0;
  for (size_t i = 0; i < num_iterations; ++i) {
    printf("[iter %lu] ", i);
    fflush(stdout);

  // We assume all data which PIM executed is prepared before launching PIM kernel.
  // For example, when PIM design is implemented in system-memory,
  // all data movement cost can be removed.
    double elapsed_time_iter = -get_current_time();
    switch (device) {
      case 0: matmul_cpu(srcA, srcB, dstC, p_size, r_size, q_size); break;
      case 1: matmul_mpim(srcA_pim0, srcB_pim0, dstC_pim0, srcA_pim1, srcB_pim1, dstC_pim1, 
                          pim_code0, pim_code1, p_size, r_size, q_size, 0, 1); break;
      case 2: matmul_decoupled_pim(srcA_pim, srcB_pim, dstC_pim, pim_code, p_size, r_size, q_size); break;
    }
    elapsed_time_iter += get_current_time();

    printf("%.4f ms\n", elapsed_time_iter*1000);
//    printf("%.4f ns\n", elapsed_time_iter*1000000000);
    elapsed_time_sum += elapsed_time_iter;
  }


  /* Print performance results */
  double elapsed_time_avg = elapsed_time_sum / num_iterations;
  printf("\n-------------------- Benchmark Summary --------------------\n");
  // printf("Avg. time        : %.4f ms\n", elapsed_time_avg*1000);
  printf("Avg. time        : %.4f ns\n", elapsed_time_avg*1000000000);
  printf("Avg. performance : %.1f GFLOPs\n",
         2.0 * M * N * K / elapsed_time_avg / 1e9);


    /* Finalize matrix multiplication */
  switch (device) {
    case 0: matmul_cpu_finalize(p_size, r_size, q_size); break;
    case 1: matmul_pim_finalize(srcA_pim, srcB_pim, dstC_pim, p_size, r_size, q_size); break;
    case 2: matmul_decoupled_pim_finalize(srcA_pim, srcB_pim, dstC_pim, p_size, r_size, q_size); break;
  }

  if (print_matrix) {
    printf("\n---------------------- Print Matrix -----------------------\n");
    // printf("MATRIX A:\n");
    // print_mat(srcA, M, N);
    // printf("MATRIX B:\n");
    // print_mat(srcB, M, N);
    // printf("MATRIX C:\n");
    // print_mat(dstC, M, N); 
    printf("MATRIX C_ans:\n");
    print_mat(C_ans, M, N); 
    printf("MATRIX PIM:\n");
    for (int i = 0; i < dstC_size; ++i) { printf("%+.3f ", short_to_float(dstC_pim[i])); }
    printf("\n");
  }

  if (validation) {
    printf("\n----------------------- Validation ------------------------\n");
    switch (device) {
      case 0: printf("Validation Result: VALID\n"); break;      
      case 1: check_multi_mat_mul(dstC_pim, C_ans, p_size, r_size, q_size); break;
      case 2: check_mat_mul(dstC_pim, C_ans, p_size, r_size, q_size); break;
    }  
  }

  printf("\n===========================================================\n");

  munmap(srcA, srcA_size*sizeof(float));
  munmap(srcB, srcB_size*sizeof(float));
  munmap(dstC, dstC_size*sizeof(float));
  munmap(C_ans, dstC_size*sizeof(float));

  free(pim_code0);
  free(pim_code1);
  pim_free(srcA_pim0, 0);
  pim_free(srcB_pim0, 0);
  pim_free(dstC_pim0, 0);
  pim_free(srcA_pim1, 1);
  pim_free(srcB_pim1, 1);
  pim_free(dstC_pim1, 1);

  return 0;
}
