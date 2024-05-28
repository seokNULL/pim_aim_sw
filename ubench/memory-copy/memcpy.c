#include <assert.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <getopt.h>

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/types.h>
#include <unistd.h>

/*
#include "dma_xfer_utils.c"
#define DEVICE_NAME_DEFAULT "/dev/qdma01000-MM-0"
#define SIZE_DEFAULT (32)
#define COUNT_DEFAULT (1)
*/

#define USE_MMAP (0)
#define USE_QDMA (1)
#define AIM_RESERVED_OFFSET 0x00800000 //x MB
#define DEV0_MEM_BASE 0x6000000000

// ANSI color codes
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"


#define DEVICE_NAME_DEFAULT "cpu"
#define SIZE_DEFAULT (32)

float* AllocMat(long long size);
void InitMat(float* mat, long long size);
void ZeroMat(float* mat, long long size);
void WriteToMmap(void* mmap_ptr, float* matrix_ptr, long long size);
void ReadFromMmap(float* matrix_ptr, void* mmap_ptr, long long size);
void CompareMatrices(float* matrix1, float* matrix2, long long size);
void PrintMatrixHexa(float* matrix, long long size);
void PrintMatrixElem(float* matrix, long long size);

float* AllocMat(long long size) {
    return (float*)malloc(size * sizeof(float));
}
void InitMat(float* mat, long long size) {
    srand(time(NULL));
    for (long long i = 0; i < size; ++i) {
        mat[i] = (float)rand() / (float)RAND_MAX;
    }
}
void ZeroMat(float* mat, long long size) {
    for (long long i = 0; i < size; ++i) {
        mat[i] = 0;
    }
}
void WriteToMmap(void* mmap_ptr, float* matrix_ptr, long long size) {
    float* mmap_float_ptr = (float*)mmap_ptr;
    for (long long i = 0; i < size; ++i) {
        mmap_float_ptr[i] = matrix_ptr[i];
    }
}
void ReadFromMmap(float* matrix_ptr, void* mmap_ptr, long long size) {
    float* mmap_float_ptr = (float*)mmap_ptr;
    for (long long i = 0; i < size; ++i) {
        matrix_ptr[i] = mmap_float_ptr[i];
    }
}
void CompareMatrices(float* matrix1, float* matrix2, long long size) {
    int same = 1;
    for (long long i = 0; i < size; ++i) {
        if (matrix1[i] != matrix2[i]) {
            same = 0;
            printf("Different value at index %lld: matrix1 = %f, matrix2 = %f\n", i, matrix1[i], matrix2[i]);
        }
    }
    if (same) {
        printf("Matrices are identical.\n");
    }
}
static struct option const long_opts[] = {
	{"device", required_argument, NULL, 'd'},
	{"address", required_argument, NULL, 'a'},
	{"size", required_argument, NULL, 's'},
	{"help", no_argument, NULL, 'h'},
	{"verbose", no_argument, NULL, 'v'},
	{0, 0, 0, 0}
};
uint64_t getopt_integer(char *optarg){
	int rc;
	uint64_t value;
	rc = sscanf(optarg, "0x%lx", &value);
	if (rc <= 0)
		sscanf(optarg, "%lu", &value);
	//printf("sscanf() = %d, value = 0x%lx\n", rc, value);
	return value;
}
static void usage(const char *name){
	int i = 0;
	fprintf(stdout, "%s\n\n", name);
	fprintf(stdout, "usage: %s [OPTIONS]\n\n", name);

	fprintf(stdout, "  -%c (--%s) device (defaults to %s)\n",
		long_opts[i].val, long_opts[i].name, DEVICE_NAME_DEFAULT);
	i++;
	fprintf(stdout, "  -%c (--%s) the start address on the AXI bus\n",
	       long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout,
		"  -%c (--%s) size of a copy in elements, default %d.\n",
		long_opts[i].val, long_opts[i].name, SIZE_DEFAULT);
	i++;
	fprintf(stdout, "  -%c (--%s) print usage help and exit\n",
		long_opts[i].val, long_opts[i].name);
	i++;
	fprintf(stdout, "  -%c (--%s) verbose output\n",
		long_opts[i].val, long_opts[i].name);
}
void PrintMatrixHexa(float* matrix, long long size) {
    unsigned char* byte_ptr = (unsigned char*)matrix;

    for (long long i = 0; i < size * sizeof(float); i += 16) {
        printf(BLUE"0x%08llx: "RESET, (unsigned long long)(byte_ptr + i));

        for (long long j = 0; j < 16; j += 4) {
            if (i + j < size * sizeof(float)) {
                float* float_ptr = (float*)(byte_ptr + i + j);
                printf(GREEN"%08X "RESET, *(unsigned int*)float_ptr);
            } else {
                printf("         "); // For alignment if the last line has less than 16 bytes
            }
        }
        printf("\n");
    }
}
void PrintMatrixElem(float* matrix, long long size) {
    for (long long i = 0; i < size; i += 4) {
        printf(BLUE"0x%08llx: "RESET, (unsigned long long)(matrix + i));

        for (long long j = 0; j < 4; ++j) {
            if (i + j < size) {
                printf(GREEN"%f "RESET, matrix[i + j]);
            } else {
                printf("         "); // For alignment if the last line has less than 4 floats
            }
        }
        printf("\n");
    }
}


int main(int argc, char *argv[]) {
	int cmd_opt;
	char *device = DEVICE_NAME_DEFAULT;
	uint64_t address = 0;
	uint64_t size = SIZE_DEFAULT;
    int verbose = 0;
	while ((cmd_opt = getopt_long(argc, argv, "vhx:f:d:a:s:", long_opts, NULL)) != -1) {
		switch (cmd_opt) {
		case 0:
			/* long option */
			break;
		case 'd':
			/* device node name */
			device = strdup(optarg);
			break;
		case 'a':
			/* RAM address on the AXI bus in bytes */
			address = getopt_integer(optarg);
			break;
			/* RAM size in bytes */
		case 's':
			size = getopt_integer(optarg);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'h':
		default:
			usage(argv[0]);
			exit(0);
			break;
		}
	}

	if (verbose)
	fprintf(stdout,
		"dev %s, addr 0x%lx, size 0x%lx\n",
		device, address, size);

    float* SrcIn = AllocMat(size);
    float* DstOut = AllocMat(size);
    float* Ans = AllocMat(size);

    InitMat(SrcIn, size);
    ZeroMat(DstOut, size);
    ZeroMat(Ans, size);
    memcpy(Ans, SrcIn, size * sizeof(float));

    // Case 0: USE_MMAP
    if (device == "cpu") {
        int mem_fd = open("/dev/mem", O_RDWR | O_SYNC);
        if (mem_fd == -1) {
            printf("Failed to open /dev/mem\n");
            return 1;
        }
        uint64_t offset = DEV0_MEM_BASE + AIM_RESERVED_OFFSET;
        void* FpgaMem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, mem_fd, offset);
        // void* FpgaMem = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE, mem_fd, offset);
        if (FpgaMem == MAP_FAILED) {
            printf("Failed to mmap /dev/mem\n");
            close(mem_fd);
            return 1;
        }
        memset(FpgaMem, 0, size);
        WriteToMmap(FpgaMem, SrcIn, size);   /*Host -> Card*/ 
        ReadFromMmap(DstOut, FpgaMem, size); /*Card -> Host*/ 

        if (verbose){
            printf("Answer:\n");
            PrintMatrixElem(Ans, size);
            printf("\n==================================================\n");
            PrintMatrixHexa(Ans, size);
            // CompareMatrices(Ans, DstOut, size);
            printf("Fpga Memory:\n");
            PrintMatrixElem(FpgaMem, size);
            printf("\n==================================================\n");
            PrintMatrixHexa(FpgaMem, size);            
        }
        if (munmap(FpgaMem, size) == -1) {
            printf("Failed to munmap /dev/mem\n");
        }
        close(mem_fd);
    }

    free(SrcIn);
    free(DstOut);
    free(Ans);

    return 0;
}
