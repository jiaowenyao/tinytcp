#pragma once
#include <cstdint>
#include <semaphore.h>
#include <pthread.h>

#include "noncopyable.h"

namespace tinytcp {

class Semaphore : Noncopyable {
public:
    Semaphore(uint32_t count = 0);
    ~Semaphore();

    void wait();
    void notify();

private:
    Semaphore(const Semaphore&) = delete;
    Semaphore(const Semaphore&&) = delete;
    Semaphore& operator=(const Semaphore&) = delete;

private:
    sem_t m_semaphore;

};


// RAII的锁
template<class T>
struct ScopedLockImpl {
public:
    ScopedLockImpl(T& mutex):
        m_mutex(mutex) {
        m_mutex.lock();
        m_locked = true;
    }
    ~ScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.lock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};

template<class T>
struct ReadScopedLockImpl {
public:
    ReadScopedLockImpl(T& mutex):
        m_mutex(mutex) {
        m_mutex.rdlock();
        m_locked = true;
    }
    ~ReadScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.rdlock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};
template<class T>
struct WriteScopedLockImpl {
public:
    WriteScopedLockImpl(T& mutex):
        m_mutex(mutex) {
        m_mutex.wrlock();
        m_locked = true;
    }
    ~WriteScopedLockImpl() {
        unlock();
    }

    void lock() {
        if (!m_locked) {
            m_mutex.wrlock();
            m_locked = true;
        }
    }

    void unlock() {
        if (m_locked) {
            m_mutex.unlock();
            m_locked = false;
        }
    }

private:
    T& m_mutex;
    bool m_locked;
};

class Mutex : Noncopyable {
public:
    using Lock = ScopedLockImpl<Mutex>;

    Mutex();

    ~Mutex();

    void lock();

    void unlock();

private:
    pthread_mutex_t m_mutex;

};

class SpinLock : Noncopyable {
public:
    using Lock = ScopedLockImpl<SpinLock>;

    SpinLock();

    ~SpinLock();

    void lock();

    void unlock();

private:
    pthread_spinlock_t m_mutex;
};

class RWMutex : Noncopyable {
public:
    using ReadLock  = ReadScopedLockImpl<RWMutex>;
    using WriteLock = WriteScopedLockImpl<RWMutex>;

    RWMutex();
    ~RWMutex();
    void rdlock();
    void wrlock();
    void unlock();
private:
    pthread_rwlock_t m_lock;

};
} // namespace tinytcp


