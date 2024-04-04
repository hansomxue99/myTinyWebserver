#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <mutex>
#include <future>
#include <functional>
#include <utility>
#include <thread>
#include <vector>
#include "../CGImysql/cgi_mysql.h"
#include "../http/http.h"
/*
** Name: 
** Description: 实现一个线程安全的队列
** Author: wkxue
** Create time: 2024/02/07 22:58
*/
template<typename T>
class SafeQueue {
public:
    SafeQueue() {}
    SafeQueue(SafeQueue &&other) {}
    ~SafeQueue() {}

    bool empty() {
        std::unique_lock<std::mutex> lock(m_mutex); // unique_lock: RAII编程技巧
        return m_queue.empty();
    }

    int size() {
        std::unique_lock<std::mutex> lock(m_mutex);
        return m_queue.size();
    }

    void push(T &t) {
        std::unique_lock<std::mutex> lock(m_mutex);
        m_queue.emplace(t);
    }

    bool pop(T &t) {
        std::unique_lock<std::mutex> lock(m_mutex);
        if (m_queue.empty()) {
            return false;
        }
        t = std::move(m_queue.front());
        m_queue.pop();
        return true;
    }

private:
    std::queue<T>   m_queue;    // 任务队列
    std::mutex      m_mutex;    // 互斥锁
};


class ThreadPool {
public:
    ThreadPool(const int n_threads = 4) : m_threads(std::vector<std::thread>(n_threads)), m_shutdown(false) {}
    ThreadPool(const ThreadPool &) = delete;
    ThreadPool(ThreadPool &&) = delete;
    ThreadPool &operator=(const ThreadPool &) = delete;
    ThreadPool &operator=(ThreadPool &&) = delete;

    void init() {
        for (int i=0; i<m_threads.size(); ++i) {
            m_threads.at(i) = std::thread(ThreadWorker(this, i));
        }
    }

    void shutdown() {
        m_shutdown = true;
        m_condition_lock.notify_all();
        
        for (int i=0; i<m_threads.size(); ++i) {
            if (m_threads.at(i).joinable()) {
                m_threads.at(i).join();
            }
        }
    }

    template<typename F, typename... Args>
    auto submit(F &&f, Args &&...args) -> std::future<decltype(f(args...))> {
        std::function<decltype(f(args...))()> func = std::bind(std::forward<F>(f), std::forward<Args>(args)...); 

        auto task_ptr = std::make_shared<std::packaged_task<decltype(f(args...))()>>(func);
        
        std::function<void()> warpper_func = [task_ptr]() {
            (*task_ptr)();
        };

        m_queue.push(warpper_func);

        m_condition_lock.notify_one();

        return task_ptr->get_future();
    }

private:
    class ThreadWorker {
    public:
        ThreadWorker(ThreadPool *pool, const int id) : m_pool(pool), m_id(id) {}
        /*
        关于仿函数使类作为可调用对象创建多线程，参考： 
        https://zh.cppreference.com/w/cpp/thread/thread/thread
        */ 
        void operator()() {
            std::function<void()> func;
            bool is_pop_queue;

            while (!m_pool->m_shutdown) {
                {
                    std::unique_lock<std::mutex> lock(m_pool->m_condition_mutex);
                    if (m_pool->m_queue.empty()) {
                        m_pool->m_condition_lock.wait(lock);
                    }
                    is_pop_queue = m_pool->m_queue.pop(func);
                }

                if (is_pop_queue) {
                    func();
                }
            }
        }
    private:
        int m_id;
        ThreadPool *m_pool;
    };
private:
    bool m_shutdown;
    std::vector<std::thread> m_threads;
    SafeQueue<std::function<void()>> m_queue;
    std::condition_variable m_condition_lock;
    std::mutex m_condition_mutex;
};

#endif
