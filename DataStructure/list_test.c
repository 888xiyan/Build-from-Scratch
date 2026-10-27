/*
 * list_test.c —— 通用链表的自测与用法演示
 *
 * 覆盖：空链表/NULL 边界、FIFO 出入队、头插 LIFO、按值删除（头/中/尾/
 * 不存在）、destroy 回调的触发时机、遍历，以及"取走"与"丢弃"两种语义。
 *
 * 编译产物：DataStructure/build/list_test（见 .vscode/tasks.json）
 */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "list.h"

static int freed_count; // destroy 回调一共被调用了多少次

static void counting_free(void *data)
{
    freed_count++;
    free(data);
}

static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y); // 返回 -1/0/1，避免大数相减溢出
}

static void add_to_sum(void *data, void *user)
{
    *(long *)user += *(int *)data;
}

static int *new_int(int value)
{
    int *p = malloc(sizeof(*p));
    assert(p != NULL);
    *p = value;
    return p;
}

int main(void)
{
    /* ---------- 1. 空链表与 NULL 参数的边界 ---------- */
    list_t *q = list_create(NULL); // destroy 为 NULL：data 归自己管
    assert(q != NULL);
    assert(list_is_empty(q));
    assert(list_size(q) == 0);
    assert(list_pop_front(q) == NULL);
    assert(list_peek_front(q) == NULL);

    assert(list_append(NULL, NULL) == -1); // list 为 NULL
    assert(list_append(q, NULL) == -1);    // data 不允许为 NULL
    assert(list_prepend(q, NULL) == -1);
    assert(list_remove(q, NULL, cmp_int) == NULL);
    list_destroy(NULL); // 对 NULL 安全，不崩溃即可
    list_clear(NULL);

    /* ---------- 2. 尾插头出 = FIFO 队列 ---------- */
    for (int i = 1; i <= 3; i++) {
        assert(list_append(q, new_int(i)) == 0);
    }
    assert(list_size(q) == 3);
    assert(!list_is_empty(q));

    assert(*(int *)list_peek_front(q) == 1); // peek 只看不取
    assert(list_size(q) == 3);

    for (int i = 1; i <= 3; i++) { // 按入队顺序出队
        int *v = list_pop_front(q);
        assert(v != NULL && *v == i);
        free(v); // destroy 为 NULL，data 由调用方释放
    }
    assert(list_is_empty(q));
    assert(list_size(q) == 0);

    /* ---------- 3. 头插 + 头出 = LIFO 栈 ---------- */
    assert(list_prepend(q, new_int(1)) == 0);
    assert(list_prepend(q, new_int(2)) == 0);
    int *v = list_pop_front(q);
    assert(v != NULL && *v == 2); // 后插的先出
    free(v);
    v = list_pop_front(q);
    assert(v != NULL && *v == 1);
    free(v);

    /* ---------- 4. 按值删除（"取走"）：头 / 尾 / 最后一个 / 不存在 / 中间 ---------- */
    int key;
    int *d;
    for (int i = 1; i <= 3; i++) {
        assert(list_append(q, new_int(i)) == 0);
    }

    key = 1;
    d = list_remove(q, &key, cmp_int); // 删头节点
    assert(d != NULL && *d == 1);
    free(d); // "取走"后 data 归调用方释放
    assert(list_size(q) == 2);

    key = 3;
    d = list_remove(q, &key, cmp_int); // 删尾节点（tail 回退）
    assert(d != NULL && *d == 3);
    free(d);
    assert(list_size(q) == 1);

    key = 2;
    d = list_remove(q, &key, cmp_int); // 删最后一个（head/tail 都置空）
    assert(d != NULL && *d == 2);
    free(d);
    assert(list_is_empty(q));

    key = 99;
    assert(list_remove(q, &key, cmp_int) == NULL); // 没找到
    assert(list_remove(q, &key, NULL) == NULL);    // cmp 为 NULL

    for (int i = 10; i <= 13; i++) {
        assert(list_append(q, new_int(i)) == 0);
    }
    key = 12;
    d = list_remove(q, &key, cmp_int); // 删中间节点
    assert(d != NULL && *d == 12);
    free(d);
    assert(list_size(q) == 3); // 剩 10, 11, 13，顺序不变
    v = list_pop_front(q);
    assert(v != NULL && *v == 10);
    free(v);
    v = list_pop_front(q);
    assert(v != NULL && *v == 11);
    free(v);
    v = list_pop_front(q);
    assert(v != NULL && *v == 13);
    free(v);
    list_destroy(q);

    /* ---------- 5. "取走"不触发 destroy，"整体丢弃"才触发 ---------- */
    list_t *managed = list_create(counting_free);
    assert(managed != NULL);
    assert(list_append(managed, new_int(10)) == 0);
    assert(list_append(managed, new_int(20)) == 0);
    assert(list_append(managed, new_int(30)) == 0);

    freed_count = 0;

    // (a) list_remove 是"取走"：data 归还调用方，不触发 destroy
    key = 20;
    d = list_remove(managed, &key, cmp_int);
    assert(d != NULL && *d == 20);
    assert(freed_count == 0);
    free(d); // 归还了就由调用方处理

    // (b) list_pop_front 同样是"取走"：不触发 destroy
    int *taken = list_pop_front(managed);
    assert(taken != NULL && *taken == 10);
    assert(freed_count == 0);
    free(taken);

    // (c) list_clear 是"整体丢弃"：对剩余元素（30）触发 destroy
    list_clear(managed);
    assert(freed_count == 1);
    assert(list_is_empty(managed));

    // (d) list_destroy 同样先按约定释放剩余元素（40），再销毁结构本身
    assert(list_append(managed, new_int(40)) == 0);
    list_destroy(managed);
    assert(freed_count == 2);

    /* ---------- 6. foreach 遍历（user 当累加器） ---------- */
    list_t *r = list_create(counting_free);
    assert(r != NULL);
    for (int i = 1; i <= 4; i++) {
        assert(list_append(r, new_int(i)) == 0);
    }

    long sum = 0;
    list_foreach(r, add_to_sum, &sum);
    assert(sum == 10);

    list_foreach(NULL, add_to_sum, &sum); // NULL 安全
    list_foreach(r, NULL, &sum);
    assert(sum == 10); // 上面两次都不该重复累加

    assert(list_peek_front(r) != NULL && *(int *)list_peek_front(r) == 1);

    list_destroy(r);
    assert(freed_count == 6); // 之前 2 个 + r 的 4 个

    printf("list_test: 全部断言通过\n");
    return 0;
}
