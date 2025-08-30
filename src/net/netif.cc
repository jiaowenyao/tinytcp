#include "netif.h"
#include "log.h"
#include "config.h"
#include "macro.h"
#include "magic_enum.h"
#include "network.h"
#include "link_layer.h"
#include "protocol.h"
#include "plat/sys_plat.h"
#include "src/endiantool.h"
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
    m_in_q = std::make_unique<LockFreeRingQueue<PktBuffer::ptr>>(g_netif_in_queue_size->value());
    TINYTCP_ASSERT2(m_in_q != nullptr, "m_in_q init error");
    m_out_q = std::make_unique<LockFreeRingQueue<PktBuffer::ptr>>(g_netif_out_queue_size->value());
    TINYTCP_ASSERT2(m_out_q != nullptr, "m_out_q init error");
}

INetIF::~INetIF() {

}

bool INetIF::is_local_broadcast(const ipaddr_t& ipaddr) {
    return ipaddr.q_addr == 0xFFFFFFFF;
}

bool INetIF::is_direct_broadcast(const ipaddr_t& ipaddr) {
    uint32_t host_ip = ipaddr.q_addr & (~m_netmask.q_addr);
    return host_ip == (0xFFFFFFFF & (~m_netmask.q_addr));
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
    PktBuffer::ptr pktbuf;
    while (!m_in_q->is_empty()) {
        m_in_q->pop(&pktbuf);
        pktbuf->free();
    }
}

void INetIF::clear_out_queue() {
    PktBuffer::ptr pktbuf;
    while (!m_out_q->is_empty()) {
        m_out_q->pop(&pktbuf);
        pktbuf->free();
    }
}

PktBuffer::ptr INetIF::get_buf_from_in_queue(int timeout_ms) {
    PktBuffer::ptr buf;
    if (timeout_ms < 0) {
        timeout_ms = -1;
    }
    bool ok = m_in_q->pop(&buf, timeout_ms);
    if (ok) {
        buf->reset_access();
        return buf;
    }
    return nullptr;
}

net_err_t INetIF::put_buf_to_in_queue(PktBuffer::ptr buf, int timeout_ms) {
    bool ok = m_in_q->push(buf, timeout_ms);
    if (ok) {
        m_network->exmsg_netif_in(this);
        return net_err_t::NET_ERR_OK;
    }
    return net_err_t::NET_ERR_FULL;
}

PktBuffer::ptr INetIF::get_buf_from_out_queue(int timeout_ms) {
    PktBuffer::ptr buf;
    if (timeout_ms < 0) {
        timeout_ms = -1;
    }
    bool ok = m_out_q->pop(&buf, timeout_ms);
    if (ok) {
        buf->reset_access();
        return buf;
    }
    return nullptr;
}

net_err_t INetIF::put_buf_to_out_queue(PktBuffer::ptr buf, int timeout_ms) {
    bool ok = m_out_q->push(buf, timeout_ms);
    if (ok) {
        return net_err_t::NET_ERR_OK;
    }
    return net_err_t::NET_ERR_FULL;
}

net_err_t INetIF::netif_out(const ipaddr_t& ipaddr, PktBuffer::ptr buf) {
    if (m_type != NETIF_TYPE_LOOP) {
        net_err_t err = link_out(ipaddr, buf);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "netif out: link out failed";
            return err;
        }
        return net_err_t::NET_ERR_OK;
    }
    else {
        net_err_t err = put_buf_to_out_queue(buf, 0);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_INFO(g_logger) << "netif out: put buf failed";
            return err;
        }
        return send();
    }
    return net_err_t::NET_ERR_OK;
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

// 环回接口只用把接收到的数据重新放到输入队列即可
net_err_t LoopNet::send() {
    PktBuffer::ptr buf = get_buf_from_out_queue(0);
    if (buf != nullptr) {
        net_err_t err = put_buf_to_in_queue(buf, 0);
        if ((int8_t)err < 0) {
            // buf->free();
            return err;
        }
    }
    return net_err_t::NET_ERR_OK;
}

EtherNet::EtherNet(INetWork* network, const char* name, void* ops_data)
    : INetIF(network, name, ops_data) {

    // 启动arp定时器
    auto p = network->get_protocal_stack()->add_timer(m_arp_processor.get_cache_timeout(), std::bind(&ARPProcessor::cache_timer, &m_arp_processor), true);
}

EtherNet::~EtherNet() {

}

net_err_t EtherNet::open() {
    pcap_data_t* dev_data = (pcap_data_t*)m_ops_data;
    pcap_t* pcap = pcap_device_open(dev_data->ip, dev_data->hwaddr);
    char errbuf[1024] = {0};
    if (pcap_getnonblock(pcap, errbuf) == -1) {
        TINYTCP_LOG_ERROR(g_logger) << "Non-block mode error: " << errbuf << std::endl;
    }

    if (pcap == nullptr) {
        TINYTCP_LOG_ERROR(g_logger) << "pcap open failed!, name=" << m_name;
        return net_err_t::NET_ERR_IO;
    }

    m_type = NETIF_TYPE_ETHER;
    m_mtu = ETHER_MTU;
    ipaddr_from_str(m_ipaddr, dev_data->ip);
    m_hwaddr.reset(dev_data->hwaddr, 6);

    m_ops_data = pcap;
    m_send_thread = std::make_unique<Thread>(std::bind(&INetWork::send_func, m_network, this), "netif(" + std::string(m_name) + ")_send_thread");
    m_recv_thread = std::make_unique<Thread>(std::bind(&INetWork::recv_func, m_network, this), "netif(" + std::string(m_name) + ")_recv_thread");

    PktBuffer::ptr buf = m_arp_processor.make_gratuitous(this);
    if (buf != nullptr) {
        net_err_t err = ether_raw_out(NET_PROTOCOL_ARP, ether_broadcast_addr(), buf);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "ether open: send a gratuitous error";
        }
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t EtherNet::close() {
    pcap_t* pcap = (pcap_t*)m_ops_data;
    pcap_close(pcap);
    return net_err_t::NET_ERR_OK;
}

net_err_t EtherNet::send() {

    return net_err_t::NET_ERR_OK;
    // return m_network->exmsg_netif_out(this);
}

static net_err_t is_pkt_ok(ether_pkt_t* frame, int total_size) {
    if (total_size > (sizeof(ether_hdr_t) + ETHER_MTU)) {
        TINYTCP_LOG_WARN(g_logger) << "frame size too big, size=" << total_size;
        return net_err_t::NET_ERR_SIZE;
    }
    if (total_size < sizeof(ether_hdr_t)) {
        TINYTCP_LOG_WARN(g_logger) << "frame size too small, size=" << total_size;
        return net_err_t::NET_ERR_SIZE;
    }
    return net_err_t::NET_ERR_OK;
}

net_err_t EtherNet::link_open() {

    return net_err_t::NET_ERR_OK;
}

void EtherNet::link_close() {

    m_arp_processor.clear(this);
}

net_err_t EtherNet::arp_in(PktBuffer::ptr buf) {
    TINYTCP_LOG_INFO(g_logger) << "arp in";

    net_err_t err = buf->set_cont_header(sizeof(arp_pkt_t));
    if ((int8_t)err < 0) {
        return err;
    }

    arp_pkt_t* arp_packet = (arp_pkt_t*)buf->get_data();
    if (!arp_packet->is_pkt_ok(buf->get_capacity())) {
        TINYTCP_LOG_WARN(g_logger) << "arp_packet check error";
        return net_err_t::NET_ERR_NONE;
    }

    ipaddr_t target_ipaddr(*(uint32_t*)arp_packet->target_ipaddr);
    if (m_ipaddr == target_ipaddr) {
        TINYTCP_LOG_DEBUG(g_logger) << "recieve an arp for me";
        m_arp_processor.cache_insert(this, *(uint32_t*)arp_packet->sender_ipaddr, arp_packet->sender_hwaddr);

        if (net_to_host(arp_packet->opcode) == ARP_REQUEST) {
            return make_arp_response(buf);
        }
    }
    else {
        TINYTCP_LOG_DEBUG(g_logger) << "recieve an arp, not for me";

    }

    return net_err_t::NET_ERR_OK;
}

net_err_t EtherNet::link_in(PktBuffer::ptr buf) {
    buf->set_cont_header(sizeof(ether_hdr_t));
    ether_pkt_t* pkt = (ether_pkt_t*)buf->get_data();
    net_err_t err = is_pkt_ok(pkt, buf->get_capacity());
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "ether pkt error";
        return err;
    }

    // debug_print_ether_pkt(*pkt, buf->get_capacity());
    switch (net_to_host(pkt->hdr.protocol)) {
        case NET_PROTOCOL_ARP: {
            TINYTCP_LOG_DEBUG(g_logger) << "get arp pkt";
            buf->remove_header(sizeof(ether_hdr_t));
            return arp_in(buf);
            break;
        }
        case NET_PROTOCOL_IPv4: {
            TINYTCP_LOG_DEBUG(g_logger) << "get ipv4 pkt";
            break;
        }
        default: {
            TINYTCP_LOG_DEBUG(g_logger) << "get other";
            break;
        }
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t EtherNet::link_out(const ipaddr_t& ip, PktBuffer::ptr buf) {
    if (m_ipaddr == ip) {
        net_err_t err = ether_raw_out(NET_PROTOCOL_ARP, ether_broadcast_addr(), buf);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "ether_row_out error: " << magic_enum::enum_name(err);
            // buf->free();
        }
        return err;
    }

    const uint8_t* hwaddr = m_arp_processor.arp_find(this, ip);
    if (hwaddr != nullptr) {
        return ether_raw_out(NET_PROTOCOL_ARP, hwaddr, buf);
    }
    else {
        return arp_resolve(ip, buf);
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t EtherNet::arp_resolve(const ipaddr_t& ipaddr, PktBuffer::ptr buf) {
    return m_arp_processor.arp_resolve(this, ipaddr, buf);
}

net_err_t EtherNet::make_arp_request(const ipaddr_t& dest) {
    TINYTCP_LOG_DEBUG(g_logger) << "make arp request";
    PktBuffer::ptr buf = m_arp_processor.make_request(this, dest);
    if (buf == nullptr) {
        TINYTCP_LOG_WARN(g_logger) << "make_arp_request error";
        return net_err_t::NET_ERR_NONE;
    }
    net_err_t err = ether_raw_out(NET_PROTOCOL_ARP, ether_broadcast_addr(), buf);
    if ((int8_t)err < 0) {
        // buf->free();
    }
    return err;
}

net_err_t EtherNet::make_arp_response(PktBuffer::ptr buf) {
    m_arp_processor.make_response(this, buf);
    return ether_raw_out(NET_PROTOCOL_ARP, ((arp_pkt_t*)buf->get_data())->target_hwaddr, buf);
}

net_err_t EtherNet::ether_raw_out(uint16_t protocol, const uint8_t* dest, PktBuffer::ptr buf) {
    uint32_t size = buf->get_capacity();
    if (size < ETHER_DATA_MIN) {
        TINYTCP_LOG_INFO(g_logger) << "resize from " << size << " to " << ETHER_DATA_MIN;
        net_err_t err = buf->resize(ETHER_DATA_MIN);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_INFO(g_logger) << "resize buf error: " << magic_enum::enum_name(err);
            return err;
        }
        buf->reset_access();
        buf->seek(size);
        buf->fill(0x00, ETHER_DATA_MIN - size);

        size = ETHER_DATA_MIN;
    }

    net_err_t err = buf->alloc_header(sizeof(ether_hdr_t));
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "alloc header error: " << magic_enum::enum_name(err);
        return err;
    }

    ether_pkt_t* pkt = (ether_pkt_t*)buf->get_data();
    memcpy(pkt->hdr.dest, dest, ETHER_HWA_SIZE);
    memcpy(pkt->hdr.src, m_hwaddr.addr, ETHER_HWA_SIZE);
    pkt->hdr.protocol = host_to_net(protocol);

    // test
    // debug_print_ether_pkt(*pkt, size);
    // char test[size + sizeof(ether_hdr_t)];
    // buf->seek(0);
    // buf->read((uint8_t*)test, sizeof(test));
    // std::string str = std::string(test, sizeof(test));
    // TINYTCP_LOG_DEBUG(g_logger) << "pkt real data:" << std::make_pair(str, true);
    // test

    if (memcmp(m_hwaddr.addr, dest, ETHER_HWA_SIZE) == 0) {
        return put_buf_to_in_queue(buf, 0);
    }
    else {
        err = put_buf_to_out_queue(buf, 0);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "put_buf_to_out_queue error: " << magic_enum::enum_name(err);
            return err;
        }
        return send();
    }

    return net_err_t::NET_ERR_OK;
}

} // namespace tinytcp


