#pragma once
#include "net_err.h"
#include "exmsg.h"
#include "memblock.h"
#include "src/timer.h"

namespace tinytcp {

class INetWork;
class IPProtocol;

class IProtocolStack : public TimerManager {

public:
    IProtocolStack() = default;
    virtual ~IProtocolStack() = default;
    virtual net_err_t init()  = 0;
    virtual net_err_t start() = 0;

public:
    // 内存池操作，获取和释放消息的内存
    exmsg_t* get_msg_block();
    net_err_t release_msg_block(exmsg_t* msg);
    // 操作协议栈的消息队列
    net_err_t push_msg(exmsg_t* msg, uint32_t timeout_ms);
    net_err_t pop_msg();

    INetWork* get_network() const noexcept { return m_network.get(); }
    IPProtocol* get_ipprotocol() const noexcept { return m_ipprotocol.get(); }
protected:
    MemBlock::uptr m_mem_block = nullptr;
    LockFreeRingQueue<exmsg_t*>::uptr m_msg_queue = nullptr;
    std::unique_ptr<INetWork> m_network;
    std::unique_ptr<IPProtocol> m_ipprotocol;
};

} // namespace tinytcp

