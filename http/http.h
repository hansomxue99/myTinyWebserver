#ifndef HTTP_H
#define HTTP_H

#include "../localepoll/localepoll.h"

#include <sys/types.h>
#include <sys/unistd.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <arpa/inet.h>

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

    HTTP_CODE process_read();
    bool process_write(HTTP_CODE ret);
    HTTP_CODE process_request();

    char *get_line();
    LINE_STATUS parse_line();
    HTTP_CODE parse_request_line(char *text);
    HTTP_CODE parse_header(char *text);
    HTTP_CODE parse_content(char *text);

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
    MySQL *mysql;

private:
    int vsock_fd;
    sockaddr_in vaddress;
    LocalEpoll lepoll;
    int log_close;
    int vepoll_mod;

    long vchecked_idx;
    long vread_idx;
    char vread_buf[READ_BUF_SIZE];
    int vstart_line;

    METHOD vmethod;
    CHECK_STATE vcheck_state;
    char *vurl;
    char *vversion;

    bool vlinger;
    int cgi;    // 是否启用post
    char *doc_root;
    char *vhost;
    long vcontent_len;
    char *vrequest_str;

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