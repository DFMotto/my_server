#ifndef RIO_H
#define RIO_H
//本模块实现健壮的io包，详情见csapp第十章
#include <sys/types.h>

#define RIO_BUFFERSIZE 8192

typedef struct //该类为一个读的缓冲区，是应用级的缓冲区，不是系统内核级别的
{
    int rio_fd;//指向进行读写工作的fd
    ssize_t rio_cnt;//缓冲区中未读数据的字节大小
    //ssize_t:这个数据类型用来表示可以被执行读写操作的数据块的大小.它和size_t类似
    //但必需是signed.意即：它表示的是sign size_t类型的。
    char *rio_bufptr;//下一个未读的字节
    char rio_buf[RIO_BUFFERSIZE];//缓冲区本体
}rio_t;

ssize_t rio_readn(int fd,void *usrbuf,size_t n);
ssize_t rio_writen(int fd,void *usrbuf,size_t n);
void rio_readinitb(rio_t *rp,int fd);
ssize_t rio_readnb(rio_t *rp,void *usrbuf,size_t n);
ssize_t rio_readlineb(rio_t *rp,void *usrbuf,size_t maxlen);
#endif
