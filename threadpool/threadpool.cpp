#include "threadpool.h"

/*
** Name: 构造函数
** Description: 
** Author: wkxue
** Create time: 2023/11/24 16:12
*/
template <typename T>
ThreadPool<T>::ThreadPool(int actor_model, int thread_number, int max_requests)
     : vactor_model(actor_model), vthread_number(thread_number), vmax_requests(max_requests), vthreads(NULL) {
    
    if (thread_number <= 0 || max_requests <= 0)    throw exception();
    vthreads = new pthread_t[vthread_number];
    if (!vthreads)  throw exception();

    for (int i=0; i<thread_number; ++i) {
        if (pthread_create(vthreads+i, NULL, worker, this) != 0) {
            delete[] vthreads;
            throw exception();
        }
        if (pthread_detach(vthreads[i])) {
            delete[] vthreads;
            throw exception();
        }
    }
}

/*
** Name: 析构函数
** Description: 
** Author: wkxue
** Create time: 2023/11/24 16:13
*/
template <typename T>
ThreadPool<T>::~ThreadPool() {
    delete[] vthreads;
}

/*
** Name: Thread append function
** Description: 
** Author: wkxue
** Create time: 2023/11/24 15:05
*/
template <typename T>
bool ThreadPool<T>::append(T *request, int state) {
    vlock_queue.lock();
    if (vwork_queue.size() >= vmax_requests) {
        vlock_queue.unlock();
        return false;
    }
    // 相当于一个生产者
    request->vstate = state;
    vwork_queue.push_back(request);
    vlock_queue.unlock();
    vstate_queue.post();
    return true;
}

/*
** Name: Thread append fucntion
** Description: without request state!
** Author: wkxue
** Create time: 2023/11/24 15:09
*/
template <typename T>
bool ThreadPool<T>::append_p(T *request) {
    vlock_queue.lock();
    if (vwork_queue.size() >= vmax_requests) {
        vlock_queue.unlock();
        return false;
    }
    vwork_queue.push_back(request);
    vlock_queue.unlock();
    vstate_queue.post();
    return true;
}

/*
** Name: worker thread
** Description: 
** Author: wkxue
** Create time: 2023/11/24 15:21
*/
template <typename T>
void *ThreadPool<T>::worker(void *arg) {
    ThreadPool *pool = (ThreadPool *)arg;
    pool->run();
    return pool;
}

/*
** Name: 工作线程的处理函数
** Description: 
** Author: wkxue
** Create time: 2023/11/24 15:59
*/
template <typename T>
void ThreadPool<T>::run() {
    while (true) {
        // 相当于消费者
        vstate_queue.wait();
        vlock_queue.lock();
        if (vwork_queue.empty()) {
            vlock_queue.unlock();
            continue;
        }

        T *request = vwork_queue.front();
        vwork_queue.pop_front();
        vlock_queue.unlock();

        if (!request)   continue;

        // 处理线程proactor/reactor
        // TODO
    }
}
