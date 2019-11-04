#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include "dbg.h"
#include "rio.h"
//rio_readn:鲁棒性地读入n字节的数据，无缓冲
ssize_t rio_readn(int fd,void *usrbuf,size_t n)
{
    size_t nleft=n;
    ssize_t nread;
    char *bufp=(char *)usrbuf;
    while(nleft>0)
    {
        if((nread=read(fd,bufp,nleft))<0)
        {
            if(errno==EINTR)
                nread=0;
            else
                return -1;
        }
        else if(nread==0)
            break;
        nleft-=nread;
        bufp+=nread;
    }
    return (n-nleft);
}

ssize_t rio_writen(int fd,void *usrbuf,size_t n)
{
    size_t nleft = n;
    ssize_t nwritten;
    char *bufp=(char *)usrbuf;
    while(nleft>0)
    {
        if((nwritten=write(fd,bufp,nleft))<=0)
        {
            if(errno==EINTR)
                nwritten=0;
            else
            {
                log_err("errno==%d\n",errno);
                return -1;
            }
        }
        nleft-=nwritten;
        bufp+=nwritten;
    }
    return n;
}

//rio_read:带缓冲区的rio的核心函数
static ssize_t rio_read(rio_t *rp,char *usrbuf,size_t n)
{
    size_t cnt;
    while(rp->rio_cnt<=0)//当rio的缓冲区为空时从套接字读入数据填满它
    {
        rp->rio_cnt=read(rp->rio_fd,rp->rio_buf,sizeof(rp->rio_buf));
        if(rp->rio_cnt<0)
        {
            if(errno==EAGAIN)
                return -EAGAIN;
            if(errno!=EINTR)
                return -1;
         }
        else if(rp->rio_cnt==0)
            return 0;
        else
            rp->rio_bufptr=rp->rio_buf;
    }

    //将剩余的字节读入用户缓冲区，字节数为指定的字节数和在缓冲区中字节数小的那个
    cnt=n;
    if(rp->rio_cnt<(ssize_t)n)
        cnt=rp->rio_cnt;
    memcpy(usrbuf,rp->rio_bufptr,cnt);
    rp->rio_bufptr+=cnt;
    rp->rio_cnt-=cnt;
    return cnt;
}

void rio_readinitb(rio_t *rp,int fd)
{
    rp->rio_fd=fd;
    rp->rio_cnt=0;
    rp->rio_bufptr=rp->rio_buf;
}

//带缓冲区的RIO
ssize_t rio_readnb(rio_t *rp,void *usrbuf,size_t n)
{
    size_t nleft=n;
    ssize_t nread;
    char *bufp=(char *)usrbuf;

    while(nleft>0)
    {
        if((nread=rio_read(rp,bufp,nleft))<0)
        {
            if(errno==EINTR)
                nread=0;
            else
                return -1;
        }
        else if(nread==0)
            break;
        nleft-=nread;
        bufp+=nread;
    }
    return (n-nleft);
}

ssize_t rio_readlineb(rio_t *rp,void *usrbuf,size_t maxlen)
{
    size_t n;
    ssize_t rc;
    char c,*bufp=(char *)usrbuf;
    for(n=1;n<maxlen;n++)
    {
        if((rc=rio_read(rp,&c,1))==1)
        {
            *bufp++ =c;
            if(c=='\n')
                break;
        }
        else if(rc==0)
        {
            if(n==1)
               return 0;
            else break;
        }
        else if(rc==-EAGAIN)
            return rc;
        else
            return -1;
    }
    *bufp=0;
    return n;
}
