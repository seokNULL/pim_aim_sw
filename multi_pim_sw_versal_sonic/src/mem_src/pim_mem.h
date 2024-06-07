#ifndef _PIM_MEM_H_
#define _PIM_MEM_H_
/*
 * PIM driver interface file
 * This header file only used in src folder
 */
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
#include "../drv_src/pim_mem_lib_user.h"
#include "../../include/pim.h"

#include "../drv_src/multi_dev_lib_user.h"
#include <pthread.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12
#define NUM_PAGES 512 /* HUGE_PAGE_SIZE / PAGE_SIZE */

/* Record keeping for pim_mem */
typedef struct __pim_mem {
    /* The number of page to guarantee physically continuity */
    uint ncont_page;

    /* The total number of lists in the mem_pool, whether or not available */
    uint ntotal_mem_pools;

    /* Double-linked mem_pool */
    struct mem_pool *curr_mem_pool;
    struct mem_pool *head_mem_pool;
    struct mem_pool *tail_mem_pool;

    /* Mutex for mutual exclusive mem_pool */
    pthread_mutex_t lock;
} _pim_mem;

/* 
 * Record keeping for mem_pool 
 * mem_pool manages page
 * Size of mem_pool is 2MB (Huge page size)
 */
struct mem_pool {
    void *start_va;
#ifdef DEBUG_PIM_MEM
    uint index;
#endif
    /* Indicates the start of the unused page */
    uint32_t curr_idx;

    /* The number of available lists in the mem_pool */
    uint32_t nfree_pages;

    /* This flag means large allocated mem_pool */
    bool large_mem_pool;

    struct page *head_page;

    /* Linked list mem_pool */
    struct mem_pool *next;
    struct mem_pool *prev;

    /* 
     * Bit vetor for used flag of page (512bits)
     * For architecture compatibility, use char type bit-vector instead of AVX2 
     */
    char bit_vector[NUM_PAGES + 1];
};

/* Page structure */
struct page {
    void *addr;
#ifdef DEBUG_PIM_MEM
    uint index;
#endif
    /* Since the argument of pim_free has no size, we need it for tracking */
    uint32_t alloc_size;
};


/* TODO: implement atomic RD/WR */
#define update_member_pim_mem(member, value) \
    pim_mem.member = value

#define get_member_pim_mem(member) \
    pim_mem.member

#define init_pim_mem()       \
    pim_mem.ntotal_mem_pools = 0; \
    pim_mem.curr_mem_pool = NULL; \
    pim_mem.head_mem_pool = NULL; \
    pim_mem.tail_mem_pool = NULL;


#define ASSERT_PRINT(fmt, ...) \
    printf(fmt, ##__VA_ARGS__); \
    assert(0);

#define list_for_prev(pos) \
    for (; pos != NULL; pos=pos->prev)

#define list_for_next(pos) \
    for (; pos != NULL; pos=pos->next)

#define list_for_next_value(pos, value) \
    for (uint32_t i = 0; i < value; i++, pos=pos->next)

#define list_add_tail(new_mem_pool, curr_mem_pool, tail_mem_pool) \
    new_mem_pool->prev = tail_mem_pool; \
    tail_mem_pool->next = new_mem_pool; \
    curr_mem_pool = new_mem_pool;

#define container_of(ptr, type, member) ({          \
    void *__mptr = (void *)(ptr);                   \
    ((type *)(__mptr - offsetof(type, member))); })

// extern void *mmap_drv(int size);
// extern void __init_pim_drv(void);
// extern void __release_pim_drv(void);


extern void *multi_mmap_drv(int size, int fpga_id);
extern void __init_mpim_drv(void);
extern void __release_mpim_drv(void);

#ifdef __cplusplus
}
#endif

#endif