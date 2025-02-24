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

#include "version.h"
#include "dmautils.h"
#include "qdma_nl.h"
#include "dmaxfer.h"

#include <pim.h>
#include <pthread.h>
/*
#include "dma_xfer_utils.c"
#define DEVICE_NAME_DEFAULT "/dev/qdma01000-MM-0"
#define SIZE_DEFAULT (32)
#define COUNT_DEFAULT (1)
*/

// #define NUM_THREADS

// ANSI color codes
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m"
#define CYAN "\033[36m"

#define USE_MMAP     (0)
#define USE_QDMA_H2C (1)
#define USE_QDMA_C2H (2)
#define USE_QDMA_C2C (3)
#define USE_CDMA 	 (4)


#define AIM_RESERVED_OFFSET 0x00800000 //8 MB

/*Device 0's base memory address need to be checked before progrma execution*/
#define DEV0_MEM_BASE 0x6000000000 //Device 0 - 8GB assumed 
#define DEVICE_NAME_DEFAULT "cpu"
#define SIZE_DEFAULT (32) //elements

#define DEV0_CDMA 0
#define PERF
#define ITER 5
// struct timespec start_time[2], end_time[2];
struct timespec statrt_qdma, end_qdma;
struct timespec statrt_cdma, end_cdma;
uint64_t dur_qdma, dur_cdma;

static unsigned int pci_bus = 0x01;
static unsigned int pci_dev = 0x00;
static int fun_id = 0x0;
static unsigned int num_q = 1; //Board number
static int is_vf = 0;

/*QDMA related information*/
#define QDMA_Q_NAME_LEN     100
struct queue_info *q_info;
int q_count;

enum qdma_q_dir {
	QDMA_Q_DIR_H2C,
	QDMA_Q_DIR_C2H,
	QDMA_Q_DIR_BIDI
};

enum qdma_q_mode {
	QDMA_Q_MODE_MM,
	QDMA_Q_MODE_ST
};

struct queue_info {
	char *q_name;
	int qid;
	int pf;
	enum qdmautils_io_dir dir;
};

enum qdma_q_mode mode = QDMA_Q_MODE_MM;
enum qdma_q_dir dir = QDMA_Q_DIR_BIDI;

float* AllocMat(long long size);
void InitMat(float* mat, long long size);
void ZeroMat(float* mat, long long size);
void WriteToMmap(void* mmap_ptr, float* matrix_ptr, long long size);
void ReadFromMmap(float* matrix_ptr, void* mmap_ptr, long long size);
void CompareMatrices(float* matrix1, float* matrix2, long long size);
void PrintMatrixHexa(float* matrix, long long size);
void PrintMatrixElem(float* matrix, long long size);

int qdma_validate_qrange(void);
void qdma_q_prep_name(struct queue_info *q_info, int qid, int pf);

int qdma_prepare_queue(struct queue_info *q_info, enum qdmautils_io_dir dir, int qid, int pf);
int qdma_prepare_q_start(struct xcmd_info *xcmd, enum qdmautils_io_dir dir, int qid, int pf);
int qdma_prepare_q_del(struct xcmd_info *xcmd, enum qdmautils_io_dir dir, int qid, int pf);
int qdma_prepare_q_add(struct xcmd_info *xcmd, enum qdmautils_io_dir dir, int qid, int pf);
int qdma_prepare_q_stop(struct xcmd_info *xcmd, enum qdmautils_io_dir dir, int qid, int pf);
int qdma_create_queue(enum qdmautils_io_dir dir, int qid, int pf);
void qdma_env_cleanup();
void qdma_queues_cleanup(struct queue_info *q_info, int q_count);
int qdma_destroy_queue(enum qdmautils_io_dir dir, int qid, int pf);
int qdma_setup_queues(struct queue_info **pq_info);
int qdma_dump_queue(struct queue_info **pq_info);

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

        for (long long j = 0; j < 16; j += 2) {
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
    for (long long i = 0; i < size; i += 8) {
        printf(BLUE"0x%08llx: "RESET, (unsigned long long)(matrix + i));

        for (long long j = 0; j < 8; ++j) {
            if (i + j < size) {
                printf(GREEN"%f "RESET, matrix[i + j]);
            } else {
                printf("         "); // For alignment if the last line has less than 4 floats
            }
        }
        printf("\n");
    }
}


int qdma_validate_qrange(void){
	struct xcmd_info xcmd;
	int ret;

	memset(&xcmd, 0, sizeof(struct xcmd_info));
	xcmd.op = XNL_CMD_DEV_INFO;
	xcmd.if_bdf = (pci_bus << 12) | (pci_dev << 4) | fun_id;

	/* Get dev info from qdma driver */
	ret = qdma_dev_info(&xcmd);
	if (ret < 0) {
		printf("Failed to read qmax for PF: %d\n", fun_id);
		return ret;
	}
	if (!xcmd.resp.dev_info.qmax) {
		printf("Error: invalid qmax assigned to function :%d qmax :%u\n",
				fun_id, xcmd.resp.dev_info.qmax);
		return -EINVAL;
	}

	return 0;
}
int qdma_prepare_queue(struct queue_info *q_info, enum qdmautils_io_dir dir, int qid, int pf){
	int ret;

	if (!q_info) {
		printf("Error: Invalid queue info\n");
		return -EINVAL;
	}

	qdma_q_prep_name(q_info, qid, pf);
	q_info->dir = dir;
	ret = qdma_create_queue(q_info->dir, qid, pf);
	if (ret < 0) {
		printf("Q creation Failed PF:%d QID:%d\n",
				pf, qid);
		return ret;
	}
	q_info->qid = qid;
	q_info->pf = pf;

	return ret;
}
void qdma_q_prep_name(struct queue_info *q_info, int qid, int pf){
	q_info->q_name = calloc(QDMA_Q_NAME_LEN, 1);
	snprintf(q_info->q_name, QDMA_Q_NAME_LEN, "/dev/qdma%s%05x-%s-%d",
			(is_vf) ? "vf" : "",
			(pci_bus << 12) | (pci_dev << 4) | pf,
			(mode == QDMA_Q_MODE_MM) ? "MM" : "ST",
			qid);
}
int qdma_prepare_q_start(struct xcmd_info *xcmd, enum qdmautils_io_dir dir, int qid, int pf){
	struct xcmd_q_parm *qparm;


	if (!xcmd) {
		printf("Error: Invalid Input Param\n");
		return -EINVAL;
	}
	qparm = &xcmd->req.qparm;

	xcmd->op = XNL_CMD_Q_START;
	xcmd->vf = is_vf; 
	xcmd->if_bdf = (pci_bus << 12) | (pci_dev << 4) | pf;
	qparm->idx = qid;
	qparm->num_q = 1;

	if (mode == QDMA_Q_MODE_MM)
		qparm->flags |= XNL_F_QMODE_MM;
	else if (mode == QDMA_Q_MODE_ST)
		qparm->flags |= XNL_F_QMODE_ST;
	else {
		printf("Error: Invalid mode\n");
		return -EINVAL;	
	}

	if (dir == DMAXFER_IO_WRITE)
		qparm->flags |= XNL_F_QDIR_H2C;
	else if (dir == DMAXFER_IO_READ)
		qparm->flags |= XNL_F_QDIR_C2H;
	else {
		printf("Error: Invalid Direction\n");
		return -EINVAL;	
	}

	qparm->flags |= (XNL_F_CMPL_STATUS_EN | XNL_F_CMPL_STATUS_ACC_EN |
			XNL_F_CMPL_STATUS_PEND_CHK | XNL_F_CMPL_STATUS_DESC_EN |
			XNL_F_FETCH_CREDIT);

	return 0;
}
int qdma_prepare_q_del(struct xcmd_info *xcmd, enum qdmautils_io_dir dir, int qid, int pf){
	struct xcmd_q_parm *qparm;

	if (!xcmd) {
		printf("Error: Invalid Input Param\n");
		return -EINVAL;
	}

	qparm = &xcmd->req.qparm;

	xcmd->op = XNL_CMD_Q_DEL;
	xcmd->vf = is_vf; 
	xcmd->if_bdf = (pci_bus << 12) | (pci_dev << 4) | pf;
	qparm->idx = qid;
	qparm->num_q = 1;

	if (mode == QDMA_Q_MODE_MM)
		qparm->flags |= XNL_F_QMODE_MM;
	else if (mode == QDMA_Q_MODE_ST)
		qparm->flags |= XNL_F_QMODE_ST;
	else {
		printf("Error: Invalid mode\n");
		return -EINVAL;	
	}

	if (dir == DMAXFER_IO_WRITE)
		qparm->flags |= XNL_F_QDIR_H2C;
	else if (dir == DMAXFER_IO_READ)
		qparm->flags |= XNL_F_QDIR_C2H;
	else {
		printf("Error: Invalid Direction\n");
		return -EINVAL;	
	}

	return 0;
}
int qdma_create_queue(enum qdmautils_io_dir dir, int qid, int pf){
	struct xcmd_info xcmd;
	int ret;

	memset(&xcmd, 0, sizeof(struct xcmd_info));
	ret = qdma_prepare_q_add(&xcmd, dir, qid, pf);
	if (ret < 0)
		return ret;

	ret = qdma_q_add(&xcmd);
	if (ret < 0) {
		printf("Q_ADD failed, ret :%d\n", ret);
		return ret;
	}

	memset(&xcmd, 0, sizeof(struct xcmd_info));
	ret = qdma_prepare_q_start(&xcmd, dir, qid, pf);
	if (ret < 0)
		return ret;

	ret = qdma_q_start(&xcmd);
	if (ret < 0) {
		printf("Q_START failed, ret :%d\n", ret);
		qdma_prepare_q_del(&xcmd, dir, qid, pf);
		qdma_q_del(&xcmd);
	}

	return ret;
}
int qdma_prepare_q_add(struct xcmd_info *xcmd, enum qdmautils_io_dir dir, int qid, int pf){
	struct xcmd_q_parm *qparm;

	if (!xcmd) {
		printf("Error: Invalid Input Param\n");
		return -EINVAL;
	}

	qparm = &xcmd->req.qparm;

	xcmd->op = XNL_CMD_Q_ADD;
	xcmd->vf = is_vf; 
	xcmd->if_bdf = (pci_bus << 12) | (pci_dev << 4) | pf;
	qparm->idx = qid;
	qparm->num_q = 1;

	if (mode == QDMA_Q_MODE_MM)
		qparm->flags |= XNL_F_QMODE_MM;
	else if (mode == QDMA_Q_MODE_ST)
		qparm->flags |= XNL_F_QMODE_ST;
	else {
		printf("Error: Invalid mode\n");
		return -EINVAL;	
	}
	if (dir == DMAXFER_IO_WRITE)
		qparm->flags |= XNL_F_QDIR_H2C;
	else if (dir == DMAXFER_IO_READ)
		qparm->flags |= XNL_F_QDIR_C2H;
	else {
		printf("Error: Invalid Direction\n");
		return -EINVAL;	
	}
	qparm->sflags = qparm->flags;

	return 0;
}
int qdma_setup_queues(struct queue_info **pq_info){
	struct queue_info *q_info;
	unsigned int qid;
	unsigned int q_count;
	unsigned int q_index;
	int ret;

	if (!pq_info) {
		printf("Error: Invalid queue info\n");
		return -EINVAL;
	}

	if (dir == QDMA_Q_DIR_BIDI)
		q_count = num_q * 2;
	else	
		q_count = num_q;

	*pq_info = q_info = (struct queue_info *)calloc(q_count, sizeof(struct queue_info));
	if (!q_info) {
		printf("Error: OOM\n");
		return -ENOMEM;
	}

	q_index = 0;
	for (qid = 0; qid < num_q; qid++) {
		if ((dir == QDMA_Q_DIR_BIDI) ||
				(dir == QDMA_Q_DIR_H2C)) {
			ret = qdma_prepare_queue(q_info + q_index,
					DMAXFER_IO_WRITE,
					qid,
					fun_id);
			if (ret < 0)
				break;
			q_index++;
		}
		if ((dir == QDMA_Q_DIR_BIDI) ||
				(dir == QDMA_Q_DIR_C2H)) {
			ret = qdma_prepare_queue(q_info + q_index,
				DMAXFER_IO_READ,
					qid,
					fun_id);
			if (ret < 0)
				break;
			q_index++;
		}
	}
	if (ret < 0) {
		// qdma_queues_cleanup(q_info, q_index);
		return ret;
	}

	return q_count; 
}
void qdma_env_cleanup(){
	qdma_queues_cleanup(q_info, q_count);

	if (q_info)
		free(q_info);
	q_info = NULL;
	q_count = 0;
}
void qdma_queues_cleanup(struct queue_info *q_info, int q_count){
	unsigned int q_index;

	if (!q_info || q_count < 0)
		return;

	for (q_index = 0; q_index < q_count; q_index++) {
		qdma_destroy_queue(q_info[q_index].dir,
				q_info[q_index].qid,
				q_info[q_index].pf);
		free(q_info[q_index].q_name);
	}
}
int qdma_destroy_queue(enum qdmautils_io_dir dir, int qid, int pf){
	struct xcmd_info xcmd;
	int ret;

	memset(&xcmd, 0, sizeof(struct xcmd_info));
	ret = qdma_prepare_q_stop(&xcmd, dir, qid, pf);
	if (ret < 0)
		return ret;	

	ret = qdma_q_stop(&xcmd);
	if (ret < 0)
		printf("Q_STOP failed, ret :%d\n", ret);

	memset(&xcmd, 0, sizeof(struct xcmd_info));
	qdma_prepare_q_del(&xcmd, dir, qid, pf);
	ret = qdma_q_del(&xcmd);
	if (ret < 0)
		printf("Q_DEL failed, ret :%d\n", ret);

	return ret;
}
int qdma_prepare_q_stop(struct xcmd_info *xcmd, enum qdmautils_io_dir dir, int qid, int pf){
	struct xcmd_q_parm *qparm;

	if (!xcmd)
		return -EINVAL;

	qparm = &xcmd->req.qparm;

	xcmd->op = XNL_CMD_Q_STOP;
	xcmd->vf = is_vf; 
	xcmd->if_bdf = (pci_bus << 12) | (pci_dev << 4) | pf;
	qparm->idx = qid;
	qparm->num_q = 1;

	if (mode == QDMA_Q_MODE_MM)
		qparm->flags |= XNL_F_QMODE_MM;
	else if (mode == QDMA_Q_MODE_ST)
		qparm->flags |= XNL_F_QMODE_ST;
	else
		return -EINVAL;	

	if (dir == DMAXFER_IO_WRITE)
		qparm->flags |= XNL_F_QDIR_H2C;
	else if (dir == DMAXFER_IO_READ)
		qparm->flags |= XNL_F_QDIR_C2H;
	else
		return -EINVAL;	


	return 0;
}


typedef struct 
{
  /* data */
  int tid;
  void* Src;
  void* Dst;
  size_t size;
  /* qdma */  
  const char* devname;
  int qdma_fd;
  int offset;
  /* cdma */
} ThreadArgs;

void* OverlapCopy(void* thread_args) {
    ThreadArgs* args = (ThreadArgs*) thread_args;
	int tid = args->tid;
    int ret;
	unsigned long dur_qdma_total = 0;
	unsigned long dur_cdma_total = 0;

    switch (tid) {
        case 0: {
			for(int i=0; i<ITER; i++){
			
			clock_gettime(CLOCK_MONOTONIC, &statrt_qdma);	
			  ret = write_from_buffer(args->devname, args->qdma_fd, args->Src, args->size, args->offset);
			  ret = read_to_buffer(args->devname, args->qdma_fd, args->Dst, args->size, args->offset);
			  if(ret<0) return -1;
			clock_gettime(CLOCK_MONOTONIC, &end_qdma);
			dur_qdma = BILLION * (end_qdma.tv_sec - statrt_qdma.tv_sec) + (end_qdma.tv_nsec - statrt_qdma.tv_nsec);
			dur_qdma_total += dur_qdma;
			printf("QDMA Memcpy time: %lu nanoseconds \n", dur_qdma);			
			}
			break;
		} 

        case 1: {
			for(int i=0; i<ITER; i++){
			clock_gettime(CLOCK_MONOTONIC, &statrt_cdma);				
              if(internalMemcpy(args->Dst, args->Src, args->size, DEV0_CDMA)<0){
				return -1;
			  }
			clock_gettime(CLOCK_MONOTONIC, &end_cdma);			  
			dur_cdma = BILLION * (end_cdma.tv_sec - statrt_cdma.tv_sec) + (end_cdma.tv_nsec - statrt_cdma.tv_nsec);
			dur_cdma_total += dur_cdma;

			printf("CDMA Memcpy time: %lu nanoseconds \n", dur_cdma); 
			}
            break;
        }
        default:
            fprintf(stderr, "Unknown thread id: %d\n", tid);
    }

    double avg_dur_qdma = (double)dur_qdma_total / ITER;
	double avg_dur_cdma = (double)dur_cdma_total / ITER;\

    printf("Average QDMA Memcpy time: %f nanoseconds \n", avg_dur_qdma);
	printf("Average CDMA Memcpy time: %f nanoseconds \n", avg_dur_cdma);


    return NULL;
}


int main(int argc, char *argv[]) {
	int cmd_opt;
	char *device = DEVICE_NAME_DEFAULT;
	uint64_t size = SIZE_DEFAULT;
    uint64_t address = 0;
    int verbose = 0;
    int qdma_direction = 0;
    

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
    float* SrcIn = AllocMat(size);
    float* DstOut = AllocMat(size);

    InitMat(SrcIn, size);
    ZeroMat(DstOut, size);

	printf("Matrix Src Address(VA): %p\n", (void*)SrcIn);
	printf("Matrix Dst Address(VA): %p\n", (void*)DstOut);
	printf("Size: %lu(elemets), %lu(bytes)\n", size, size*sizeof(float));


	    ssize_t ret;
    	ret = qdma_validate_qrange();
        q_count = 0;
	    /* Addition and Starting of queues handled here */
	    q_count = qdma_setup_queues(&q_info);
	    if (q_count < 0) {
	    	printf("qdma_setup_queues failed, ret:%d\n", q_count);
	    	return q_count; 
	    }
		/*Current queue status dump */
    	printf("Queue Information:\n");
    	printf("  Device Name: %s\n", q_info->q_name);
    	printf("  ID: %d\n", q_info->qid);
    	printf("  PF: %d\n", q_info->pf);

		char* devname = q_info->q_name;
		int qdma_fd = open(devname, O_RDWR);
		int cdma_fd = 0;
		init_pim_drv();
		if(cdma_fd = open(PL_DMA_DRV, O_RDWR|O_SYNC) < 0){
			perror("CDMA driver open failed");
			exit(-1);
		}
		float *FpgaSrc = (float*) pim_malloc(size*sizeof(float), DEV0_CDMA);
		float *FpgaDst = (float*) pim_malloc(size*sizeof(float), DEV0_CDMA);
		if((FpgaSrc == MAP_FAILED)||(FpgaDst == MAP_FAILED)){
			printf("FPGA alloc failed\n");
			return -1;
		} 
		uint64_t SrcPa = VA2PA((uint64_t)&FpgaSrc[0]);
		uint64_t DstPa = VA2PA((uint64_t)&FpgaDst[0]);
    	// if(verbose){
			printf("Source PA:0x%lx \n", SrcPa);
    		printf("Destination PA:0x%lx \n", DstPa);
		// }
		ZeroMat(FpgaSrc,size);
		ZeroMat(FpgaDst,size);
		InitMat(FpgaSrc,size);

		pim_args *pim_code;
    	int code_size = sizeof(pim_args);
    	pim_code = (pim_args *)malloc(1024*code_size);

    	pthread_t threads[2];
		ThreadArgs thread_args[2];
    

    	for (int i = 0; i < 2; i++) {
    	    thread_args[i].tid = i;
			if(i==0){
				thread_args[i].devname = devname;
				thread_args[i].qdma_fd = qdma_fd;
				thread_args[i].offset = AIM_RESERVED_OFFSET;

				thread_args[i].Src = SrcIn;
				thread_args[i].Dst = DstOut;
				thread_args[i].size = sizeof(float)*size;
			}
			else if(i==1){
				thread_args[i].Src = FpgaSrc;
				thread_args[i].Dst = FpgaDst;
				thread_args[i].size = sizeof(float)*size;
			}

			// clock_gettime(CLOCK_MONOTONIC, &start_time[i]);
    	    if (pthread_create(&threads[i], NULL, OverlapCopy, (void*)&thread_args[i])) {
    	        fprintf(stderr, "Error creating thread %d\n", i);
    	        return -1;
    	    }
    	}
	    for (int i = 0; i < 2; i++) {
	        if (pthread_join(threads[i], NULL)) {
	            fprintf(stderr, "Error joining thread %d\n", i);
	            return -1;
	        }
			// clock_gettime(CLOCK_MONOTONIC, &end_time[i]);
	    }
		

    // for (int i = 0; i < 2; i++) {
    //     uint64_t elapsed_time = BILLION * (end_time[i].tv_sec - start_time[i].tv_sec) +
    //                           (end_time[i].tv_nsec - start_time[i].tv_nsec);
    //     printf("Thread %d execution time: %lu nano seconds\n", i, elapsed_time);
    // }

	atexit(qdma_env_cleanup);
	close(qdma_fd);
	close(cdma_fd);

    free(SrcIn);
    free(DstOut);
    // free(Ans);

    return 0;
}
