#ifndef TIMER_H
#define TIMER_H

#include <vector>
#include <queue>

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

class Timer;
struct client_data {
    sockaddr_in address;
    int sockfd;
    Timer *timer;
};

class Timer {
public:
    Timer() : expire(0), cb_func(nullptr), user_data(nullptr) {}
    ~Timer() {}

    void (* cb_func)(client_data *);
    client_data *user_data;
    time_t expire;
};

class TimerCompare {
public:
    bool operator()(const Timer* a, const Timer* b) const {
        return a->expire > b->expire; // 小顶堆排序
    }
};


class TimerHeap {
public:
    TimerHeap() {}
    ~TimerHeap() {}

    void add_timer(Timer *timer);
    void adjust_timer(Timer *timer);
    void del_timer(Timer *timer);
    void tick();

private:
    std::priority_queue<Timer*, std::vector<Timer*>, TimerCompare> min_heap;
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
    TimerHeap m_timer_list;
    static int u_epollfd;
    int m_TIMESLOT;
};

void cb_func(client_data *user_data);

#endif