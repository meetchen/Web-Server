#include "lockUntils.h"

Locker::Locker()
{
    if(pthread_mutex_init(&m_mutex, NULL))
    {
        perror("pthread_mutex_init");
        throw std::exception();
    }
}

Locker::~Locker()
{
    pthread_mutex_destroy(&m_mutex);
}

bool Locker::lock()
{
    return pthread_mutex_lock(&m_mutex) == 0;
}

bool Locker::unlock()
{
    return pthread_mutex_unlock(&m_mutex) == 0;
}

pthread_mutex_t Locker::get()
{
    return this -> m_mutex;
}


Cond::Cond()
{
    if (pthread_cond_init(&m_cond, NULL)) 
    {
        throw std::exception();
    }
}


Cond::~Cond() noexcept(false)
{
    if (pthread_cond_destroy(&m_cond))
    {
        throw std::exception();
    }
}

bool Cond::wait(pthread_mutex_t *mt)
{
    return pthread_cond_wait(&m_cond, mt);
}

bool Cond::notify_one()
{
    return pthread_cond_signal(&m_cond);
}

bool Cond::notify_all()
{
    return pthread_cond_broadcast(&m_cond);
}

Sem::Sem(int num)
{
    if (sem_init(&m_sem, 0, num) != 0)
    {
        perror("sem_init");
        exit(-1);
    }
}

Sem::~Sem()
{
    sem_destroy(&m_sem);
}

bool Sem::wait()
{
    return sem_wait(&m_sem);
}

bool Sem::post()
{
    return sem_post(&m_sem);
}
