#include <sys/time.h>
#include "timer.h"

static int timer_comp(void *ti, void *tj)
{
    my_timer_node *timeri = (my_timer_node *)ti;
    my_timer_node *timerj = (my_timer_node *)tj;

    return (timeri->key < timerj->key)? 1: 0;
}//比较两个计时器大小

my_pq_t my_timer;//my_timer是一个存有计时器的优先级队列
size_t my_current_msec;

static void my_time_update()
{//更新校准现在的时间
    struct timeval tv;
    int rc;

    rc = gettimeofday(&tv, NULL);//获取1970到现在的精确秒数，存在tv中
    check(rc == 0, "my_time_update: gettimeofday error");

    my_current_msec = tv.tv_sec * 1000 + tv.tv_usec / 1000;//将秒和微秒换算为毫秒
    debug("in my_time_update, time = %zu", my_current_msec);
}


int my_timer_init()//初始化一个计时器队列，但不增加节点
{
    int rc;
    rc = my_pq_init(&my_timer, timer_comp,default_pq_size);
    check(rc == my_ok, "my_pq_init error");
   
    my_time_update();
    return my_ok;
}

int my_find_timer() {
    my_timer_node *timer_node;
    int time = MY_TIMER_INFINITE;
    int rc;

    while (!my_pq_is_empty(&my_timer))
    {//找出优先级最高的计时器，即key最小的（最先入队），key储存的是毫秒数
        debug("my_find_timer");
        my_time_update();
        timer_node = (my_timer_node *)my_pq_min(&my_timer);
        check(timer_node != NULL, "my_pq_min error");

        if (timer_node->deleted)
        {//若deleted为1则删除
            rc = my_pq_del_min(&my_timer); 
            check(rc == 0, "my_pq_delmin");
            free(timer_node);
            continue;
        }
             
        time = (int) (timer_node->key - my_current_msec);
        debug("in my_find_timer, key = %zu, cur = %zu",
                timer_node->key,
                my_current_msec);
        time = (time > 0? time: 0);
        break;
    }
    
    return time;//将计时器经过的时间返回
}

void my_handle_expire_timers() 
{//处理超时的计时器
    debug("in my_handle_expire_timers");
    my_timer_node *timer_node;
    int rc;

    while (!my_pq_is_empty(&my_timer))
    {
        debug("my_handle_expire_timers, size = %zu", my_pq_size(&my_timer));
        my_time_update();
        timer_node = (my_timer_node *)my_pq_min(&my_timer);//取出优先级最高的计时器
        check(timer_node != NULL, "my_pq_min error");

        if (timer_node->deleted)
        {
            rc = my_pq_del_min(&my_timer); 
            check(rc == 0, "my_handle_expire_timers: my_pq_delmin error");
            free(timer_node);
            continue;
        }
        
        if (timer_node->key > my_current_msec)//未超时则不理
        {
            return;
        }

        if (timer_node->handler)//已经超时，且处理函数已经实现时，则进入处理函数
        {
            timer_node->handler(timer_node->rq);
            timer_node->deleted=1;//有没有都无所谓，整个节点已经被析构了
        }
        rc = my_pq_del_min(&my_timer); 
        check(rc == 0, "my_handle_expire_timers: my_pq_delmin error");
        free(timer_node);
    }
}

void my_add_timer(my_http_request_t *rq, size_t timeout, timer_handler_pt handler)
{
    int rc;
    my_timer_node *timer_node = (my_timer_node *)malloc(sizeof(my_timer_node));
    check(timer_node != NULL, "my_add_timer: malloc error");

    my_time_update();
    rq->timer = timer_node;
    timer_node->key = my_current_msec + timeout;//key是最终终止时间，超过key则为超时
    debug("in my_add_timer, key = %zu", timer_node->key);
    timer_node->deleted = 0;
    timer_node->handler = handler;
    timer_node->rq = rq;

    rc = my_pq_insert(&my_timer, timer_node);
    check(rc == 0, "my_add_timer: my_pq_insert error");
}

void my_del_timer(my_http_request_t *rq)
{
    debug("in my_del_timer");
    my_time_update();
    my_timer_node *timer_node = rq->timer;
    check(timer_node != NULL, "my_del_timer: rq->timer is NULL");

    timer_node->deleted = 1;
}
