#include "webserver.h"

WebServer::WebServer() {
    users = new HTTP[MAX_FD];

    char server_path[200];
    getcwd(server_path, 200);
    char root[6] = "/root";
    m_root = (char *)malloc(strlen(server_path)+strlen(root)+1);
    strcpy(m_root, server_path);
    strcat(m_root, root);

    users_timer = new client_data[MAX_FD];
}

WebServer::~WebServer() {
    close(m_epollfd);
    close(m_listenfd);
    close(m_pipefd[1]);
    close(m_pipefd[0]);
    delete[] users;
    delete[] users_timer;
    delete m_pool;
}

void WebServer::init(int port, string user, string passwd, string dbname, int log_write, int linger, 
                    int trigmode, int sql_num, int thread_num, int close_log, int actor_mode) {
    m_port = port;
    m_user = user;
    m_passwd = passwd;
    m_dbname = dbname;
    m_sqlnum = sql_num;
    m_threadnum = thread_num;
    m_log_write = log_write;
    log_close = close_log;
    m_LINGER = linger;
    m_actormode = actor_mode;

    // init trigmode
    switch (trigmode)
    {
    case LT_LT:
        m_LISTENTRIGMode = LT_MOD;
        m_CONNTRIGMode = LT_MOD;
        break;
    case LT_ET:
        m_LISTENTRIGMode = LT_MOD;
        m_CONNTRIGMode = ET_MOD;
        break;
    case ET_LT:
        m_LISTENTRIGMode = ET_MOD;
        m_CONNTRIGMode = LT_MOD;
        break;
    case ET_ET:
        m_LISTENTRIGMode = ET_ET;
        m_CONNTRIGMode = ET_MOD;
        break;
    default:
        break;
    }

    // init log mode
    if (log_close == 0) {
        if (LOG_ASYN == m_log_write) {
            Log::get_instance()->init("/home/ubuntu/myTinyWebserver/log_txt/ServerLog", close_log, 2000, 800000, 800);
        } else {
            Log::get_instance()->init("/home/ubuntu/myTinyWebserver/log_txt/ServerLog", close_log, 2000, 800000, 0);
        }
    }

    // init sql_pool
    m_connPool = CGImysql::getInstance();
    m_connPool->init("localhost", m_user, m_passwd, m_dbname, 3306, m_sqlnum, log_close);
    users->initmysql_result(m_connPool);

    // init thread_pool
    m_pool = new ThreadPool<HTTP>(m_actormode, m_connPool, m_threadnum);
}

void WebServer::eventListen() {
    int ret = 0;

    // socket 监听
    m_listenfd = socket(PF_INET, SOCK_STREAM, 0);
    assert(m_listenfd >= 0);

    if (0 == m_LINGER) {
        struct linger tmp = {0, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    } else if (1 == m_LINGER) {
        struct linger tmp = {1, 1};
        setsockopt(m_listenfd, SOL_SOCKET, SO_LINGER, &tmp, sizeof(tmp));
    }

    // 绑定端口和IP
    /*
    sockaddr_in:
        - sin_family
        - sin_port
        - sin_addr:
            - s_addr
    */
    struct sockaddr_in allow_addr;
    bzero(&allow_addr, sizeof(allow_addr));
    allow_addr.sin_family = AF_INET;
    allow_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    allow_addr.sin_port = htons(m_port);

    // 设置端口复用，先复用后绑定端口
    int reuse_opt = 1; // 设置为1表示可以复用
    setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuse_opt, sizeof(reuse_opt));
    ret = bind(m_listenfd, (struct sockaddr*)&allow_addr, sizeof(allow_addr));
    assert(ret >= 0);

    // 监听
    ret = listen(m_listenfd, 5);   // 后面的数字不关键
    assert(ret >= 0);

    // 开启定时器
    signal_timer.init(TIMESLOT);

    // 创建epoll
    m_epollfd = epoll_create(5); // 后面的数字不关键
    assert(m_epollfd != -1);

    // 有了epoll之后，那就必须把文件描述符相关的内容都关联上去
    lepoll.add_epollfd(m_epollfd, m_listenfd, false, m_LISTENTRIGMode);
    HTTP::vepoll_fd = m_epollfd;
    
    // 创建管道用于信号通信,并将其添加到信号
    ret = socketpair(PF_UNIX, SOCK_STREAM, 0, m_pipefd);
    assert(ret != -1);
    lepoll.set_nonblock(m_pipefd[1]);
    lepoll.add_epollfd(m_epollfd, m_pipefd[0], false, 0);
    signal_timer.add_sig(SIGPIPE, SIG_IGN);
    signal_timer.add_sig(SIGALRM, signal_timer.sig_handler, false);
    signal_timer.add_sig(SIGTERM, signal_timer.sig_handler, false);
    alarm(TIMESLOT);

    SignalTimer::u_epollfd = m_epollfd;
    SignalTimer::u_pipefd = m_pipefd;
}

void WebServer::eventLoop() {
    bool timeout = false;
    bool stop_server = false;

    while (!stop_server) {
        int number = epoll_wait(m_epollfd, events, MAX_EVENT_NUMBER, -1);

        if (number < 0 && errno != EINTR) {
            LOG_ERROR("%s", "epoll failure");
            break;
        }

        for (int i=0; i<number; ++i) {
            int sockfd = events[i].data.fd;

            if (sockfd == m_listenfd) { // 新的连接
                bool flag = deal_connection();
                if (flag == false)  continue;
            } else if (events[i].events & (EPOLLRDHUP | EPOLLHUP | EPOLLERR)) { // 服务器端关闭
                Timer *timer = users_timer[sockfd].timer;
                deal_timer(timer, sockfd);
            } else if ((sockfd == m_pipefd[0]) && (events[i].events & EPOLLIN)) {   // 处理信号
                bool flag = deal_signal(timeout, stop_server);
                if (flag == false) {
                    LOG_ERROR("%s", "dealclientdata failure");
                }
            } else if (events[i].events & EPOLLIN) {    // 处理客服连接的接收数据
                deal_read(sockfd);
            } else if (events[i].events & EPOLLOUT) {   // 处理客户连接上的写数据 
                deal_write(sockfd);
            }
        }

        if (timeout) {
            signal_timer.timer_handler();
            LOG_INFO("%s", "timer_tick");
            timeout = false;
        }
    }
}

bool WebServer::deal_connection() {
    struct sockaddr_in client_address;
    socklen_t client_addrlen = sizeof(client_address);

    if (LT_MOD == m_LISTENTRIGMode) {
        int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlen);
        if (connfd < 0) {
            LOG_ERROR("%s:errno is:%d", "accept error", errno);
            return false;
        }
        if (HTTP::vuser_count >= MAX_FD) {
            signal_timer.show_error(connfd, "Internal server busy");
            LOG_ERROR("%s", "Internal server busy");
            return false;
        }
        timer(connfd, client_address);
    } else {
        while(1) {
            int connfd = accept(m_listenfd, (struct sockaddr *)&client_address, &client_addrlen);
            if (connfd < 0) {
                LOG_ERROR("%s:errno is:%d", "accept error", errno);
                break;
            }
            if (HTTP::vuser_count >= MAX_FD) {
                signal_timer.show_error(connfd, "Internal server busy");
                LOG_ERROR("%s", "Internal server busy");
                break;
            }
            timer(connfd, client_address);
        }
        return false;
    }

    return true;
}

bool WebServer::deal_timer(Timer *timer, int sockfd) {
    timer->cb_func(&users_timer[sockfd]);

    if (timer) {
        signal_timer.m_timer_list.del_timer(timer);
    }

    LOG_INFO("close fd %d", users_timer[sockfd].sockfd);
}

bool WebServer::deal_signal(bool &timeout, bool &stop_server) {
    int ret = 0;
    int sig;
    char signals[1024];
    ret = recv(m_pipefd[0], signals, sizeof(signals), 0);
    if (ret == -1 || ret == 0)  return false;
    else {
        for (int i=0; i<ret; ++i) {
            switch (signals[i])
            {
            case SIGALRM:
                timeout = true;
                break;
            case SIGTERM:
                stop_server = true;
                break;
            default:
                break;
            }
        }
    }
    return true;
}

void WebServer::deal_read(int sockfd) {
    Timer *timer = users_timer[sockfd].timer;

    if (REACTOR == m_actormode) {
        if (timer)  adjust_timer(timer);
        m_pool->append(users+sockfd, 0);
        while (1) {
            if (1 == users[sockfd].improv) {
                if (1 == users[sockfd].timer_flag) {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    } else {
        if (users[sockfd].http_read()) {
            LOG_INFO("deal with the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));
            m_pool->append_p(users+sockfd);
            if (timer)  adjust_timer(timer);
        } else {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::deal_write(int sockfd) {
    Timer *timer = users_timer[sockfd].timer;

    if (REACTOR == m_actormode) {
        if (timer)  adjust_timer(timer);

        m_pool->append(users + sockfd, 1);

        while (1) {
            if (1 == users[sockfd].improv) {
                if (1 == users[sockfd].timer_flag) {
                    deal_timer(timer, sockfd);
                    users[sockfd].timer_flag = 0;
                }
                users[sockfd].improv = 0;
                break;
            }
        }
    } else {
        if (users[sockfd].http_write()) {
            LOG_INFO("send data to the client(%s)", inet_ntoa(users[sockfd].get_address()->sin_addr));

            if (timer)  adjust_timer(timer);
        }
        else {
            deal_timer(timer, sockfd);
        }
    }
}

void WebServer::timer(int connfd, struct sockaddr_in client_address) {
    // 初始化http连接
    users[connfd].http_init(connfd, client_address, m_root, m_CONNTRIGMode, log_close, m_user, m_passwd, m_dbname);

    // 初始化client——data
    users_timer[connfd].address = client_address;
    users_timer[connfd].sockfd = connfd;

    Timer *timer = new Timer;
    timer->user_data = &users_timer[connfd];
    timer->cb_func = cb_func;
    time_t cur = time(NULL);
    timer->expire = cur + 3*TIMESLOT;

    users_timer[connfd].timer = timer;
    signal_timer.m_timer_list.add_timer(timer);
}

void WebServer::adjust_timer(Timer *timer) {
    time_t cur = time(NULL);
    timer->expire = cur + 3 * TIMESLOT;
    signal_timer.m_timer_list.adjust_timer(timer);
    LOG_INFO("%s", "adjust timer once");
}
