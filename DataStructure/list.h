#ifndef LIST_H
#define LIST_H

/*
 * list.h —— 通用单链表的公共接口
 *
 * 为什么"别的代码可以直接用"：
 *   1. 节点里存 void *，链表不关心数据的具体类型；
 *   2. 会变化的部分（怎么释放、怎么比较、怎么遍历）用函数指针交给
 *      使用方，链表只负责节点之间的链接关系；
 *   3. 结构体定义和函数原型只放在这个头文件里（include guard 保证只
 *      定义一次），使用方只 #include "list.h"、再把 list.c 加进编译即可；
 *   4. 链表自身不做线程同步：多线程场景由调用方加锁包住链表操作
 *      （ThreadPool 的任务队列就是这样用 pthread_mutex 保护的）。
 *
 * 内存所有权约定（只有两种语义，按操作区分）：
 *   - 节点（list_node_t）永远由链表分配和释放，使用方接触不到；
 *   - "取走"（list_pop_front / list_remove）——data 所有权交还调用方，
 *     由调用方决定保留还是释放；链表只释放节点，不调用 destroy。
 *     这样无论 destroy 是否为 NULL，调用方都拿得到指针，不会泄漏；
 *   - "整体丢弃"（list_clear / list_destroy）——链表对所有剩余元素
 *     调用 destroy 回调释放 data；destroy 为 NULL 时只释放节点，
 *     data 归调用方（需要调用方自己事先清干净，或另行持有指针）。
 *
 * 边界行为：所有接口对 NULL 参数安全；空链表 pop/peek 返回 NULL；
 * data 不允许为 NULL（否则无法区分"空链表"和"元素就是 NULL"）。
 */

#include <stdbool.h>
#include <stddef.h>

/* ======================== 节点 ======================== */

typedef struct list_node {
    void *data;             // 指向使用者的数据，类型无关。
    struct list_node *next; // 下一个节点；NULL 表示链表结尾。
} list_node_t;

/* ======================== 链表 ======================== */

typedef struct list {
    list_node_t *head;       // 出队端；NULL 表示空链表。
    list_node_t *tail;       // 入队端；保证尾插 O(1)，头删删到空时同步置 NULL。
    size_t size;             // 元素个数；list_size 取长度是 O(1)。
    void (*destroy)(void *); // 释放 data 的回调；NULL 表示 data 归调用方管。
} list_t;

/* ======================== 接口 ======================== */

/*
 * 创建链表，成功返回链表指针，内存不足返回 NULL。
 * destroy 是"链表丢弃元素时释放 data"的回调，传 NULL 表示 data 由调用方管理。
 */
list_t *list_create(void (*destroy)(void *));

/*
 * 销毁链表：对所有剩余元素调用 destroy（若有），释放全部节点和链表本身。
 * list 为 NULL 时安全返回。
 */
void list_destroy(list_t *list);

/*
 * 清空链表但保留结构本身：释放所有元素和节点，
 * 链表回到刚创建时的空状态，可以继续使用。
 */
void list_clear(list_t *list);

/*
 * 尾部追加（入队），O(1)。成功返回 0；
 * list 或 data 为 NULL、内存不足返回 -1（data 仍归调用方）。
 */
int list_append(list_t *list, void *data);

/*
 * 头部插入，O(1)。返回值同 list_append。
 * 配合 list_pop_front 可当 LIFO 栈使用。
 */
int list_prepend(list_t *list, void *data);

/*
 * 取出头部元素（出队），O(1)。成功返回 data（所有权交回调用方），
 * 空链表或 list 为 NULL 返回 NULL。链表只释放节点，不调用 destroy。
 */
void *list_pop_front(list_t *list);

/*
 * 查看头部元素但不取走，O(1)。空链表或 list 为 NULL 返回 NULL。
 */
void *list_peek_front(const list_t *list);

/*
 * 删除第一个使 cmp(data, key) == 0 的元素，O(n)，成功返回它的 data。
 * 与 list_pop_front 相同的"取走"语义：所有权交还调用方，链表只释放
 * 节点、不调用 destroy——所以 destroy 为 NULL 时指针也不会丢，
 * 调用方总能拿到 data 自己处理。
 * 未找到，或 list/key/cmp 为 NULL，返回 NULL
 * （约定 data 不允许为 NULL，因此 NULL 一定表示失败）。
 */
void *list_remove(list_t *list, const void *key,
                  int (*cmp)(const void *data, const void *key));

/*
 * 按头到尾顺序遍历，对每个元素调用 fn(data, user)，
 * user 是交给回调的自定义参数（可当累加器、上下文用）。
 * 注意：fn 可以修改 data 指向的内容，但不能增删链表节点，
 * 否则遍历会踩到已释放/错乱的节点，行为未定义。
 * list 或 fn 为 NULL 时安全返回。
 */
void list_foreach(const list_t *list, void (*fn)(void *data, void *user),
                  void *user);

/* 元素个数，O(1)。list 为 NULL 时返回 0。 */
size_t list_size(const list_t *list);

/* 链表是否为空，O(1)。list 为 NULL 视为为空。 */
bool list_is_empty(const list_t *list);

#endif /* LIST_H */
