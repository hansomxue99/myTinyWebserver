#ifndef LOCALEPOLL_H
#define LOCALEPOLL_H

#include <sys/epoll.h>
#include <unistd.h>
#include <fcntl.h>

#define ET_MOD 1
#define LT_MOD 0

/*
** Name: Epoll class defined by developer
** Description: 
** Author: wkxue
** Create time: 2023/11/23 13:04
*/
class LocalEpoll {
public:
    LocalEpoll() {}
    ~LocalEpoll() {}

    int set_nonblock(int fd);

    void add_epollfd(int epoll_fd, int fd, bool one_shot, int trig_mod);
    void remove_epollfd(int epoll_fd, int fd);
    void modify_epollfd(int epoll_fd, int fd, int ev);
};  

#endif