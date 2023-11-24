#include "localepoll.h"

/*
** Name: set fd nonblock function
** Description: 
** Author: wkxue
** Create time: 2023/11/23 13:05
*/
int LocalEpoll::set_nonblock(int fd) {
    int old_opt = fcntl(fd, F_GETFL);
    int new_opt = old_opt | O_NONBLOCK;
    fcntl(fd, F_SETFL, new_opt);
    return old_opt;
}

/*
** Name: add epoll fd function
** Description: 
** Author: wkxue
** Create time: 2023/11/23 13:08
*/
void LocalEpoll::add_epollfd(int epoll_fd, int fd, bool one_shot, int trig_mod) {
    /*
    epoll_event:
        - events
        - data:
            - fd
    */
    epoll_event event;
    event.data.fd = fd;
    if (trig_mod == ET_MOD)
        event.events = EPOLLIN | EPOLLRDHUP | EPOLLET;
    else
        event.events = EPOLLIN | EPOLLRDHUP;

    // 防止同一个通信被多个线程处理
    if (one_shot)   event.events |= EPOLLONESHOT;

    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &event);
    set_nonblock(fd);
}

/*
** Name: remove fd from epoll_fd function
** Description: 
** Author: wkxue
** Create time: 2023/11/23 16:17
*/
void LocalEpoll::remove_epollfd(int epoll_fd, int fd) {
    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
    close(fd);
}

/*
** Name: modify fd in epoll_fd function
** Description: 
** Author: wkxue
** Create time: 2023/11/23 19:01
*/
void LocalEpoll::modify_epollfd(int epoll_fd, int fd, int ev) {
    epoll_event event;
    event.data.fd = fd;
    event.events = ev | EPOLLET | EPOLLONESHOT | EPOLLRDHUP;
    epoll_ctl(epoll_fd, EPOLL_CTL_MOD, fd, &event);
}


