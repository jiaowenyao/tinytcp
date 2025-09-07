#pragma once


#include "net_err.h"
#include "exmsg.h"
#include "protocol_stack.h"
#include "memblock.h"
#include "src/thread.h"
#include "src/lock_free_ring_queue.h"
#include "src/singleton.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>


namespace tinytcp {

class ProtocolStack : public IProtocolStack {

public:
    ProtocolStack();
    ~ProtocolStack() = default;
    net_err_t init()  override;
    net_err_t start() override;

    void work_thread_func();
    net_err_t do_netif_in(exmsg_t::ptr msg);

    void on_timer_inserted_at_front() override;

// 协议栈工作线程相关
private:
    Thread::uptr m_work_thread;

// 定时器相关
public:
    void timer_thread_init();
    void timer_thread_func();
    void tickle_event();
    exmsg_t::ptr get_timer_msg_block();
    // net_err_t release_timer_msg_block(exmsg_t* msg);
private:
    int m_epoll_fd;
    int m_event_fd;
    Thread::uptr m_timer_thread;
    MemBlock::uptr m_func_exmsg_mem_block = nullptr;
    LockFreeRingQueue<exmsg_t::ptr>::uptr m_func_exmsg_msg_queue = nullptr;

};

using ProtocolStackMgr = Singleton<ProtocolStack>;


} // namespace tinytcp


