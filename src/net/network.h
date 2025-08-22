#pragma once

#include <memory>
#include <list>
#include "netif.h"
#include "net_err.h"
#include "protocol_stack.h"
#include <map>

namespace tinytcp {

using NetListIt = std::list<INetIF*>::iterator;

class INetWork {
public:
    using ptr  = std::shared_ptr<INetWork>;
    using uptr = std::unique_ptr<INetWork>;

    explicit INetWork(IProtocolStack* protocal_stack);
    virtual ~INetWork();
    virtual net_err_t init() = 0;
    virtual net_err_t start() = 0;
    virtual void recv_func(void*) {}
    virtual void send_func(void*) {}

public:
    // 把接收到的数据放入协议栈的消息队列中，并设置等待时间
    net_err_t msg_send(exmsg_t* msg, int32_t timeout_ms);
    // 接收网卡数据
    net_err_t exmsg_netif_in(INetIF* netif);
    // 把数据从网卡中发出
    net_err_t exmsg_netif_out();

    INetIF* netif_open(const char* dev_name, void* ops_data = nullptr);
    std::list<INetIF*>::iterator find_netif_by_name(const std::string& name);
    net_err_t netif_close(std::list<INetIF*>::iterator& netif_it);
    net_err_t set_active(INetIF* netif);
    net_err_t set_deactive(INetIF* netif);

    void set_default(INetIF* netif) noexcept { m_default_netif = netif; }

    PktBuffer* get_buf_from_in_queue(NetListIt netif_it, int timeout_ms = -1);
    net_err_t put_buf_to_in_queue(NetListIt netif_it, PktBuffer* buf, int timeout_ms = -1);
    PktBuffer* get_buf_from_out_queue(NetListIt netif_it, int timeout_ms = -1);
    net_err_t put_buf_to_out_queue(NetListIt netif_it, PktBuffer* buf, int timeout_ms = -1);

    void debug_print();

protected:
    IProtocolStack* m_protocal_stack = nullptr;
    std::list<INetIF*> m_netif_list;      // 网络接口列表
    INetIF* m_default_netif = nullptr;    // 默认使用的网络接口

protected:
    using NetIFFactoryFunc = std::function<std::unique_ptr<INetIF>(INetWork*, const char*, void*)>;
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
    // 接收和发送线程函数
    void recv_func(void*) override;
    void send_func(void*) override;

private:
};

std::ostream& operator<<(std::ostream& os, const ipaddr_t& ipaddr);
std::ostream& operator<<(std::ostream& os, const INetIF& netif);

} // namespace tinytcp

