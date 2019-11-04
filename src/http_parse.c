#include "http.h"
#include "http_parse.h"
#include "error.h"

int my_http_parse_request_line(my_http_request_t *r)
{
    u_char ch,*p,*m;//有符号的char
    size_t pi;
    //通过枚举类型来解析http请求，这些枚举类型代表指针当前位置
    enum
    {
        sw_start=0,
        sw_method,
        sw_spaces_before_uri,
        sw_after_slash_in_uri,
        sw_http,
        sw_http_H,
        sw_http_HT,
        sw_http_HTT,
        sw_http_HTTP,
        sw_first_major_digit,
        sw_major_digit,
        sw_first_minor_digit,
        sw_minor_digit,
        sw_space_after_digit,
        sw_almost_done
    }state;

    state=r->state;//状态一开始初始化为r的对象，一般为start，即从头开始解析
    log_info("ready to parese request line, start = %d, last= %d", (int)r->pos, (int)r->last);
    for(pi=r->pos;pi<r->last;pi++)//r->pos为指针pi解析到的位置，r->last是http请求行最后一个字符位置
    {
        p=(u_char *)&r->buf[pi%MAX_BUF];//p为当前处理的位置的指针，pi为整形数，指的是当前字符在缓冲区中的下标
        ch=*p;//当前要处理的字符值
        switch(state)//通过状态这一枚举量来选择处理方式，break会跳出switch回到上层的for循环
        {
        case sw_start:
            r->request_start=p;//给请求结构体的成员赋初值
        //http请求行应该是一个大写的请求方法，处理错误情况
            if(ch==CR||ch==LF)//跳过换行符
                break;
            if((ch<'A'||ch>'Z')&&ch!='_')
                return MY_HTTP_PARSE_INVALID_METHOD;//无法解析的请求方法
            state=sw_method;//将状态改为选择请求方法
            break;
        case sw_method:
            if(ch==' ')
            {
                r->method_end=p;
                m=r->request_start;

                //当ch没有读到第一个空格时（即请求方法未读取完成）会直接break，成功读取请求方法后才切换状态
                switch(p-m)//选择不同的请求方法
                {
                case 3:
                    if(my_str3_cmp(m,'G','E','T',' '))
                    {
                        r->method=MY_HTTP_GET;
                        break;
                    }
                    break;
                case 4:
                    if(my_str30cmp(m,'P','O','S','T'))
                    {
                        r->method=MY_HTTP_POST;
                        break;
                    }
                     if(my_str30cmp(m,'H','E','A','D'))
                    {
                        r->method=MY_HTTP_HEAD;
                        break;
                    }
                     break;
                default:
                     r->method=MY_HTTP_UNKNOWN;
                     break;
                }
                state=sw_spaces_before_uri;
                break;
            }
            if((ch<'A'||ch>'Z')&&ch!='_')
            {
                return MY_HTTP_PARSE_INVALID_METHOD;
            }
            break;
        case sw_spaces_before_uri://处理uri连接前的空格
            if(ch=='/')
            {
                r->uri_start=p;
                state=sw_after_slash_in_uri;
                break;
            }
            switch(ch)
            {
            case ' ':
                break;
            default:
                return MY_HTTP_PARSE_INVALID_REQUEST;
            }
            break;
        case sw_after_slash_in_uri://把uri连接裁剪出来,读到空格（即uri连接已经读完）切换下一个状态
            switch(ch)
            {
            case ' ':
                r->uri_end=p;
                state=sw_http;
                break;
            default:
                break;
            }
            break;
        case sw_http://以下几个case将依次读完HTTP4个字符
            switch(ch)
            {
            case ' ':
                break;
            case 'H':
                state=sw_http_H;
                break;
            default:
                return MY_HTTP_PARSE_INVALID_REQUEST;
            }
            break;
        case sw_http_H:
            switch(ch)
            {
            case 'T':
                state=sw_http_HT;
                break;
            default:
                return MY_HTTP_PARSE_INVALID_REQUEST;
            }
            break;
        case sw_http_HT:
            switch (ch)
            {
            case 'T':
                state=sw_http_HTT;
                break;
            default:
                return MY_HTTP_PARSE_INVALID_REQUEST;
            }
            break;
        case sw_http_HTT:
            switch(ch)
            {
            case 'P':
                state=sw_http_HTTP;
                break;
            default:
                return MY_HTTP_PARSE_INVALID_REQUEST;
            }
            break;

        case sw_http_HTTP:
            switch(ch)
            {
            case '/':
                state=sw_first_major_digit;
                break;
            default:
                return MY_HTTP_PARSE_INVALID_REQUEST;
            }
            break;
            
        case sw_first_major_digit://获取版本号首位字符，格式为major.minor
            if(ch<'1'||ch>'9')
            {
                return MY_HTTP_PARSE_INVALID_REQUEST;
            }
            r->http_major=ch-'0';
            state=sw_major_digit;
            break;
        case sw_major_digit:
            if(ch=='.')
            {
                state=sw_first_minor_digit;
                break;
            }
            if(ch<'0'||ch>'9')
                return MY_HTTP_PARSE_INVALID_REQUEST;
            r->http_major=r->http_major*10+ch-'0';
            break;
        case sw_first_minor_digit:
            if(ch<'0'||ch>'9')
                return MY_HTTP_PARSE_INVALID_REQUEST;
            r->http_minor=ch-'0';
            state=sw_minor_digit;
            break;

        case sw_minor_digit://请求行结尾或者后面还有信息
            if(ch==CR)
            {
                state=sw_almost_done;
                break;
            }
            if(ch==LF)
                goto done;
            if(ch==' ')
            {
                state=sw_space_after_digit;
                break;
            }
            if(ch<'0'||ch>'9')
                return MY_HTTP_PARSE_INVALID_REQUEST;
            r->http_minor=r->http_minor*10+ch-'0';
            break;
        case sw_space_after_digit:
            switch(ch)
            {
                case ' ':
                    break;
                case CR:
                    state=sw_almost_done;
                    break;
                case LF:
                    goto done;
                default:
                    return MY_HTTP_PARSE_INVALID_REQUEST;
            }
            break;
        case sw_almost_done://解析请求行完成
            r->request_end=p-1;
            switch(ch)
            {
            case LF:
                goto done;
            default:
                return MY_HTTP_PARSE_INVALID_REQUEST;
            }
        }
    }
    r->pos=pi;
    r->state=state;
    return MY_AGAIN;
done:
    r->pos=pi+1;
    if(r->request_end==NULL)
        r->request_end=p;            
    r->state=sw_start;
    return my_ok;
}

int my_http_parse_request_body(my_http_request_t *r)
{
    //解析http请求头
    log_info("ready to parese request body, start = %d, last= %d", (int)r->pos, (int)r->last);

    u_char ch,*p;
    size_t pi;
    enum
    {
        sw_start=0,
        sw_key,
        sw_spaces_before_colon,
        sw_spaces_after_colon,
        sw_value,
        sw_cr,
        sw_crlf,
        sw_crlfcr
    }state;            
    state=r->state;
    check(state==0,"state should be 0");
    my_http_header_t *hd;//用来存放每一行请求头
    for(pi=r->pos;pi<r->last;pi++)
    {
        p=(u_char *)&r->buf[pi%MAX_BUF];
        ch=*p;
        switch(state)
        {
        case sw_start:
            if(ch==CR||ch==LF)
                break;
            r->cur_header_key_start=p;//key为头部字段名
            state=sw_key;
            break;
        case sw_key:
            if(ch==' ')
            {
                r->cur_header_key_end=p;
                state=sw_spaces_before_colon;
                break;
            }
            if(ch==':')
            {
                r->cur_header_key_end=p;
                state=sw_spaces_after_colon;
                break;
            }
            break;
        case sw_spaces_before_colon:
            if(ch==' ')
                break;
            else if(ch==':')
            {
                state=sw_spaces_after_colon;
                break;
            }
            else
                return MY_HTTP_PARSE_INVALID_HEADER;
        case sw_spaces_after_colon:
            if(ch==' ')
                break;
            state=sw_value;
            r->cur_header_value_start=p;
            break;
        case sw_value:
            if(ch==CR)
            {
                r->cur_header_value_end=p;
                state=sw_cr;
            }
            if(ch==LF)
            {
                r->cur_header_value_end=p;
                state=sw_crlf;
            }
            break;
        case sw_cr:
            if(ch==LF)
            {
                state=sw_crlf;
                //将上面得到的数据加入r的请求头数据表中
                hd=(my_http_header_t *)malloc(sizeof(my_http_header_t));
                hd->key_start=r->cur_header_key_start;
                hd->key_end=r->cur_header_key_end;
                hd->value_start=r->cur_header_value_start;
                hd->value_end=r->cur_header_value_end;
                list_add(&(hd->list),&(r->list));
                break;
            }
            else
                return MY_HTTP_PARSE_INVALID_HEADER;
        case sw_crlf:
            if(ch==CR)
                state=sw_crlfcr;
            else
            {
                r->cur_header_key_start=p;
                state=sw_key;
            }
            break;
        case sw_crlfcr:
            switch(ch)
            {
            case LF:
                goto done;
            default:
                return MY_HTTP_PARSE_INVALID_HEADER;
            }
            break;
        }
    }
    r->pos=pi;
    r->state=state;
    return MY_AGAIN;
done:
    r->pos=pi+1;
    r->state=sw_start;
    return my_ok;
}
