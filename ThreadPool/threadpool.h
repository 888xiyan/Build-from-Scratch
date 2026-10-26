#ifndef THREADPOOL_H
#define THREADPOOL_H

/*
 * threadpool.h —— 线程池的公共接口与数据结构
 *
 * 规则：
 *   1. 结构体定义、函数原型只放在这个头文件里（靠 include guard 保证只定义一次）；
 *   2. 所有 .c 文件只 #include "threadpool.h"，不要互相 #include .c 文件，
 *      否则会出现 redefinition of 'struct task'。
 */

#include <pthread.h>
#include <stdbool.h>
#include <stddef.h>

// pthread.h 是 POSIX 线程库的头文件，声明了创建和管理线程所需的类型和函数。在线程池里，常用到这些：
// - pthread_t：线程标识，用来保存线程。
// - pthread_create()：创建线程，让它开始执行指定函数。
// - pthread_join()：等待线程结束并回收它。
// - pthread_mutex_t、pthread_mutex_lock()、pthread_mutex_unlock()：互斥锁，用来保护任务队列等共享数据。
// - pthread_cond_t、pthread_cond_wait()、pthread_cond_signal()
//   以及 pthread_cond_broadcast()：条件变量，让工作线程在没有任务时等待，有任务时被唤醒。

/* ======================== 任务 ======================== */

typedef void (*task_fn)(void *);
// task_fn 是要执行的函数。void (*)(void *) 表示它接收一个 void * 参数，不返回结果。

typedef struct task {
    task_fn function;  // function 是要执行的函数。
    void *arg;         // 传给函数的数据。用 void * 是为了让任务能携带不同类型的数据。
                       // 约定：arg 的内存由任务函数自己负责释放（见 main.c 的 print_number）。
    struct task *next; // 把任务连成链表，多个任务排起来就是等待执行的队列。
} task_t;

/* ======================= 线程池 ======================= */

typedef struct threadpool {
    pthread_t *threads;       // 线程池中的所有工作线程。
    size_t thread_count;      // 线程池中工作线程的数量。

    task_t *head;             // 任务队列的头节点（出队端）。
    task_t *tail;             // 任务队列的尾节点（入队端）。

    pthread_mutex_t mutex;    // 用于保护任务队列的互斥锁。
    pthread_cond_t condition; // 用于通知工作线程"有新任务 / 要停止"。

    bool stopping;            // 标记线程池是否正在停止。
} threadpool_t;

/* ======================== 接口 ======================== */

/*
 * 创建线程池，成功返回池指针；thread_count 为 0、内存不足或
 * 线程创建失败时返回 NULL（并清理已创建的资源）。
 */
threadpool_t *threadpool_create(size_t thread_count);

/*
 * 提交一个任务到队尾，并唤醒一个等待中的工作线程。
 * 成功返回 0；池/函数为空、池已关闭或内存不足返回 -1。
 */
int threadpool_submit(threadpool_t *pool, task_fn function, void *arg);

/*
 * 关闭并销毁线程池：
 *   置 stopping + broadcast 唤醒所有线程 -> 等它们排空队列退出 ->
 *   join -> 清理残留任务 -> 销毁锁/条件变量并释放内存。
 * 成功返回 0，pool 为 NULL 返回 -1。
 */
int threadpool_destroy(threadpool_t *pool);

/* 工作线程入口函数（内部使用，由 threadpool_create 传给 pthread_create）。 */
void *worker(void *arg);

#endif /* THREADPOOL_H */
