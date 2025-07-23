#pragma once

#include <memory>
#include <pthread.h>
#include <functional>
#include <semaphore.h>
#include "mutex.h"
#include "noncopyable.h"

namespace tinytcp {

class Thread : Noncopyable {
public:
    using ptr = std::shared_ptr<Thread>;

    Thread(std::function<void()> cb, const std::string& name);
    ~Thread();

    pid_t pid() const { return m_pid; }
    const std::string& name() const { return m_name; }

    void join();

    static Thread* get_this();
    static const std::string& get_name();
    static void set_name(const std::string& name);

private:
    Thread(const Thread&) = delete;
    Thread(const Thread&&) = delete;
    Thread& operator=(const Thread&) = delete;

    static void* run(void* arg);    
private:
    pid_t m_pid = -1;
    pthread_t m_thread = 0;
    std::function<void()> m_cb;
    std::string m_name;
    Semaphore m_semaphore;
};
    
} // namespace tinytcp


