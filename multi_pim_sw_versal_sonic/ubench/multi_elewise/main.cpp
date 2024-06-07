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
#include <getopt.h>

#include <pim.h>
#include "convert_numeric.h"
#include "util.h"

static bool print_matrix = false;
static bool validation = false;
static size_t device = 0;
static size_t M = 16;
static size_t N = 256;
static size_t opcode = 0;
static size_t num_iterations = 1;

static char elewise_type_string[6][64] = {
    "CPU (sequential)", "PIM", "Multi-PIM",
};

static void print_help(const char *prog_name) {
  printf("Usage: %s [-pvh] [-n num_iterations] device M N\n", prog_name);
  printf("Options:\n");
  printf("     -p : print matrix. (default: off)\n");
  printf("     -v : validate matmul. (default: off)\n");
  printf("     -h : print this page.\n");
  printf("     -n : number of iterations (default: 1)\n");
  printf("      device : type of device(default: 0)\n");
  printf("            0 : CPU (sequential)\n");
  printf("            1 : PIM (Silent-PIM)\n");
  printf("            2 : Multi-PIM (Silent-PIM)\n");
  printf("      M : number of rows of matrix A, B and C. (default: 16)\n");
  printf("      N : number of columns of matrix A, B and C. (default: 256)\n");
  printf("      opcode: type of operational code (0:addition(default), 1:subtraction, 2:multiplication)\n");
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
      case 3: opcode = (size_t) atoi(argv[i]); break;
      default: break;
    }
  }

  // printf("============= Elementwise Operation Benchmark =============\n");
  // printf("- Elemetwise Type: %s\n", elewise_type_string[device]);
  // printf("- Problem size: M = %lu, N = %lu\n", M, N);
  // printf("- A = (%lu) x (%lu), B = (%lu) x (%lu), C = (%lu) x (%lu)\n", M, N, M, N, M, N);
  // printf("- Number of iterations: %lu\n", num_iterations);
  // printf("- Print matrix: %s\n", print_matrix ? "on" : "off");
  // printf("- Validation: %s\n", validation ? "on" : "off");
}


void elewise_cpu(float *A, float *B, float *C, int M, int N, int opcode){
  //0: Addition
  //1: Subtraction
  //2: Multiplication
  switch (opcode) {
    case 0: for(int i = 0; i < M * N; i++){C[i] = A[i] + B[i];} break;
    case 1: for(int i = 0; i < M * N; i++){C[i] = A[i] - B[i];} break;
    case 2: for(int i = 0; i < M * N; i++){C[i] = A[i] * B[i];} break;
    default: break;
  }
}

void elewise_pim(short *A, short *B, short *C, pim_args *pim_code, int M, int N, int opcode, int fpga_id){
  //0: Addition
  //1: Subtraction
  //2: Multiplication
    pim_code->srcA_va   = (uint64_t)&A[0];
    pim_code->srcB_va   = (uint64_t)&B[0];
    pim_code->dstC_va   = (uint64_t)&C[0];
    pim_code->p_size    = M;
    pim_code->q_size    = N;
    pim_code->r_size    = N;

    pim_code->srcA_size = M * N * sizeof(short); // temp instruction for checking single tx

  switch (opcode) {
    case 0: elewise_add(pim_code, fpga_id); break;
    case 1: elewise_sub(pim_code, fpga_id); break;
    case 2: elewise_mul(pim_code, fpga_id); break;
    default: break;
  }
}

void elewise_mpim(short *A0, short *B0, short *C0, short *A1, short *B1, short *C1, pim_args *pim_code0, pim_args *pim_code1, int M, int N, int opcode){
  //0: Addition
  //1: Subtraction
  //2: Multiplication
    pim_code0->srcA_va   = (uint64_t)&A0[0];
    pim_code0->srcB_va   = (uint64_t)&B0[0];
    pim_code0->dstC_va   = (uint64_t)&C0[0];
    pim_code0->p_size    = M;
    pim_code0->q_size    = N;
    pim_code0->r_size    = N;

    pim_code1->srcA_va   = (uint64_t)&A1[0];
    pim_code1->srcB_va   = (uint64_t)&B1[0];
    pim_code1->dstC_va   = (uint64_t)&C1[0];
    pim_code1->p_size    = M;
    pim_code1->q_size    = N;
    pim_code1->r_size    = N;


  switch (opcode) {
    case 0: 
    {
      elewise_add(pim_code0, 0);
      elewise_add(pim_code1, 1);
      break;
    }
    default: break;
  }
}

int main(int argc, char *argv[])
{
    parse_opt(argc, argv);
    fflush(stdout);
    int fpga_id = 1;
  /* Allocate and initialize matrices on CPU and PIM*/
    int p_size = M;
    int q_size = N;
    int r_size = q_size;

    int srcA_size = p_size * q_size;
    int srcB_size = p_size * q_size;
    int dstC_size = p_size * q_size;

    int half_srcA_size = srcA_size / 2;
    int half_srcB_size = srcB_size / 2;
    int half_dstC_size = dstC_size / 2;

    init_mpim_drv();

    float *srcA = (float *)(mmap(NULL, srcA_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *srcB = (float *)(mmap(NULL, srcB_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *dstC = (float *)(mmap(NULL, dstC_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));
    float *C_ans = (float *)(mmap(NULL, dstC_size*sizeof(float), PROT_WRITE | PROT_READ, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0));

    short *srcA_pim0 = (short *)mpim_malloc(half_srcA_size*sizeof(short), 0);
    short *srcB_pim0 = (short *)mpim_malloc(half_srcB_size*sizeof(short), 0);
    short *dstC_pim0 = (short *)mpim_malloc(half_dstC_size*sizeof(short), 0);

    short *srcA_pim1 = (short *)mpim_malloc(half_srcA_size*sizeof(short), 1);
    short *srcB_pim1 = (short *)mpim_malloc(half_srcB_size*sizeof(short), 1);
    short *dstC_pim1 = (short *)mpim_malloc(half_dstC_size*sizeof(short), 1);
    
    pim_args *pim_code0, *pim_code1;
    int size = sizeof(pim_args);
    pim_code0 = (pim_args *)malloc(1024*1024*size);
    pim_code1 = (pim_args *)malloc(1024*1024*size);

    if (srcA_pim0 == MAP_FAILED) {
        printf("PL srcA call failure.\n");
        return -1;
    }
    if (srcB_pim0 == MAP_FAILED) {
        printf("PL srcB call failure.\n");
        return -1;
    }
    if (dstC_pim0 == MAP_FAILED) {
        printf("PL dstC call failure.\n");
        return -1;
    }
    if (srcA_pim1 == MAP_FAILED) {
        printf("PL srcA call failure.\n");
        return -1;
    }
    if (srcB_pim1 == MAP_FAILED) {
        printf("PL srcB call failure.\n");
        return -1;
    }
    if (dstC_pim1 == MAP_FAILED) {
        printf("PL dstC call failure.\n");
        return -1;
    }

    // while(1){
    //   int i;
    //   for (i = 0; i < 100; i++) {
    //     srcA_pim0[i] = 1;
    //   }
    //   usleep(10);
    // }

    zero_mat(srcA, p_size, q_size);
    zero_mat(srcB, p_size, q_size);
    zero_mat(dstC, p_size, q_size);
    

    zero_mat(srcA_pim0, p_size/2, q_size);
    zero_mat(srcB_pim0, p_size/2, q_size);
    zero_mat(dstC_pim0, p_size/2, q_size);

    zero_mat(srcA_pim1, p_size/2, q_size);
    zero_mat(srcB_pim1, p_size/2, q_size);
    zero_mat(dstC_pim1, p_size/2, q_size);

    rand_mat(srcA, p_size, q_size);
    rand_mat(srcB, p_size, q_size);

    //for validation
    elewise_cpu(srcA, srcB, C_ans, M, N, opcode);

  /* Initialize Elementwise Operation */
    srand((unsigned int)time(NULL));  // For reset random seed
    copy_mat_multi_input(srcA, srcA_pim0, srcA_pim1, srcA_size);
    copy_mat_multi_input(srcB, srcB_pim0, srcB_pim1, srcB_size);


  /* Run elementwise operation for num_iterations */
  // printf("\n--------------------- Run Benchmark -----------------------\n");
    double elapsed_time_sum = 0;
    for (size_t i = 0; i < num_iterations; ++i) {
      // printf("[iter %lu] ", i);
      fflush(stdout);

      double elapsed_time_iter = -get_current_time();
      switch (device) {
        case 0: elewise_cpu(srcA, srcB, dstC, M, N, opcode); break;      
        // case 1: elewise_pim(srcA_pim, srcB_pim, dstC_pim, M, N, opcode); break;      
        case 2: elewise_mpim(srcA_pim0, srcB_pim0, dstC_pim0, srcA_pim1, srcB_pim1, dstC_pim1, pim_code0, pim_code1, M/2, N, opcode); break;
      }
      elapsed_time_iter += get_current_time();


      // printf("%.4f ms\n", elapsed_time_iter*1000);
      elapsed_time_sum += elapsed_time_iter;
    }


  /* Print performance results */
  double elapsed_time_avg = elapsed_time_sum / num_iterations;
  printf("\n-------------------- Benchmark Summary --------------------\n");
  printf("Avg. time        : %.4f ms\n", elapsed_time_avg*1000);
  printf("Avg. performance : %.1f GFLOPs\n",
         2.0 * M * N / elapsed_time_avg / 1e9);

  /* Finalize Elementwise Operation */
  
  switch (device) {
    case 0: break;      
    case 1: break;
    case 2: break;
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
    for (int i = 0; i < dstC_size; ++i) { printf("%+.3f ", short_to_float(dstC_pim0[i])); }
    printf("\n");
  }

  if (validation) {
    printf("\n----------------------- Validation ------------------------\n");
    switch (device) {
      case 0: printf("Validation Result: VALID\n"); break;      
      case 1: check_elewise(dstC_pim0, C_ans, M, N, opcode); break;
      case 2: check_multi_elewise(dstC_pim0, dstC_pim1, C_ans, M, N, opcode); break;
    }    
  }

  printf("\n===========================================================\n");
  

    munmap(srcA, srcA_size*sizeof(float));
    munmap(srcB, srcB_size*sizeof(float));
    munmap(dstC, dstC_size*sizeof(float));

    // free(pim_code);
    mpim_free(srcA_pim0, 0);
    mpim_free(srcB_pim0, 0);
    mpim_free(dstC_pim0, 0);
    mpim_free(srcA_pim1, 1);
    mpim_free(srcB_pim1, 1);
    mpim_free(dstC_pim1, 1);
    return 0;
}




