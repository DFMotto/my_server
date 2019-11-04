#ifndef EPOLL_H
#define EPOLL_H

#define MAXEVENTS 1024
#include <sys/epoll.h>
int my_epoll_create(int flags);
void my_epoll_add(int epfd,int fs,struct epoll_event *event);
void my_epoll_mod(int epfd,int fs,struct epoll_event *event);
void my_epoll_del(int epfd,int fs,struct epoll_event *event);
int my_epoll_wait(int epfd,struct epoll_event *events,int maxevents,int timeout);

#endif
