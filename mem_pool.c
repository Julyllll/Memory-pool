#include "mem_pool.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void mfree_arg(void *arg) {
    if (!arg) return;
    void *val;

    memcpy(&val, arg, sizeof(val));
    memcpy(arg, &(void *){NULL}, sizeof(val));
    free(val);
}

int madd_dynarray(void *tab_ptr, int *nb_ptr, void *elem) {
    if (!tab_ptr || !nb_ptr || !elem) return -1;
    void **tab;
    memcpy(&tab, tab_ptr, sizeof(tab));
    do {
        void *new_arry = NULL;
        size_t temp_len = *nb_ptr;
        size_t elt_size = sizeof(*tab);
        if (!((*nb_ptr) & ((*nb_ptr) - 1))) {
            temp_len = (*nb_ptr) ? (*nb_ptr) << 1 : 1;
            if (temp_len > ((2147483647) / elt_size))
                temp_len = 0;
            else {
                new_arry = realloc(tab, temp_len * elt_size);
                if (!new_arry)
                    temp_len = 0;
                else
                    tab = new_arry;
            }
        }
        if (temp_len) {
            tab[*nb_ptr] = elem;
            memcpy(tab_ptr, &tab, sizeof(tab));
            *nb_ptr += 1;
        } else {
            return -1;
        }
    } while (0);
    return 0;
}

void *mem_block_malloc(mem_pool_t *mem_pool, int block_size, const char *file, int line) {
    if (block_size <= 0) return NULL;
    if (!mem_pool) return NULL;  // calloc(1, ocean_align(block_size, OCEAN_ALIGNMENT));

    pss_thread_mutex_lock(&mem_pool->mutex);
    mem_page_t *mem_page = NULL;
    mem_page_t *mem_page_temp = NULL;
    block_size = ocean_align(block_size, OCEAN_ALIGNMENT);
    if (block_size > mem_pool->max_block_size) {
        // if (mem_pool->max_block_size * 2 > block_size) block_size = mem_pool->max_block_size * 2;//TODO 是否扩大2倍？
        // block_size = ocean_align(block_size, OCEAN_ALIGNMENT);
        mem_page = mem_page_create(mem_pool, block_size);
    } else {
        mem_page_temp = mem_page_get(mem_pool, -1, block_size);
        // if (mem_page_temp) printf("\n\n~~~~~~~~~get page size :%d,block_size:%d,max_size:%d\n\n", mem_page_temp->block_size, block_size, mem_pool->max_block_size);
        // if (mem_page_temp && mem_page_temp->block_size / 2 >= block_size) 
        //     mem_page = mem_page_create(mem_pool, block_size);
        if (mem_page_temp && mem_page_temp->first && *(mem_page_temp->first + BLOCK_POINT_LEN * 2) == 1 && mem_page_temp->block_size - block_size > sizeof(mem_page_t))
            mem_page = mem_page_create(mem_pool, block_size);
        else
            mem_page = mem_page_temp;
    }

    if (!mem_page) {
        pss_thread_mutex_unlock(&mem_pool->mutex);
        return NULL;  // calloc(1, block_size);
    }

    if (mem_page->first == NULL || *(mem_page->first + BLOCK_POINT_LEN * 2) == 1) {
        char *new_block = (char *)calloc(1, sizeof(char) * (mem_page->block_size + BLOCK_HEAD_LEN));
        if (new_block == NULL) {
            pss_thread_mutex_unlock(&mem_pool->mutex);
            return NULL;
        }
        mem_pool->cur_size += (mem_page->block_size + BLOCK_HEAD_LEN);
        if (mem_pool->top_size < mem_pool->cur_size) mem_pool->top_size = mem_pool->cur_size;
        if (mem_pool->cur_size > mem_pool->max_size) {
            pss_thread_mutex_unlock(&mem_pool->mutex);
            mem_pool_free_unused(mem_pool);  //! 因为此函数里面加锁了，避免死锁
            pss_thread_mutex_lock(&mem_pool->mutex);
        }

        memcpy(new_block + BLOCK_POINT_LEN * 2 + USE_LEN, &mem_page->index, sizeof(mem_page->index));
        
        if (mem_page->first == NULL) {
            mem_page->first = mem_page->last = new_block;
            *(char **)(new_block) = *(char **)(new_block + BLOCK_POINT_LEN) = NULL;
        } else {
            *(char **)(new_block) = NULL;
            *(char **)(new_block + BLOCK_POINT_LEN) = mem_page->first;
            *(char **)(mem_page->first) = new_block;
            mem_page->first = new_block;
            *(char **)(mem_page->last + BLOCK_POINT_LEN) = NULL;
        }
    }

    char *cur_block = mem_page->first;
    mem_page->first = *(char **)(cur_block + BLOCK_POINT_LEN);
    if (mem_page->first == NULL) {
        mem_page->first = mem_page->last = cur_block;
        *(char **)(cur_block) = *(char **)(cur_block + BLOCK_POINT_LEN) = NULL;
    } else {
        *(char **)(cur_block + BLOCK_POINT_LEN) = NULL;
        *(char **)(cur_block) = mem_page->last;
        *(char **)(mem_page->last + BLOCK_POINT_LEN) = cur_block;
        mem_page->last = cur_block;
        *(char **)(mem_page->first) = NULL;
    }
    *(cur_block + BLOCK_POINT_LEN * 2) = 1;

    pss_thread_mutex_unlock(&mem_pool->mutex);
    return (void *)(cur_block + BLOCK_HEAD_LEN);
}

int mem_block_free(mem_pool_t *mem_pool, void *buff) {  
    if (buff && mem_pool) {
        pss_thread_mutex_lock(&mem_pool->mutex);

        char *block = (char *)buff - BLOCK_HEAD_LEN;
        if (block) {
            int page_index = -1;
            if (block + BLOCK_POINT_LEN * 2 + USE_LEN) memcpy(&page_index, block + BLOCK_POINT_LEN * 2 + USE_LEN, sizeof(page_index));
            mem_page_t *mem_page = mem_page_get(mem_pool, page_index, -1);
            if (mem_page) {
                char **block_pre = (char **)block;
                char **block_next = (char **)(block + BLOCK_POINT_LEN);

                if (*block_pre)
                    *(char **)((char *)*block_pre + BLOCK_POINT_LEN) = *block_next;
                else
                    mem_page->first = *block_next;
                if (*block_next)
                    *(char **)*block_next = *block_pre;
                else
                    mem_page->last = *block_pre;

                if (mem_page->first == NULL) {
                    mem_page->first = mem_page->last = block;
                    *block_pre = *block_next = NULL;
                } else {
                    *block_pre = NULL;
                    *block_next = mem_page->first;
                    *(char **)mem_page->first = block;
                    mem_page->first = block;
                    *(char **)(mem_page->last + BLOCK_POINT_LEN) = NULL;
                }
                memset(buff, 0, mem_page->block_size);
                *(block + BLOCK_POINT_LEN * 2) = 0;
                pss_thread_mutex_unlock(&mem_pool->mutex);
                return 0;
            }
        }
        pss_thread_mutex_unlock(&mem_pool->mutex);
    }
    // if (buff) free(buff);
    return -1;
}

int mem_block_get_size(mem_pool_t *mem_pool, void *buff) {  
    int block_size = 0;
    if (buff && mem_pool) {
        pss_thread_mutex_lock(&mem_pool->mutex);
        char *block = (char *)buff - BLOCK_HEAD_LEN;
        if (block) {
            int page_index = -1;
            if (block + BLOCK_POINT_LEN * 2 + USE_LEN) memcpy(&page_index, block + BLOCK_POINT_LEN * 2 + USE_LEN, sizeof(page_index));
            mem_page_t *mem_page = mem_page_get(mem_pool, page_index, -1);
            if (mem_page) {
                block_size = mem_page->block_size;
            }
        }
        pss_thread_mutex_unlock(&mem_pool->mutex);
    }
    return block_size;
}

mem_page_t *mem_page_create(mem_pool_t *mem_pool, int block_size) {
    if (!mem_pool || block_size <= 0) {
        printf("mem page malloc failed,block size :%d.\n", block_size);
        return NULL;
    }

    block_size = ocean_align(block_size, OCEAN_ALIGNMENT);
    int i = 0;
    for (i = 0; i < mem_pool->n_pages; i++) {
        if (mem_pool->pages[i]->block_size == block_size) {  //! 看看是否已有相同的page
            return mem_pool->pages[i];
        }
    }

    mem_page_t *mem_page = (mem_page_t *)calloc(1, sizeof(mem_page_t));
    if (mem_page == NULL) return NULL;

    mem_page->block_size = block_size;
    mem_page->first = mem_page->last = NULL;
    mem_page->index = mem_pool->n_pages;

    if (madd_dynarray(&mem_pool->pages, &mem_pool->n_pages, mem_page) < 0) {
        mfree_arg(&mem_page);
        mem_page = NULL;
    } else if (block_size > mem_pool->max_block_size) {
        mem_pool->max_block_size = block_size;
    }
    return mem_page;
}

mem_page_t *mem_page_get(mem_pool_t *mem_pool, int page_index, int block_size) {
    if (!mem_pool || page_index >= mem_pool->n_pages || block_size > mem_pool->max_block_size) {
        return NULL;
    }
    mem_page_t *mem_page = NULL;
    if (page_index >= 0) {
        mem_page = mem_pool->pages[page_index];
    } else if (block_size > 0) {
        int i = 0;
        for (i = 0; i < mem_pool->n_pages; i++) {
            if (mem_pool->pages[i]->block_size >= block_size) {
                if (!mem_page || mem_page->block_size > mem_pool->pages[i]->block_size) {
                    mem_page = mem_pool->pages[i];
                    // if (mem_page->block_size >= mem_pool->max_block_size) break;//TODO 继续查找接近block size大小的page 避免较大的浪费
                }
            }
        }
    }
    return mem_page;
}

int mem_page_destory(mem_page_t **mem_page) {
    if (!(*mem_page)) return -1;

    mem_page_t *page = *mem_page;

    char *first = page->first;
    char *first_next = NULL;
    while (first) {
        first_next = *(char **)(first + BLOCK_POINT_LEN);
        free(first);
        first = first_next;
    }
    mfree_arg(mem_page);

    return 0;
}

mem_pool_t *mem_pool_create(int max_size) {
    mem_pool_t *mem_pool = (mem_pool_t *)calloc(1, sizeof(mem_pool_t));
    if (mem_pool == NULL) return NULL;
    mem_pool->max_size = max_size > 0 ? max_size : DEFAULT_POOL_SIZE;

#ifdef MTG_ECOS_PLAT
    pss_thread_mutex_attr_t mutex_attr;
    mutex_attr.attr = "mpool";
    pss_thread_mutex_init(&mem_pool->mutex, &mutex_attr);
#else
    pss_thread_mutex_init(&mem_pool->mutex, NULL);
#endif

    return mem_pool;
}

int mem_pool_destory(mem_pool_t *mem_pool) {
    if (mem_pool == NULL) return -1;

    pss_thread_mutex_lock(&mem_pool->mutex);
    int i;
    for (i = 0; i < mem_pool->n_pages; i++) {
        mem_page_destory(&mem_pool->pages[i]);
    }
    mfree_arg(&mem_pool->pages);
    mem_pool->n_pages = 0;
    pss_thread_mutex_unlock(&mem_pool->mutex);
    pss_thread_mutex_destroy(&mem_pool->mutex);

    free(mem_pool);
    return 0;
}

void mem_page_free_unused(mem_pool_t *mem_pool, mem_page_t **mem_page) {
    if (!(*mem_page)) return;

    mem_page_t *page = *mem_page;
    char *block = page->first;
    char *next = NULL;
    char **block_pre = NULL;
    char **block_next = NULL;

    while (block) {
        block_pre = (char **)block;
        block_next = (char **)(block + BLOCK_POINT_LEN);
        next = *(char **)(block + BLOCK_POINT_LEN);

        if (*(block + BLOCK_POINT_LEN * 2) == 0) {  //! 如果is_use标识是0 代表未在使用 则进行系统free
            if (*block_pre)
                *(char **)((char *)*block_pre + BLOCK_POINT_LEN) = *block_next;
            else
                page->first = *block_next;
            if (*block_next)
                *(char **)*block_next = *block_pre;
            else
                page->last = *block_pre;

            mem_pool->cur_size -= (page->block_size + BLOCK_HEAD_LEN);
            free(block);
        }
        block = next;
    }
}

void mem_pool_free_unused(mem_pool_t *mem_pool) {
    if (mem_pool) {
        pss_thread_mutex_lock(&mem_pool->mutex);
        int i;
        for (i = 0; i < mem_pool->n_pages; i++) {
            mem_page_free_unused(mem_pool, &mem_pool->pages[i]);
        }
        pss_thread_mutex_unlock(&mem_pool->mutex);
    }
}
#ifdef OCEAN_HIGH_PLATFORM
void mem_pool_print_pages(mem_pool_t *mem_pool) {
    if (!mem_pool) {
        printf("Invalid memory pool!\n");
        return;
    }

    printf("*****Memory Pool Status start:\n");
    pss_thread_mutex_lock(&mem_pool->mutex);

    printf("Current Size: %u,Top Size: %u,Max Size: %u\n", (unsigned int)mem_pool->cur_size, (unsigned int)mem_pool->top_size, (unsigned int)mem_pool->max_size);
    printf("Number of Pages: %d Max Block Size: %d Align len:%ld\n", mem_pool->n_pages, mem_pool->max_block_size, BLOCK_HEAD_LEN);

    int i = 0;
    // size_t used_size = 0;
    // size_t free_size = 0;
    for (i = 0; i < mem_pool->n_pages; i++) {
        mem_page_t *page = mem_pool->pages[i];
        int block_count = 0;
        int used_count = 0;
        int free_count = 0;
        char *block = page->first;
        while (block != NULL) {
            int is_used = *(block + BLOCK_POINT_LEN * 2);
            if (is_used) {
                // used_size += page->block_size;
                used_count++;
            } else {
                // free_size += page->block_size;
                free_count++;
            }
            block = *(char **)(block + BLOCK_POINT_LEN);
            block_count++;
        }
        printf("Page %-3d Block Size:%-7d Totoal Blocks:%-5d Allocs:%-4d Frees:%-4d Page Size:%ld\n", i,
                    page->block_size, block_count, used_count, free_count, (page->block_size + BLOCK_HEAD_LEN) * block_count);
    }
    // printf( "*****Total Used Size: %zu,Total Free Size: %zu\n", used_size, free_size);

    pss_thread_mutex_unlock(&mem_pool->mutex);
    printf("*****Memory Pool Status end:\n");
}
#endif
