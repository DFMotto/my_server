#ifndef MY_TIMER_H
#define MY_TIMER_H

#include "priority_queue.h"
#include "http_request.h"

#define MY_TIMER_INFINITE -1
#define TIMEOUT_DEFAULT 500     //500ms后超时

typedef int (*timer_handler_pt)(my_http_request_t *rq);

typedef struct my_timer_node_s{
    size_t key;
    int deleted;    //若客户端先关闭连接，设为1
    timer_handler_pt handler;
    my_http_request_t *rq;
} my_timer_node;

int my_timer_init();
int my_find_timer();
void my_handle_expire_timers();

extern my_pq_t my_timer;
extern size_t my_current_msec;

void my_add_timer(my_http_request_t *rq, size_t timeout, timer_handler_pt handler);
void my_del_timer(my_http_request_t *rq);

#endif
