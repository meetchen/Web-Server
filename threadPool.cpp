
#include "threadPool.h"



// 只有在声明的时候写默认参数
template<typename T>
ThreadPool<T>::ThreadPool(int threadNum, int maxReq):
    m_threadNum(threadNum),
    m_maxReq(maxReq),
    m_notStop(true),
    m_thread(NULL)
{

    if (threadNum <=0 || maxReq <= 0)
    {
        throw std::exception();
    }

    m_thread = new pthread_t[threadNum];

    for (int i = 0; i < threadNum; ++i)
    {
        printf("thread %dth init....\n", i + 1);
        int ret = pthread_create(m_thread + i, nullptr, worker, this);
        if (ret != 0)
        {
            perror("pthread_create");
            delete [] m_thread;
            throw std::exception();
        }
        pthread_detach(m_thread[i]);
    }
}

template<typename T>
ThreadPool<T>::~ThreadPool()
{
    delete []m_thread;
    m_notStop = false;
}

template<typename T>
void *ThreadPool<T>::worker(void *arg)
{
    ThreadPool* ptr = (ThreadPool *)arg;
    ptr->run();
    return ptr;
}

template<typename T>
void ThreadPool<T>::run()
{
    while (m_notStop)
    {
        m_sem.wait();
        m_lock.lock();
        if (m_workerqueue.empty())
        {
            m_lock.unlock();
            continue;
        }
        auto p = m_workerqueue.front();
        m_workerqueue.pop_front();
        m_lock.unlock();
        if (!p) continue;
        p -> process();
    }
}


template<typename T>
bool ThreadPool<T>::addWork(T *req)
{
    m_lock.lock();
    if (m_workerqueue.size() > m_maxReq)
    {
        m_lock.unlock();
        return false;
    }
    m_workerqueue.push_back(req);
    m_sem.post();
    m_lock.unlock();
    return true;
}