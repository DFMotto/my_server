#include <stdint.h>
#include <getopt.h>
#include <signal.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include "util.h"
#include "timer.h"
#include "http.h"
#include "my_epoll.h"
#include "thread_pool.h"

#define CONF "my_server.conf"
#define PROGRAM_VERSION "1.0"

extern struct epoll_event *events;

static const struct option long_options[]=
{
    {"help",no_argument,NULL,'?'},
    {"version",no_argument,NULL,'V'},
    {"conf",required_argument,NULL,'c'},
    {NULL,0,NULL,0}
};

static void usage()//初始界面
{
   fprintf(stderr,
	"my_server [option]... \n"
	"  -c|--conf <config file>  Specify config file. Default ./my_server.conf.\n"//指定配置文件
	"  -?|-h|--help             This information.\n"
	"  -V|--version             Display program version.\n"
	);
}

int main(int argc, char* argv[])
{
    int rc;
    int opt = 0;
    int options_index = 0;
    char *conf_file = CONF;

    //解析命令行参数

    if (argc == 1)
    {
        usage();
        return 0;
    }

    while ((opt=getopt_long(argc, argv,"Vc:?h",long_options,&options_index)) != EOF) 
    {
        switch (opt) 
        {
            case  0 : break;
            case 'c':
                conf_file = optarg;
                break;
            case 'V':
                printf(PROGRAM_VERSION"\n");
                return 0;
            case ':':
            case 'h':
            case '?':
                usage();
                return 0;
        }
    }

    debug("conffile = %s", conf_file);

    if (optind < argc) {
        log_err("non-option ARGV-elements: ");
        while (optind < argc)
            log_err("%s ", argv[optind++]);
        return 0;
    }

    //读取配置文件,配置文件结构在util.h中
    char conf_buf[BUFFER_SIZE];
    my_conf_t cf;
    rc = read_conf(conf_file, &cf, conf_buf, BUFFER_SIZE);
    check(rc == my_conf_ok, "read conf err");

    /*
    *   sigpipe的信号处理
    *   当客户端关闭连接时。服务器还在向它写数据，则会第一次写后会受到RST响应
    *   第二次写内核会向进程发出sigpipe信号，导致异常终止
    */
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = SIG_IGN;//该参数意为忽略信号
    sa.sa_flags = 0;
    if (sigaction(SIGPIPE, &sa, NULL)) //sigaction函数第一个参数为要捕获的信号类型，第二个为处理方式
    {
        log_err("install sigal handler for SIGPIPE failed");
        return 0;
    }

    /*
    * 创建监听socket
    */
    int listenfd;
    struct sockaddr_in clientaddr;
    // initialize clientaddr and inlen to solve "accept Invalid argument" bug
    socklen_t inlen = 1;
    memset(&clientaddr, 0, sizeof(struct sockaddr_in));  
    
    listenfd = open_listenfd(cf.port);

    rc = set_socket_non_blocking(listenfd);
    check(rc == 0, "make_socket_non_blocking");

    /*
    * create epoll and add listenfd to ep
    */
    int epfd = my_epoll_create(0);
    struct epoll_event event;
    
    my_http_request_t *request = (my_http_request_t *)malloc(sizeof(my_http_request_t));
    my_init_request_t(request, listenfd, epfd, &cf);

    event.data.ptr = (void *)request;
    event.events = EPOLLIN | EPOLLET;
    my_epoll_add(epfd, listenfd, &event);

    
    //create thread pool
    
    
    my_threadpool_t *tp = threadpool_init(cf.thread_num);
    check(tp != NULL, "threadpool_init error");
    
    
     // initialize timer
    my_timer_init();

    log_info("my_server started.");
    int n;
    int i, fd;
    int time;

    /* epoll_wait loop */
    while (1) {
        time = my_find_timer();
        debug("wait time = %d", time);
        n = my_epoll_wait(epfd, events, MAXEVENTS, time);
        my_handle_expire_timers();
        
        for (i = 0; i < n; i++) 
        {
            my_http_request_t *r = (my_http_request_t *)events[i].data.ptr;
            fd = r->fd;
            
            if (listenfd == fd)
            {
                /* 建立新连接 */

                int infd;
                while(1) 
                {
                    infd = accept(listenfd, (struct sockaddr *)&clientaddr, &inlen);
                    if (infd < 0) 
                    {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) 
                        {
                            /* we have processed all incoming connections */
                            break;
                        }
                        else 
                        {
                            log_err("accept");
                            break;
                        }
                    }

                    rc = set_socket_non_blocking(infd);
                    check(rc == 0, "make_socket_non_blocking");
                    log_info("new connection fd %d", infd);
                    
                    my_http_request_t *request = (my_http_request_t *)malloc(sizeof(my_http_request_t));
                    if (request == NULL)
                    {
                        log_err("malloc(sizeof(my_http_request_t))");
                        break;
                    }

                    my_init_request_t(request, infd, epfd, &cf);
                    event.data.ptr = (void *)request;
                    event.events = EPOLLIN | EPOLLET | EPOLLONESHOT;

                    my_epoll_add(epfd, infd, &event);
                    my_add_timer(request, TIMEOUT_DEFAULT, my_http_close_conn);//超时处理函数为关闭连接
                }   // end of while of accept

            } 
            else
            {
                if ((events[i].events & EPOLLERR) ||
                    (events[i].events & EPOLLHUP) ||
                    (!(events[i].events & EPOLLIN))) 
                {
                    log_err("epoll error fd: %d", r->fd);
                    close(fd);
                    continue;
                }

                log_info("new data from fd %d", fd);
                rc = threadpool_add(tp, do_request, events[i].data.ptr);
                check(rc == 0, "threadpool_add");

                //do_request(events[i].data.ptr);
            }
        }   //end of for
    }   // end of while(1)
    

    
    if (threadpool_destroy(tp, 1) < 0)
    {
        log_err("destroy threadpool failed");
    }
    

    return 0;
}
