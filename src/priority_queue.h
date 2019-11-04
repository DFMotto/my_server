#ifndef PRIORITY_QUEUE_H
#define PRIORITY_QUEUE_H
#include "dbg.h"
#include "error.h"

#define default_pq_size 10
typedef int (*my_pq_comparator_pt)(void *pi,void *pj);
//函数指针宏定义，my_pq_comparator_pt a，a将自动替换成返回值为int的一个比较函数


typedef struct
{
    void **pq;
    size_t nalloc;//nalloc=now_alloc
    size_t size;//size_t=unsigned int
    my_pq_comparator_pt comp;
}my_pq_t;

//pq成员函数
int my_pq_init(my_pq_t *my_pq,my_pq_comparator_pt comp,size_t size);
int my_pq_is_empty(my_pq_t *my_pq);
size_t my_pq_size(my_pq_t *my_pq);
int my_pq_insert(my_pq_t *my_pq,void *ele);
void *my_pq_min(my_pq_t *my_pq);//查找最低优先权的元素并且返回该元素的指针
int my_pq_del_min(my_pq_t *my_pq);
int my_pq_sink(my_pq_t *my_pq,size_t i);

#endif
