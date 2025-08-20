#pragma once

#include "ipaddr.h"
#include "net_err.h"
#include "pktbuf.h"
#include "src/lock_free_ring_queue.h"
#include <string.h>
#include <functional>
#include <map>

namespace tinytcp {


#define NETIF_HWADDR_SIZE 32
#define NETIF_NAME_SIZE   10

struct netif_hwaddr_t {
    uint8_t addr[NETIF_HWADDR_SIZE];
    uint8_t len;

    netif_hwaddr_t() : len(0) {
        memset(addr, 0, sizeof(addr));
    }

    const netif_hwaddr_t operator=(const netif_hwaddr_t& other) {
        if (this == &other) {
            return *this;
        }
        memcpy(addr, other.addr, other.len);
        len = other.len;
        return *this;
    }
};


enum netif_type_t {
    NETIF_TYPE_NONE = 0,
    NETIF_TYPE_ETHER,
    NETIF_TYPE_LOOP,

    // 类型最大值，用来比较
    NETIF_TYPE_SIZE,
};

class INetWork;

// network interface, 不同的网卡协议有不同的实现
class INetIF {

public:
    enum NETIF_STATE {
        NETIF_CLOSED,
        NETIF_OPENED,
        NETIF_ACTIVE,
    };

public:

    INetIF(INetWork* network, const char* name, void* ops_data = nullptr);
    virtual ~INetIF();

    const char* get_name() const noexcept { return m_name; } 
    netif_hwaddr_t get_hwaddr() const noexcept { return m_hwaddr; }
    ipaddr_t get_ipaddr() const noexcept { return m_ipaddr; }
    ipaddr_t get_netmask() const noexcept { return m_netmask; }
    ipaddr_t get_gateway() const noexcept { return m_gateway; }
    netif_type_t get_type() const noexcept { return m_type; }
    uint32_t get_mtu() const noexcept { return m_mtu; }
    int32_t get_state() const noexcept { return m_state; }

    void set_name(const char* name);
    void set_mtu(uint32_t mtu) noexcept { m_mtu = mtu; }
    void set_state(NETIF_STATE state) noexcept { m_state = state; }

    void clear_in_queue();
    void clear_out_queue();

    // 操作网卡队列
    PktBuffer* get_buf_from_in_queue(int timeout_ms = -1);
    net_err_t put_buf_to_in_queue(PktBuffer* buf, int timeout_ms = -1);
    PktBuffer* get_buf_from_out_queue(int timeout_ms = -1);
    net_err_t put_buf_to_out_queue(PktBuffer* buf, int timeout_ms = -1);

    // 把数据包发送给指定地址
    net_err_t netif_out(const ipaddr_t& ipaddr, PktBuffer* buf);

    void debug_print();

public:
    // 网卡属性初始化
    virtual net_err_t open() { return net_err_t::NET_ERR_OK; }
    virtual net_err_t close();
    virtual net_err_t send() { return net_err_t::NET_ERR_OK; }

protected:
    INetWork* m_network;
    void* m_ops_data = nullptr;
    char m_name[NETIF_NAME_SIZE] = {0};
    netif_hwaddr_t m_hwaddr;

    ipaddr_t m_ipaddr;  // ip地址
    ipaddr_t m_netmask; // 掩码
    ipaddr_t m_gateway; // 网关

    netif_type_t m_type = NETIF_TYPE_NONE;
    uint32_t m_mtu = 0;     // 最大传输单元

    NETIF_STATE m_state;

    LockFreeRingQueue<PktBuffer*>::uptr m_in_q;
    LockFreeRingQueue<PktBuffer*>::uptr m_out_q;
};

net_err_t ipaddr_from_str(ipaddr_t& dest, const char* str);

class LoopNet : public INetIF {
public:
    LoopNet(INetWork* network, const char* name, void* ops_data = nullptr);
    ~LoopNet();

    net_err_t open() override;
    net_err_t close() override;
    net_err_t send() override;

private:

};

class EtherNet : public INetIF {
public:

private:

};


std::ostream& operator<<(std::ostream& os, const netif_hwaddr_t& hwaddr);

} // namespace tinytcp

