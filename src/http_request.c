//本文件所实现的功能，初始化http请求结构和out结构，
//通过对http请求头结构体my_http_header_t进行解析，对其进行初步处理
//比如判断是否无视这个头部，是否保持长连接，请求的资源是否被修改过
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <math.h>
#include <time.h>
#include <unistd.h>
#include "http.h"
#include "http_request.h"
#include "error.h"

static int my_http_process_ignore(my_http_request_t *r,my_http_out_t *out,char *data,int len);
static int my_http_process_connection(my_http_request_t *r,my_http_out_t *out,char *data,int len);
static int my_http_process_if_modified_since(my_http_request_t *r, my_http_out_t *out, char *data, int len);

my_http_header_handle_t my_http_headers_in[]=
{
    {"Host",my_http_process_ignore},
    {"Connection",my_http_process_connection},
    {"IF-Modified-Since",my_http_process_if_modified_since},
    {"",my_http_process_ignore}
};
//http头部处理数组，类型见http_request.h

int my_init_request_t(my_http_request_t *r,int fd,int epfd,my_conf_t *cf)
{
    r->fd=fd;
    r->epfd=epfd;
    r->pos=r->last=0;
    r->state=0;
    r->root=cf->root;
    INIT_LIST_HEAD(&(r->list));
    return my_ok;
}
//对http请求结构体进行初始化，传入一个用来储存的r

int my_free_request_t(my_http_request_t *r)
{
    (void) r;
    return my_ok;    
}

int my_init_out_t(my_http_out_t *o,int fd)//out系列函数用来返回响应消息
{
    o->fd=fd;
    o->keep_alive=0;
    o->modified=1;
    o->status=0;
    return my_ok;
}

int my_free_out_t(my_http_out_t *o)
{
    (void) o;
    return my_ok;
}

void my_http_handle_header(my_http_request_t *r,my_http_out_t *o)
{
    list_head *pos;
    my_http_header_t *hd;//http请求头部信息
    my_http_header_handle_t *header_in;//用于处理头部的结构体
    int len;

    list_for_each(pos,&(r->list))//从pos开始遍历r->list
    {
        hd=list_entry(pos,my_http_header_t,list);//指向pos节点这个结构体的首地址，post与list类型嗯相同
        
        //开始处理
        for(header_in=my_http_headers_in;strlen(header_in->name)>0;header_in++)//headers_in数组的首地址被赋值给header_in，循环通过比较key字符找出应该调用headers_in中的哪一个处理方法
        {
            if(strncmp(hd->key_start,header_in->name,hd->key_end-hd->key_start)==0)//比较http请求头部冒号前的内容
            {
                len=hd->value_end-hd->value_start;
                (*(header_in->handler))(r,o,hd->value_start,len);//调用宏定义的处理函数
                break;
            }
        }
        //处理完后删除
        list_del(pos);
        free(hd);
    }
}

int my_http_close_conn(my_http_request_t *r)
{
    //关闭一个连接会自动将它从所有epoll中移除
    close(r->fd);
    free(r);
    return my_ok;
}

static int my_http_process_ignore(my_http_request_t *r,my_http_out_t *out,char *data,int len)
{
    (void) r;
    (void) out;
    (void) data;
    (void) len;
    return my_ok;
}

static int my_http_process_connection(my_http_request_t *r,my_http_out_t *out,char *data,int len)
{
    (void) r;
    if(strncasecmp("keep-alive",data,len)==0)
        out->keep_alive=1;
    return my_ok;
}//该函数检查是否需要使用keep_alive模式

static int my_http_process_if_modified_since(my_http_request_t *r,my_http_out_t *out,char *data,int len)
{
    (void) r;
    (void) len;
    struct tm tm;//time.h中的时间结构
    if(strptime(data,"%a, %d %b %Y %H %M:%S GMT",&tm)==(char *)NULL)//将字符串格式的时间转换为tm结构
    {
        return my_ok;
    }
    time_t client_time=mktime(&tm);
    double time_diff=difftime(out->mtime,client_time);
    if(fabs(time_diff)<1e-6)
    {
        out->modified=0;
        out->status=MY_HTTP_NOT_MODIFIED;
    }
    return my_ok;
}//本函数判断是所请求的资源是否被修改过

const char *getshortmsg_from_status_code(int status_code)
{
    if(status_code==MY_HTTP_OK)
        return "OK";
    if(status_code==MY_HTTP_NOT_MODIFIED)
        return "Not Modified";
    if(status_code==MY_HTTP_NOT_FOUND)
        return "Not Found";
    return "Unknown";
}
