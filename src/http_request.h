//http_request实现将http请求转换成request对象
#ifndef HTTP_REQUEST_H
#define HTTP_REQUEST_H

#include <time.h>
#include "http.h"

#define MY_AGAIN EAGAIN
#define MY_HTTP_PARSE_INVALID_METHOD 10//http方法解析无效
#define MY_HTTP_PARSE_INVALID_REQUEST 11//http请求解析无效
#define MY_HTTP_PARSE_INVALID_HEADER 12//http头部解析无效

#define MY_HTTP_UNKNOWN 0x0001
#define MY_HTTP_GET 0x0002
#define MY_HTTP_HEAD 0x0004
#define MY_HTTP_POST 0x0008

#define MY_HTTP_OK 200
#define MY_HTTP_NOT_MODIFIED 304//客户端所求资源上次修改时间，304为未修改，可以将缓存返回给客户端
#define MY_HTTP_NOT_FOUND 404

#define MAX_BUF 8124

typedef struct my_http_request_s
{
    void *root;
    int fd;
    int epfd;
    char buf[MAX_BUF];//循环（环形）缓冲区，实际储存http报文的地方
    size_t pos,last;
    int state;
    void *request_start;
    void *method_end;//不包括自身method_end
    int method;
    void *uri_start;
    void *uri_end;//不包括自身uri_end
    void *path_start;
    void *path_end;
    void *query_start;
    void *query_end;
    int http_major;
    int http_minor;
    void *request_end;

    struct list_head list;//储存http头部
    void *cur_header_key_start;
    void *cur_header_key_end;
    void *cur_header_value_start;
    void *cur_header_value_end;

    void *timer;
}my_http_request_t;

typedef struct//这个类型是服务器用来给客户端返回响应消息用的 
{
    int fd;
    int keep_alive;
    time_t mtime;//文件的改变时间
    int modified;//通过IF-MODIFIED-TIME与mtime进行比较判断文件从上次修改后是否有过改变
    // If-Modified-Since是标准的HTTP请求头标签，在发送HTTP请求时，把浏览器端缓存页面的最
    // 后修改时间一起发到服务器去，服务器会把这个时间与服务器上实际文件的最后修改时间进行比较。
    int status;
}my_http_out_t;

typedef struct my_http_header_S
{
    void *key_start,*key_end;//不包括end
    void *value_start,*value_end;
    list_head list;
}my_http_header_t;

typedef int (*my_http_header_handler_pt)(my_http_request_t *r,my_http_out_t *o,char *data,int len);
//理解复杂声明可用的“右左法则”：
//从变量名看起，先往右，再往左，碰到一个圆括号就调转阅读的方向；括号内分析完就跳出括号，还是按先右后
//左的顺序，如此循环，直到整个声明分析完
//这里的变量名为my_http_header_handler_pt,是一个函数指针，最右边的括号内为形参，返回值为int
//通过typedef，原来复杂的函数指针对象可以直接用my_http_header_handler_pt声明

typedef struct
{
    char *name;
    my_http_header_handler_pt handler;
}my_http_header_handle_t;
//http请求头处理类型

void my_http_handle_header(my_http_request_t *r,my_http_out_t *o);
int my_http_close_conn(my_http_request_t *r);

int my_init_request_t(my_http_request_t *r,int fd,int epfd,my_conf_t *cf);
int my_free_request_t(my_http_request_t *r);

int my_init_out_t(my_http_out_t *o,int fd);
int my_free_out_t(my_http_out_t *o);

const char *getshortmsg_from_status_code(int status_code);
extern my_http_header_handle_t my_http_headers_in[];
#endif
