#include "threadpool.h"
#include <stdlib.h>

/* threadpool_t、task_t 的定义以及 worker 的声明都在 threadpool.h 中，
 * 这里只放函数实现。 */

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

    // 初始化任务结构体。
    task->function = function;
    task->arg = arg;
    task->next = NULL;

    pthread_mutex_lock(&pool->mutex); // 加锁，保护对任务队列的访问。

    /* 线程池已经开始关闭，不再接收新任务 */
    if (pool->stopping) {
        pthread_mutex_unlock(&pool->mutex); // 解锁，释放对任务队列的访问。
        free(task);
        return -1;
    }

    /* 把任务追加到队尾 */
    if (pool->tail == NULL) {
        /* 队列原本为空 */
        pool->head = task;
        pool->tail = task;
    } else {
        pool->tail->next = task;
        pool->tail = task;
    }

    /* 通知一个等待中的工作线程 */
    pthread_cond_signal(&pool->condition);

    pthread_mutex_unlock(&pool->mutex); // 解锁，释放对任务队列的访问。
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
    pool->head = NULL;
    pool->tail = NULL;
    pool->stopping = false;

    if (pthread_mutex_init(&pool->mutex, NULL) != 0) {
        // 如果初始化互斥锁失败，则释放线程数组和线程池内存并返回 NULL。
        free(pool->threads);
        free(pool);
        return NULL;
    }

    if (pthread_cond_init(&pool->condition, NULL) != 0) {
        // 如果初始化条件变量失败，则销毁互斥锁，释放线程数组和线程池内存并返回 NULL。
        pthread_mutex_destroy(&pool->mutex);
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

    // 3. 清空残留任务（保险起见）
    while (pool->head) {
        task_t *tmp = pool->head;
        pool->head = pool->head->next;
        free(tmp);
    }

    // 4. 释放资源
    pthread_cond_destroy(&pool->condition);
    pthread_mutex_destroy(&pool->mutex);
    free(pool->threads);
    free(pool);
    return 0;
}
