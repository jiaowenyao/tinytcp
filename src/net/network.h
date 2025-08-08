#pragma once

#include <memory>
#include "net_err.h"
#include "protocol_stack.h"
#include "src/thread.h"

namespace tinytcp {

class INetWork {
public:
    using ptr  = std::shared_ptr<INetWork>;
    using uptr = std::unique_ptr<INetWork>;

    INetWork(IProtocolStack* protocal_stack);
    virtual ~INetWork() = default;
    virtual net_err_t init() = 0;
    virtual net_err_t start() = 0;
    // 接收网卡数据
    virtual net_err_t netif_in() = 0;
    // 把数据从网卡中发出
    virtual net_err_t netif_out() = 0;
    // 把接收到的数据放入协议栈的消息队列中，并设置等待时间
    virtual net_err_t msg_send(exmsg_t* msg, int32_t timeout_ms) = 0;


protected:
    Thread::ptr m_recv_thread = nullptr;
    Thread::ptr m_send_thread = nullptr;

    IProtocolStack* m_protocal_stack = nullptr;

private:

};


// pcap库让网卡接收和发送数据
class PcapNetWork : public INetWork {
public:
    using ptr  = std::shared_ptr<PcapNetWork>;
    using uptr = std::unique_ptr<PcapNetWork>;

    PcapNetWork(IProtocolStack* protocal_stack);
    ~PcapNetWork() = default;
    net_err_t init() override;
    net_err_t start() override;
    net_err_t netif_in() override;
    net_err_t netif_out() override;
    net_err_t msg_send(exmsg_t* msg, int32_t timeout_ms) override;

public:
    static void recv_func();
    static void send_func();

private:
};



} // namespace tinytcp

