#include "thread.h"
#include "log.h"


namespace tinytcp {

static thread_local Thread* t_thread = nullptr;
static thread_local std::string t_thread_name = "UNKNOW";

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

Thread* Thread::get_this() {
    return t_thread;
}

const std::string& Thread::get_name() {
    return t_thread_name;
}

void Thread::set_name(const std::string& name) {
    if (t_thread != nullptr) {
        t_thread->m_name = name;
    }
    t_thread_name = name;
}

Thread::Thread(std::function<void()> cb, const std::string& name)
    : m_cb(cb)
    , m_name(name) {
    if (name.empty()) {
        m_name = "UNKNOW";
    }
    int rt = pthread_create(&m_thread, nullptr, &Thread::run, this);
    if (rt != 0) {
        TINYTCP_LOG_ERROR(g_logger) << "pthread create thread fail, rt=" << rt
                                  << " name=" << name;
        throw std::logic_error("pthread_create error");
    }
    /**
     * 细节：用信号量保证任务启动之后构造函数才返回,用于某些需要线程里面初始化资源的场景。
     * 因为pthread_create之后，无法保证先执行构造函数返回后的主程序运行 还是 线程函数
     * 比如加入线程池时，线程池要获取当前线程的pid，那就得想让线程启动，得到pid之后，构造函数再返回
     * */
    m_semaphore.wait();
}

Thread::~Thread() {
    if (m_thread) {
        pthread_detach(m_thread);
    }
}

void Thread::join() {
    if (m_thread != 0) {
        int rt = pthread_join(m_thread, nullptr);
        if (rt != 0) {
            TINYTCP_LOG_ERROR(g_logger) << "pthread_join thread fail, rt=" << rt
                                      << " name=" << m_name;
        }
        m_thread = 0;
    }
}

void* Thread::run(void* arg) {
    Thread* thread = static_cast<Thread*>(arg);
    t_thread = thread;
    t_thread_name = thread->m_name;
    thread->m_pid = tinytcp::get_thread_id();
    pthread_setname_np(pthread_self(), thread->m_name.substr(0, 15).c_str());

    std::function<void()> cb;
    cb.swap(thread->m_cb);

    // 保证在run函数跑起来之后，Thread的构造函数才返回
    thread->m_semaphore.notify();

    cb();

    return nullptr;
}


    
} // namespace tinytcp



