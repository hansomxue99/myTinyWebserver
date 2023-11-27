#include "http.h"
#include "../log/log.h"

#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/mman.h>

#include <iostream>
using namespace std;

//定义http响应的一些状态信息
const char *ok_200_title = "OK";
const char *ok_string = "<html><body></body></html>";
const char *error_400_title = "Bad Request";
const char *error_400_form = "Your request has bad syntax or is inherently impossible to staisfy.\n";
const char *error_403_title = "Forbidden";
const char *error_403_form = "You do not have permission to get file form this server.\n";
const char *error_404_title = "Not Found";
const char *error_404_form = "The requested file was not found on this server.\n";
const char *error_500_title = "Internal Error";
const char *error_500_form = "There was an unusual problem serving the request file.\n";


void HTTP::http_init() {
    vmethod = GET;
    vcheck_state = CHECK_STATE_REQUESTLINE;
    vurl = 0;
    vversion = 0;

    mysql = NULL;
    vlinger = false;
    cgi = 0;
    vhost = 0;
    vcontent_len = 0;
    vstate = 0;

    vchecked_idx = 0;
    vread_idx = 0;
    vstart_line = 0;
    
    vwrite_idx = 0;
    vbyte_have_send = 0;
    vbyte_to_send = 0;

    memset(vread_buf, '\0', READ_BUF_SIZE);
    memset(vwrite_buf, '\0', WRITE_BUF_SIZE);
    memset(vreal_file, '\0', FILENAME_LEN);
}

void HTTP::http_init(int sockfd, const sockaddr_in &addr, char *root, int epoll_mod,
                     int close_log, string user, string passwd, string sqlname) {
    vsock_fd = sockfd;
    vaddress = addr;
    
    lepoll.add_epollfd(vepoll_fd, vsock_fd, true, vepoll_mod);
    ++vuser_count;

    doc_root = root;
    vepoll_mod = epoll_mod;
    log_close = close_log;
    
    strcpy(sql_user, user.c_str());
    strcpy(sql_passwd, passwd.c_str());
    strcpy(sql_name, sqlname.c_str());

    http_init();
}

/*
** Name: http close function
** Description: 
** Author: wkxue
** Create time: 2023/11/25 18:55
*/
void HTTP::http_close() {
    if (vsock_fd != -1) {
        cout << "close" << vsock_fd << endl;
        lepoll.remove_epollfd(vepoll_fd, vsock_fd);
        vsock_fd = -1;
        --vuser_count;
    }
}

/*
** Name: HTTP process function
** Description: 主状态负责大的框架，从状态真正去处理
** Author: wkxue
** Create time: 2023/11/26 10:00
*/
void HTTP::http_process() {
    HTTP_CODE read_ret = process_read();
    if (read_ret == NO_REQUEST) {
        lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLIN);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret) http_close();
    lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLOUT);
}

bool HTTP::http_write() {
    int tmp = 0;

    if (vbyte_to_send == 0) {
        lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLIN);
        http_init();
        return true;
    }

    while (1) {
        tmp = writev(vsock_fd, viovec, viovec_count);
        if (tmp < 0) {
            if (errno == EAGAIN) {
                lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLOUT);
                return true;
            }
            unmap();
            return false;
        }

        vbyte_have_send += tmp;
        vbyte_to_send -= tmp;
        if (vbyte_have_send >= viovec[0].iov_len) {
            viovec[0].iov_len = 0;
            viovec[1].iov_base = vfile_addr + (vbyte_have_send - vwrite_idx);
            viovec[1].iov_len = vbyte_to_send;
        } else {
            viovec[0].iov_base = vwrite_buf + vbyte_have_send;
            viovec[0].iov_len = viovec[0].iov_len - vbyte_have_send;
        }

        if (vbyte_to_send <= 0) {
            unmap();
            lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLIN);
            if (vlinger) {
                http_init();
                return true;
            } else {
                return false;
            }
        }
    }
}

bool HTTP::http_read() {
    if (vread_idx >= READ_BUF_SIZE) {
        return false;
    }
    int byte_read = 0;
    if (LT_MOD == vepoll_mod) {
        byte_read = recv(vsock_fd, vread_buf+vread_idx, READ_BUF_SIZE-vread_idx, 0);
        if (byte_read <= 0)  return false;
        vread_idx += byte_read;
    } else {
        while (true) {
            byte_read = recv(vsock_fd, vread_buf+vread_idx, READ_BUF_SIZE-vread_idx, 0);
            if (byte_read == -1) {
                if (errno == EAGAIN || errno == EWOULDBLOCK)    break;
                return false;
            } else if (byte_read == 0) {
                return false;
            }
            vread_idx += byte_read;
        }
    }
    return true;
}

void HTTP::unmap() {
    if (vfile_addr) {
        munmap(vfile_addr, vfile_stat.st_size);
        vfile_addr = 0;
    }
}

/*********************HTTP READ*****************************/
/*
** Name: http read function
** Description: http 读取并解析请求报文
** Author: wkxue
** Create time: 2023/11/26 14:50
*/
HTTP::HTTP_CODE HTTP::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    while ((vcheck_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = parse_line()) == LINE_OK) {
        text = get_line();
        vstart_line = vchecked_idx;   // start_line记录的是当前行的字符个数
        LOG_INFO("%s", text);

        switch (vcheck_state)
        {
        case CHECK_STATE_REQUESTLINE:
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST) return BAD_REQUEST;
            break;
        case CHECK_STATE_HEADER:
            ret = parse_header(text);
            if (ret == BAD_REQUEST) return BAD_REQUEST;
            else if (ret == GET_REQUEST)    ; //TODO
            break;
        case CHECK_STATE_CONTENT:
            ret = parse_content(text);
            if (ret == GET_REQUEST) ;   // TODO
            line_status = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
        } 
    }
    return NO_REQUEST;
}

/*
** Name: get line function
** Description: 获取一行开始的指针
** Author: wkxue
** Create time: 2023/11/26 14:25
*/
char* HTTP::get_line() {
    return vread_buf + vstart_line;
}

/*
** Name: parse line function
** Description: 从状态机，对读取的http报文的解析每一行
** Author: wkxue
** Create time: 2023/11/26 10:35
*/
HTTP::LINE_STATUS HTTP::parse_line() {
    char tmp;
    while (vchecked_idx < vread_idx) {
        tmp = vread_buf[vchecked_idx];

        if (tmp == '\r') {
            if (vread_idx == (vchecked_idx+1))  return LINE_OPEN;
            else if (vread_buf[vchecked_idx+1] == '\n') {
                vread_buf[vchecked_idx++] = '\0';
                vread_buf[vchecked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        } else if (tmp == '\n') {
            if (vchecked_idx > 1 && vread_buf[vchecked_idx-1] == '\r') {
                vread_buf[vchecked_idx-1] = '\0';
                vread_buf[vchecked_idx++] = '\0';
                return LINE_OK;
            }
            return LINE_BAD;
        }

        ++vchecked_idx;
    }
    return LINE_OPEN;
}

/*
** Name: parse request function
** Description: 解析请求行
** Author: wkxue
** Create time: 2023/11/26 12:40
*/
HTTP::HTTP_CODE HTTP::parse_request_line(char *text) {
    vurl = strpbrk(text, " \t");
    if (!vurl)  return BAD_REQUEST;
    *vurl++ = '\0';
    vurl += strspn(vurl, " \t");

    // 解析请求类型
    char *method = text;
    if (strcasecmp(method, "GET") == 0) {
        vmethod = GET;
    } else if (strcasecmp(method, "POST") == 0) {
        vmethod = POST;
        // TODO
    } else {
        return BAD_REQUEST;
    }

    // 解析HTTP版本
    vversion = strpbrk(vurl, " \t");
    if (!vversion)  return BAD_REQUEST;
    *vversion++ = '\0';
    vversion += strspn(vurl, " \t");    // 至此，method,url,version分离
    if (strcasecmp(vversion, "HTTP/1.1") == 0)  return BAD_REQUEST;

    // 解析URL
    if (strncasecmp(vurl, "http://", 7) == 0) {
        vurl += 7;
        vurl = strchr(vurl, '/');
    } else if (strncasecmp(vurl, "https://", 8) == 0) {
        vurl += 8;
        vurl = strchr(vurl, '/');
    }
    if (!vurl || vurl[0] != '/')    return BAD_REQUEST;
    // 根目录
    if (strlen(vurl) == 1) {
        strcat(vurl, "home.html");  // 注意，这里将根目录固定了
    }
    vcheck_state = CHECK_STATE_HEADER;  // 转换check状态
    return NO_REQUEST;
}

/*
** Name: parse header function
** Description: 解析http头部报文
** Author: wkxue
** Create time: 2023/11/26 13:41
*/
HTTP::HTTP_CODE HTTP::parse_header(char *text) {
    if (text[0] == '\0') {
        if (vcontent_len != 0) {
            vcheck_state = CHECK_STATE_CONTENT;
            return NO_REQUEST;
        }
        return GET_REQUEST;
    } else if (strncasecmp(text, "Connection:", 11) == 0) {
        text += 11;
        text += strspn(text, " \t");
        if (strcasecmp(text, "keep-alive") == 0) {
            vlinger = true; // 长连接
        }
    } else if (strncasecmp(text, "Content-length:", 15) == 0) {
        text += 15;
        text += strspn(text, " \t");
        vcontent_len = atol(text);
    } else if (strncasecmp(text, "Host:", 5) == 0) {
        text += 5;
        text += strspn(text, " \t");
        vhost = text;
    } else {
        LOG_INFO("oop! unkonwn header: %s", text);
    }
    return NO_REQUEST;
}

/*
** Name: parse content fucntion
** Description: 解析http报文内容
** Author: wkxue
** Create time: 2023/11/26 14:13
*/
HTTP::HTTP_CODE HTTP::parse_content(char *text) {
    if (vread_idx >= (vcontent_len + vchecked_idx)) {
        text[vcontent_len] = '\0';
        vrequest_str = text;
        return GET_REQUEST;
    }
    return NO_REQUEST;
}

/*
** Name: http request fucntion
** Description: http 如何响应请求
** Author: wkxue
** Create time: 2023/11/26 14:49
*/
HTTP::HTTP_CODE HTTP::process_request() {
    // TODO
    return NO_REQUEST;
}

/*********************HTTP WRITE*****************************/
/*
** Name: http write function
** Description: http发送响应给web
** Author: wkxue
** Create time: 2023/11/26 14:50
*/
bool HTTP::process_write(HTTP::HTTP_CODE ret) {
    // 根据解析的HTTP CODE进行响应
    switch (ret)
    {
    case INTERNAL_ERROR:
        add_status_line(500, error_500_title);
        add_header(strlen(error_500_form));
        if (!add_content(error_500_form))   return false;
        break;
    case BAD_REQUEST:
        add_status_line(404, error_404_title);
        add_header(strlen(error_404_form));
        if (!add_content(error_404_form))   return false;
        break;
    case FORBIDDEN_REQUEST:
        add_status_line(403, error_403_title);
        add_header(strlen(error_403_form));
        if (!add_content(error_403_form))   return false;
        break;
    case FILE_REQUEST:
        add_status_line(200, ok_200_title);
        if (vfile_stat.st_size != 0) {
            add_header(vfile_stat.st_size);
            viovec[0].iov_base = vwrite_buf;
            viovec[0].iov_len = vwrite_idx;
            viovec[1].iov_base = vfile_addr;
            viovec[1].iov_len = vfile_stat.st_size;
            viovec_count = 2;
            vbyte_to_send = vwrite_idx + vfile_stat.st_size;
            return true;
        } else {
            add_header(strlen(ok_string));
            if (!add_content(ok_string))   return false;
        }
        break;
    default:
        return false;
    }
    viovec[0].iov_base = vwrite_buf;
    viovec[0].iov_len = vwrite_idx;
    viovec_count = 1;
    vbyte_to_send = vwrite_idx;
    return true;
}

/*
** Name: add response function
** Description: 
** Author: wkxue
** Create time: 2023/11/26 15:46
*/
bool HTTP::add_response(const char *format, ...) {
    if (vwrite_idx >= WRITE_BUF_SIZE)   return false;

    va_list arg_list;
    va_start(arg_list, format);
    int len = vsnprintf(vwrite_buf+vwrite_idx, WRITE_BUF_SIZE-1-vwrite_idx, format, arg_list);
    if (len >= (WRITE_BUF_SIZE-1-vwrite_idx)) {
        va_end(arg_list);
        return false;
    }
    vwrite_idx += len;
    va_end(arg_list);

    LOG_INFO("request: %s", vwrite_buf);

    return true;
}

bool HTTP::add_blank_line() {
    return add_response("%s", "\r\n");
}

bool HTTP::add_linger() {
    return add_response("Connection:%s\r\n", (vlinger == true) ? "keep-alive" : "close");
}

bool HTTP::add_status_line(int status, const char *title) {
    return add_response("%s %d %s\r\n", "HTTP/1.1", status, title);
}

bool HTTP::add_header(int content_len) {
    return add_content_length(content_len) && add_linger && add_blank_line();
}

bool HTTP::add_content_length(int content_len) {
    return add_response("Content-Length:%d\r\n", content_len);
}
bool HTTP::add_content_type() {
    return add_response("Content-Type:%s\r\n", "text/html");
}

bool HTTP::add_content(const char *content) {
    return add_response("%s", content);
}

