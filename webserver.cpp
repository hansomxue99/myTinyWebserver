#include "webserver.h"

WebServer::WebServer() {

}

WebServer::~WebServer() {

}

void WebServer::eventListen() {
    int ret = 0;

    // socket 监听
    listen_fd = socket(PF_INET, SOCK_STREAM, 0);
    assert(listen_fd >= 0);

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
    allow_addr.sin_port = htons(port);

    // 设置端口复用，先复用后绑定端口
    int reuse_opt = 1; // 设置为1表示可以复用
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &reuse_opt, sizeof(reuse_opt));
    ret = bind(listen_fd, (struct sockaddr*)&allow_addr, sizeof(allow_addr));
    assert(ret >= 0);

    // 监听
    ret = listen(listen_fd, 5);   // 后面的数字不关键
    assert(ret >= 0);

    // 开启定时器
    // TODO

    // 创建epoll
    epoll_event events[MAX_EVENT_NUMBER];
    epoll_fd = epoll_create(5); // 后面的数字不关键
    assert(epoll_fd != -1);

    // 有了epoll之后，那就必须把文件描述符相关的内容都关联上去
    lepoll.add_epollfd(epoll_fd, listen_fd, false, ET_MOD);
    // TODO
}