/**
 * @file    sort.c
 * @brief   Sorting and searching (see sort.h).
 */
#include "sort.h"

/* byte-wise swap: works for any element size and alignment */
static void swap_bytes(uint8_t *a, uint8_t *b, size_t size)
{
    while (size-- > 0U) {
        const uint8_t t = *a;
        *a++ = *b;
        *b++ = t;
    }
}

void sort_insertion(void *base, size_t n, size_t size, sort_cmp_fn cmp)
{
    uint8_t *arr = (uint8_t *)base;
    for (size_t i = 1U; i < n; i++) {
        /* bubble element i down by swaps: no temporary buffer needed */
        for (size_t j = i; (j > 0U) && (cmp(&arr[(j - 1U) * size], &arr[j * size]) > 0); j--) {
            swap_bytes(&arr[(j - 1U) * size], &arr[j * size], size);
        }
    }
}

static void sift_down(uint8_t *arr, size_t start, size_t end, size_t size, sort_cmp_fn cmp)
{
    size_t root = start;
    while (((2U * root) + 1U) < end) {
        size_t child = (2U * root) + 1U;
        if (((child + 1U) < end) && (cmp(&arr[child * size], &arr[(child + 1U) * size]) < 0)) {
            child++;                                /* bigger child */
        }
        if (cmp(&arr[root * size], &arr[child * size]) >= 0) {
            return;
        }
        swap_bytes(&arr[root * size], &arr[child * size], size);
        root = child;
    }
}

void sort_heap(void *base, size_t n, size_t size, sort_cmp_fn cmp)
{
    uint8_t *arr = (uint8_t *)base;
    if (n < 2U) {
        return;
    }
    /* build a max-heap, then repeatedly move the max to the end */
    for (size_t start = n / 2U; start > 0U; start--) {
        sift_down(arr, start - 1U, n, size, cmp);
    }
    for (size_t end = n - 1U; end > 0U; end--) {
        swap_bytes(&arr[0], &arr[end * size], size);
        sift_down(arr, 0U, end, size, cmp);
    }
}

size_t sort_lower_bound(const void *key, const void *base, size_t n, size_t size, sort_cmp_fn cmp)
{
    const uint8_t *arr = (const uint8_t *)base;
    size_t lo = 0U;
    size_t hi = n;
    while (lo < hi) {
        const size_t mid = lo + ((hi - lo) / 2U);    /* no (lo+hi) overflow */
        if (cmp(&arr[mid * size], key) < 0) {
            lo = mid + 1U;
        } else {
            hi = mid;
        }
    }
    return lo;
}

int32_t sort_bsearch(const void *key, const void *base, size_t n, size_t size, sort_cmp_fn cmp)
{
    const size_t i = sort_lower_bound(key, base, n, size, cmp);
    if ((i < n) && (cmp(&((const uint8_t *)base)[i * size], key) == 0)) {
        return (int32_t)i;
    }
    return -1;
}

bool sort_is_sorted(const void *base, size_t n, size_t size, sort_cmp_fn cmp)
{
    const uint8_t *arr = (const uint8_t *)base;
    for (size_t i = 1U; i < n; i++) {
        if (cmp(&arr[(i - 1U) * size], &arr[i * size]) > 0) {
            return false;
        }
    }
    return true;
}

int sort_cmp_i32(const void *a, const void *b)
{
    const int32_t x = *(const int32_t *)a;
    const int32_t y = *(const int32_t *)b;
    return (x > y) - (x < y);           /* no overflow, unlike x - y */
}

int sort_cmp_u32(const void *a, const void *b)
{
    const uint32_t x = *(const uint32_t *)a;
    const uint32_t y = *(const uint32_t *)b;
    return (x > y) - (x < y);
}

void sort_i32(int32_t *a, size_t n)
{
    /* insertion sort below 16 elements, heapsort above */
    if (n <= 16U) {
        for (size_t i = 1U; i < n; i++) {
            const int32_t v = a[i];
            size_t        j = i;
            while ((j > 0U) && (a[j - 1U] > v)) {
                a[j] = a[j - 1U];
                j--;
            }
            a[j] = v;
        }
    } else {
        sort_heap(a, n, sizeof(int32_t), sort_cmp_i32);
    }
}

int32_t sort_i32_select(int32_t *a, size_t n, size_t k)
{
    size_t lo = 0U;
    size_t hi = (n == 0U) ? 0U : (n - 1U);
    if ((n == 0U) || (k >= n)) {
        return 0;
    }
    while (lo < hi) {
        /* median-of-three pivot avoids the O(n^2) case on sorted input */
        const size_t mid = lo + ((hi - lo) / 2U);
        int32_t x = a[lo];
        int32_t y = a[mid];
        int32_t z = a[hi];
        const int32_t pivot = (x < y) ? ((y < z) ? y : ((x < z) ? z : x))
                                      : ((x < z) ? x : ((y < z) ? z : y));
        size_t i = lo;
        size_t j = hi;
        while (i <= j) {
            while (a[i] < pivot) { i++; }
            while (a[j] > pivot) { j--; }
            if (i <= j) {
                const int32_t t = a[i];
                a[i] = a[j];
                a[j] = t;
                i++;
                if (j == 0U) {
                    break;
                }
                j--;
            }
        }
        if (k <= j) {
            hi = j;
        } else if (k >= i) {
            lo = i;
        } else {
            return a[k];
        }
    }
    return a[k];
}

int32_t sort_i32_median(int32_t *a, size_t n)
{
    return sort_i32_select(a, n, n / 2U);
}
