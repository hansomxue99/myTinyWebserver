#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <assert.h>

#include "../localepoll/localepoll.h"
#include "../http/http.h"
// #include "../threadpool/threadpool.h"
#include "../threadpool/threadpool_new.h"
// #include "../timer/timer.h"
#include "../timer/timer_new.h"

#define LT_LT   0
#define LT_ET   1
#define ET_LT   2
#define ET_ET   3

#define LOG_SYN     0
#define LOG_ASYN    1

#define REACTOR     1
#define PROACTOR    0

const int MAX_EVENT_NUMBER = 10000; // 最大事件数
const int MAX_FD = 65536;           // 最大文件描述符
const int TIMESLOT = 5;             // 最小超时单位

class WebServer {
public:
    WebServer();
    ~WebServer();

    void init(int port, string user, string passwd, string dbname, int log_write, int linger, 
              int trigmode, int sql_num, int thread_num, int close_log, int actor_mode);

    void eventListen();
    void eventLoop();

private:
    void timer(int connfd, struct sockaddr_in);
    void adjust_timer(Timer *timer);

    bool deal_connection();
    bool deal_timer(Timer *timer, int sockfd);
    bool deal_signal(bool &timeout, bool &stop_server);
    void deal_read(int sockfd);
    void deal_write(int sockfd);
public:
    int     m_port;
    char    *m_root;
    int     m_log_write;
    int     log_close;
    int     m_actormode;

    int     m_pipefd[2];
    int     m_epollfd;
    HTTP    *users;
    int     m_listenfd;

    epoll_event events[MAX_EVENT_NUMBER];
    LocalEpoll lepoll;
    int m_LINGER;
    int m_TRIGMode;
    int m_LISTENTRIGMode;
    int m_CONNTRIGMode;

    // 数据库
    CGImysql    *m_connPool;
    string      m_user;
    string      m_passwd;
    string      m_dbname;
    int         m_sqlnum;

    //线程池
    // ThreadPool<HTTP> *m_pool;
    ThreadPool *m_pool;
    int m_threadnum;

    // 定时器
    client_data *users_timer;
    SignalTimer signal_timer;
};

#endif