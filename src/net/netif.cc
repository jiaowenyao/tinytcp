#include "netif.h"
#include "log.h"
#include "config.h"
#include "macro.h"
#include "magic_enum.h"
#include "network.h"
#include "plat/sys_plat.h"
#include <iomanip>


namespace tinytcp {

static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static tinytcp::ConfigVar<uint32_t>::ptr g_netif_in_queue_size =
    tinytcp::Config::look_up("tcp.netif_in_queue_size", 1024U, "netif in queue size, 网卡输入队列的大小");
static tinytcp::ConfigVar<uint32_t>::ptr g_netif_out_queue_size =
    tinytcp::Config::look_up("tcp.netif_out_queue_size", 1024U, "netif out queue size, 网卡输出队列的大小");

std::ostream& operator<<(std::ostream& os, const netif_hwaddr_t& hwaddr) {
    for (uint8_t i = 0; i < hwaddr.len; ++i) {
        if (i == 0) {
            os << std::hex << std::setw(2) << std::setfill('0') << (int)hwaddr.addr[i];
        }
        else {
            os << "-";
            os << std::hex << std::setw(2) << std::setfill('0') << (int)hwaddr.addr[i];
        }
    }
    return os;
}

INetIF::INetIF(INetWork* network, const char* name, void* ops_data)
    : m_network(network)
    , m_state(NETIF_OPENED)
    , m_ops_data(ops_data) {
    set_name(name);
    m_in_q = std::make_unique<LockFreeRingQueue<PktBuffer*>>(g_netif_in_queue_size->value());
    TINYTCP_ASSERT2(m_in_q != nullptr, "m_in_q init error");
    m_out_q = std::make_unique<LockFreeRingQueue<PktBuffer*>>(g_netif_out_queue_size->value());
    TINYTCP_ASSERT2(m_out_q != nullptr, "m_out_q init error");
}

INetIF::~INetIF() {

}

net_err_t INetIF::close() { return net_err_t::NET_ERR_OK; }

#if defined(DEBUG_INETIF)
void INetIF::debug_print() {
    TINYTCP_LOG_DEBUG(g_logger)
        << "\nname=       " << std::string(m_name, strlen(m_name))
        << "\nstate=      " << magic_enum::enum_name(m_state)
        << "\ntype=       " << magic_enum::enum_name(m_type)
        << "\nmtu=        " << m_mtu
        << "\nip=         " << m_ipaddr
        << "\nmask=       " << m_netmask
        << "\ngateway=    " << m_gateway
        << "\nhwaddr=     " << m_hwaddr
        << "\nin_q_size=  " << m_in_q->size()
        << "\nhout_q_size=" << m_out_q->size()
        << "\n\n";
}
#else
void INetIF::debug_print() {}
#endif // DEBUG_INETIF


net_err_t ipaddr_from_str(ipaddr_t& dest, const char* str) {
    if (!str) {
        return net_err_t::NET_ERR_PARAM;
    }

    dest.type = ipaddr_t::IPADDR_V4;
    dest.q_addr = 0;

    uint8_t* p = dest.a_addr;
    uint8_t sub_addr = 0;
    char c;
    while ((c = *str++) != '\0') {
        if ((c >= '0') && (c <= '9')) {
            sub_addr = sub_addr * 10 + (c - '0');
        }
        else if (c == '.') {
            *p++ = sub_addr;
            sub_addr = 0;
        }
        else {
            return net_err_t::NET_ERR_PARAM;
        }
    }
    *p = sub_addr;
    return net_err_t::NET_ERR_OK;
}

void INetIF::set_name(const char* name) {
    memcpy(m_name, name, strlen(name));
}

void INetIF::clear_in_queue() {
    PktBuffer* pktbuf;
    while (!m_in_q->is_empty()) {
        m_in_q->pop(&pktbuf);
        pktbuf->free();
    }
}

void INetIF::clear_out_queue() {
    PktBuffer* pktbuf;
    while (!m_out_q->is_empty()) {
        m_out_q->pop(&pktbuf);
        pktbuf->free();
    }
}

PktBuffer* INetIF::get_buf_from_in_queue(int timeout_ms) {
    PktBuffer* buf;
    if (timeout_ms < 0) {
        timeout_ms = -1;
    }
    m_in_q->pop(&buf, timeout_ms);
    return buf;
}

net_err_t INetIF::put_buf_to_in_queue(PktBuffer* buf, int timeout_ms) {
    bool ok = m_in_q->push(buf, timeout_ms);
    if (ok) {
        m_network->exmsg_netif_in(this);
        return net_err_t::NET_ERR_OK;
    }
    return net_err_t::NET_ERR_FULL;
}

PktBuffer* INetIF::get_buf_from_out_queue(int timeout_ms) {
    PktBuffer* buf;
    if (timeout_ms < 0) {
        timeout_ms = -1;
    }
    m_out_q->pop(&buf, timeout_ms);
    return buf;
}

net_err_t INetIF::put_buf_to_out_queue(PktBuffer* buf, int timeout_ms) {
    m_out_q->push(buf, timeout_ms);
    return net_err_t::NET_ERR_OK;
}

net_err_t INetIF::netif_out(const ipaddr_t& ipaddr, PktBuffer* buf) {
    net_err_t err = put_buf_to_out_queue(buf, 0);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_INFO(g_logger) << "netif out failed";
        return err;
    }
    return send();
}

LoopNet::LoopNet(INetWork* network, const char* name, void* ops_data)
    : INetIF(network, name, ops_data) {

}
LoopNet::~LoopNet() {

}

net_err_t LoopNet::open() {
    m_type = NETIF_TYPE_LOOP;

    // ipaddr_from_str(m_ipaddr, "127.0.0.1");
    m_ipaddr = "127.0.0.1";
    ipaddr_from_str(m_netmask, "255.0.0.0");

    // test
    // m_hwaddr.addr[0] = 124;
    // m_hwaddr.addr[1] = 8;
    // m_hwaddr.len = 2;
    // test


    return net_err_t::NET_ERR_OK;
}

net_err_t LoopNet::close() {
    INetIF::close();
    return net_err_t::NET_ERR_OK;
}

net_err_t LoopNet::send() {
    PktBuffer* buf = get_buf_from_out_queue(0);
    if (buf != nullptr) {
        net_err_t err = put_buf_to_in_queue(buf, 0);
        if ((int8_t)err < 0) {
            buf->free();
            return err;
        }
    }
    return net_err_t::NET_ERR_OK;
}

EtherNet::EtherNet(INetWork* network, const char* name, void* ops_data)
    : INetIF(network, name, ops_data) {

}

EtherNet::~EtherNet() {

}

net_err_t EtherNet::open() {
    pcap_data_t* dev_data = (pcap_data_t*)m_ops_data;
    pcap_t* pcap = pcap_device_open(dev_data->ip, dev_data->hwaddr);

    if (pcap == nullptr) {
        TINYTCP_LOG_ERROR(g_logger) << "pcap open failed!, name=" << m_name;
        return net_err_t::NET_ERR_IO;
    }

    m_type = NETIF_TYPE_ETHER;
    m_mtu = 1500;
    ipaddr_from_str(m_ipaddr, dev_data->ip);
    m_hwaddr.reset(dev_data->hwaddr, 6);

    m_ops_data = pcap;
    m_send_thread = std::make_unique<Thread>(std::bind(&INetWork::send_func, m_network, this), "netif(" + std::string(m_name) + ")_send_thread");
    m_recv_thread = std::make_unique<Thread>(std::bind(&INetWork::recv_func, m_network, this), "netif(" + std::string(m_name) + ")_recv_thread");

    return net_err_t::NET_ERR_OK;
}

net_err_t EtherNet::close() {
    pcap_t* pcap = (pcap_t*)m_ops_data;
    pcap_close(pcap);
    return net_err_t::NET_ERR_OK;
}

net_err_t EtherNet::send() {

    return net_err_t::NET_ERR_OK;
}

} // namespace tinytcp


