#include <stdio.h>
#include "threadpool.h"
//  function 是要执行的函数。void (*)(void *) 表示它接收一个 void * 参数，不返回结果。



// void print_number(void *arg)
// {
//     int number = *(int *)arg;
//     printf("任务执行：number = %d\n", number);
// }
// int main(void)
// {
//     int number = 42;

//     task_t task = {
//         .function = print_number,
//         .arg = &number,
//         .next = NULL
//     };

//     // 工作线程取出任务后，执行这一句：
//     task.function(task.arg);

//     return 0;
// }