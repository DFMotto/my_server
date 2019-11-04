#ifndef HTTP_PARSE_H
#define HTTP_PARSE_H
#define CR '\r'//'\r'——回车，光标回到本行开头
#define LF '\n'
#define CRLFCRLF "\r\n\r\n"
int my_http_parse_request_line(my_http_request_t *r);
int my_http_parse_request_body(my_http_request_t *r);
#endif
