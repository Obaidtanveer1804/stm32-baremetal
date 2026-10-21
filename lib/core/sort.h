/**
 * @file    sort.h
 * @brief   Sorting and searching without recursion or malloc:
 *          insertion sort (small/nearly sorted arrays), heapsort
 *          (O(n log n) worst case, O(1) memory), binary search,
 *          plus fast int32 helpers and k-th element selection.
 */
#ifndef SORT_H
#define SORT_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef int (*sort_cmp_fn)(const void *a, const void *b);

void   sort_insertion(void *base, size_t n, size_t size, sort_cmp_fn cmp);
void   sort_heap(void *base, size_t n, size_t size, sort_cmp_fn cmp);
/** Index of @p key in a sorted array, or -1. */
int32_t sort_bsearch(const void *key, const void *base, size_t n, size_t size, sort_cmp_fn cmp);
/** First index whose element is >= key (insertion point). */
size_t sort_lower_bound(const void *key, const void *base, size_t n, size_t size, sort_cmp_fn cmp);
bool   sort_is_sorted(const void *base, size_t n, size_t size, sort_cmp_fn cmp);

/* int32 specialisations (no function pointer per comparison) */
void    sort_i32(int32_t *a, size_t n);
int32_t sort_i32_median(int32_t *a, size_t n);    /**< reorders a */
/** k-th smallest (0-based), quickselect, average O(n). Reorders a. */
int32_t sort_i32_select(int32_t *a, size_t n, size_t k);
int     sort_cmp_i32(const void *a, const void *b);
int     sort_cmp_u32(const void *a, const void *b);

#endif /* SORT_H */
