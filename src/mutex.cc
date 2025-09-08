#include "mutex.h"
#include <stdexcept>


namespace tinytcp {

Semaphore::Semaphore(uint32_t count) {
    if (sem_init(&m_semaphore, 0, count) != 0) {
        throw std::logic_error("sem_init error");
    }
}

Semaphore::~Semaphore() {
    sem_destroy(&m_semaphore);
}

void Semaphore::wait() {
    if (sem_wait(&m_semaphore) != 0) {
        throw std::logic_error("sem_wait error");
    }
}

int Semaphore::wait_timeout(int timeout) {
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    ts.tv_sec += timeout / 1000;
    ts.tv_nsec += (timeout % 1000) * 1000;

    int ret = sem_timedwait(&m_semaphore, &ts);
    return ret;
}

void Semaphore::notify() {
    if (sem_post(&m_semaphore) != 0) {
        throw std::logic_error("sem_post error");
    }
}


// 互斥锁
Mutex::Mutex() {
    pthread_mutex_init(&m_mutex, nullptr);
}

Mutex::~Mutex() {
    pthread_mutex_destroy(&m_mutex);
}

void Mutex::lock() {
    pthread_mutex_lock(&m_mutex);
}

void Mutex::unlock() {
    pthread_mutex_unlock(&m_mutex);
}


// 自旋锁
SpinLock::SpinLock() {
    pthread_spin_init(&m_mutex, 0);
}

SpinLock::~SpinLock() {
    pthread_spin_destroy(&m_mutex);
}

void SpinLock::lock() {
    pthread_spin_lock(&m_mutex);
}

void SpinLock::unlock() {
    pthread_spin_unlock(&m_mutex);
}


// 读写锁
RWMutex::RWMutex() {
    pthread_rwlock_init(&m_lock, nullptr);
}
RWMutex::~RWMutex() {
    pthread_rwlock_destroy(&m_lock);
}
void RWMutex::rdlock() {
    pthread_rwlock_rdlock(&m_lock);
}
void RWMutex::wrlock() {
    pthread_rwlock_wrlock(&m_lock);
}
void RWMutex::unlock() {
    pthread_rwlock_unlock(&m_lock);
}



} // namespace tinytcp



