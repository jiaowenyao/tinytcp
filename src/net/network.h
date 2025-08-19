#pragma once

#include <memory>
#include <list>
#include "netif.h"
#include "net_err.h"
#include "protocol_stack.h"
#include "src/thread.h"

namespace tinytcp {

class INetWork {
public:
    using ptr  = std::shared_ptr<INetWork>;
    using uptr = std::unique_ptr<INetWork>;

    explicit INetWork(IProtocolStack* protocal_stack);
    virtual ~INetWork();
    virtual net_err_t init() = 0;
    virtual net_err_t start() = 0;
    // 接收网卡数据
    virtual net_err_t netif_in() = 0;
    // 把数据从网卡中发出
    virtual net_err_t netif_out() = 0;
    // 把接收到的数据放入协议栈的消息队列中，并设置等待时间
    virtual net_err_t msg_send(exmsg_t* msg, int32_t timeout_ms) = 0;

public:
    INetIF* netif_open(const char* dev_name, void* ops_data = nullptr);


protected:
    Thread::uptr m_recv_thread = nullptr;
    Thread::uptr m_send_thread = nullptr;
    IProtocolStack* m_protocal_stack = nullptr;
    std::list<INetIF*> m_netif_list;      // 网络接口列表
    INetIF* m_default_netif = nullptr;    // 默认使用的网络接口

protected:
    using NetIFFactoryFunc = std::function<std::unique_ptr<INetIF>(const char*, void*)>;
    static std::map<std::string, NetIFFactoryFunc> s_netif_factory_registry;
public:
    static bool register_netif_factory(const std::string& type_name, NetIFFactoryFunc factory_func);
};


// pcap库让网卡接收和发送数据
class PcapNetWork : public INetWork {
public:
    using ptr  = std::shared_ptr<PcapNetWork>;
    using uptr = std::unique_ptr<PcapNetWork>;

    explicit PcapNetWork(IProtocolStack* protocal_stack);
    ~PcapNetWork() = default;
    net_err_t init() override;
    net_err_t start() override;
    net_err_t netif_in() override;
    net_err_t netif_out() override;
    net_err_t msg_send(exmsg_t* msg, int32_t timeout_ms) override;
    // 接收和发送线程函数
    void recv_func();
    void send_func();

private:
};



} // namespace tinytcp

