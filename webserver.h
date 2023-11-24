#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/epoll.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>
#include <assert.h>

#include "./localepoll/localepoll.h"

const int MAX_EVENT_NUMBER = 10000;

class WebServer {
    WebServer();
    ~WebServer();

    void eventListen();

private:
    int listen_fd;
    int epoll_fd;
    LocalEpoll lepoll;
public:
    int port;
};

#endif