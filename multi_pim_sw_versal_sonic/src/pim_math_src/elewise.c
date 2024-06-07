#include "pim_math.h"

#include <time.h>

int _elewise(pim_args *pim_args, uint32_t opcode, int fpga_id);
int _bias_op(pim_args *pim_args, uint32_t opcode, int fpga_id);
/* Wrapper for code reuse */
int _elewise_add(struct id_pim_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    return _elewise(in.pim_args, OPCODE_ELE_ADD, fpga_id_out);
}
int _elewise_sub(struct id_pim_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    return _elewise(in.pim_args, OPCODE_ELE_SUB, fpga_id_out);
}
int _elewise_mul(struct id_pim_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    return _elewise(in.pim_args, OPCODE_ELE_MUL, fpga_id_out);
}
int _bias_add(struct id_pim_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    return _bias_op(in.pim_args, OPCODE_ELE_ADD, fpga_id_out);
}
int _bias_sub(struct id_pim_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    return _bias_op(in.pim_args, OPCODE_ELE_SUB, fpga_id_out);
}
int _bias_mul(struct id_pim_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    return _bias_op(in.pim_args, OPCODE_ELE_MUL, fpga_id_out);
}

int _elewise(pim_args *pim_args, uint32_t opcode, int fpga_id)
{
	uint32_t desc_idx, next_desc, p_loop_var, r_loop_var, p, r;
	uint32_t p_size, r_size;
    uint64_t A_pa, B_pa, C_pa, offset;
    uint64_t A_base, B_base, C_base;
	uint64_t srcA_va, srcB_va, dstC_va;

	srcA_va = pim_args->srcA_va;
    srcB_va = pim_args->srcB_va;
    dstC_va = pim_args->dstC_va;

    p_size = pim_args->p_size;
    r_size = pim_args->r_size;

	desc_idx = 0;
    uint64_t desc_base = pl_dma[fpga_id].desc_base;
    next_desc = desc_base + 0x40;

    p_loop_var = p_size;
    r_loop_var = r_size;
    A_base = 0x0ULL;
    B_base = 0x0ULL;
    C_base = 0x0ULL;
    PIM_MATH_LOG("%s: p:%d, r:%d\n", __func__, p_size, r_size);
    for (p = 0; p < p_loop_var; p++){
        for(r = 0; r < r_loop_var; r = r + (REG_SIZE * NUM_BANKS)){
            PIM_MATH_LOG("[p:%d, r:%d] \n", p, r);
            // offset = (p * r_size_aligned * TYPE_SIZE) + (r * TYPE_SIZE);
            offset = (p * r_size * TYPE_SIZE) + (r * TYPE_SIZE);
            if ((offset % HUGE_PAGE_SIZE) == 0) {
                A_base = VA2PA(srcA_va + offset, fpga_id);
                B_base = VA2PA(srcB_va + offset, fpga_id);
                C_base = VA2PA(dstC_va + offset, fpga_id);
                A_pa = A_base;
                B_pa = B_base;
                C_pa = C_base;
            } else {
                A_pa = A_base + offset;
                B_pa = B_base + offset;
                C_pa = C_base + offset;
            }
            PIM_RD_INSTR(&desc_idx, &next_desc, A_pa, BRAM_DUMMY, PIM_WIDTH, RD_A_ATTR | (opcode << 0x4), fpga_id);
            PIM_RD_INSTR(&desc_idx, &next_desc, B_pa, BRAM_DUMMY, PIM_WIDTH, RD_B_ATTR | (opcode << 0x4), fpga_id);
            PIM_WR_INSTR(&desc_idx, &next_desc, BRAM_DUMMY, C_pa, PIM_WIDTH, WR_C_ATTR | (opcode << 0x4), fpga_id);
        }
    }
    pim_args->desc_idx = desc_idx - 1;
    pim_args->desc_last = next_desc - 0x40;
    if (pim_exec(pim_args, fpga_id) < 0) {
        printf("DMA transaction failed \n");
        return -1;
    }
    return 0;
}

int _bias_op(pim_args *pim_args, uint32_t opcode, int fpga_id)
{
    uint32_t desc_idx, next_desc, r_loop_var, q_loop_var, q, r;
    uint32_t q_size, r_size;
    uint32_t A_pa, B_pa, C_pa, offset, A_off;
    uint64_t A_base, B_base, C_base;
    uint64_t srcA_va, srcB_va, dstC_va;

    srcA_va = pim_args->srcA_va;
    srcB_va = pim_args->srcB_va;
    dstC_va = pim_args->dstC_va;

    q_size = pim_args->q_size;
    r_size = pim_args->r_size;

    desc_idx = 0;
    uint64_t desc_base = pl_dma[fpga_id].desc_base;
    next_desc = desc_base + 0x40;

    q_loop_var = q_size;
    r_loop_var = r_size;

    A_base = 0x0ULL;
    B_base = 0x0ULL;
    C_base = 0x0ULL;

    PIM_MATH_LOG("%s: q:%d, r:%d\n", __func__, q_size, r_size);
    for (r = 0; r < r_loop_var; r = r + (REG_SIZE * NUM_BANKS)){
        for (q = 0; q < q_loop_var; q++){
            PIM_MATH_LOG("[q:%d, r:%d] \n", q, r);
            A_off = (r * TYPE_SIZE);
            if ((A_off % HUGE_PAGE_SIZE) == 0) {
                A_base = VA2PA(srcA_va + A_off, fpga_id);
                A_pa = A_base;
            } else {
                A_pa = A_base + A_off;
            }
            offset = (q * r_size * TYPE_SIZE) + (r * TYPE_SIZE);
            if ((offset % HUGE_PAGE_SIZE) == 0) {
                B_base = VA2PA(srcB_va + offset, fpga_id);
                C_base = VA2PA(dstC_va + offset, fpga_id);
                B_pa = B_base;
                C_pa = C_base;
            } else {
                B_pa = B_base + offset;
                C_pa = C_base + offset;
            }
            PIM_RD_INSTR(&desc_idx, &next_desc, A_pa, BRAM_DUMMY, PIM_WIDTH, RD_A_ATTR | (opcode << 0x4), fpga_id);
            PIM_RD_INSTR(&desc_idx, &next_desc, B_pa, BRAM_DUMMY, PIM_WIDTH, RD_B_ATTR | (opcode << 0x4), fpga_id);
            PIM_WR_INSTR(&desc_idx, &next_desc, BRAM_DUMMY, C_pa, PIM_WIDTH, WR_C_ATTR | (opcode << 0x4), fpga_id);        
        }
    }
    pim_args->desc_idx = desc_idx - 1;
    pim_args->desc_last = next_desc - 0x40;
    if (pim_exec(pim_args, fpga_id) < 0) {
        printf("DMA transaction failed \n");
        return -1;
    }
    return 0;
}