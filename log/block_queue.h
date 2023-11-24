#ifndef BLOCK_QUEUE_H
#define BLOCK_QUEUE_H

#include <time.h>
#include <sys/time.h>
#include "locker.h"

/*
** Name: BlockQueue
** Description: 
** Author: wkxue
** Create time: 2023/11/22 13:44
*/
template <class T>
class BlockQueue {
public:
    BlockQueue(int max_size = 1000)
         : vmax_size(max_size), vsize(0), vfront(-1), vback(-1) {
        if (max_size <= 0)  exit(-1);
        array = new T[max_size];
    }
    ~BlockQueue() {
        mutex.lock();
        if (array != NULL) {
            delete []array;
            array = NULL;
        }
        mutex.unlock();
    }

    void clear() {
        mutex.lock();
        vsize = 0;
        vfront = -1;
        vback = -1;
        mutex.unlock();
    }
    bool full() {
        mutex.lock();
        if (vsize >= vmax_size) {
            mutex.unlock();
            return true;
        }
        mutex.unlock();
        return false;
    }
    bool empty() {
        mutex.lock();
        if (0 == vsize) {
            mutex.unlock();
            return true;
        }
        mutex.unlock();
        return false;
    }
    bool front(T &value) {
        mutex.lock();
        if (0 == vsize) {
            mutex.unlock();
            return false;
        }
        value = array[vfront];
        mutex.unlock();
        return true;
    }
    bool back(T &value) {
        mutex.lock();
        if (0 == vsize) {
            mutex.unlock();
            return false;
        }
        value = array[vback];
        return true;
    }

    int size() {
        int ret = 0;
        mutex.lock();
        ret = vsize;
        mutex.unlock();
        return ret;
    }
    int max_size() {
        int ret = 0;
        mutex.lock();
        ret = vmax_size;
        mutex.unlock();
        return ret;
    }

    bool push(const T &value) {
        mutex.lock();
        if (vsize >= vmax_size) {
            cond.broadcast();
            mutex.unlock();
            return false;
        }
        vback = (vback + 1) % vmax_size;
        array[vback] = value;
        ++vsize;
        cond.broadcast();
        mutex.unlock();
        return true;
    }
    bool pop(T &value) {
        mutex.lock();
        while (vsize <= 0) {
            if (!cond.wait(mutex.get())) {
                mutex.unlock();
                return false;
            }
        }
        vfront = (vfront + 1) % vmax_size;
        value = array[vfront];
        --vsize;
        mutex.unlock();
        return true;
    }
    bool pop(T &value, int ms_timeout) {
        struct timespec t = {0, 0};
        struct timeval now = {0, 0};
        gettimeofday(&now, NULL);
        mutex.lock();
        
        if (vsize <= 0) {
            t.tv_sec = now.tv_sec + ms_timeout / 1000;
            t.tv_nsec = (ms_timeout % 1000) * 1000;
            if (!cond.timewait(mutex.get(), t)) {
                mutex.unlock();
                return false;
            }
        }

        if (vsize <= 0) {
            mutex.unlock();
            return false;
        }

        vfront = (vfront + 1) % vmax_size;
        value = array[vfront];
        --vsize;
        mutex.unlock();
        return true;
    }

private:
    MutexLocker mutex;
    Cond        cond;

    T   *array;
    int vsize;
    int vmax_size;
    int vfront;
    int vback;
};

#endif
