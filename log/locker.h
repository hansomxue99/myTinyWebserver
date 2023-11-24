#ifndef LOCKER_H
#define LOCKER_H

#include <semaphore.h>
#include <pthread.h>

#include <exception>
using namespace std;

/*
** Name: semaphore
** Description: 
** Author: wkxue
** Create time: 2023/11/22 12:58
*/
class Sem {
public:
    Sem() {
        if (sem_init(&sem, 0, 0) != 0)  throw exception();
    }
    Sem(int num) {
        if (sem_init(&sem, 0, num) != 0)    throw exception();
    }
    ~Sem() {
        sem_destroy(&sem);
    }
    bool wait() {
        return sem_wait(&sem) == 0;
    }
    bool post() {
        return sem_post(&sem) == 0;
    }

private:
    sem_t sem;
};

/*
** Name: MutexLocker
** Description: 
** Author: wkxue
** Create time: 2023/11/22 12:59
*/
class MutexLocker {
public:
    MutexLocker() {
        if (pthread_mutex_init(&mutex, NULL) != 0)    throw exception();
    }
    ~MutexLocker() {
        pthread_mutex_destroy(&mutex);
    }
    bool lock() {
        return pthread_mutex_lock(&mutex) == 0;
    }
    bool unlock() {
        return pthread_mutex_unlock(&mutex) == 0;
    }
    pthread_mutex_t *get() {
        return &mutex;
    }

private:
    pthread_mutex_t mutex;
};

/*
** Name: Condition
** Description: 
** Author: wkxue
** Create time: 2023/11/22 13:29
*/
class Cond {
public:
    Cond() {
        if (pthread_cond_init(&cond, NULL) != 0)    throw exception();
    }
    ~Cond() {
        pthread_cond_destroy(&cond);
    }
    bool wait(pthread_mutex_t *mutex) {
        int ret = 0;
        ret = pthread_cond_wait(&cond, mutex);
        return ret == 0;
    }
    bool timewait(pthread_mutex_t *mutex, struct timespec t) {
        int ret = 0;
        ret = pthread_cond_timedwait(&cond, mutex, &t);
        return ret == 0;
    }
    bool signal() {
        return pthread_cond_signal(&cond) == 0;
    }
    bool broadcast() {
        return pthread_cond_broadcast(&cond) == 0;
    }

private:
    pthread_cond_t cond;
};

#endif
