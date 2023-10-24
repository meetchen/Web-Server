#pragma once

#include <list>
#include <iostream>
#include "lockUntils.h"

template<typename T>
class ThreadPool
{
    public:
        ThreadPool(int threadNum = 10, int maxReq = 10000) ;
        ~ThreadPool();
        bool addWork(T* req);
    private:
    
        // 保存所有的线程
        pthread_t *m_thread;
        // 保存需要处理的任务
        std::list<T*> m_workerqueue;
        // 线程池线程数量
        int m_threadNum;
        // 最大的任务个数
        int m_maxReq;
        // 当线程池关闭时，其余线程可以结束线程
        bool m_notStop;
        // 信号量，用于记录任务队列的待处理状态
        Sem m_sem;
        // 用于同步锁
        Locker m_lock;

        static void * worker(void *);
        void run();

        

};
