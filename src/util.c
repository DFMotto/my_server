#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>
#include "util.h"
#include "dbg.h"
int open_listenfd(int port)
{
    if(port<=0)
        port=3000;
    int listenfd,optval=1;
    struct sockaddr_in serveraddr;

    if((listenfd=socket(AF_INET,SOCK_STREAM,0))<0)
        return -1;

    if(setsockopt(listenfd,SOL_SOCKET,SO_REUSEADDR,(const void* )&optval,sizeof(int))<0)
        return -1;
    //该函数用来对socket接口设置一些选项值，第二个参数是被设置的选项的级别，如果在套接字级别上，则为SOL_SOCKET
    //第三个是设定的选项，这里的reuse是允许多个socket复用ip地址，optval是
    //指向存放选项待设置的新值的缓冲区，最后一个参数是缓冲区长度

    //init serveraddr
    bzero((char *)&serveraddr,sizeof(serveraddr));
    serveraddr.sin_family=AF_INET;
    serveraddr.sin_addr.s_addr=htonl(INADDR_ANY);
    serveraddr.sin_port=htons((unsigned short)port);
    if(bind(listenfd,(struct sockaddr *)&serveraddr,sizeof(serveraddr))<0)
        return -1;

    if(listen(listenfd,LISTEN_Q)<0)
        return -1;

    return listenfd;
}

int set_socket_non_blocking(int fd)
{
    int flags,s;
    //use fcntl
    flags=fcntl(fd,F_GETFL,0);//取得文件描述符状态
    if(flags==-1)
    {
        log_err("fcntl GETFL  error");
        return -1;
    }

    flags |= O_NONBLOCK;//set non blocking
    s=fcntl(fd,F_SETFL,flags);
    if(s==-1)
    {
        log_err("set non blocking err");
        return -1;
    }
    return 0;
}

int read_conf(char *filename,my_conf_t *cf,char *buf,int length)//从文件fp中读取my_conf结构体的数据，即配置信息
{
    FILE *fp =fopen(filename,"r");
    if(!fp)
    {
        log_err("cannot open file :%s",filename);
        return my_conf_error;
    }

    int pos =0;//记录位置
    char *delim_pos;//分割符"="的位置
    int line_len;//行长度
    char *current_pos=buf+pos;

    while(fgets(current_pos,length-pos,fp))
    {
        delim_pos=strstr(current_pos,DELIM);
        line_len=strlen(current_pos);

        if(!delim_pos)
            return my_conf_error;
        if(current_pos[strlen(current_pos)-1]=='\n')
            current_pos[strlen(current_pos)-1]='\0';
        if(strncmp("root",current_pos,4)==0)
            cf->root=delim_pos+1;
        if(strncmp("port",current_pos,4)==0)
            cf->port=atoi(delim_pos+1);
        if(strncmp("threadnum",current_pos,9)==0)
            cf->thread_num=atoi(delim_pos+1);
        current_pos+=line_len;
    }
    fclose(fp);
    return my_conf_ok;
}
