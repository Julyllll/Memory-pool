#ifndef _MEM_POOL_H_
#define _MEM_POOL_H_

#include <pthread.h>

#define MB (1024 * 1024)
#define DEFAULT_POOL_SIZE (64 * MB)

#define ocean_align(d, a) (((d) + (a - 1)) & ~(a - 1))
#define OCEAN_ALIGNMENT sizeof(unsigned long int)

#define BLOCK_POINT_LEN sizeof(void *)
#define USE_LEN sizeof(char)
#define PAGE_INDEX_LEN sizeof(int)
#define BLOCK_HEAD_LEN ocean_align((BLOCK_POINT_LEN * 2 + USE_LEN + PAGE_INDEX_LEN), OCEAN_ALIGNMENT)

//! 我们的block结构是这样的顺序
// typedef struct mem_block_s {
//     struct mem_block_s *prev;
//     struct mem_block_s *next;
//     struct mem_pool_t *mem_pool;
//     char is_use;
//     int block_id;
//     int page_index;
//     char buff[];
// } mem_block_t;

typedef struct mem_page_s {
    int index;
    int block_size;
    char *first;
    char *last;
} mem_page_t;

typedef struct mem_pool_s {
    int cur_size;
    int max_size;
    int top_size;
    int max_block_size;
    int n_pages;
    mem_page_t **pages;
    pthread_mutex_t mutex;
} mem_pool_t;

void *mem_block_malloc(mem_pool_t *mem_pool, int block_size, const char *file, int line);
int mem_block_free(mem_pool_t *mem_pool, void *buff);
int mem_block_get_size(mem_pool_t *mem_pool, void *buff);

mem_page_t *mem_page_create(mem_pool_t *mem_pool, int block_size);
mem_page_t *mem_page_get(mem_pool_t *mem_pool, int page_index, int block_size);
int mem_page_destory(mem_page_t **mem_page);

mem_pool_t *mem_pool_create(int max_size);
int mem_pool_destory(mem_pool_t *mem_pool);
void mem_pool_free_unused(mem_pool_t *mem_pool);
void mem_pool_print_pages(mem_pool_t *mem_pool);
#endif
