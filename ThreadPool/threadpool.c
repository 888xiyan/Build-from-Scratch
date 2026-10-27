#include "threadpool.h"
#include <stdlib.h>

/* threadpool_t、task_t 的定义以及 worker 的声明都在 threadpool.h 中，
 * 这里只放函数实现。 */

/*
 * 传给 list_create 的销毁回调：队列"丢弃"或清空残留任务时，
 * 用它释放 task 结构体本身。arg 不在这里释放——
 * 约定仍然是"arg 归任务函数自己管"（见 main.c 的 print_number）。
 */
static void task_free(void *data)
{
    free(data);
}

/* 成功返回 0，失败返回 -1 */
int threadpool_submit(threadpool_t *pool, task_fn function, void *arg)
{
    if (pool == NULL || function == NULL) {
        // 如果传入的线程池指针或任务函数指针为空，则返回失败。
        return -1;
    }

    task_t *task = malloc(sizeof(*task));
    if (task == NULL) {
        // 如果分配任务内存失败，则返回失败。
        return -1;
    }

    // 初始化任务结构体（不需要再手工串 next，入队由链表完成）。
    task->function = function;
    task->arg = arg;

    pthread_mutex_lock(&pool->mutex); // 加锁，保护对任务队列的访问。

    /* 线程池已经开始关闭，不再接收新任务 */
    if (pool->stopping) {
        pthread_mutex_unlock(&pool->mutex); // 解锁，释放对任务队列的访问。
        free(task);
        return -1;
    }

    /* 追加到队尾：分配节点、拼接指针、更新 size 都由通用链表完成 */
    if (list_append(pool->queue, task) != 0) {
        pthread_mutex_unlock(&pool->mutex);
        free(task);
        return -1;
    }

    pthread_mutex_unlock(&pool->mutex); // 解锁，释放对任务队列的访问。

    /*
     * 通知一个等待中的工作线程。放在解锁之后是安全的：
     * "队列非空"是在锁内改的，worker 要么还没进 wait（重新检查时
     * 能看到任务），要么已进 wait（这次 signal 会把它叫醒），不存在
     * 丢失唤醒；而且先解锁能让被唤醒的线程少一次"醒来即撞锁"的无谓切换。
     */
    pthread_cond_signal(&pool->condition);
    return 0;
}


threadpool_t *threadpool_create(size_t thread_count)
{
    if (thread_count == 0) {
        // 如果传入的线程数量为 0，则返回 NULL。
        return NULL;
    }

    threadpool_t *pool = calloc(1, sizeof(*pool));
    if (pool == NULL) {
        // 如果分配线程池内存失败，则返回 NULL。
        return NULL;
    }

    // 分配线程数组内存。
    pool->threads = calloc(thread_count, sizeof(*pool->threads));
    if (pool->threads == NULL) {
        // 如果分配线程数组内存失败，则释放线程池内存并返回 NULL。
        free(pool);
        return NULL;
    }

    // 初始化线程池的基本字段。
    pool->thread_count = thread_count;
    pool->stopping = false;

    // 创建任务队列（通用链表）。残留任务的释放也统一交给它的 destroy 回调。
    pool->queue = list_create(task_free);
    if (pool->queue == NULL) {
        // 如果分配队列内存失败，则释放线程数组和线程池内存并返回 NULL。
        free(pool->threads);
        free(pool);
        return NULL;
    }

    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        // 如果初始化互斥锁失败，则按创建的逆序清理已创建的资源并返回 NULL。
        list_destroy(pool->queue);
        free(pool->threads);
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->condition, NULL) != 0) {
        // 如果初始化条件变量失败，则按创建的逆序清理已创建的资源并返回 NULL。
        pthread_mutex_destroy(&pool->mutex);
        list_destroy(pool->queue);
        free(pool->threads);
        free(pool);
        return NULL;
    }

    // 创建线程池的工作线程。
    size_t created = 0;

    for (; created < thread_count; created++) {
        if (pthread_create(&pool->threads[created],
                           NULL,
                           worker,
                           pool) != 0) {
            // 如果创建线程失败，则跳出循环，稍后回收已创建的线程。
            break;
        }
    }

    /*
     * 如果只创建成功一部分，就唤醒并回收已经创建的线程，
     * 然后清理资源。
     */
    if (created != thread_count) {
        pthread_mutex_lock(&pool->mutex);
        pool->stopping = true;
        pthread_cond_broadcast(&pool->condition);
        pthread_mutex_unlock(&pool->mutex);

        for (size_t i = 0; i < created; i++) {
            pthread_join(pool->threads[i], NULL);
        }

        pthread_cond_destroy(&pool->condition);
        pthread_mutex_destroy(&pool->mutex);
        list_destroy(pool->queue);
        free(pool->threads);
        free(pool);
        return NULL;
    }

    return pool;
}

int threadpool_destroy(threadpool_t *pool)
{
    if (pool == NULL) return -1;

    // 1. 唤醒所有工作线程，并标记停止
    pthread_mutex_lock(&pool->mutex);
    pool->stopping = true;
    pthread_cond_broadcast(&pool->condition);   // 必须用 broadcast，signal 只唤醒一个
    pthread_mutex_unlock(&pool->mutex);

    // 2. 等所有线程退出（停止语义是"排空队列后再退"）
    for (size_t i = 0; i < pool->thread_count; i++) {
        pthread_join(pool->threads[i], NULL);
    }

    // 3. 清空残留任务（保险起见；正常情况下 worker 已把队列排空）。
    //    list_destroy 会对每个残留任务调用 task_free 释放它，
    //    然后释放所有节点和链表本身。
    list_destroy(pool->queue);

    // 4. 释放资源
    pthread_cond_destroy(&pool->condition);
    pthread_mutex_destroy(&pool->mutex);
    free(pool->threads);
    free(pool);
    return 0;
}
