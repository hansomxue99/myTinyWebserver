#include "timer_new.h"
#include "../http/http.h"

#include <sys/time.h>
#include <unistd.h>



void TimerHeap::add_timer(Timer *timer) {
    if (!timer) return;
    min_heap.push(timer);
}

void TimerHeap::adjust_timer(Timer *timer) {

}

void TimerHeap::del_timer(Timer *timer) {

}

void TimerHeap::tick() {
    time_t cur = time(NULL);
    while (!min_heap.empty()) {
        Timer *tmp = min_heap.top();
        if (cur < tmp->expire) break;
        min_heap.pop();
        tmp->cb_func(tmp->user_data);
    }
}

void SignalTimer::init(int timeslot) {
    m_TIMESLOT = timeslot;
}

void SignalTimer::sig_handler(int sig) {
    int save_errno = errno;
    int msg = sig;
    send(u_pipefd[1], (char *)&msg, 1, 0);
    errno = save_errno;
}

void SignalTimer::add_sig(int sig, void(handler)(int), bool restart) {
    struct sigaction sa;
    memset(&sa, '\0', sizeof(sa));
    sa.sa_handler = handler;

    if (restart)    sa.sa_flags |= SA_RESTART;
    sigfillset(&sa.sa_mask);
    assert(sigaction(sig, &sa, NULL) != -1);
}

void SignalTimer::timer_handler() {
    m_timer_list.tick();
    alarm(m_TIMESLOT);
}

void SignalTimer::show_error(int connfd, const char *info) {
    send(connfd, info, strlen(info), 0);
    close(connfd);
}

int* SignalTimer::u_pipefd = 0;
int SignalTimer::u_epollfd = 0;

class SignalTimer;
void cb_func(client_data *user_data) {
    epoll_ctl(SignalTimer::u_epollfd, EPOLL_CTL_DEL, user_data->sockfd, 0);
    assert(user_data);
    close(user_data->sockfd);
    HTTP::vuser_count--;
}
