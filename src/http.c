#include <strings.h>
#include <sys/mman.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <fcntl.h>
#include "http.h"
#include "http_parse.h"
#include "http_request.h"
#include "my_epoll.h"
#include "error.h"
#include "timer.h"

static const char* get_file_type(const char *type);
static void parse_uri(char *uri,int length,char *filename,char *querystring);
static void do_error(int fd,char *cause,char *errnum,char *shortmsg,char *longmsg);
static void serve_static(int fd,char *filename,size_t filesize,my_http_out_t *out);
static char *ROOT =NULL;

mime_type_t my_mime[]=//用来标注用什么程序处理相应的文件
{
    {".html","text/html"},
    {".xml","text/xml"},
    {".xhtml","application/xhtml+xml"},
    {".txt","text/plain"},
    {".rtf","application/rtf"},
    {".pdf","application/pdf"},
    {".word", "application/msword"},
    {".png", "image/png"},
    {".gif", "image/gif"},
    {".jpg", "image/jpeg"},
    {".jpeg", "image/jpeg"},
    {".au", "audio/basic"},
    {".mpeg", "video/mpeg"},
    {".mpg", "video/mpeg"},
    {".avi", "video/x-msvideo"},
    {".gz", "application/x-gzip"},
    {".tar", "application/x-tar"},
    {".css", "text/css"},
    {NULL ,"text/plain"}
};

void do_request(void *ptr)//以ptr为参数处理一次http请求，ptr为指向请求结构体的指针
{
    my_http_request_t *r=(my_http_request_t *)ptr;
    int fd=r->fd;//用来读入数据的文件描述符
    int rc,n;
    char filename[SHORTLINE];
    struct stat sbuf;//struct stat这个结构体是用来描述一个linux系统文件系统中的文件属性的结构
    ROOT=r->root;
    char *plast=NULL;//为last的指针
    size_t remain_size;

    my_del_timer(r);
    for(;;)
    {
        //将数据读入到缓冲区buf中，last为读入数据后的位置
        plast=&r->buf[r->last%MAX_BUF];
        remain_size=MIN(MAX_BUF-(r->last-r->pos)-1,MAX_BUF-r->last%MAX_BUF);//读入整个缓冲区
        n=read(fd,plast,remain_size);//返回读出的字节数
        check(r->last-r->pos<MAX_BUF,"request buffer overflow");
        if(n==0)
        {
            log_info("read return 0,ready to close fd %d,remain_size= %zu",fd,remain_size);
            goto err;
        }
        if(n<0)
        {
            if(errno!=EAGAIN)
            {
                log_err("read err,error=%d",errno);
                goto err;
            }
            break;
        }
        r->last+=n;
        check(r->last-r->pos<MAX_BUF,"request buffer overflow");
        log_info("ready to parse request line");
        rc=my_http_parse_request_line(r);//对r中的请求行进行处理
        if(rc==MY_AGAIN)
            continue;
        else if(rc!=my_ok)
        {
            log_err("rc!=my_ok");
            goto err;
        }
        log_info("method == %.*s", (int)(r->method_end - r->request_start), (char *)r->request_start);
        log_info("uri == %.*s", (int)(r->uri_end - r->uri_start), (char *)r->uri_start);
        debug("ready to parse request body");
        rc=my_http_parse_request_body(r);//对r中的请求体进行处理
        if(rc==MY_AGAIN)
            continue;
        else if(rc!=my_ok)
        {
            log_err("rc!=my_ok");
            goto err;
        }
        //开始处理请求头，先初始化out类型
        my_http_out_t *out=(my_http_out_t *)malloc(sizeof(my_http_out_t));
        if(out==NULL)
        {
            log_err("no enough space for my_http_out_t");
            exit(1);
        }
        rc=my_init_out_t(out ,fd);
        check(rc==my_ok,"my_init_out_t");
        parse_uri(r->uri_start,r->uri_end-r->uri_start,filename,NULL);
        if(stat(filename,&sbuf)<0)
        {
            do_error(fd,filename,"404","NOT FOUND","cant find the file");
            continue;
        }
        if(!(S_ISREG(sbuf.st_mode))||!(S_IRUSR & sbuf.st_mode))//判断是否是一个常规文件和是否有用户读权限
        {
            do_error(fd, filename, "403", "Forbidden","can't read the file");
            continue;
        }
        out->mtime=sbuf.st_mtime;//mtime是最近修改时间
        my_http_handle_header(r,out);//处理请求头
        check(list_empty(&(r->list))==1,"header list should be empty");
        if(out->status==0)
        {
            out->status=MY_HTTP_OK;
        }
        serve_static(fd,filename,sbuf.st_size,out);
        if(!out->keep_alive)
        {
            log_info("no keep alive!close");
            free(out);
            goto close;
        }
        free(out);
    }
    //每一次请求修改重新注册一次epoll事件
    struct epoll_event event;
    event.data.ptr=ptr;
    event.events=EPOLLIN|EPOLLET|EPOLLONESHOT;//边缘触发，状态改变才会有新一次触发。使用oneshot避免多个线程处理一个事件
    my_epoll_mod(r->epfd,r->fd,&event);//更改fd的监听事件，在这里将r增加为监听对象
    my_add_timer(r,TIMEOUT_DEFAULT,my_http_close_conn);
    return;
err:
close:
    rc=my_http_close_conn(r);
    check(rc==0,"do request:my_http_close_conn");
}

static void parse_uri(char *uri,int uri_length,char *filename,char *querystring)
{
    check(uri!=NULL,"parse_uri: uri is NULL");//uri为指向uri字符串开头的指针
    uri[uri_length]='\0';
    char *question_mark=strchr(uri,'?');//查找uri中首次出现？的位置，问号要发送动态的查询请求，uri的地址在这之前应该已经完成
    int file_length;
    if(question_mark)
    {
        file_length=(int)(question_mark-uri);
        debug("file_length=(question_mark-uri)%d",file_length);
    }
    else
    {
        file_length=uri_length;
        debug("file_length=uri_length=%d",file_length);
    }
    //if(querystring){}
    strcpy(filename,ROOT);//将全局变量root的内容复制到filename
    if(uri_length>(SHORTLINE>>1))//uri过长
    {
        log_err("uri too long:%.*s",uri_length,uri);
        return;
    }
    debug("before strncat,filename=%s,uri=%.*s,file_length=%d",filename,uri,file_length);
    strncat(filename,uri,file_length);//加入资源地址，filename为请求资源的文件名
    //给末尾加上index.html,将uri解析为url，详情见csapp 
    char *last_comp=strrchr(filename,'/');//查找最后一次出现
    char *last_dot=strrchr(last_comp,'.');
    if(last_dot==NULL&&filename[strlen(filename)-1]!='/')
        strcat(filename,"/");
    if(filename[strlen(filename)-1]=='/')
        strcat(filename,"index.html");
    log_info("filename=%s",filename);
    return;
}

static void do_error(int fd,char *cause,char *errnum,char *shortmsg,char *longmsg)
{
    char header[MAXLINE],body[MAXLINE];
    sprintf(body, "<html><title> Error</title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\n", body);
    sprintf(body, "%s%s: %s\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\n</p>", body, longmsg, cause);
    sprintf(body, "%s<hr><em>my web server</em>\n</body></html>", body);

    sprintf(header, "HTTP/1.1 %s %s\r\n", errnum, shortmsg);
    sprintf(header, "%sServer: my\r\n", header);
    sprintf(header, "%sContent-type: text/html\r\n", header);
    sprintf(header, "%sConnection: close\r\n", header);
    sprintf(header, "%sContent-length: %d\r\n\r\n", header, (int)strlen(body));
    rio_writen(fd, header, strlen(header));
    rio_writen(fd, body, strlen(body));
    return;
}

static void serve_static(int fd,char *filename,size_t filesize,my_http_out_t *out)//返回http响应消息
{
    char header[MAXLINE];
    char buf[SHORTLINE];
    size_t n;
    struct tm tm;
    const char *file_type;
    const char *dot_pos=strrchr(filename,'.');
    file_type=get_file_type(dot_pos);

    sprintf(header,"HTTP/1.1 %d %s\r\n",out->status,getshortmsg_from_status_code(out->status));//响应行
    if(out->keep_alive)
    {
        sprintf(header,"%sConnection: keep-alive\r\n",header);
        sprintf(header,"%sKeep-Alive: timeout=%d\r\n",header,TIMEOUT_DEFAULT);
    }
    if(out->modified)
    {
        sprintf(header,"%sContent-type: %s\r\n",header,file_type);
        sprintf(header,"%sContent-length: %zu\r\n",header,filesize);
        localtime_r(&(out->mtime),&tm);
        strftime(buf, SHORTLINE,  "%a, %d %b %Y %H:%M:%S GMT", &tm);
        sprintf(header, "%sLast-Modified: %s\r\n", header, buf);
    }
    sprintf(header, "%sServer: my\r\n", header);
    sprintf(header, "%s\r\n", header);
    //使用rio发送信息
    n=(size_t)rio_writen(fd,header,strlen(header));
    check(n==strlen(header),"rio_writen error,errno=%d",errno);
    if(n!=strlen(header))
    {
        log_err("n!=strlen(header)");
        goto out;
    }
    if(!out->modified)
        goto out;
    //只读打开文件
    int srcfd=open(filename,O_RDONLY,0);
    check(srcfd>2,"open error");
    char *srcaddr=mmap(NULL,filesize,PROT_READ,MAP_PRIVATE,srcfd,0);
    check(srcaddr!=(void *)-1,"mmap error");
    close(srcfd);
    n=rio_writen(fd,srcaddr,filesize);

    munmap(srcaddr,filesize);
out:
    return;
}

static const char* get_file_type(const char *type)
{
    if(type==NULL)
        return "text/plain";
    int i;
    for(i=0;my_mime[i].type!=NULL;i++)
    {
        if(strcmp(type,my_mime[i].type)==0)
            return my_mime[i].value;
    }
    return my_mime[i].value;
}
