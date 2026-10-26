#include "threadpool.h"
#include <stdlib.h>

void *worker(void *arg)
{
    threadpool_t *pool = arg; // 将传入的参数转换为线程池指针。

    for (;;) {
        pthread_mutex_lock(&pool->mutex); // 加锁，保护对任务队列的访问。

        /* 队列为空时睡眠；被唤醒后重新检查条件 */
        while (pool->head == NULL && !pool->stopping) {
            pthread_cond_wait(&pool->condition, &pool->mutex);
        }

        /*
         * 正在关闭，并且队列里的任务已经处理完。
         * 这时线程可以退出。
         */
        if (pool->head == NULL && pool->stopping) {
            pthread_mutex_unlock(&pool->mutex);
            return NULL;
        }

        /* 从队首取出一个任务 */
        task_t *task = pool->head;
        pool->head = task->next;

        if (pool->head == NULL) {
            pool->tail = NULL;
        }

        pthread_mutex_unlock(&pool->mutex);

        /* 在锁外执行任务，避免在持有锁的情况下执行耗时操作 */
        task->function(task->arg);
        free(task);
    }
}
