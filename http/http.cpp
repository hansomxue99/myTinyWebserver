#include "http.h"

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

int HTTP::vepoll_fd = -1;  // 提供 veepoll_fd 的定义
int HTTP::vuser_count = 0;  // 提供 vuser_count 的定义
map<string, string> users;

void HTTP::http_init() {
    vmethod = GET;
    vcheck_state = CHECK_STATE_REQUESTLINE;
    vurl = 0;
    vversion = 0;

    // mysql = NULL;
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
** Name: 初始化mysql
** Description: 
** Author: wkxue
** Create time: 2023/11/30 12:46
*/
void HTTP::initmysql_result(CGImysql *connPool) {
    MYSQL *mysql = NULL;
    CGImysqlRAII mysqlcon(&mysql, connPool);
    if (mysql_query(mysql, "SELECT username,passwd FROM user")) {
        LOG_ERROR("SELECT error:%s\n", mysql_error(mysql));
    }

    MYSQL_RES *result = mysql_store_result(mysql);
    int num_fields = mysql_num_fields(result);
    MYSQL_FIELD *fields = mysql_fetch_fields(result);

    while (MYSQL_ROW row = mysql_fetch_row(result)) {
        string tmp1(row[0]);
        string tmp2(row[1]);
        users[tmp1] = tmp2;
    }
    for(auto it : users){
	cout << it.first <<" "<< it.second <<endl;
    }
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
        lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLIN, vepoll_mod);
        return;
    }
    bool write_ret = process_write(read_ret);
    if (!write_ret) http_close();
    lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLOUT, vepoll_mod);
}

/*
** Name: HTTP 写响应报文函数
** Description: 调用接口变量：vbyte_to_send(待发送), vbyte_have_send(已发送)
** Author: wkxue
** Create time: 2023/11/29 14:13
*/
bool HTTP::http_write() {
    int tmp = 0;

    // 响应报文为空，重新开始
    if (vbyte_to_send == 0) {
        lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLIN, vepoll_mod);
        http_init();
        return true;
    }

    while (1) {
        // 将响应报文发送给web端
        tmp = writev(vsock_fd, viovec, viovec_count);
        if (tmp < 0) {
            if (errno == EAGAIN) {
                lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLOUT, vepoll_mod);
                return true;
            }
            unmap();
            return false;
        }

        vbyte_have_send += tmp;
        vbyte_to_send -= tmp;
        // 判断头部信息是否发送完成
        if (vbyte_have_send >= viovec[0].iov_len) {
            viovec[0].iov_len = 0;
            viovec[1].iov_base = vfile_addr + (vbyte_have_send - vwrite_idx);
            viovec[1].iov_len = vbyte_to_send;
        } else {
            viovec[0].iov_base = vwrite_buf + vbyte_have_send;
            viovec[0].iov_len = viovec[0].iov_len - vbyte_have_send;
        }

        // 数据发送完成
        if (vbyte_to_send <= 0) {
            unmap();
            lepoll.modify_epollfd(vepoll_fd, vsock_fd, EPOLLIN, vepoll_mod);
            if (vlinger) {  // 长连接，后续还有数据
                http_init();
                return true;
            } else {
                return false;
            }
        }
    }
}

/*
** Name: HTTP 读取函数
** Description: 将http报文一次性读取，接口变量：vread_idx(当前缓存区长度), vread_buf(缓存区)
** Author: wkxue
** Create time: 2023/11/29 13:20
*/
bool HTTP::http_read() {
    if (vread_idx >= READ_BUF_SIZE) {
        return false;
    }
    int byte_read = 0;
    // LT 读取模式：一次性读完
    if (LT_MOD == vepoll_mod) {
        byte_read = recv(vsock_fd, vread_buf+vread_idx, READ_BUF_SIZE-vread_idx, 0);
        if (byte_read <= 0)  return false;
        vread_idx += byte_read;
    } else {    // ET模式：非阻塞循环读
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
** Description: http 读取并解析请求报文 vstart=0
** Author: wkxue
** Create time: 2023/11/26 14:50
*/
HTTP::HTTP_CODE HTTP::process_read() {
    LINE_STATUS line_status = LINE_OK;
    HTTP_CODE ret = NO_REQUEST;
    char *text = 0;

    // 注意：parse_line每调用一次则是更新接口变量vchecked_idx(指向新的一行)
    while ((vcheck_state == CHECK_STATE_CONTENT && line_status == LINE_OK) || (line_status = parse_line()) == LINE_OK) {
        text = vread_buf + vstart_line;
        vstart_line = vchecked_idx;   // start_line记录的是当前行起始地址相对于read_buf的偏移量
        LOG_INFO("%s", text);
        switch (vcheck_state)
        {
        // 请求行 buf+0
        case CHECK_STATE_REQUESTLINE:
            ret = parse_request_line(text);
            if (ret == BAD_REQUEST) return BAD_REQUEST;
            break;
        // 头部字段
        // buf + request_len
        case CHECK_STATE_HEADER:
            ret = parse_header(text);
            if (ret == BAD_REQUEST) return BAD_REQUEST;
            else if (ret == GET_REQUEST)    return do_request();
            break;
        // 报文内容
        // buf + request_len + header_len
        case CHECK_STATE_CONTENT:
            ret = parse_content(text);
            if (ret == GET_REQUEST) do_request();
            line_status = LINE_OPEN;
            break;
        default:
            return INTERNAL_ERROR;
        } 
    }
    return NO_REQUEST;
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
        cgi = 1;
    } else {
        return BAD_REQUEST;
    }

    // 解析HTTP版本，仅支持http/1.1
    vversion = strpbrk(vurl, " \t");
    if (!vversion)  return BAD_REQUEST;
    *vversion++ = '\0';
    vversion += strspn(vurl, " \t");    // 至此，method,url,version分离
    if (strcasecmp(vversion, "HTTP/1.1") != 0)  return BAD_REQUEST;
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
        strcat(vurl, "judge.html");  // 注意，这里将根目录固定了judge，长度超过5会有bug
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
    // 读到空行，vcontent_len=0表示GET，!=0表示POST
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
        vcontent_str = text;
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
MutexLocker m_lock;
HTTP::HTTP_CODE HTTP::do_request() {
    int len = strlen(doc_root);
    strcpy(vreal_file, doc_root);
    char *p = strrchr(vurl, '/');

    if (cgi==1 && (*(p+1)=='2' || *(p+1)=='3')) {
        char flag = vurl[1];
        char *tmp = (char *)malloc(sizeof(char)*200);
        strcpy(tmp, "/");
        strcat(tmp, vurl+2);
        free(tmp);
        char name[100], passwd[100];
        int i, j = 0;
        for (i=5; vcontent_str[i]!='&'; ++i) {
            name[i-5] = vcontent_str[i];
        }
        name[i-5] = '\0';

        for (i=i+10; vcontent_str[i]!='\0'; ++i, ++j) {
            passwd[j] = vcontent_str[i];
        }
        passwd[j] = '\0';

        if (*(p+1) == '3') {
            char *sql_insert = (char *)malloc(sizeof(char) * 200);
            strcpy(sql_insert, "INSERT INTO user(username, passwd) VALUES(");
            strcat(sql_insert, "'");
            strcat(sql_insert, name);
            strcat(sql_insert, "', '");
            strcat(sql_insert, passwd);
            strcat(sql_insert, "')");
            if (users.find(name) == users.end()) {
                m_lock.lock();
                int res = mysql_query(mysql, sql_insert);
                users.insert(pair<string, string>(name, passwd));
                m_lock.unlock();

                if (!res)   strcpy(vurl, "/log.html");
                else strcpy(vurl, "/registerError.html");
                cout << vurl << endl;
            } else {
                strcpy(vurl, "/registerError.html");
            }
        } else if (*(p+1) == '2') {
            if (users.find(name) != users.end() && users[name] == passwd) 
                strcpy(vurl, "/welcome.html");
            else
                strcpy(vurl, "/logError.html");
        }
    }

    if (*(p+1) == '0') {
        char *tmp = (char *)malloc(sizeof(char)*200);
        strcpy(tmp, "/register.html");
        strncpy(vreal_file+len, tmp, strlen(tmp));
        free(tmp);
    } else if (*(p+1) == '1') {
        char *tmp = (char *)malloc(sizeof(char)*200);
        strcpy(tmp, "/log.html");
        strncpy(vreal_file+len, tmp, strlen(tmp));
        free(tmp);
    } else if (*(p+1) == '5') {
        char *tmp = (char *)malloc(sizeof(char)*200);
        strcpy(tmp, "/picture.html");
        strncpy(vreal_file+len, tmp, strlen(tmp));
        free(tmp);
    } else if (*(p+1) == '6') {
        char *tmp = (char *)malloc(sizeof(char)*200);
        strcpy(tmp, "/video.html");
        strncpy(vreal_file+len, tmp, strlen(tmp));
        free(tmp);
    } else if (*(p+1) == '7') {
        char *tmp = (char *)malloc(sizeof(char)*200);
        strcpy(tmp, "/fans.html");
        strncpy(vreal_file+len, tmp, strlen(tmp));
        free(tmp);
    } else {
        strncpy(vreal_file+len, vurl, FILENAME_LEN-len-1);
    }

    if (stat(vreal_file, &vfile_stat) < 0)  return NO_RESOURCE;
    if (!(vfile_stat.st_mode & S_IROTH))    return FORBIDDEN_REQUEST;
    if (S_ISDIR(vfile_stat.st_mode))    return BAD_REQUEST;

    int fd = open(vreal_file, O_RDONLY);
    vfile_addr = (char *)mmap(0, vfile_stat.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);
    return FILE_REQUEST;
}

/*********************HTTP WRITE*****************************/
/*
** Name: http write function
** Description: http发送响应给web，调用接口变量write_buf，file_addr
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
    return add_content_length(content_len) && add_linger() && add_blank_line();
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

