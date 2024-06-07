#include "pim_mem.h"

void _multi_real_free(void);

#define MEM_USE_TRACK
#ifdef MEM_USE_TRACK
static size_t total_size[2] = {0, 0}; // can set dynamically using totaldevnum
#endif
// pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
static _pim_mem *mem_pool_list;
/*
 * Find the first occurrence of find in s, where the search is limited to the
 * first slen characters of s.
 */
static inline char *_strnstr(const char *s, const char *find, size_t slen)
{
    char c, sc;
    size_t len;

    if ((c = *find++) != '\0') {
        len = strlen(find);
        do {
            do {
                if (slen-- < 1 || (sc = *s++) == '\0')
                    return (NULL);
            } while (sc != c);
            if (len > slen)
                return (NULL);
        } while (strncmp(s, find, len) != 0);
        s--;
    }
    return ((char *)s);
}

/* Set the value of the bit_vector to 1 as many as the number of nalloc_pages 
 * bit_vector value
 *   1 means in use
 *   0 means free
 */
static inline int _set_bit_vector(struct mem_pool *curr_mem_pool, int nalloc_pages)
{
    uint64_t idx = -1;
    char free_flag[nalloc_pages + 1];
    memset(free_flag, '0', nalloc_pages * sizeof(char));
    free_flag[nalloc_pages] = '\0';

    PIM_MEM_LOG("\t  set_bit_vector\n");
    PIM_MEM_LOG("BIT[%d]:  %s \n", curr_mem_pool->index, curr_mem_pool->bit_vector);
    PIM_MEM_LOG("alloc: %d \n", nalloc_pages);
    /* 
     * Divide into 2 rounds to reduce search complexity.
     * First round: find enough page from current index to end [idx:512]
     */
    char *ptr =strstr(curr_mem_pool->bit_vector + curr_mem_pool->curr_idx, free_flag);
    if (ptr) {
        char used_flag[nalloc_pages + 1];
        memset(used_flag, '1', nalloc_pages * sizeof(char));
        used_flag[nalloc_pages] = '\0';
        idx = (uint64_t)ptr - (uint64_t)(curr_mem_pool->bit_vector);
        strncpy(curr_mem_pool->bit_vector + idx, used_flag, nalloc_pages);
        PIM_MEM_LOG("used: %s \n", used_flag);
        PIM_MEM_LOG("free: %s \n", free_flag);
        PIM_MEM_LOG("BIT[%d]:  %s \n", curr_mem_pool->index, curr_mem_pool->bit_vector);
        curr_mem_pool->nfree_pages = curr_mem_pool->nfree_pages - nalloc_pages;
        curr_mem_pool->curr_idx = idx + nalloc_pages;
    } else {
        /* Second round: find enough page from start to current index [0:idx-1] */
        PIM_MEM_LOG("NO FIND first round \n");
        char *ptr =_strnstr(curr_mem_pool->bit_vector, free_flag, curr_mem_pool->curr_idx - 1);
        if (ptr) {
            char used_flag[nalloc_pages + 1];
            memset(used_flag, '1', nalloc_pages * sizeof(char));
            used_flag[nalloc_pages] = '\0';
            idx = (uint64_t)ptr - (uint64_t)(curr_mem_pool->bit_vector);
            strncpy(curr_mem_pool->bit_vector + idx, used_flag, nalloc_pages);
            PIM_MEM_LOG("BIT[%d]:  %s \n", curr_mem_pool->index, curr_mem_pool->bit_vector);
            curr_mem_pool->nfree_pages = curr_mem_pool->nfree_pages - nalloc_pages;
            curr_mem_pool->curr_idx = idx + nalloc_pages;
        } else {
            PIM_MEM_LOG("NO FIND second round \n");
        }
    }
    return idx;
}

/* Clear the value of the bit_vector to 0 as many as the number of nalloc_pages */
static inline void _clr_bit_vector(struct mem_pool *mem_pool, int idx, int nalloc_pages)
{
    char free_flag[nalloc_pages + 1];
    memset(free_flag, '0', nalloc_pages * sizeof(char));
    free_flag[nalloc_pages] = '\0';

    PIM_MEM_LOG("\nclr_bit_vector\n");
    PIM_MEM_LOG("BIT[%d]:  %s \n", mem_pool->index, mem_pool->bit_vector);
    PIM_MEM_LOG("free: %s \n", free_flag);
    PIM_MEM_LOG("alloc: %d \n", nalloc_pages);
    PIM_MEM_LOG("IDX: %d \n", idx);

    strncpy(mem_pool->bit_vector + idx, free_flag, nalloc_pages);
    mem_pool->curr_idx = idx;
    mem_pool->nfree_pages = mem_pool->nfree_pages + nalloc_pages;    

    PIM_MEM_LOG("BIT[%d]:  %s \n", mem_pool->index, mem_pool->bit_vector);
    PIM_MEM_LOG("CURR_IDX: %u \n", mem_pool->curr_idx);

}

/* TODO: implement atomic RD/WR */
#define update_member_mpim_mem(member, value, fpga_id) \
    mem_pool_list[fpga_id].member = value

#define get_member_mpim_mem(member, fpga_id) \
    mem_pool_list[fpga_id].member

#define init_mpim_mem(fpga_id)       \
    mem_pool_list[fpga_id].ntotal_mem_pools = 0; \
    mem_pool_list[fpga_id].curr_mem_pool = NULL; \
    mem_pool_list[fpga_id].head_mem_pool = NULL; \
    mem_pool_list[fpga_id].tail_mem_pool = NULL;

/* New mem_pool allocator */
static struct mem_pool *_multi_mem_pool_allocation(int fpga_id)
{
    struct mem_pool *mem_pool_obj;
    mem_pool_obj = malloc(sizeof(struct mem_pool));
    if (mem_pool_obj == NULL) {
        ASSERT_PRINT("Internal Error during malloc \n");
    }

    // Memory allocation to PIM driver (Huge page or IO mapping)
    mem_pool_obj->start_va = multi_mmap_drv(HUGE_PAGE_SIZE, fpga_id);
    if (mem_pool_obj->start_va == NULL) {
        ASSERT_PRINT("Out-of memory \n");
    }

    struct page *page_obj = (struct page *)malloc(sizeof(struct page) * NUM_PAGES);
    if (page_obj == NULL) {
        printf("Internal Error during malloc \n");
        return NULL;
    }    
    for (int i = 0; i < NUM_PAGES; i++) {
        page_obj[i].addr = mem_pool_obj->start_va + (i * PAGE_SIZE);
    #ifdef DEBUG_PIM_MEM
        page_obj[i].index = i;
    #endif
    }
    mem_pool_obj->next = mem_pool_obj->prev = NULL;
    mem_pool_obj->curr_idx = 0;
    memset(mem_pool_obj->bit_vector, '0', NUM_PAGES * sizeof(char));
    mem_pool_obj->bit_vector[NUM_PAGES] = '\0';    
    mem_pool_obj->head_page = &page_obj[0];
    mem_pool_obj->nfree_pages = NUM_PAGES;
    #ifdef DEBUG_PIM_MEM
        mem_pool_obj->index = get_member_mpim_mem(ntotal_mem_pools, fpga_id);
    #endif        
    mem_pool_obj->large_mem_pool = false;
    update_member_mpim_mem(ntotal_mem_pools, get_member_mpim_mem(ntotal_mem_pools, fpga_id) + 1, fpga_id);
    PIM_MEM_LOG("    New mem_pool: %d - nfree_pages:%d/%d \n", mem_pool_obj->index, mem_pool_obj->nfree_pages, NUM_PAGES);
    return mem_pool_obj;
}

/* New mem_pool allocator */
static struct mem_pool *_multi_large_mem_pool_allocation(struct mem_pool *curr_mem_pool, uint32_t num_mem_pool, int fpga_id)
{
    void *large_mem_pool = NULL; 
    struct mem_pool *ret_obj = NULL;
    struct mem_pool *mem_pool_obj[num_mem_pool];
    struct page *page_obj;
    PIM_MEM_LOG("Large mem_pool :%d \n", num_mem_pool);

    large_mem_pool = multi_mmap_drv(HUGE_PAGE_SIZE * num_mem_pool, fpga_id);
    if (large_mem_pool == NULL) {
        ASSERT_PRINT("Out-of memory (large-mem_pool) \n");
    }
    for (size_t i = 0; i < num_mem_pool; i++) {
        mem_pool_obj[i] = malloc(sizeof(struct mem_pool));
        if (mem_pool_obj[i] == NULL) {
            ASSERT_PRINT("Internal Error during malloc \n");
        }
        mem_pool_obj[i]->start_va = large_mem_pool + (i * HUGE_PAGE_SIZE);
        PIM_MEM_LOG("start_va[%lu]: %p \n", i, mem_pool_obj[i]->start_va);
        mem_pool_obj[i]->next = mem_pool_obj[i]->prev = NULL;
        page_obj = (struct page *)malloc(sizeof(struct page) * NUM_PAGES);
        if (page_obj == NULL) {
            ASSERT_PRINT("Internal Error during malloc \n");
        }        
        for (int j = 0; j < NUM_PAGES; j++) {
            page_obj[j].addr = mem_pool_obj[i]->start_va + (j * PAGE_SIZE); // Address is virtual address
        #ifdef DEBUG_PIM_MEM
            page_obj[j].index = j;
        #endif
        }
        mem_pool_obj[i]->curr_idx = 0;
        memset(mem_pool_obj[i]->bit_vector, '0', NUM_PAGES * sizeof(char));
        mem_pool_obj[i]->bit_vector[NUM_PAGES] = '\0';
        mem_pool_obj[i]->head_page = &page_obj[0];
        mem_pool_obj[i]->nfree_pages = 0;
        mem_pool_obj[i]->large_mem_pool = true;
        #ifdef DEBUG_PIM_MEM        
            mem_pool_obj[i]->index = get_member_mpim_mem(ntotal_mem_pools, fpga_id);
        #endif
        update_member_mpim_mem(ntotal_mem_pools, get_member_mpim_mem(ntotal_mem_pools, fpga_id) + 1, fpga_id);
        if (i == 0) {
            ret_obj = mem_pool_obj[i];
            PIM_MEM_LOG("    mem_pool_obj[%lu]:%p (RET: %d) \n", i, mem_pool_obj[i], ret_obj->index);
        }
        PIM_MEM_LOG("    New Large mem_pool: %d (nfree_pages:%d/%d)\n", mem_pool_obj[i]->index, mem_pool_obj[i]->nfree_pages, NUM_PAGES);

        PIM_MEM_LOG("    NEW: %d, CURR: %d , TAIL: %d \n", mem_pool_obj[i]->index, curr_mem_pool->index, get_member_mpim_mem(tail_mem_pool, fpga_id)->index);
        list_add_tail(mem_pool_obj[i], curr_mem_pool, get_member_mpim_mem(tail_mem_pool, fpga_id));
        PIM_MEM_LOG("    NEW: %d, CURR: %d , TAIL: %d \n", mem_pool_obj[i]->index, curr_mem_pool->index, get_member_mpim_mem(tail_mem_pool, fpga_id)->index);
        update_member_mpim_mem(tail_mem_pool, curr_mem_pool, fpga_id);
        PIM_MEM_LOG("    CURR: %d , TAIL: %d (RET: %d) \n", curr_mem_pool->index, get_member_mpim_mem(tail_mem_pool, fpga_id)->index, ret_obj->index);
    }
    // Clear large_mem_pool flag of the last generated mem_pool
    mem_pool_obj[num_mem_pool-1]->large_mem_pool = false;
    ret_obj->head_page[0].alloc_size = NUM_PAGES;
    return ret_obj;
}

/* External function */
void *mpim_malloc(size_t size, int fpga_id)
{
#ifndef __PIM_MALLOC__
    //return  mmap(0x0, size, (PROT_READ | PROT_WRITE), MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    return malloc(size);
#else
#ifdef MEM_USE_TRACK
    // Test Free
    size_t total_alloc_size = 0;
#endif
    void *ret_ptr=NULL;
    int idx = 0;
    int i;
    uint nalloc_pages = 0;    
    /* Initialization mem_pool */
    // pthread_mutex_lock(&mutex); // TODO: pim_malloc should be called before thread invoke?
    if (mem_pool_list == NULL) {
        // Initialization PIM driver
        printf("\nPIM MALLOC NULL CHECK\n\n");
        __init_mpim_drv();
        mem_pool_list = (_pim_mem*)malloc(sizeof(_pim_mem)*totalDevNum);
        atexit(_multi_real_free);
        for (i = 0; i < totalDevNum; i++) {
            struct mem_pool *new_mem_pool = _multi_mem_pool_allocation(i);

            update_member_mpim_mem(curr_mem_pool, new_mem_pool, i);
            update_member_mpim_mem(head_mem_pool, new_mem_pool, i);
            update_member_mpim_mem(tail_mem_pool, new_mem_pool, i);

            // pthread_mutex_init(&mem_pool_list[i].lock, NULL);
        }
    }
    /* Lock mem_pool */
    // pthread_mutex_lock(&mem_pool_list[fpga_id].lock);
    // pthread_mutex_unlock(&mutex);
    struct mem_pool *head_mem_pool = get_member_mpim_mem(head_mem_pool, fpga_id);
    struct mem_pool *curr_mem_pool = get_member_mpim_mem(curr_mem_pool, fpga_id);
    nalloc_pages = size / PAGE_SIZE ? (size / PAGE_SIZE) : 1;
    nalloc_pages = size % PAGE_SIZE ? nalloc_pages + 1 : nalloc_pages;
    // if (nalloc_pages != 1) nalloc_pages = size % PAGE_SIZE ? nalloc_pages + 1 : nalloc_pages;
    PIM_MEM_LOG("\n pim_malloc, size = %lu (%d-page) --> \n", size, nalloc_pages);

    if (size > HUGE_PAGE_SIZE) {
        /* Large mem_pool allocator */
        uint64_t tmp = size / HUGE_PAGE_SIZE;
        uint32_t num_mem_pool = size % HUGE_PAGE_SIZE ? (tmp+1) : tmp;
        PIM_MEM_LOG("Num mem_pool: %d (Size:%lu) \n", num_mem_pool, size);
        uint32_t cnt = 0;
        uint32_t iter = 0;      //jisung
        uint32_t need_more = 0; //jisung
        bool find_mem_pool = false;
        struct mem_pool *tail_mem_pool = get_member_mpim_mem(tail_mem_pool, fpga_id);
        struct mem_pool *need_more_mem_pool = NULL;     //jisung
        list_for_prev(tail_mem_pool) {      
            PIM_MEM_LOG("\t Search large mem_pool[CHUNK:%d] - page:%d \n", tail_mem_pool->index, tail_mem_pool->nfree_pages);
            /* Allocation of large mem_pool must guarantee virtual address continuous */
            if ((tail_mem_pool->nfree_pages == NUM_PAGES) && (tail_mem_pool->prev && (tail_mem_pool->prev->nfree_pages == NUM_PAGES))) {
                cnt++;
            } else {
                /* Lookup from the end: need more memory pools to complete memory space for the request */
                if (iter == 0 && tail_mem_pool->nfree_pages == NUM_PAGES){
                        need_more_mem_pool = tail_mem_pool;
                        need_more = num_mem_pool - cnt - 1;
                        PIM_MEM_LOG("\t\t Need %d more mem_pools if start from tail\n", need_more);
                    }
                cnt = 0;
                iter++;
            }
            /* Allocatable space was detected */
            if (cnt == (num_mem_pool - 1)) {
                find_mem_pool = true;
                tail_mem_pool = tail_mem_pool->prev;
                break;
            }
        }
        /* Allocatable space was detected */
        if (find_mem_pool) {
            ret_ptr = tail_mem_pool->head_page[0].addr;
            update_member_mpim_mem(curr_mem_pool, tail_mem_pool, fpga_id);            
            PIM_MEM_LOG("\t\t Find large mem_pool [CHUNK:%d] (num_mem_pool:%d)\n", tail_mem_pool->index, num_mem_pool);
            list_for_next_value(tail_mem_pool, num_mem_pool) {
                tail_mem_pool->large_mem_pool = true;
                tail_mem_pool->nfree_pages = 0;
                if (i==(num_mem_pool-1))    tail_mem_pool->large_mem_pool = false;
            }
        /* Allocation from the end: need 1 mem_pool more */
	    } else if (need_more_mem_pool !=NULL && need_more == 1) {
            ret_ptr = need_more_mem_pool->head_page[0].addr;
            tail_mem_pool = get_member_mpim_mem(tail_mem_pool, fpga_id);
            struct mem_pool *tmp_mem_pool = _multi_mem_pool_allocation(fpga_id);
            update_member_mpim_mem(tail_mem_pool, tmp_mem_pool, fpga_id);
            update_member_mpim_mem(curr_mem_pool, need_more_mem_pool, fpga_id);
            tail_mem_pool->next = tmp_mem_pool;
            tmp_mem_pool->prev = tail_mem_pool;
            list_for_next_value(need_more_mem_pool, num_mem_pool) {
                need_more_mem_pool->large_mem_pool = true;
                need_more_mem_pool->nfree_pages = 0;
                if (i==(num_mem_pool-1))    need_more_mem_pool->large_mem_pool = false;
            }
        /* Allocation from the end: need N mem_pool more */
        } else if (need_more_mem_pool !=NULL && need_more > 1) {
            ret_ptr = need_more_mem_pool->head_page[0].addr;
            tail_mem_pool = get_member_mpim_mem(tail_mem_pool, fpga_id);
            struct mem_pool *tmp_mem_pool = _multi_large_mem_pool_allocation(need_more_mem_pool, need_more, fpga_id);
            update_member_mpim_mem(curr_mem_pool, need_more_mem_pool, fpga_id);
            tail_mem_pool->next = tmp_mem_pool;
            tmp_mem_pool->prev = tail_mem_pool;
            list_for_next_value(need_more_mem_pool, num_mem_pool) {
                need_more_mem_pool->large_mem_pool = true;
                need_more_mem_pool->nfree_pages = 0;
                if (i==(num_mem_pool-1))    need_more_mem_pool->large_mem_pool = false;
            }
        } else {
            /* If there is no large mem_pool with enough page, mem_pool expansion is required */
            struct mem_pool *tmp_mem_pool = _multi_large_mem_pool_allocation(curr_mem_pool, num_mem_pool, fpga_id);//
            ret_ptr = tmp_mem_pool->head_page[0].addr;
            update_member_mpim_mem(curr_mem_pool, tmp_mem_pool, fpga_id);
        }
#ifdef MEM_USE_TRACK
        // Test alloc
        total_alloc_size += num_mem_pool*HUGE_PAGE_SIZE;
#endif
        goto done;
    } else if (nalloc_pages > curr_mem_pool->nfree_pages) {
        PIM_MEM_LOG("Full (%d / %d)\n", nalloc_pages, curr_mem_pool->nfree_pages);
        goto find_mem_pool;
    } else {
        /* 
            Scenario 1: Not a large mem_pool size
            Scenario 2: There is enough page in the curr_mem_pool
         */
        head_mem_pool = curr_mem_pool;
    }

find_mem_pool:
    /* Search if there are enough free_lists from head to tail mem_pool */
    list_for_next(head_mem_pool) {
        PIM_MEM_LOG("\t Search mem_pool[CHUNK:%d] - num_free:%d/512 \n", head_mem_pool->index, head_mem_pool->nfree_pages);
        if (nalloc_pages <= head_mem_pool->nfree_pages) {
            PIM_MEM_LOG("\t Find [CHUNK:%d]\n", head_mem_pool->index);
            break;
        }
    }
    if (head_mem_pool == NULL) {
        /* If there is no mem_pool with enough page, mem_pool expansion is required */
        PIM_MEM_LOG("\t\tCould not find mem_pool with enough page\n");
        struct mem_pool *new_mem_pool = _multi_mem_pool_allocation(fpga_id);
        list_add_tail(new_mem_pool, head_mem_pool, get_member_mpim_mem(tail_mem_pool, fpga_id));
        update_member_mpim_mem(tail_mem_pool, head_mem_pool, fpga_id);
    }
    idx = _set_bit_vector(head_mem_pool, nalloc_pages);
    PIM_MEM_LOG("\t\tset_used[CHUNK:%d] nalloc_pages: %d, (idx:%d) \n", head_mem_pool->index, nalloc_pages, idx);
    if (idx < 0) {
        head_mem_pool = head_mem_pool->next;
        goto find_mem_pool;
    }
    ret_ptr = head_mem_pool->head_page[idx].addr;
    head_mem_pool->head_page[idx].alloc_size = nalloc_pages;
    update_member_mpim_mem(curr_mem_pool, head_mem_pool, fpga_id);
#ifdef MEM_USE_TRACK
    // Test alloc
    total_alloc_size += nalloc_pages*PAGE_SIZE;
#endif
done:
    PIM_MEM_LOG(" Complete return VA: %p (Remain page: %d/%d) (ALLOC_SIZE:%d) \n\n", ret_ptr,  head_mem_pool->nfree_pages, NUM_PAGES, head_mem_pool->head_page[idx].alloc_size);
    
    /* Unlock mem_pool */
    // pthread_mutex_unlock(&mem_pool_list[fpga_id].lock);

#ifdef MEM_USE_TRACK
    total_size[fpga_id] += total_alloc_size;
    printf("****===> PIM Memory Alloction at Device: #%d Size: %uB Addr: 0x%x Used_Space: %u\n", fpga_id, nalloc_pages*PAGE_SIZE, ret_ptr, total_size[fpga_id]);
#endif
    return ret_ptr;
#endif
}

/* 
 * External function
 * pim_free function does not really free the FPGA DRAM
 * Only initialization of mem_pool and page structure is executed
 */
void mpim_free(void *addr, int fpga_id)
{
#ifndef __PIM_MALLOC__
    free(addr);
#else
    /* Lock mem_pool */
    // pthread_mutex_lock(&mem_pool_list[fpga_id].lock);

    struct mem_pool *head_mem_pool = get_member_mpim_mem(head_mem_pool, fpga_id); // TODO: check if fpga_id matches id of addr
    // Test Free
    size_t total_free_size = 0;

    PIM_MEM_LOG("\n pim_free, addr: %lx\n", (uint64_t)addr);
    /* 
     * Find free page
     */
    list_for_next(head_mem_pool) {
        // First, find mem_pool using address range [mem_pool_start < addr < mem_pool_end]
        PIM_MEM_LOG("Search CHUNK[%d] - %p \n", head_mem_pool->index, head_mem_pool->start_va);
        if ((head_mem_pool->start_va <= addr) &&
            ((head_mem_pool->start_va + HUGE_PAGE_SIZE - 1) >= addr)) {

            if (head_mem_pool->large_mem_pool == true) {
                goto need_free;
            }
            // Second, calculate page address
            uint64_t page_idx = (((uint64_t)(addr - head_mem_pool->start_va)) >> PAGE_SHIFT);
            struct page *curr_page= &head_mem_pool->head_page[page_idx];
            uint32_t free_size = curr_page->alloc_size;
            if (free_size == 0) {
                ASSERT_PRINT("Free size can never be zero! \n");
            }
            PIM_MEM_LOG("\tFind Addr [CHUNK:%d][LIST:%d - %p] (VA: %p, HEAD: %p) \n", head_mem_pool->index, curr_page->index, curr_page, head_mem_pool->start_va, head_mem_pool->head_page);
            PIM_MEM_LOG("\t                     (free_size:%u, page_idx:%lu)\n", free_size, page_idx);
            
#ifdef MEM_USE_TRACK
            // Test Free
            total_free_size += free_size*PAGE_SIZE;
            // total_size[fpga_id] -= free_size*PAGE_SIZE;
            // printf("****<=== PIM Memory Free at Device: #%d Addr: 0x%x ", fpga_id, addr);
            // printf("free_size: %u Used_Space: %u\n", free_size*PAGE_SIZE, total_size[fpga_id]);
#endif
            _clr_bit_vector(head_mem_pool, page_idx, free_size);
            goto done_free;
        }
    }
    ASSERT_PRINT("Free address not found ! \n");
need_free:
    list_for_next(head_mem_pool) {
        head_mem_pool->nfree_pages = NUM_PAGES;
        // Test Free
        total_free_size += NUM_PAGES*PAGE_SIZE;
        PIM_MEM_LOG("\t\t CON'T Find Addr [CHUNK:%d] (VA: %p) (Free:%u/512) \n", head_mem_pool->index, head_mem_pool->start_va, head_mem_pool->nfree_pages);
        if (!head_mem_pool->large_mem_pool) {
            goto done_free;
        }
        else {
            head_mem_pool->large_mem_pool = false;
        }
    }
done_free:
#ifdef MEM_USE_TRACK
    total_size[fpga_id] -= total_free_size;
    /* Unlock mem_pool */
    // pthread_mutex_unlock(&mem_pool_list[fpga_id].lock);
    printf("****<=== PIM Memory Free at Device: #%d Addr: 0x%x ", fpga_id, addr);
    printf("free_size: %u Used_Space: %u\n", total_free_size, total_size[fpga_id]);
#endif
#endif
    return;
}

/* _real_free function munmap the FPGA DRAM and initialize global mem_pool
 * When the process that called the pim_malloc function ends, this function is automatically executed
 * TODO: atexit function change to GC
 */
void _multi_real_free(void)
{
    PIM_MEM_LOG("\n EXIT (PID: %d)\n", getpid());
    int i;
    for(i = 0; i < totalDevNum; i++) {
        struct mem_pool *head_mem_pool = get_member_mpim_mem(head_mem_pool, i);
        struct mem_pool *tmp;

        while (head_mem_pool != NULL)
        {
            tmp = head_mem_pool;
            head_mem_pool = head_mem_pool->next;
            struct page *head_page = tmp->head_page;
            PIM_MEM_LOG("Free Chunk [%d] %p, %p \n", tmp->index, tmp, head_page);
            free(head_page);
            PIM_MEM_LOG("\n");
            free(tmp);
        }    
        init_mpim_mem(i);
    }
    __release_mpim_drv();
}


void* _pim_malloc(struct malloc_args in)
{

    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    void *ret_ptr = mpim_malloc(in.size, fpga_id_out);
    return ret_ptr;
    // int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    // return mpim_malloc(in.size, fpga_id_out);
}
void _pim_free(struct addr_args in)
{
    int fpga_id_out = in.fpga_id? in.fpga_id : 0;
    mpim_free(in.addr, fpga_id_out);
}
// #include "pim_mem.h"

// void _multi_real_free(void);

// static size_t total_size[2] = {0, 0}; // can set dynamically using totaldevnum
// static _pim_mem *mem_pool_list;
// /*
//  * Find the first occurrence of find in s, where the search is limited to the
//  * first slen characters of s.
//  */
// static inline char *_strnstr(const char *s, const char *find, size_t slen)
// {
//     char c, sc;
//     size_t len;

//     if ((c = *find++) != '\0') {
//         len = strlen(find);
//         do {
//             do {
//                 if (slen-- < 1 || (sc = *s++) == '\0')
//                     return (NULL);
//             } while (sc != c);
//             if (len > slen)
//                 return (NULL);
//         } while (strncmp(s, find, len) != 0);
//         s--;
//     }
//     return ((char *)s);
// }

// /* Set the value of the bit_vector to 1 as many as the number of nalloc_pages 
//  * bit_vector value
//  *   1 means in use
//  *   0 means free
//  */
// static inline int _set_bit_vector(struct mem_pool *curr_mem_pool, int nalloc_pages)
// {
//     uint64_t idx = -1;
//     char free_flag[nalloc_pages + 1];
//     memset(free_flag, '0', nalloc_pages * sizeof(char));
//     free_flag[nalloc_pages] = '\0';

//     PIM_MEM_LOG("\t  set_bit_vector\n");
//     PIM_MEM_LOG("BIT[%d]:  %s \n", curr_mem_pool->index, curr_mem_pool->bit_vector);
//     PIM_MEM_LOG("alloc: %d \n", nalloc_pages);
//     /* 
//      * Divide into 2 rounds to reduce search complexity.
//      * First round: find enough page from current index to end [idx:512]
//      */
//     char *ptr =strstr(curr_mem_pool->bit_vector + curr_mem_pool->curr_idx, free_flag);
//     if (ptr) {
//         char used_flag[nalloc_pages + 1];
//         memset(used_flag, '1', nalloc_pages * sizeof(char));
//         used_flag[nalloc_pages] = '\0';
//         idx = (uint64_t)ptr - (uint64_t)(curr_mem_pool->bit_vector);
//         strncpy(curr_mem_pool->bit_vector + idx, used_flag, nalloc_pages);
//         PIM_MEM_LOG("used: %s \n", used_flag);
//         PIM_MEM_LOG("free: %s \n", free_flag);
//         PIM_MEM_LOG("BIT[%d]:  %s \n", curr_mem_pool->index, curr_mem_pool->bit_vector);
//         curr_mem_pool->nfree_pages = curr_mem_pool->nfree_pages - nalloc_pages;
//         curr_mem_pool->curr_idx = idx + nalloc_pages;
//     } else {
//         /* Second round: find enough page from start to current index [0:idx-1] */
//         PIM_MEM_LOG("NO FIND first round \n");
//         char *ptr =_strnstr(curr_mem_pool->bit_vector, free_flag, curr_mem_pool->curr_idx - 1);
//         if (ptr) {
//             char used_flag[nalloc_pages + 1];
//             memset(used_flag, '1', nalloc_pages * sizeof(char));
//             used_flag[nalloc_pages] = '\0';
//             idx = (uint64_t)ptr - (uint64_t)(curr_mem_pool->bit_vector);
//             strncpy(curr_mem_pool->bit_vector + idx, used_flag, nalloc_pages);
//             PIM_MEM_LOG("BIT[%d]:  %s \n", curr_mem_pool->index, curr_mem_pool->bit_vector);
//             curr_mem_pool->nfree_pages = curr_mem_pool->nfree_pages - nalloc_pages;
//             curr_mem_pool->curr_idx = idx + nalloc_pages;
//         } else {
//             PIM_MEM_LOG("NO FIND second round \n");
//         }
//     }
//     return idx;
// }

// /* Clear the value of the bit_vector to 0 as many as the number of nalloc_pages */
// static inline void _clr_bit_vector(struct mem_pool *mem_pool, int idx, int nalloc_pages)
// {
//     char free_flag[nalloc_pages + 1];
//     memset(free_flag, '0', nalloc_pages * sizeof(char));
//     free_flag[nalloc_pages] = '\0';

//     PIM_MEM_LOG("\nclr_bit_vector\n");
//     PIM_MEM_LOG("BIT[%d]:  %s \n", mem_pool->index, mem_pool->bit_vector);
//     PIM_MEM_LOG("free: %s \n", free_flag);
//     PIM_MEM_LOG("alloc: %d \n", nalloc_pages);
//     PIM_MEM_LOG("IDX: %d \n", idx);

//     strncpy(mem_pool->bit_vector + idx, free_flag, nalloc_pages);
//     mem_pool->curr_idx = idx;
//     mem_pool->nfree_pages = mem_pool->nfree_pages + nalloc_pages;    

//     PIM_MEM_LOG("BIT[%d]:  %s \n", mem_pool->index, mem_pool->bit_vector);
//     PIM_MEM_LOG("CURR_IDX: %u \n", mem_pool->curr_idx);

// }

// /* TODO: implement atomic RD/WR */
// #define update_member_mpim_mem(member, value, fpga_id) \
//     mem_pool_list[fpga_id].member = value

// #define get_member_mpim_mem(member, fpga_id) \
//     mem_pool_list[fpga_id].member

// #define init_mpim_mem(fpga_id)       \
//     mem_pool_list[fpga_id].ntotal_mem_pools = 0; \
//     mem_pool_list[fpga_id].curr_mem_pool = NULL; \
//     mem_pool_list[fpga_id].head_mem_pool = NULL; \
//     mem_pool_list[fpga_id].tail_mem_pool = NULL;

// /* New mem_pool allocator */
// static struct mem_pool *_multi_mem_pool_allocation(int fpga_id)
// {
//     struct mem_pool *mem_pool_obj;
//     mem_pool_obj = malloc(sizeof(struct mem_pool));
//     if (mem_pool_obj == NULL) {
//         ASSERT_PRINT("Internal Error during malloc \n");
//     }

//     // Memory allocation to PIM driver (Huge page or IO mapping)
//     mem_pool_obj->start_va = multi_mmap_drv(HUGE_PAGE_SIZE, fpga_id);
//     if (mem_pool_obj->start_va == NULL) {
//         ASSERT_PRINT("Out-of memory \n");
//     }

//     struct page *page_obj = (struct page *)malloc(sizeof(struct page) * NUM_PAGES);
//     if (page_obj == NULL) {
//         printf("Internal Error during malloc \n");
//         return NULL;
//     }    
//     for (int i = 0; i < NUM_PAGES; i++) {
//         page_obj[i].addr = mem_pool_obj->start_va + (i * PAGE_SIZE);
//     #ifdef DEBUG_PIM_MEM
//         page_obj[i].index = i;
//     #endif
//     }
//     mem_pool_obj->next = mem_pool_obj->prev = NULL;
//     mem_pool_obj->curr_idx = 0;
//     memset(mem_pool_obj->bit_vector, '0', NUM_PAGES * sizeof(char));
//     mem_pool_obj->bit_vector[NUM_PAGES] = '\0';    
//     mem_pool_obj->head_page = &page_obj[0];
//     mem_pool_obj->nfree_pages = NUM_PAGES;
//     #ifdef DEBUG_PIM_MEM
//         mem_pool_obj->index = get_member_mpim_mem(ntotal_mem_pools, fpga_id);
//     #endif        
//     mem_pool_obj->large_mem_pool = false;
//     update_member_mpim_mem(ntotal_mem_pools, get_member_mpim_mem(ntotal_mem_pools, fpga_id) + 1, fpga_id);
//     PIM_MEM_LOG("    New mem_pool: %d - nfree_pages:%d/%d \n", mem_pool_obj->index, mem_pool_obj->nfree_pages, NUM_PAGES);
//     return mem_pool_obj;
// }

// /* New mem_pool allocator */
// static struct mem_pool *_multi_large_mem_pool_allocation(struct mem_pool *curr_mem_pool, uint32_t num_mem_pool, int fpga_id)
// {
//     void *large_mem_pool = NULL; 
//     struct mem_pool *ret_obj = NULL;
//     struct mem_pool *mem_pool_obj[num_mem_pool];
//     struct page *page_obj;
//     PIM_MEM_LOG("Large mem_pool :%d \n", num_mem_pool);

//     large_mem_pool = multi_mmap_drv(HUGE_PAGE_SIZE * num_mem_pool, fpga_id);
//     if (large_mem_pool == NULL) {
//         ASSERT_PRINT("Out-of memory (large-mem_pool) \n");
//     }
//     for (size_t i = 0; i < num_mem_pool; i++) {
//         mem_pool_obj[i] = malloc(sizeof(struct mem_pool));
//         if (mem_pool_obj[i] == NULL) {
//             ASSERT_PRINT("Internal Error during malloc \n");
//         }
//         mem_pool_obj[i]->start_va = large_mem_pool + (i * HUGE_PAGE_SIZE);
//         PIM_MEM_LOG("start_va[%lu]: %p \n", i, mem_pool_obj[i]->start_va);
//         mem_pool_obj[i]->next = mem_pool_obj[i]->prev = NULL;
//         page_obj = (struct page *)malloc(sizeof(struct page) * NUM_PAGES);
//         if (page_obj == NULL) {
//             ASSERT_PRINT("Internal Error during malloc \n");
//         }        
//         for (int j = 0; j < NUM_PAGES; j++) {
//             page_obj[j].addr = mem_pool_obj[i]->start_va + (j * PAGE_SIZE); // Address is virtual address
//         #ifdef DEBUG_PIM_MEM
//             page_obj[j].index = j;
//         #endif
//         }
//         mem_pool_obj[i]->curr_idx = 0;
//         memset(mem_pool_obj[i]->bit_vector, '0', NUM_PAGES * sizeof(char));
//         mem_pool_obj[i]->bit_vector[NUM_PAGES] = '\0';
//         mem_pool_obj[i]->head_page = &page_obj[0];
//         mem_pool_obj[i]->nfree_pages = 0;
//         mem_pool_obj[i]->large_mem_pool = true;
//         #ifdef DEBUG_PIM_MEM        
//             mem_pool_obj[i]->index = get_member_mpim_mem(ntotal_mem_pools, fpga_id);
//         #endif
//         update_member_mpim_mem(ntotal_mem_pools, get_member_mpim_mem(ntotal_mem_pools, fpga_id) + 1, fpga_id);
//         if (i == 0) {
//             ret_obj = mem_pool_obj[i];
//             PIM_MEM_LOG("    mem_pool_obj[%lu]:%p (RET: %d) \n", i, mem_pool_obj[i], ret_obj->index);
//         }
//         PIM_MEM_LOG("    New Large mem_pool: %d (nfree_pages:%d/%d)\n", mem_pool_obj[i]->index, mem_pool_obj[i]->nfree_pages, NUM_PAGES);

//         PIM_MEM_LOG("    NEW: %d, CURR: %d , TAIL: %d \n", mem_pool_obj[i]->index, curr_mem_pool->index, get_member_mpim_mem(tail_mem_pool, fpga_id)->index);
//         list_add_tail(mem_pool_obj[i], curr_mem_pool, get_member_mpim_mem(tail_mem_pool, fpga_id));
//         PIM_MEM_LOG("    NEW: %d, CURR: %d , TAIL: %d \n", mem_pool_obj[i]->index, curr_mem_pool->index, get_member_mpim_mem(tail_mem_pool, fpga_id)->index);
//         update_member_mpim_mem(tail_mem_pool, curr_mem_pool, fpga_id);
//         PIM_MEM_LOG("    CURR: %d , TAIL: %d (RET: %d) \n", curr_mem_pool->index, get_member_mpim_mem(tail_mem_pool, fpga_id)->index, ret_obj->index);
//     }
//     // Clear large_mem_pool flag of the last generated mem_pool
//     mem_pool_obj[num_mem_pool-1]->large_mem_pool = false;
//     ret_obj->head_page[0].alloc_size = NUM_PAGES;
//     return ret_obj;
// }

// /* External function */
// void *mpim_malloc(size_t size, int fpga_id)
// {
// #ifndef __PIM_MALLOC__
//     //return  mmap(0x0, size, (PROT_READ | PROT_WRITE), MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
//     return malloc(size);
// #else
//     void *ret_ptr=NULL;
//     int idx = 0;
//     int i;
//     uint nalloc_pages = 0;    
//     /* Initialization mem_pool */
//     if (mem_pool_list == NULL) {
//         // Initialization PIM driver
//         __init_mpim_drv();

//         mem_pool_list = (_pim_mem*)malloc(sizeof(_pim_mem)*totalDevNum);
//         atexit(_multi_real_free);
//         for (i = 0; i < totalDevNum; i++) {
//             struct mem_pool *new_mem_pool = _multi_mem_pool_allocation(i);

//             update_member_mpim_mem(curr_mem_pool, new_mem_pool, i);
//             update_member_mpim_mem(head_mem_pool, new_mem_pool, i);
//             update_member_mpim_mem(tail_mem_pool, new_mem_pool, i);
//         }
//     }
//     struct mem_pool *head_mem_pool = get_member_mpim_mem(head_mem_pool, fpga_id);
//     struct mem_pool *curr_mem_pool = get_member_mpim_mem(curr_mem_pool, fpga_id);
//     nalloc_pages = size / PAGE_SIZE ? (size / PAGE_SIZE) : 1;
//     nalloc_pages = size % PAGE_SIZE ? nalloc_pages + 1 : nalloc_pages;
//     // if (nalloc_pages != 1) nalloc_pages = size % PAGE_SIZE ? nalloc_pages + 1 : nalloc_pages;
//     PIM_MEM_LOG("\n pim_malloc, size = %lu (%d-page) --> \n", size, nalloc_pages);

//     if (size > HUGE_PAGE_SIZE) {
//         /* Large mem_pool allocator */
//         uint64_t tmp = size / HUGE_PAGE_SIZE;
//         uint32_t num_mem_pool = size % HUGE_PAGE_SIZE ? (tmp+1) : tmp;
//         PIM_MEM_LOG("Num mem_pool: %d (Size:%lu) \n", num_mem_pool, size);
//         uint32_t cnt = 0;
//         bool find_mem_pool = false;
//         struct mem_pool *tail_mem_pool = get_member_mpim_mem(tail_mem_pool, fpga_id);
//         list_for_next(tail_mem_pool) {
//             PIM_MEM_LOG("\t Search large mem_pool[CHUNK:%d] - page:%d \n", tail_mem_pool->index, tail_mem_pool->nfree_pages);
//             /* Allocation of large mem_pool must guarantee virtual address continuous */
//             if ((tail_mem_pool->nfree_pages == NUM_PAGES) && (tail_mem_pool->prev && (tail_mem_pool->prev->nfree_pages == NUM_PAGES))) {
//                 cnt++;
//             } else {
//                 cnt = 0;
//             }
//             if (cnt == (num_mem_pool - 1)) {
//                 find_mem_pool = true;
//                 tail_mem_pool = tail_mem_pool->prev;
//                 break;
//             }
//         }
//         if (find_mem_pool) {
//             ret_ptr = tail_mem_pool->head_page[0].addr;
//             update_member_mpim_mem(curr_mem_pool, tail_mem_pool, fpga_id);            
//             PIM_MEM_LOG("\t\t Find large mem_pool [CHUNK:%d] (num_mem_pool:%d)\n", tail_mem_pool->index, num_mem_pool);
//             list_for_next_value(tail_mem_pool, num_mem_pool) {
//                 tail_mem_pool->large_mem_pool = true;
//                 tail_mem_pool->nfree_pages = 0;
//             }
//         } else {
//             /* If there is no large mem_pool with enough page, mem_pool expansion is required */
//             struct mem_pool *tmp_mem_pool = _multi_large_mem_pool_allocation(curr_mem_pool, num_mem_pool, fpga_id);//
//             ret_ptr = tmp_mem_pool->head_page[0].addr;
//             update_member_mpim_mem(curr_mem_pool, tmp_mem_pool, fpga_id);
//         }
//         goto done;
//     } else if (nalloc_pages > curr_mem_pool->nfree_pages) {
//         PIM_MEM_LOG("Full (%d / %d)\n", nalloc_pages, curr_mem_pool->nfree_pages);
//         goto find_mem_pool;
//     } else {
//         /* 
//             Scenario 1: Not a large mem_pool size
//             Scenario 2: There is enough page in the curr_mem_pool
//          */
//         head_mem_pool = curr_mem_pool;
//     }

// find_mem_pool:
//     /* Search if there are enough free_lists from head to tail mem_pool */
//     list_for_next(head_mem_pool) {
//         PIM_MEM_LOG("\t Search mem_pool[CHUNK:%d] - num_free:%d/512 \n", head_mem_pool->index, head_mem_pool->nfree_pages);
//         if (nalloc_pages <= head_mem_pool->nfree_pages) {
//             PIM_MEM_LOG("\t Find [CHUNK:%d]\n", head_mem_pool->index);
//             break;
//         }
//     }
//     if (head_mem_pool == NULL) {
//         /* If there is no mem_pool with enough page, mem_pool expansion is required */
//         PIM_MEM_LOG("\t\tCould not find mem_pool with enough page\n");
//         struct mem_pool *new_mem_pool = _multi_mem_pool_allocation(fpga_id);
//         list_add_tail(new_mem_pool, head_mem_pool, get_member_mpim_mem(tail_mem_pool, fpga_id));
//         update_member_mpim_mem(tail_mem_pool, head_mem_pool, fpga_id);
//     }
//     idx = _set_bit_vector(head_mem_pool, nalloc_pages);
//     PIM_MEM_LOG("\t\tset_used[CHUNK:%d] nalloc_pages: %d, (idx:%d) \n", head_mem_pool->index, nalloc_pages, idx);
//     if (idx < 0) {
//         head_mem_pool = head_mem_pool->next;
//         goto find_mem_pool;
//     }
//     ret_ptr = head_mem_pool->head_page[idx].addr;
//     head_mem_pool->head_page[idx].alloc_size = nalloc_pages;
//     update_member_mpim_mem(curr_mem_pool, head_mem_pool, fpga_id);
// done:
//     PIM_MEM_LOG(" Complete return VA: %p (Remain page: %d/%d) (ALLOC_SIZE:%d) \n\n", ret_ptr,  head_mem_pool->nfree_pages, NUM_PAGES, head_mem_pool->head_page[idx].alloc_size);
    
//     total_size[fpga_id] += nalloc_pages*PAGE_SIZE;
//     printf("****===> PIM Memory Alloction at Device: #%d Size: %uB Addr: 0x%x Used_Space: %u\n", fpga_id, nalloc_pages*PAGE_SIZE, ret_ptr, total_size[fpga_id]);
//     return ret_ptr;
// #endif
// }

// /* 
//  * External function
//  * pim_free function does not really free the FPGA DRAM
//  * Only initialization of mem_pool and page structure is executed
//  */
// void mpim_free(void *addr, int fpga_id)
// {
// #ifndef __PIM_MALLOC__
//     free(addr);
// #else
//     struct mem_pool *head_mem_pool = get_member_mpim_mem(head_mem_pool, fpga_id); // TODO: check if fpga_id matches id of addr

//     PIM_MEM_LOG("\n pim_free, addr: %lx\n", (uint64_t)addr);
//     /* 
//      * Find free page
//      */
//     list_for_next(head_mem_pool) {
//         // First, find mem_pool using address range [mem_pool_start < addr < mem_pool_end]
//         PIM_MEM_LOG("Search CHUNK[%d] - %p \n", head_mem_pool->index, head_mem_pool->start_va);
//         if ((head_mem_pool->start_va <= addr) &&
//             ((head_mem_pool->start_va + HUGE_PAGE_SIZE - 1) >= addr)) {

//             if (head_mem_pool->large_mem_pool == true) {
//                 goto need_free;
//             }
//             // Second, calculate page address
//             uint64_t page_idx = (((uint64_t)(addr - head_mem_pool->start_va)) >> PAGE_SHIFT);
//             struct page *curr_page= &head_mem_pool->head_page[page_idx];
//             uint32_t free_size = curr_page->alloc_size;
//             if (free_size == 0) {
//                 ASSERT_PRINT("Free size can never be zero! \n");
//             }
//             PIM_MEM_LOG("\tFind Addr [CHUNK:%d][LIST:%d - %p] (VA: %p, HEAD: %p) \n", head_mem_pool->index, curr_page->index, curr_page, head_mem_pool->start_va, head_mem_pool->head_page);
//             PIM_MEM_LOG("\t                     (free_size:%u, page_idx:%lu)\n", free_size, page_idx);
//             total_size[fpga_id] -= free_size*PAGE_SIZE;
//             printf("****<=== PIM Memory Free at Device: #%d Addr: 0x%x ", fpga_id, addr);
//             printf("free_size: %u Used_Space: %u\n", free_size*PAGE_SIZE, total_size[fpga_id]);
//             _clr_bit_vector(head_mem_pool, page_idx, free_size);
//             goto done_free;
//         }
//     }
//     ASSERT_PRINT("Free address not found ! \n");
// need_free:
//     list_for_next(head_mem_pool) {
//         head_mem_pool->nfree_pages = NUM_PAGES;
//         PIM_MEM_LOG("\t\t CON'T Find Addr [CHUNK:%d] (VA: %p) (Free:%u/512) \n", head_mem_pool->index, head_mem_pool->start_va, head_mem_pool->nfree_pages);
//         if (!head_mem_pool->large_mem_pool) {
//             goto done_free;
//         }
//         else {
//             head_mem_pool->large_mem_pool = false;
//         }
//     }
// done_free:
// #endif
//     return;
// }

// /* _real_free function munmap the FPGA DRAM and initialize global mem_pool
//  * When the process that called the pim_malloc function ends, this function is automatically executed
//  * TODO: atexit function change to GC
//  */
// void _multi_real_free(void)
// {
//     PIM_MEM_LOG("\n EXIT (PID: %d)\n", getpid());
//     int i;
//     for(i = 0; i < totalDevNum; i++) {
//         struct mem_pool *head_mem_pool = get_member_mpim_mem(head_mem_pool, i);
//         struct mem_pool *tmp;

//         while (head_mem_pool != NULL)
//         {
//             tmp = head_mem_pool;
//             head_mem_pool = head_mem_pool->next;
//             struct page *head_page = tmp->head_page;
//             PIM_MEM_LOG("Free Chunk [%d] %p, %p \n", tmp->index, tmp, head_page);
//             free(head_page);
//             PIM_MEM_LOG("\n");
//             free(tmp);
//         }    
//         init_mpim_mem(i);
//     }
//     __release_mpim_drv();
// }


// void* _pim_malloc(struct malloc_args in)
// {
//     int fpga_id_out = in.fpga_id? in.fpga_id : 0;
//     return mpim_malloc(in.size, fpga_id_out);
// }
// void _pim_free(struct addr_args in)
// {
//     int fpga_id_out = in.fpga_id? in.fpga_id : 0;
//     mpim_free(in.addr, fpga_id_out);
// }