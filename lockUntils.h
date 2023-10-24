#pragma once

#include <mutex>
#include <pthread.h>
#include <exception>
#include <semaphore.h>

// 封装各种同步机制所需要的工具, 互斥锁、条件变量、信号量

class Locker
{
    public:
        Locker();
        ~Locker();
        bool lock();
        bool unlock();
        pthread_mutex_t get();
        
    private:
        pthread_mutex_t m_mutex;
};

class Cond
{
    public:
        Cond();
        ~Cond() noexcept(false);
        bool notify_one();
        bool notify_all();
        bool wait(pthread_mutex_t *mt);

    private:
        pthread_cond_t m_cond;
};

class Sem
{
    public:
        Sem(int num = 10);
        ~Sem();
        bool wait();
        bool post();
    private:
        sem_t m_sem;
};


