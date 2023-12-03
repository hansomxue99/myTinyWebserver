#include "timer.h"

SortTimerList::SortTimerList() {
    head == NULL;
    tail == NULL;
}

SortTimerList::~SortTimerList() {
    Timer *tmp = head;
    while (tmp) {
        head = tmp->next;
        delete tmp;
        tmp = head;
    }
}

void SortTimerList::add_timer(Timer *timer, Timer *list_head) {
    Timer *prev = list_head;
    Timer *tmp = prev->next;

    while (tmp) {
        if (timer->expire < tmp->expire) {
            prev->next = timer;
            timer->next = tmp;
            tmp->prev = timer;
            timer->prev = prev;
            break;
        }
        prev = tmp;
        tmp = tmp->next;
    }

    if (!tmp) {
        prev->next = timer;
        timer->prev = prev;
        timer->next = NULL;
        tail = timer;
    }
}

void SortTimerList::add_timer(Timer *timer) {
    if (!timer) return;
    
    if (!head) {
        head = tail = timer;
        return;
    }

    if (timer->expire < head->expire) {
        timer->next = head;
        head->prev = timer;
        head = timer;
        return;
    }
    add_timer(timer, head);
}

void SortTimerList::adjust_timer(Timer *timer) {
    if (!timer) return;

    Timer *tmp = timer->next;
    if (!tmp || timer->expire < tmp->expire)    return;

    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        timer->next = NULL;
        add_timer(timer, head);
    } else {
        timer->prev->next = timer->next;
        timer->next->prev = timer->prev;
        add_timer(timer, timer->next);
    }
}

void SortTimerList::del_timer(Timer *timer) {
    if (!timer) return;

    if ((timer == head) && (timer == tail)) {
        delete timer;
        head = NULL;
        tail = NULL;
        return;
    }

    if (timer == head) {
        head = head->next;
        head->prev = NULL;
        delete timer;
        return;
    }
    if (timer == tail) {
        tail = tail->prev;
        tail->next = NULL;
        delete timer;
        return;
    }

    timer->prev->next = timer->next;
    timer->next->prev = timer->prev;
    delete timer;
}

void SortTimerList::tick() {
    if (!head)  return;

    time_t cur = time(NULL);
    Timer *tmp = head;

    while (tmp) {
        if (cur < tmp->expire)  break;

        // 执行定时器事件
        tmp->cb_func(tmp->user_data);

        head = tmp->next;
        if (head)   head->prev = NULL;
        delete tmp;
        tmp = head;
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
