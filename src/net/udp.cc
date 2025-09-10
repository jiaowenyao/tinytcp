#include "udp.h"
#include "src/log.h"
#include "src/config.h"
#include "src/net/net.h"
#include "src/api/net_api.h"
#include "src/endiantool.h"
#include "src/net/protocol.h"


namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

static ConfigVar<uint32_t>::ptr g_net_port_dyn_start =
    Config::look_up("udp.net_port_dyn_start", 1024U, "动态分配端口的起始");
static ConfigVar<uint32_t>::ptr g_net_port_dyn_end =
    Config::look_up("udp.net_port_dyn_start", 65535U, "动态分配端口的结束");

static std::list<UDPSock*> g_udp_list;

static bool is_port_used(int port) {
    for (auto& sock : g_udp_list) {
        if (sock->get_local_port() == port) {
            return true;
        }
    }
    return false;
}

static net_err_t alloc_port(Sock* sock) {
    static int search_index = g_net_port_dyn_start->value();
    for (int i = g_net_port_dyn_start->value(); i <= g_net_port_dyn_end->value(); ++i) {
        int port = search_index++;
        if (search_index > g_net_port_dyn_end->value()) {
            search_index = g_net_port_dyn_start->value();
        }
        if (!is_port_used(port)) {
            sock->set_local_port(port);
            return net_err_t::NET_ERR_OK;
        }
    }
    return net_err_t::NET_ERR_NONE;
}

UDPSock::UDPSock(int family, int protocol)
    : Sock(family, protocol) {
    if (protocol == 0) {
        m_protocol = IPPROTO_UDP;
    }
    m_recv_wait = new sock_wait_t;
}
UDPSock::~UDPSock() {
    for (auto it = g_udp_list.begin(); it != g_udp_list.end(); ++it) {
        if (*it == this) {
            it = g_udp_list.erase(it);
            break;
        }
    }
}

void UDPSock::init() {
    g_udp_list.push_back(this);
}

net_err_t UDPSock::udp_out(const ipaddr_t& dest, uint16_t port, PktBuffer::ptr buf) {
    auto p = ProtocolStackMgr::get_instance();
    ipaddr_t src = m_local_ip;
    if (m_local_ip == ipaddr_t(0U)) {
        route_entry_t* rt = p->get_ipprotocol()->route_find(dest);
        if (rt == nullptr) {
            TINYTCP_LOG_ERROR(g_logger) << "no route";
            return net_err_t::NET_ERR_UNREACH;
        }
        src = rt->netif->get_ipaddr();
    }
    net_err_t err = buf->alloc_header(sizeof(udp_hdr_t));
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "add header error";
        return net_err_t::NET_ERR_SIZE;
    }

    udp_hdr_t* udp_hdr = (udp_hdr_t*)buf->get_data();
    udp_hdr->src_port = host_to_net(m_local_port);
    udp_hdr->dest_port = host_to_net(port);
    udp_hdr->total_len = host_to_net((uint16_t)buf->get_capacity());
    udp_hdr->checksum = 0;
    udp_hdr->checksum = checksum_peso(buf, dest, src, NET_PROTOCOL_UDP);

    err = p->get_ipprotocol()->ipv4_out(NET_PROTOCOL_UDP, dest, m_local_ip, buf);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "udp ipv4 out error";
        return err;
    }
    return net_err_t::NET_ERR_OK;
}

net_err_t UDPSock::sendto(const void* buf, size_t len, int flags,
                            const struct sockaddr* dest, socklen_t dest_len,
                            ssize_t* result_len) {
    auto pktmgr = PktMgr::get_instance();
    PktBuffer::ptr pktbuf = pktmgr->get_pktbuffer();
    if (!pktbuf || !pktbuf->alloc(len)) {
        TINYTCP_LOG_ERROR(g_logger) << "no buffer";
        return net_err_t::NET_ERR_MEM;
    }

    net_err_t err = pktbuf->write((uint8_t*)buf, len);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "pktbuf write error";
        return err;
    }

    auto p = ProtocolStackMgr::get_instance();
    // auto default_netif = p->get_network()->get_default();

    ipaddr_t dest_ipaddr(((sockaddr_in*)dest)->sin_addr.s_addr);
    uint16_t dest_port = net_to_host(((sockaddr_in*)dest)->sin_port);
    if (m_remote_ip.q_addr != 0 && !(m_remote_ip == dest_ipaddr)) {
        TINYTCP_LOG_ERROR(g_logger) << "dest ip is incorrect";
        return net_err_t::NET_ERR_NOT_SUPPORT;
    }
    if (m_remote_port != 0 && m_remote_port != dest_port) {
        TINYTCP_LOG_ERROR(g_logger) << "dest port is incorrect";
        return net_err_t::NET_ERR_NOT_SUPPORT;
    }

    if (m_local_port == 0) {
        m_err = alloc_port(this);
        if ((int8_t)m_err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "no port avaliable";
            return net_err_t::NET_ERR_NONE;
        }
    }

    err = udp_out(dest_ipaddr, dest_port, pktbuf);

    // err = p->get_ipprotocol()->ipv4_out(m_protocol, dest_ipaddr, m_local_ip, pktbuf);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "send error";
        return err;
    }

    *result_len = len;
    return net_err_t::NET_ERR_OK;
}

net_err_t UDPSock::recvfrom(const void* buf, size_t len, int flags,
                            struct sockaddr* src, socklen_t src_len,
                            ssize_t* result_len) {
    PktBuffer::ptr pktbuf = pop_buf();
    if (!pktbuf) {
        *result_len = 0;
        return net_err_t::NET_ERR_NEED_WAIT;
    }
    udp_from_t* from = (udp_from_t*)pktbuf->get_data();
    sockaddr_in* addr = (sockaddr_in*)src;
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = host_to_net(from->port);
    memcpy(&addr->sin_addr, (void*)&from->from, IPV4_ADDR_SIZE);

    pktbuf->remove_header(sizeof(udp_from_t));

    int size = std::min(pktbuf->get_capacity(), (uint32_t)len);
    pktbuf->reset_access();
    net_err_t err = pktbuf->read((uint8_t*)buf, size);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "pktbuf read error";
        return err;
    }
    *result_len = size;
    return net_err_t::NET_ERR_OK;
}


static UDPSock* find_udp(const ipaddr_t& src_ip, uint16_t src_port, const ipaddr_t& dest_ip, uint16_t dest_port) {
    if (!dest_port) {
        return nullptr;
    }
    for (auto& sock : g_udp_list) {
        if (sock->get_local_port() != dest_port) {
            continue;
        }
        if (!(sock->get_local_ip() == ipaddr_t(0U)) && !(sock->get_local_port() == dest_ip)) {
            continue;
        }
        if (!(sock->get_remote_ip() == ipaddr_t(0U)) && !(sock->get_remote_port() == src_ip)) {
            continue;
        }
        if (sock->get_remote_port() != 0 && sock->get_remote_port() != src_port) {
            continue;
        }
        return sock;
    }
    return nullptr;
}

static net_err_t is_pkt_ok(udp_pkt_t* pkt, int size) {
    if (size < sizeof(udp_hdr_t) && size < net_to_host(pkt->hdr.total_len)) {
        TINYTCP_LOG_ERROR(g_logger) << "udp packet size error";
        return net_err_t::NET_ERR_SIZE;
    }
    return net_err_t::NET_ERR_OK;
}

net_err_t udp_in(PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip) {
    int iphdr_size = ((ipv4_hdr_t*)buf->get_data())->get_header_size();
    net_err_t err = buf->set_cont_header(sizeof(udp_hdr_t) + iphdr_size);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "udp in set cont header error";
        return err;
    }

    udp_pkt_t* udp_pkt = (udp_pkt_t*)(buf->get_data() + iphdr_size);
    uint16_t local_port = net_to_host(udp_pkt->hdr.dest_port);
    uint16_t remote_port = net_to_host(udp_pkt->hdr.src_port);

    UDPSock* sock = find_udp(src_ip, remote_port, dest_ip, local_port);
    if (sock == nullptr) {
        TINYTCP_LOG_ERROR(g_logger) << "no udp for packet";
        return net_err_t::NET_ERR_UNREACH;
    }

    buf->remove_header(iphdr_size);
    buf->reset_access();
    udp_pkt = (udp_pkt_t*)buf->get_data();
    if (udp_pkt->hdr.checksum) {
        if (checksum_peso(buf, dest_ip, src_ip, NET_PROTOCOL_UDP) != 0) {
            TINYTCP_LOG_ERROR(g_logger) << "udp check sum failed";
            return net_err_t::NET_ERR_BROKEN;
        }
    }

    err = is_pkt_ok(udp_pkt, buf->get_capacity());
    if ((int8_t)err < 0) {
        return err;
    }


    buf->reset_access();
    // 留一部分头用来存储数据来源的信息
    buf->remove_header(sizeof(udp_hdr_t) - sizeof(udp_from_t));
    udp_from_t* from = (udp_from_t*)buf->get_data();
    from->port = remote_port;
    from->from = src_ip.q_addr;
    if (sock->push_buf(buf)) {
        sock->wakeup(SOCK_WAIT_READ, net_err_t::NET_ERR_OK);
    }
    else {
        TINYTCP_LOG_ERROR(g_logger) << "buf full";
        return net_err_t::NET_ERR_MEM;
    }
    return net_err_t::NET_ERR_OK;
}

std::ostream& operator<<(std::ostream& os, const udp_hdr_t& hdr) {
    os  << "src_port=" << net_to_host(hdr.src_port)
        << "\ndest_port=" << net_to_host(hdr.dest_port)
        << "\ntotal_len=" << net_to_host(hdr.total_len)
        << "\nchecksum=" << hdr.checksum;
    return os;
}


} // namespace tinytcp

