#include <stdio.h>
#include <stdlib.h>
#include "threadpool.h"

void print_number(void *arg)
{
    int number = *(int *)arg;
    printf("任务执行：number = %d\n", number);
    free(arg);   // 释放传入的内存
}

int main(void) {
    threadpool_t *pool = threadpool_create(4);
    if (pool == NULL) {
        printf("线程池创建失败\n");
        return 1;
    }

    for (int i = 0; i < 10; i++) {
        int *n = malloc(sizeof(int));
        *n = i;
        threadpool_submit(pool, print_number, n);   // arg 的释放在任务函数里做
    }

    threadpool_destroy(pool);   // 能正常返回，就证明线程都被正确唤醒并回收了
    printf("线程池已销毁\n");
    return 0;
}
