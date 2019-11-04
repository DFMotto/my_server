#include "thread_pool.h"
typedef enum
{
    i_shutdown=1,
    g_shutdown=2,
}my_threadpool_sd_t;//设定停止线程的类型

static int threadpool_free(my_threadpool_t *pool);//释放线程（析构）
static void *threadpool_worker(void *arg);//工作线程

my_threadpool_t *threadpool_init(int thread_num)
{
    if(thread_num<=0)
    {
        log_err("the arg number must >0");
        return NULL;
    }
    my_threadpool_t *pool;//创建对象
    if((pool=(my_threadpool_t *)malloc(sizeof(my_threadpool_t)))==NULL)
    {
        goto err;
    } 
    pool->thread_count=0;
    pool->queue_size=0;
    pool->shutdown=0;
    pool->started=0;
    pool->threads=(pthread_t *)malloc(sizeof(pthread_t)*thread_num);//threads里面存的是已拥有的工作线程id，是一个数组。pthread_t相当于unsigned long
    pool->head=(my_task_t *)malloc(sizeof(my_task_t));//消息队列的头节点
    if(pool->threads==NULL||pool->head==NULL)
    {
        goto err;
    }
    pool->head->func=NULL;
    pool->head->arg=NULL;
    pool->head->next=NULL;
    if(pthread_mutex_init(&(pool->lock),NULL)!=0)
    {
        goto err;
    }
    if(pthread_cond_init(&(pool->cond),NULL)!=0)
    {
        pthread_mutex_destroy(&(pool->lock));
        goto err;
    }

    int temp;
    //创建工作线程
    for(temp=0;temp<thread_num;temp++)
    {
        if(pthread_create(&(pool->threads[temp]),NULL,threadpool_worker,(void *)pool)!=0)
        {
            threadpool_destroy(pool,0);
            return NULL;
        }
        log_info("thread: %08x started",(uint32_t)pool->threads[temp]);
        pool->thread_count++;
        pool->started++;
    }
    return pool;

err:
    if(pool)
    {
        threadpool_free(pool);
    }
    return NULL;
}
//向线程池的任务队列添加任务
int threadpool_add (my_threadpool_t *pool,void (*func)(void *),void *arg)
{
    int rc,err=0;
    if(pool==NULL||func==NULL)
    {
        log_err("pool==NULL or func==NULL");
        return -1;
    }
    //检查是否上锁或中断，避免线程竞争
    if(pthread_mutex_lock(&(pool->lock))!=0)
    {
        log_err("p_thread_mutex_lock");
        return -1;
    }
    if(pool->shutdown)
    {
        err=my_tp_already_shutdown;
        goto out;
    }
    //创建消息对象
    my_task_t *task=(my_task_t *)malloc(sizeof(my_task_t));
    if(task==NULL)
    {
        log_err("task create failed");
        goto out;
    }
    task->func=func;
    task->arg=arg;
    task->next=pool->head->next;
    pool->head->next=task;
    pool->queue_size++;
    rc=pthread_cond_signal(&(pool->cond));//发送信号给条件变量，唤醒
    check(rc==0,"pthread_cond_signal");
//out会解锁（所有的都要执行）return err无论如何都能正常返回，err值不重要
out:
    if(pthread_mutex_unlock(&(pool->lock))!=0)
    {
        log_err("pthread_mutex_unlock");
        return -1;
    }
    
    return err;
}

int threadpool_free(my_threadpool_t *pool)
{
    if(pool==NULL||pool->started>0)
    {
        return -1;
    }
    if(pool->threads)
    {
        free(pool->threads);
    }
    my_task_t *old;//循环方式遍历删除任务队列中的任务
    while(pool->head->next)
    {
        old=pool->head->next;
        pool->head->next=old->next;
        free(old);
     }
    return 0;
}

int threadpool_destroy(my_threadpool_t *pool,int graceful)
{
    int err=0;
    if(pool==NULL)
    {
        log_err("pool==NULL");
        return my_tp_invalid;
    }

    if(pthread_mutex_lock(&(pool->lock))!=0)
    {
        return my_tp_lock_fail;
    }//析构前要先取得锁，保证没有其他线程在使用资源

    do
    {
        //设置shutdown的信号，并且唤醒其他线程
        if(pool->shutdown)
        {
            err=my_tp_already_shutdown;
            break;
        }
        pool->shutdown=(graceful?g_shutdown:i_shutdown);

        if(pthread_cond_broadcast(&(pool->cond))!=0)//唤醒所有阻塞在这个锁上的线程
        {
            err=my_tp_cond_broadcast;
            break;
        }

        if(pthread_mutex_unlock(&(pool->lock))!=0)
        {
            err=my_tp_lock_fail;
            break;
        }
        int temp;
        for(temp=0;temp<pool->thread_count;temp++)//join等待所有线程退出
        {
            if(pthread_join(pool->threads[temp],NULL)!=0)
            {
                err=my_tp_thread_fail;
            }
            log_info("thread %08x exit",(uint32_t)pool->threads[temp]);
        }
    }while(0);
    if(!err)
    {
        pthread_mutex_destroy(&(pool->lock));
        pthread_cond_destroy(&(pool->cond));
        threadpool_free(pool);
    }
    return err;
}

static void *threadpool_worker(void *arg)//通过这个线程来进行工作，是主线程
{
    //需要一个线程池来作为参数
    if(arg==NULL)
    {
        log_err("arg should be my_threadpool_t");
        return NULL;
    }
    my_threadpool_t *pool=(my_threadpool_t *)arg;
    my_task_t *task;
    while(1)
    {
        pthread_mutex_lock(&(pool->lock));
        //涉及对公共资源的修改，所以需要上锁
        while((pool->queue_size==0)&&(pool->shutdown))
        {
            pthread_cond_wait(&(pool->cond),&(pool->lock));//本函数做的事情实际上是（将lock解锁，把该线程加入等待队列）的原子化操作，然后再加锁lock
            //原因是如果一直不释放lock，那么pool->queue_size将一直不能发生改变，就永远无法唤醒
        }
        if(pool->shutdown==i_shutdown)
        {
            break;
        }
        else if((pool->shutdown==g_shutdown)&&pool->queue_size==0)
        {
            break;
        }//处理线程中断的情况，平和中断会逐步将queue_size减到0
        task=pool->head->next;
        if(task==NULL)
        {
            pthread_mutex_unlock(&(pool->lock));
            continue;
        }
        pool->head->next=task->next;
        pool->queue_size--;
        pthread_mutex_unlock(&(pool->lock));//涉及公共资源的修改结束，解锁
        (*(task->func))(task->arg);//因为task->func是一个函数指针，所以需要*解引用
        free(task);
    }
    //对应线程中断break的情况
    pool->started--;
    pthread_mutex_unlock(&(pool->lock));
    pthread_exit(NULL);
    return NULL;
}
