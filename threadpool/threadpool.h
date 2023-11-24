#ifndef THREADPOLL_H
#define THREADPOLL_H

#include "../log/locker.h"

#include <pthread.h>

#include <list>
#include <exception>
using namespace std;

template <typename T>
class ThreadPool {
public:
    ThreadPool(int actor_model, int thread_number, int max_requests);
    ~ThreadPool();
    bool append(T *request, int state);
    bool append_p(T *request);

private:
    static void *worker(void *arg);
    void run();

private:
    int             vthread_number;     // 线程池中的线程数
    pthread_t       *vthreads;          // 线程池
    int             vmax_requests;      // 请求队列中的最大请求数
    list<T *>       vwork_queue;        // 请求队列，其实就是http请求
    MutexLocker     vlock_queue;        // 保护请求队列的互斥锁
    Sem             vstate_queue;       // 请求队列中是否有任务需要处理
    int             vactor_model;       // 反应模型
};

#endif
