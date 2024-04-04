#ifndef TIMERR_H
#define TIMERR_H

#include <stdio.h>
#include <time.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <string.h>
#include <assert.h>

#include "../http/http.h"

class Timer;
struct client_data {
    sockaddr_in address;
    int sockfd;
    Timer *timer;
};

// class Timer{
// public:
//     Timer(): prev(NULL), next(NULL) {}
//     void (* cb_func)(client_data *);

// public:
//     Timer *prev;
//     Timer *next;
//     time_t expire;
//     client_data *user_data;
// };

class Timer{
public:
    Timer(): prev(NULL), next(NULL) {}

public:
    time_t expire;
    
    void (* cb_func)(client_data *);
    client_data *user_data;
    Timer *prev;
    Timer *next;
};

class SortTimerList{
public:
    SortTimerList();
    ~SortTimerList();

    void add_timer(Timer *timer);
    void adjust_timer(Timer *timer);
    void del_timer(Timer *timer);
    void tick();

private:
    void add_timer(Timer *timer, Timer *list_head);

    Timer *head;
    Timer *tail;
};

class SignalTimer {
public:
    SignalTimer() {}
    ~SignalTimer() {}

    void init(int timeslot);
    static void sig_handler(int sig);
    void add_sig(int sig, void(handler)(int), bool restart = true);
    void timer_handler();
    void show_error(int connfd, const char *info);
public:
    static int *u_pipefd;
    SortTimerList m_timer_list;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif