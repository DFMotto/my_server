#include "priority_queue.h"

static int resize(my_pq_t *my_pq,size_t newsize)
{
    //重新分配的大小不能小于现在已经储存的元素数
    if(newsize<=my_pq->nalloc)
    {
        log_err("size too small");
        return -1;
    }
    void **new_ptr=(void **)malloc(sizeof(void *)*newsize);
    if(!new_ptr)
    {
        log_err("resize malloc failed");
        return -1;
    }
    memcpy(new_ptr,my_pq->pq,sizeof(void *)*(my_pq->nalloc+1));//复制已有元素到新的队列中
    free(my_pq->pq);
    my_pq->pq=new_ptr;
    my_pq->size=newsize;
    return my_ok;
}

static void exchange(my_pq_t *my_pq,size_t i,size_t j)
{
    void *temp=my_pq->pq[i];
    my_pq->pq[i]=my_pq->pq[j];
    my_pq->pq[j]=temp;
}
//优先级队列用堆实现，而swim为上滤，是插入时的操作
static void swim(my_pq_t *my_pq,size_t k)
{
    while(k>1&&my_pq->comp(my_pq->pq[k],my_pq->pq[k/2]))
    {
        exchange(my_pq,k,k/2);
        k/=2;
    }
}

static size_t sink(my_pq_t *my_pq,size_t i)//指定一个节点t并执行下滤操作
{
    size_t j;
    size_t nalloc=my_pq->nalloc;
    while(2*i<=nalloc)
    {
        j=2*i;
        if(j<nalloc&&my_pq->comp(my_pq->pq[j+1],my_pq->pq[j]))
            j++;
        if(!my_pq->comp(my_pq->pq[j],my_pq->pq[i]))
            break;
        exchange(my_pq,j,i);
        i=j;
    }
    return i;
}

int my_pq_sink(my_pq_t *my_pq,size_t i)
{
    return sink(my_pq,i);
}

int my_pq_init(my_pq_t *my_pq,my_pq_comparator_pt comp,size_t size)
{
    my_pq->pq=(void **)malloc(sizeof(void *)*(size+1));
    if(!my_pq->pq)
    {
        log_err("my_pq->pq init failed");
        return -1;
    }
    my_pq->nalloc=0;
    my_pq->size=size+1;
    my_pq->comp=comp;
    return my_ok;
}

int my_pq_is_empty(my_pq_t *my_pq)
{
    return (my_pq->nalloc==0?1:0);
}

size_t my_pq_size(my_pq_t *my_pq)
{
    return my_pq->size;
}

void *my_pq_min(my_pq_t *my_pq)
{
    if(my_pq_is_empty(my_pq))
    {
        return NULL;
    }
    else
        return my_pq->pq[1];
}

int my_pq_del_min(my_pq_t *my_pq)
{
    if(my_pq_is_empty(my_pq))
        return my_ok;

    exchange(my_pq,1,my_pq->nalloc);//堆的删除堆顶元素的操作
    my_pq->nalloc--;
    sink(my_pq,1);
    if(my_pq->nalloc>0&&my_pq->nalloc<=(my_pq->size-1)/4)
    {
        if(resize(my_pq,my_pq->size/2)<0)
        {
            return -1;
        }
    }
    return my_ok;
}

int my_pq_insert(my_pq_t *my_pq,void *value)
{
    if(my_pq->nalloc+1==my_pq->size)//超过最大容量时尝试重新分配空间
    {
        if(resize(my_pq,my_pq->size*2)<0)
        {
            return -1;
        }
    }

    my_pq->pq[++my_pq->nalloc]=value;
    swim(my_pq,my_pq->nalloc);

    return my_ok;
}
