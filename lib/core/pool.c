/**
 * @file    pool.c
 * @brief   Block pool (see pool.h). The free list is threaded through
 *          the free blocks themselves: zero bytes of overhead.
 */
#include "pool.h"

bool pool_init(pool_t *p, void *storage, size_t storage_size, size_t block_size)
{
    const size_t bs = POOL_BLOCK_SIZE(block_size);
    if ((storage == NULL) || (bs == 0U) || (storage_size < bs) ||
        (((uintptr_t)storage % sizeof(void *)) != 0U)) {
        return false;
    }
    const size_t count = storage_size / bs;
    p->start      = (uint8_t *)storage;
    p->end        = p->start + (count * bs);
    p->block_size = bs;
    p->total      = (uint16_t)((count > 0xFFFFU) ? 0xFFFFU : count);
    p->used       = 0U;
    p->peak       = 0U;
    p->failures   = 0U;
    p->free_list  = NULL;
    /* push blocks in reverse so the first alloc returns the first block */
    for (size_t i = p->total; i > 0U; i--) {
        void **blk = (void **)(void *)(p->start + ((i - 1U) * bs));
        *blk         = p->free_list;
        p->free_list = blk;
    }
    return true;
}

void *pool_alloc(pool_t *p)
{
    void **blk = (void **)p->free_list;
    if (blk == NULL) {
        p->failures++;
        return NULL;
    }
    p->free_list = *blk;
    p->used++;
    if (p->used > p->peak) {
        p->peak = p->used;
    }
    return blk;
}

bool pool_owns(const pool_t *p, const void *block)
{
    const uint8_t *b = (const uint8_t *)block;
    return (b >= p->start) && (b < p->end) && ((size_t)(b - p->start) % p->block_size == 0U);
}

bool pool_free(pool_t *p, void *block)
{
    if ((block == NULL) || !pool_owns(p, block) || (p->used == 0U)) {
        return false;
    }
    void **blk   = (void **)block;
    *blk         = p->free_list;
    p->free_list = blk;
    p->used--;
    return true;
}

uint16_t pool_available(const pool_t *p)
{
    return (uint16_t)(p->total - p->used);
}
