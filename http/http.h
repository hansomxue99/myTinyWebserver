#ifndef HTTP_H
#define HTTP_H

#include "../localepoll/localepoll.h"
#include "../log/log.h"

#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>
#include <string.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/mman.h>

#include <iostream>
#include <map>

class HTTP {
public:
    static const int READ_BUF_SIZE = 2048;
    static const int WRITE_BUF_SIZE = 1024;
    static const int FILENAME_LEN = 200;
    enum METHOD {
        GET = 0,
        POST,
        HEAD,
        PUT,
        DELETE,
        TRACE,
        OPTIONS,
        CONNECT,
        PATH
    };
    // HTTP状态码
    enum HTTP_CODE {
        NO_REQUEST = 0,
        GET_REQUEST,
        BAD_REQUEST,
        NO_RESOURCE,
        FORBIDDEN_REQUEST,
        FILE_REQUEST,
        INTERNAL_ERROR,
        CLOSED_CONNECTION
    };
    // 标识解析位置
    enum CHECK_STATE {
        CHECK_STATE_REQUESTLINE = 0,
        CHECK_STATE_HEADER,
        CHECK_STATE_CONTENT
    };
    // 标识解析状态
    enum LINE_STATUS {
        LINE_OK = 0,
        LINE_BAD,
        LINE_OPEN
    };
public:
    HTTP() {}
    ~HTTP() {}

    void http_init(int sockfd, const sockaddr_in &addr, char *, int, int, string user, string passwd, string sqlname);
    void http_close();
    void http_process();
    bool http_read();
    bool http_write();

private:
    void http_init();
    void unmap();

    HTTP_CODE process_read();   // 主状态机处理请求报文
    char *get_line();
    LINE_STATUS parse_line();   // 从状态机解析每一行
    HTTP_CODE parse_request_line(char *text);   // 解析http请求行
    HTTP_CODE parse_header(char *text);         // 解析http头部
    HTTP_CODE parse_content(char *text);        // 读取http内容
    HTTP_CODE do_request();

    bool process_write(HTTP_CODE ret);
    bool add_response(const char *format, ...);
    bool add_blank_line();
    bool add_linger();
    bool add_status_line(int status, const char *title);
    bool add_header(int content_len);
    bool add_content_length(int content_len);
    bool add_content_type();
    bool add_content(const char *content);

public:
    static int vepoll_fd;
    static int vuser_count;
    int vstate;
    // MySQL *mysql;

private:
    int vsock_fd;
    sockaddr_in vaddress;
    LocalEpoll lepoll;
    int log_close;
    int vepoll_mod;

    CHECK_STATE vcheck_state;               // 主状态机的状态
    long        vchecked_idx;               // read buf 中读取的位置
    long        vread_idx;                  // read buf 中数据的最后一个字节的下一个位置
    long        vstart_line;                // read buf 中已经解析的字符个数
    char        vread_buf[READ_BUF_SIZE];   // 读取缓冲区

    METHOD vmethod;
    char *vurl;
    char *vversion;

    bool vlinger;
    int cgi;    // 是否启用post
    char *doc_root;
    char *vhost;
    long vcontent_len;
    char *vstr;

    char vwrite_buf[WRITE_BUF_SIZE];
    int vwrite_idx;
    char vreal_file[FILENAME_LEN];

    struct stat vfile_stat;
    struct iovec viovec[2];
    int viovec_count;
    char *vfile_addr;
    int vbyte_to_send;
    int vbyte_have_send;

    char sql_user[100];
    char sql_passwd[100];
    char sql_name[100];
};

#endif