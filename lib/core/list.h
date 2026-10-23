/**
 * @file    list.h
 * @brief   Intrusive doubly-linked circular list (Linux kernel style).
 *
 * The link lives inside your struct, so there is no allocation and an
 * object can be moved between lists in O(1):
 *
 *   typedef struct { int prio; list_node_t node; } job_t;
 *   list_node_t ready;  list_init(&ready);
 *   list_push_back(&ready, &job.node);
 *   LIST_FOR_EACH(it, &ready) { job_t *j = LIST_ENTRY(it, job_t, node); }
 */
#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

typedef struct list_node {
    struct list_node *next;
    struct list_node *prev;
} list_node_t;

/** Pointer to the struct that contains @p ptr as member @p member. */
#define LIST_ENTRY(ptr, type, member) \
    ((type *)(void *)((char *)(ptr) - offsetof(type, member)))

#define LIST_FOR_EACH(it, head) \
    for (list_node_t *it = (head)->next; it != (head); it = it->next)

/** Safe against removal of the current node. */
#define LIST_FOR_EACH_SAFE(it, tmp, head) \
    for (list_node_t *it = (head)->next, *tmp = it->next; it != (head); it = tmp, tmp = it->next)

static inline void list_init(list_node_t *head)
{
    head->next = head;
    head->prev = head;
}

static inline bool list_is_empty(const list_node_t *head)
{
    return head->next == head;
}

static inline void list_insert_between(list_node_t *n, list_node_t *prev, list_node_t *next)
{
    n->prev    = prev;
    n->next    = next;
    prev->next = n;
    next->prev = n;
}

static inline void list_push_front(list_node_t *head, list_node_t *n)
{
    list_insert_between(n, head, head->next);
}

static inline void list_push_back(list_node_t *head, list_node_t *n)
{
    list_insert_between(n, head->prev, head);
}

static inline void list_remove(list_node_t *n)
{
    n->prev->next = n->next;
    n->next->prev = n->prev;
    n->next = n;           /* removed node is a valid empty list */
    n->prev = n;
}

static inline list_node_t *list_pop_front(list_node_t *head)
{
    if (list_is_empty(head)) {
        return NULL;
    }
    list_node_t *n = head->next;
    list_remove(n);
    return n;
}

static inline uint32_t list_count(const list_node_t *head)
{
    uint32_t n = 0U;
    for (const list_node_t *it = head->next; it != head; it = it->next) {
        n++;
    }
    return n;
}

/** Insert keeping the list sorted by @p cmp (stable: after equals). */
void list_insert_sorted(list_node_t *head, list_node_t *n,
                        int (*cmp)(const list_node_t *a, const list_node_t *b));

#endif /* LIST_H */
