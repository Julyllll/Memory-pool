
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct mem_block_s {
    int id;
    int is_use;
    struct mem_block_s *prev;
    struct mem_block_s *next;
    char buff[];
} mem_block_t;

typedef struct mem_pool_s {
    int block_size;
    int cur_blocks;
    int max_blocks;
    mem_block_t *first;
    mem_block_t *last;
} mem_pool_t;

void *mem_pool_malloc(mem_pool_t *mem_pool, int size) {
    if (!mem_pool) return NULL;
    if (size > mem_pool->block_size) {
        return calloc(1, size);
    }

    mem_block_t *cur_block = NULL;
    if (mem_pool->first == NULL || mem_pool->first->is_use == 1) {
        if (mem_pool->cur_blocks >= mem_pool->max_blocks) {
            return calloc(1, size);
        }

        cur_block = (mem_block_t *)calloc(1, sizeof(mem_block_t) + mem_pool->block_size);
        if (cur_block == NULL) {
            return NULL;
        }

        mem_pool->cur_blocks++;
        cur_block->id = mem_pool->cur_blocks;

        if (mem_pool->first == NULL) {
            mem_pool->first = mem_pool->last = cur_block;
            cur_block->next = cur_block->prev = NULL;
        } else {
            cur_block->prev = NULL;
            cur_block->next = mem_pool->first;
            mem_pool->first->prev = cur_block;
            mem_pool->first = cur_block;
            mem_pool->last->next = NULL;
        }
    }

    cur_block = mem_pool->first;
    mem_pool->first = cur_block->next;
    if (mem_pool->first == NULL) {
        mem_pool->first = mem_pool->last = cur_block;
        cur_block->next = cur_block->prev = NULL;
    } else {
        cur_block->next = NULL;
        cur_block->prev = mem_pool->last;
        mem_pool->last->next = cur_block;
        mem_pool->last = cur_block;
        mem_pool->first->prev = NULL;
    }
    cur_block->is_use = 1;

    return (void *)cur_block->buff;
}

int mem_pool_free(mem_pool_t *mem_pool, void *buff) {
    if (!buff || !mem_pool || !mem_pool->first) return -1;

    mem_block_t *block_pre = NULL;
    mem_block_t *block = mem_pool->last;
    while (block != NULL) {
        block_pre = block->prev;
        if ((unsigned long)buff == (unsigned long)block->buff) {
            if (block->prev)
                block->prev->next = block->next;
            else
                mem_pool->first = block->next;
            if (block->next)
                block->next->prev = block->prev;
            else
                mem_pool->last = block->prev;

            if (mem_pool->first == NULL) {
                mem_pool->first = mem_pool->last = block;
                block->prev = block->next = NULL;
            } else {
                block->prev = NULL;
                block->next = mem_pool->first;
                mem_pool->first->prev = block;
                mem_pool->first = block;
                mem_pool->last->next = NULL;
            }
            memset(block->buff, 0, mem_pool->block_size);
            block->is_use = 0;
            return 0;
        }
        block = block_pre;
    }

    free(buff); 

    return -1;
}

mem_pool_t *mem_pool_create(int block_size, int max_blocks) {
    mem_pool_t *mem_pool = (mem_pool_t *)calloc(1, sizeof(mem_pool_t));
    if (mem_pool == NULL) return NULL;
    mem_pool->block_size = block_size;
    mem_pool->max_blocks = max_blocks;
    mem_pool->first = mem_pool->last = NULL;
    return mem_pool;
}

int mem_pool_destory(mem_pool_t *mem_pool) {
    if (mem_pool == NULL) return -1;

    mem_block_t *first_next = NULL;
    mem_block_t *first = mem_pool->first;
    while (first) {
        first_next = first->next;
        free(first);
        first = first_next;
    }
    free(mem_pool);
    return 0;
}
