/**
 * @file    pool.h
 * @brief   Fixed-size block allocator, the embedded replacement for
 *          malloc: O(1) alloc/free, no fragmentation, usable from
 *          interrupts (with a critical section), statistics included.
 *
 *   static uint8_t storage[POOL_STORAGE_SIZE(sizeof(msg_t), 16)];
 *   pool_t msgs;  pool_init(&msgs, storage, sizeof(storage), sizeof(msg_t));
 *   msg_t *m = pool_alloc(&msgs);  ...  pool_free(&msgs, m);
 */
#ifndef POOL_H
#define POOL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/** Blocks are rounded up to pointer alignment (the free list needs it). */
#define POOL_BLOCK_SIZE(size) \
    ((((size) + sizeof(void *) - 1U) / sizeof(void *)) * sizeof(void *))
#define POOL_STORAGE_SIZE(size, count)  (POOL_BLOCK_SIZE(size) * (count))

typedef struct {
    void    *free_list;
    uint8_t *start;
    uint8_t *end;
    size_t   block_size;
    uint16_t total;
    uint16_t used;
    uint16_t peak;           /**< high-water mark: size your pools with it */
    uint32_t failures;       /**< allocations refused because empty        */
} pool_t;

/** @p storage must be pointer-aligned. Returns false if too small. */
bool     pool_init(pool_t *p, void *storage, size_t storage_size, size_t block_size);
void    *pool_alloc(pool_t *p);
/** Returns false (and does nothing) for a pointer not from this pool. */
bool     pool_free(pool_t *p, void *block);
bool     pool_owns(const pool_t *p, const void *block);
uint16_t pool_available(const pool_t *p);

#endif /* POOL_H */
