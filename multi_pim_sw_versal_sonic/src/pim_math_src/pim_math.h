#ifndef _PIM_MATH_LIB_
#define _PIM_MATH_LIB_

#include <sys/mman.h>
#include <stddef.h>
#include <stdio.h>
#include <assert.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <stdbool.h>
#include <pthread.h>
#include "../drv_src/pim_mem_lib_user.h"
#include "../drv_src/addr_lib_user.h"
#include "../../include/pim.h"
#include "../drv_src/multi_dev_lib_user.h"

#define OPCODE_MAT_MUL 0x851F
#define OPCODE_MAT_MUL_DECOUPLED_8x4  0x1051F
#define OPCODE_ELE_ADD 0x42F
#define OPCODE_GEMM_ADD 0x1042F

#define OPCODE_ELE_SUB 0x44F
#define OPCODE_ELE_MUL 0x48F
#define OPCODE_PROF 0x100000

#define BURST_SIZE 32 /* Byte size */
#define TYPE_SIZE sizeof(short) /* Pre-defined bfloat16 type */
#define REG_SIZE (BURST_SIZE / TYPE_SIZE) /* vecA and vACC register size */
#define vecA_SIZE 32
#define vACC_SIZE 32
#define NUM_BANKS 16
#define PIM_WIDTH (REG_SIZE * NUM_BANKS * TYPE_SIZE)

#define RD_A_ATTR 0x2
#define RD_B_ATTR 0x4
#define WR_C_ATTR 0x9

/* For Descriptor */
#define CDMA_NEXT_DE_L       0x00
#define CDMA_NEXT_DE_H       0x04
#define CDMA_DESC_SA_L       0x08
#define CDMA_DESC_SA_H       0x0C
#define CDMA_DESC_DA_L       0x10
#define CDMA_DESC_DA_H       0x14
#define CDMA_DESC_LEN        0x18
#define CDMA_DESC_STATUS     0x1C
#define CDMA_DESC_INFO       0x24 /* Use reserved bit for PIM opcode */

/* AXI CDMA Register Address Map */
#define CDMA_REG_CR          0x00
#define CDMA_REG_SR          0x04
#define CDMA_CURDESC_PNTR_L  0x08
#define CDMA_CURDESC_PNTR_H  0x0C
#define CDMA_TAILDESC_PNTR_L 0x10
#define CDMA_TAILDESC_PNTR_H 0x14
#define CDMA_REG_SA_L        0x18
#define CDMA_REG_SA_H        0x1C
#define CDMA_REG_DA_L        0x20
#define CDMA_REG_DA_H        0x24
#define CDMA_REG_BYTETRANS   0x28

#define BRAM_DUMMY 0xd0000000
//#define CONF_OFFSET_HPC_CLR 0x5000

typedef struct
{
	/* CDMA_NEXT_DE_L   */ uint32_t next_l;
	/* CDMA_NEXT_DE_H   */ uint32_t next_h;
	/* CDMA_DESC_SA_L   */ uint32_t src_l;
	/* CDMA_DESC_SA_H   */ uint32_t src_h;
	/* CDMA_DESC_DA_L   */ uint32_t dst_l;
	/* CDMA_DESC_DA_H   */ uint32_t dst_h;
	/* CDMA_DESC_LEN    */ uint32_t len;
	/* CDMA_DESC_STATUS */ uint32_t status;
} __attribute__((aligned(64))) pim_isa_t;
// } pim_isa_t;
// extern pim_isa_t *pim_isa;

static inline char *decode_opcode(int opcode) {
	switch(opcode)
	{
		case 0: return "INIT";
		case (RD_A_ATTR | OPCODE_MAT_MUL << 0x4): return "MAT_MUL_SILENT_RD_A";
		case (RD_B_ATTR | OPCODE_MAT_MUL << 0x4): return "MAT_MUL_SILENT_RD_B";
		case (WR_C_ATTR | OPCODE_MAT_MUL << 0x4): return "MAT_MUL_SILENT_WR_C";
		case (RD_A_ATTR | OPCODE_MAT_MUL_DECOUPLED_8x4 << 0x4): return "MAT_MUL_DECOUPLED_RD_BANK_PRIVATE";
		case (RD_B_ATTR | OPCODE_MAT_MUL_DECOUPLED_8x4 << 0x4): return "MAT_MUL_DECOUPLED_RD_BANK_SHARED";
		case (WR_C_ATTR | OPCODE_MAT_MUL_DECOUPLED_8x4 << 0x4): return "MAT_MUL_DECOUPLED_WR_C";
		case (RD_A_ATTR | OPCODE_ELE_ADD<<0x4): return "ELE_ADD_RD_A";
		case (RD_B_ATTR | OPCODE_ELE_ADD<<0x4): return "ELE_ADD_RD_B";
		case (WR_C_ATTR | OPCODE_ELE_ADD<<0x4): return "ELE_ADD_WR_C";
		case (RD_A_ATTR | OPCODE_ELE_SUB<<0x4): return "ELE_SUB_RD_A";
		case (RD_B_ATTR | OPCODE_ELE_SUB<<0x4): return "ELE_SUB_RD_B";
		case (WR_C_ATTR | OPCODE_ELE_SUB<<0x4): return "ELE_SUB_WR_C";
		case (RD_A_ATTR | OPCODE_ELE_MUL<<0x4): return "ELE_MUL_RD_A";
		case (RD_B_ATTR | OPCODE_ELE_MUL<<0x4): return "ELE_MUL_RD_B";
		case (WR_C_ATTR | OPCODE_ELE_MUL<<0x4): return "ELE_MUL_WR_C";
		default: return "None";
	}
}

/*
 * For Multi-PIM operation
 */
#define pim_exec(...) _pim_exec((struct exec_args){__VA_ARGS__})
#define PIM_RD_INSTR(...) _PIM_RD_INSTR((struct instr_args){__VA_ARGS__})
#define PIM_WR_INSTR(...) _PIM_WR_INSTR((struct instr_args){__VA_ARGS__})

struct pl_dma_t {
    char dev_name[50];

	uint64_t desc_base;
	uint64_t dram_base; //
	int fd;
	pim_isa_t *pim_isa;
	
	pthread_mutex_t lock;		/* protects concurrent access */
};

struct exec_args {
	pim_args *pim_args;
	int fpga_id;
};

struct instr_args {
	uint32_t *idx;
	uint32_t *next;
	uint32_t src;
	uint32_t dst;
	uint32_t length;
	uint32_t opcode;
	int fpga_id;
};

extern struct pl_dma_t *pl_dma;

extern int mpim_exec(pim_args *pim_args, int fpga_id);
int _pim_exec(struct exec_args in);

static inline void mPIM_RD_INSTR(uint32_t *idx, uint32_t *next, 
							 uint32_t src, uint32_t dst, uint32_t length, uint32_t opcode, int fpga_id) 
{
	struct pl_dma_t *pl_dma_node = &pl_dma[fpga_id];
	
	pl_dma_node->pim_isa[*idx].next_l = *next;
	pl_dma_node->pim_isa[*idx].next_h = HIGH_ADDR;
	pl_dma_node->pim_isa[*idx].src_l = src;
	pl_dma_node->pim_isa[*idx].src_h = HIGH_ADDR;
	pl_dma_node->pim_isa[*idx].dst_l = dst;
	pl_dma_node->pim_isa[*idx].dst_h = 0x0U;
	pl_dma_node->pim_isa[*idx].len = length;
	pl_dma_node->pim_isa[*idx].status = 0x0U | opcode;
    PIM_MATH_LOG("    PIM_ISA[idx:%3d] next:%x | src:%10x | dst:%10x | length: %6x | opcode: %s\n", 
    							*idx, pl_dma_node->pim_isa[*idx].next_l, pl_dma_node->pim_isa[*idx].src_l, pl_dma_node->pim_isa[*idx].dst_l, pl_dma_node->pim_isa[*idx].len, decode_opcode(opcode));
    (*idx)++;
    (*next)+=0x40;
}

static inline void mPIM_WR_INSTR(uint32_t *idx, uint32_t *next, 
							 uint32_t src, uint32_t dst, uint32_t length, uint32_t opcode, int fpga_id) 
{
	struct pl_dma_t *pl_dma_node = &pl_dma[fpga_id];

  	pl_dma_node->pim_isa[*idx].next_l = *next;
	pl_dma_node->pim_isa[*idx].next_h = HIGH_ADDR;
	pl_dma_node->pim_isa[*idx].src_l = src;
	pl_dma_node->pim_isa[*idx].src_h = 0x0U;
	pl_dma_node->pim_isa[*idx].dst_l = dst;
	pl_dma_node->pim_isa[*idx].dst_h = HIGH_ADDR;
	pl_dma_node->pim_isa[*idx].len = length;
	pl_dma_node->pim_isa[*idx].status = 0x0U | opcode;
    PIM_MATH_LOG("    PIM_ISA[idx:%3d] next:%x | src:%10x | dst:%10x | length: %6x | opcode: %s\n", 
    							*idx, pl_dma_node->pim_isa[*idx].next_l, pl_dma_node->pim_isa[*idx].src_l, pl_dma_node->pim_isa[*idx].dst_l, pl_dma_node->pim_isa[*idx].len, decode_opcode(opcode));
    (*idx)++;
    (*next)+=0x40;
}

static inline void _PIM_RD_INSTR(struct instr_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
	mPIM_RD_INSTR(in.idx, in.next, in.src, in.dst, in.length, in.opcode, fpga_id_out);
}
static inline void _PIM_WR_INSTR(struct instr_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
	mPIM_WR_INSTR(in.idx, in.next, in.src, in.dst, in.length, in.opcode, fpga_id_out);
}

#endif
