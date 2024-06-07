#ifndef _PIM_H_
#define _PIM_H_


/* For PIM memory management */
#define __PIM_MALLOC__ /* If this define is disabled, pim_malloc is changed to standard malloc function */
#define __FPGA__ /* If this define is enabled, PIM driver MUST BE loaded into the kernel */
// #define __PIM_MEM_KER__ /* Enable this define when OS buddy manages PIM memory */

/* For DMA */
// #define INTR_ENABLE /* Enable interrupt. If this define is disabled, polling mode */
// #define PERF_TIME /* For time measurement */
// #define x86_PIM_PERF /* For time measurement in x86*/

/* For debugging */
// #define DEBUG_PL_DMA /* Xilinx CDMA debugging */
// #define DEBUG_HOST_DMA /* X86 HOST debugging */
// #define DEBUG_PIM_MEM /* PIM MemPool debugging */
// #define DEBUG_PIM_MATH /* PIM Math library debugging */

/* For Multi-PIM */
#define IRQ_NUM 8
#define P2P

#define BILLION 1000000000L

/* DO NOT EDIT parameter without modification of Vivado Address Editor */
#ifdef __aarch64__
    #define HIGH_ADDR   0x00000004 /* PL DDR address is 0x40000_0000 ~ in ARM platform */
#elif defined __x86_64__
    #define HIGH_ADDR   0x00000208 /* PL DDR address is always 32 bits in x86 platform*/

#endif

typedef struct
{
	const void *srcA_ptr;
	const void *srcB_ptr;
	const void *src2_ptr;
	const void *srcbias_ptr;
	void *dstC_ptr;

	uint64_t srcA_va;
	uint64_t srcB_va;
	uint64_t bias_va;
	uint64_t dstC_va;

	uint32_t srcA_size;
	uint32_t srcB_size;
	uint32_t src2_size;
	uint32_t dstC_size;

	uint32_t p_size;
	uint32_t q_size;
	uint32_t r_size;

	uint32_t desc_idx;
	uint32_t desc_last;
	uint32_t fused_op;
	uint32_t length;

	uint64_t dma_tx;
    void *dma_tx_ptr;
    uint64_t desc_base;
    uint64_t dram_base;
    void *desc_host;
    uint64_t va;
    uint64_t pa;
    uint64_t ka; // added
} __attribute__ ((packed)) pim_args;
// } pim_args;


typedef uint16_t Bfloat16;


#define PL_DMA_DRV_PREFIX "/dev/pl_dma"
#define PL_DMA_DRV "/dev/pl_dma0"
#define PS_DMA_DRV "/dev/ps_dma"
#define X86_DMA_DRV "/dev/x86_dma"

#define XDMA_PREFIX "/dev/xdma"
#define XDMA_CPU2PIM "_h2c_0"
#define XDMA_PIM2CPU "_c2h_0"
#define XDMA_PIM2PIM "_h2c_0"

#define PL_DRV_NAME_PREFIX "pl_dma"
#define PL_DRV_NAME "pl_dma0"
#define PS_DRV_NAME "ps_dma"
#define HOST_DRV_NAME "x86_dma"
#define PIM_CONF_DRV_NAME "pim_conf_reg"

#define PL_DMA_REG_NAME "reg-region"
#define PL_DMA_DESC_NAME "desc-region"
#define CONF_BRAM_NAME "conf-bram-region"
#define CONF_REG_NAME "conf-reg-region"

#define PIM_MEM "pim-mem-region"
#define DRIVER_NAME "pim_mmap_dev"

enum {
	/* MUST skip the 0x0 ~ 0x2 */
	/* 3*/ DMA_START = 0x3,
	DESC_MEM_INFO,
	VA_TO_PA,
	VA_TO_KA,
	MEMCPY_PL2PL,
	MEMCPY_PL2PS,
	MEMCPY_PS2PL,
	/* For operator fusion opcode */
	FUSED_ELEW_ADD,
	FUSED_ELEW_SUB,
	MEMCPY_PL2RPL
};


/* 
 * For x86 host dma ioctl 
 */
enum {
	/* MUST skip the 0x0 ~ 0x2 */
	/* 3*/ HOST2HOST = 0x3,
	HOST2FPGA,
	FPGA2HOST,
	FPGA2FPGA,
	H2H_ZCPY,
};

#ifdef __cplusplus
extern "C"
{
#endif

#ifdef DEBUG_PIM_MEM
#define PIM_MEM_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__);
#else
#define PIM_MEM_LOG(fmt, ...)
#endif

#ifdef DEBUG_PIM_MATH
#define PIM_MATH_LOG(fmt, ...) printf(fmt, ##__VA_ARGS__);
#else
#define PIM_MATH_LOG(fmt, ...)
#endif

/* 
 * External functions for Multi-PIM operation
 */
struct id_pim_args {	//TODO: args retrieve to one
    pim_args *pim_args;
	int fpga_id;
};

#define matmul(...) 		_matmul((struct id_pim_args){__VA_ARGS__})
#define matmul_tiled(...) 	_matmul_tiled((struct id_pim_args){__VA_ARGS__})
#define gemm(...) 			_gemm((struct id_pim_args){__VA_ARGS__})
#define matmul_fusion(...) 	_matmul_fusion((struct id_pim_args){__VA_ARGS__})
#define elewise_add(...) 	_elewise_add((struct id_pim_args){__VA_ARGS__})
#define elewise_sub(...) 	_elewise_sub((struct id_pim_args){__VA_ARGS__})
#define elewise_mul(...) 	_elewise_mul((struct id_pim_args){__VA_ARGS__})
#define bias_add(...) 		_bias_add((struct id_pim_args){__VA_ARGS__})
#define bias_sub(...) 		_bias_sub((struct id_pim_args){__VA_ARGS__})
#define bias_mul(...) 		_bias_mul((struct id_pim_args){__VA_ARGS__})

extern int _matmul(struct id_pim_args in);
extern int _matmul_tiled(struct id_pim_args in);
extern int _gemm(struct id_pim_args in);
extern int _matmul_fusion(struct id_pim_args in);
extern int _elewise_add(struct id_pim_args in);
extern int _elewise_sub(struct id_pim_args in);
extern int _elewise_mul(struct id_pim_args in);
extern int _bias_add(struct id_pim_args in);
extern int _bias_sub(struct id_pim_args in);
extern int _bias_mul(struct id_pim_args in);

/* CDMA data copy */
extern int internalMemcpy(void *pim_dst, void *pim_src, int size, int fpga_id);
/* XDMA implementation */
extern int datacopy_cpu2pim(void *pim_dst, const void *cpu_src, int size, int fpga_id);
extern int datacopy_pim2cpu(void *cpu_dst, const void *pim_src, int size, int fpga_id);
extern int datacopy_pim2pim(void *pim_dst, const void *pim_src, int size, int dst_id);
extern void hostBcast(short** pim_dsts, short* cpu_src, int size);
extern void pimBcast(short** pim_dsts, short* pim_src, int src_id, int size);
extern void pimScatter(short** pim_dsts, short* pim_src, int src_id, int size);
extern void pimGather(short* pim_dst, short** pim_srcs, int src_id, int size);
/* PCCL implementation */

extern int totalDevNum;
extern int pccl_matmul(pim_args **pim_args);

struct malloc_args {
    size_t size;
	int fpga_id;
};
struct addr_args {
    void* addr;
	int fpga_id;
};
struct va_args {
    uint64_t addr;
	int fpga_id;
};
struct pim_mem_t{
    char dev_name[50];
    void *base;  // list of all pim_mem base addresses
    int fd; // list of all pim_mem file descriptors

    uint64_t pim_mem_size;
    uint64_t total_size;
};
struct args {
    short** pim_arr;
    short** pim_dsts;
    short** pim_srcs;
    short* pim_dst;
    short* pim_src;
    short* cpu_src;
    int size;
    int src_id;
    int fpga_id;
    pim_args *pim_code;
};

#define pim_malloc(...) _pim_malloc((struct malloc_args){__VA_ARGS__})
#define init_pim_drv(...) init_mpim_drv()
#define VA2PA(...) _mVA2PA((struct va_args){__VA_ARGS__})
#define pim_free(...) _pim_free((struct addr_args){__VA_ARGS__})

/*
 * External functions for Multi-PIM operation
 */
extern int init_mpim_drv(void);
extern int init_xdma_drv(void);
extern uint64_t _mVA2PA(struct va_args in);
extern void* _pim_malloc(struct malloc_args in);
extern void _pim_free(struct addr_args in);
// extern void *mpim_malloc(size_t size, int fpga_id);
// extern void mpim_free(void *addr, int fpga_id);



#ifdef __cplusplus
}  /* Assume C declarations for C++ */
#endif /* __cplusplus */
#endif
