/* Copyright (c) 2021 SK Hynix  Co., Ltd.
 * http://www.skhynix.com/
 *
 * SK Hynix AiM driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __QDMA_IF_H__
#define __QDMA_IF_H__

#include <linux/types.h>
#include "./libqdma_shared.h"

#define QDMA_IF_NODE	"/dev/qdma-pf-if"
#define MAX_DEVICES 8

struct bd_dev_info {
	unsigned int bd_idx[MAX_DEVICES];
	int bd_total_cnt;
};

struct qdma_aim_if_ioctl {
    int qidx; // 0 ~ (qdma_dev_conf.qsets_max - 1)
    enum queue_type_t q_type; // queue_type_t = {Q_H2C, Q_C2H, Q_CMPT, Q_H2C_C2H}
    int st;                   // 0 for mm, 1 for st
    int bd_idx;
};

struct fpga_mem_info {
    uint64_t mem_start;
    uint64_t mem_length;
    int bd_idx;
};

enum qdma_cdev_ioctl_cmd {
	QDMA_CDEV_IOCTL_NO_MEMCPY,
	QDMA_CDEV_IOCTL_PRESET,
	QDMA_CDEV_IOCTL_CMDS
};

/*
 * descriptor & writeback status
 */
/**
 * @struct - qdma_mm_desc
 * @brief	memory mapped descriptor format
 */
struct qdma_mm_desc {
	/** source address */
	__be64 src_addr;
	/** flags */
	__be32 flag_len;
	/** reserved 32 bits */
	__be32 rsvd0;
	/** destination address */
	__be64 dst_addr;
	/** reserved 64 bits */
	__be64 rsvd1;
};

/**
 * @struct - qdma_mm_packet
 * @brief	qdma data transfer unit
 */
struct qdma_mm_packet {
	struct qdma_mm_packet *next;
	struct qdma_mm_desc *mm_desc;
	uint32_t len_mm_desc;
	uint64_t chunk_size;
	uint32_t batch_size;
	uint8_t is_write;
};

#define QDMA_AIM_IF_IOCTL_MAGIC     'F'
#define QDMA_AIM_IF_IOCTL_QUEUE_ADD     			_IOWR(QDMA_AIM_IF_IOCTL_MAGIC, 0, struct qdma_aim_if_ioctl)
#define QDMA_AIM_IF_IOCTL_QUEUE_DELETE  			_IOWR(QDMA_AIM_IF_IOCTL_MAGIC, 1, struct qdma_aim_if_ioctl)
#define QDMA_AIM_IF_IOCTL_QUEUE_START   			_IOWR(QDMA_AIM_IF_IOCTL_MAGIC, 2, struct qdma_aim_if_ioctl)
#define QDMA_AIM_IF_IOCTL_QUEUE_STOP    			_IOWR(QDMA_AIM_IF_IOCTL_MAGIC, 3, struct qdma_aim_if_ioctl)
#define QDMA_CDEV_IOCTL_GET_FPGA_MEM    			_IOWR(QDMA_AIM_IF_IOCTL_MAGIC, 4, struct fpga_mem_info)
#define QDMA_AIM_IF_IOCTL_GET_DEVICE_ENTITY_INFO    _IOWR(QDMA_AIM_IF_IOCTL_MAGIC, 5, struct bd_dev_info)
#define QDMA_AIM_IF_IOCTL_GET_BD_IDX   				_IOWR(QDMA_AIM_IF_IOCTL_MAGIC, 6, unsigned int)
#define QDMA_AIM_IF_IOCTL_GET_BD_INFO  				_IOWR(QDMA_AIM_IF_IOCTL_MAGIC, 7, struct bd_dev_info)
#endif // __QDMA_IF_H__
