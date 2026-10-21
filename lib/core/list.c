/**
 * @file    list.c
 * @brief   Intrusive list helpers that are too big to inline (see list.h).
 */
#include "list.h"

void list_insert_sorted(list_node_t *head, list_node_t *n,
                        int (*cmp)(const list_node_t *a, const list_node_t *b))
{
    list_node_t *it = head->next;
    /* walk past every node <= n: equal keys keep insertion order */
    while ((it != head) && (cmp(it, n) <= 0)) {
        it = it->next;
    }
    list_insert_between(n, it->prev, it);
}
