#include "list.h"
#include <stdlib.h>

/* list_t、list_node_t 的定义和全部函数原型都在 list.h 中，这里只放实现。 */

/*
 * 释放一个"被链表整体丢弃"的节点（list_clear / list_destroy 路径）：
 *   - list->destroy 非 NULL：由它释放 data；
 *   - list->destroy 为 NULL：data 归调用方，链表只动节点。
 */
static void free_node(list_t *list, list_node_t *node)
{
    if (list->destroy != NULL) {
        list->destroy(node->data);
    }
    free(node);
}

list_t *list_create(void (*destroy)(void *))
{
    list_t *list = malloc(sizeof(*list));
    if (list == NULL) {
        return NULL;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
    list->destroy = destroy;
    return list;
}

void list_destroy(list_t *list)
{
    if (list == NULL) {
        return;
    }

    list_clear(list);
    free(list);
}

void list_clear(list_t *list)
{
    if (list == NULL) {
        return;
    }

    list_node_t *node = list->head;
    while (node != NULL) {
        list_node_t *next = node->next; // 先记住下一个，因为当前节点马上被释放。
        free_node(list, node);
        node = next;
    }

    list->head = NULL;
    list->tail = NULL;
    list->size = 0;
}

int list_append(list_t *list, void *data)
{
    if (list == NULL || data == NULL) {
        return -1;
    }

    list_node_t *node = malloc(sizeof(*node));
    if (node == NULL) {
        return -1;
    }

    node->data = data;
    node->next = NULL;

    if (list->tail == NULL) {
        // 原本是空链表，头尾都指向新节点。
        list->head = node;
        list->tail = node;
    } else {
        list->tail->next = node;
        list->tail = node;
    }

    list->size++;
    return 0;
}

int list_prepend(list_t *list, void *data)
{
    if (list == NULL || data == NULL) {
        return -1;
    }

    list_node_t *node = malloc(sizeof(*node));
    if (node == NULL) {
        return -1;
    }

    node->data = data;
    node->next = list->head; // 新节点接在原头节点前面。
    list->head = node;

    if (list->tail == NULL) {
        // 原本是空链表，尾节点也要指向新节点。
        list->tail = node;
    }

    list->size++;
    return 0;
}

void *list_pop_front(list_t *list)
{
    if (list == NULL || list->head == NULL) {
        return NULL;
    }

    list_node_t *node = list->head;
    void *data = node->data;

    list->head = node->next;
    if (list->head == NULL) {
        // 取走的是最后一个元素，尾指针跟着清空。
        list->tail = NULL;
    }
    list->size--;

    // "取走"语义：只释放节点，data 交还调用方，不调用 destroy。
    free(node);
    return data;
}

void *list_peek_front(const list_t *list)
{
    if (list == NULL || list->head == NULL) {
        return NULL;
    }

    return list->head->data;
}

void *list_remove(list_t *list, const void *key, int (*cmp)(const void *, const void *))
{
    if (list == NULL || key == NULL || cmp == NULL) {
        return NULL;
    }

    list_node_t *prev = NULL;

    for (list_node_t *node = list->head; node != NULL; node = node->next) {
        if (cmp(node->data, key) != 0) {
            prev = node; // 没匹配上，继续往后找，记住前驱。
            continue;
        }

        // 找到了：把 node 从链上摘下来。
        if (prev == NULL) {
            list->head = node->next; // 删的是头节点，头指针后移。
        } else {
            prev->next = node->next;
        }

        if (list->tail == node) {
            list->tail = prev; // 删的是尾节点，尾指针回退（prev 可能是 NULL，即整链删空）。
        }

        list->size--;

        void *data = node->data;
        // "取走"语义：只释放节点，data 交还调用方，不调用 destroy。
        free(node);
        return data;
    }

    return NULL;
}

void list_foreach(const list_t *list, void (*fn)(void *, void *), void *user)
{
    if (list == NULL || fn == NULL) {
        return;
    }

    for (list_node_t *node = list->head; node != NULL; node = node->next) {
        fn(node->data, user);
    }
}

size_t list_size(const list_t *list)
{
    return list == NULL ? 0 : list->size;
}

bool list_is_empty(const list_t *list)
{
    return list == NULL || list->head == NULL;
}
