#ifndef UTIL_H
#define UTIL_H


#define LISTEN_Q 1024

#define BUFFER_SIZE 8192

#define DELIM "="

#define my_conf_ok 0
#define my_conf_error 100

#define MIN(a,b) ((a)<(b)?(a):(b))

typedef struct my_conf_s//connect fd
{
    void *root;
    int port;
    int thread_num;
}my_conf_t;

int open_listenfd(int port);
int set_socket_non_blocking(int fd);

int read_conf(char *filename,my_conf_t *cf,char *buf,int length);
#endif
