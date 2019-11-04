#ifndef THREAD_POOL_H
#define THREAD_POOL_H
#ifdef __cplusplus
//extern C是为了让c++编译器以c语言的形式编译,g++编译器中有__cplusplus这个宏，如果有这个宏时要extern c
extern "C" {
#endif

#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <stdint.h>
#include "dbg.h"

#define THREAD_NUM 10

    typedef struct my_task_s
    {
        void (*func)(void *);//线程创建函数pthread的参数 func只是一个指针，指向形参为void*返回值为void的函数的指针； 
        void *arg;
        struct my_task_s *next;
    }my_task_t;//任务队列中的节点（单个任务）

    typedef struct {
        pthread_mutex_t lock;//用于对竞争资源上锁
        pthread_cond_t cond;//条件变量，用于唤醒睡眠的线程
        pthread_t *threads;//主线程
        my_task_t *head;//队头任务
        int thread_count;
        int queue_size;
        int shutdown;//结束标记
        int started;//启动标记
    }my_threadpool_t;//类中为应设为私有的变量

    typedef enum
    {
        my_tp_invalid=-1,
        my_tp_lock_fail=-2,
        my_tp_already_shutdown=-3,
        my_tp_cond_broadcast=-4,
        my_tp_thread_fail=-5,
    }my_threadpool_error_t;

    my_threadpool_t *threadpool_init(int thread_num);

    int threadpool_add(my_threadpool_t *pool,void (*func)(void *),void *arg);
    //本质是向线程池的任务队列中加入任务

    int threadpool_destroy(my_threadpool_t *pool,int target);

#ifdef __cplusplus
}
#endif

#endif

