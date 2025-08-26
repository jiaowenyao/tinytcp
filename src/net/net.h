#pragma once


#include "net_err.h"
#include "exmsg.h"
#include "network.h"
#include "protocol_stack.h"
#include "memblock.h"
#include "src/thread.h"
#include "src/lock_free_ring_queue.h"
#include "src/timer.h"
#include <sys/epoll.h>
#include <sys/eventfd.h>


namespace tinytcp {

class ProtocolStack : public IProtocolStack, public TimerManager {

public:
    ProtocolStack();
    ~ProtocolStack() = default;
    net_err_t init()  override;
    net_err_t start() override;

    INetWork* get_network() const noexcept { return m_network.get(); }

    void work_thread_func();
    net_err_t do_netif_in(exmsg_t* msg);

    void on_timer_inserted_at_front() override;

// 协议栈工作线程相关
private:
    INetWork::uptr m_network;
    Thread::uptr m_work_thread;

// 定时器相关
public:
    void timer_thread_init();
    void timer_thread_func();
    void tickle_event();
    exmsg_t* get_timer_msg_block();
    net_err_t release_timer_msg_block(exmsg_t* msg);
private:
    int m_epoll_fd;
    int m_event_fd;
    Thread::uptr m_timer_thread;
    MemBlock::uptr m_timer_mem_block = nullptr;
    LockFreeRingQueue<exmsg_t*>::uptr m_timer_msg_queue = nullptr;

};


} // namespace tinytcp


