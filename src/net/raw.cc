#include "raw.h"
#include "net.h"
#include "src/log.h"
#include "src/config.h"
#include "sock.h"
#include "src/api/net_api.h"
#include <string.h>


namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

RAWSock::RAWSock(int family, int protocol)
    : Sock(family, protocol) {
    if (protocol == 0) {
        m_protocol = IPPROTO_ICMP;
    }

    m_recv_wait = new sock_wait_t;
}

RAWSock::~RAWSock() {

}

net_err_t RAWSock::sendto(const void* buf, size_t len, int flags,
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
    if (m_remote_ip.q_addr != 0 && !(m_remote_ip == dest_ipaddr)) {
        TINYTCP_LOG_ERROR(g_logger) << "dest is incorrect";
        return net_err_t::NET_ERR_NOT_SUPPORT;
    }

    err = p->get_ipprotocol()->ipv4_out(m_protocol, dest_ipaddr, m_local_ip, pktbuf);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "send error";
        return err;
    }

    *result_len = len;
    return net_err_t::NET_ERR_OK;
}

net_err_t RAWSock::recvfrom(const void* buf, size_t len, int flags,
                            struct sockaddr* src, socklen_t src_len,
                            ssize_t* result_len) {
    PktBuffer::ptr pktbuf = pop_buf();
    if (!pktbuf) {
        *result_len = 0;
        return net_err_t::NET_ERR_NEED_WAIT;
    }
    ipv4_hdr_t* iphdr = (ipv4_hdr_t*)pktbuf->get_data();
    sockaddr_in* addr = (sockaddr_in*)src;
    memset(addr, 0, sizeof(*addr));
    addr->sin_family = AF_INET;
    addr->sin_port = 0;
    memcpy(&addr->sin_addr, (void*)&iphdr->src_ip, IPV4_ADDR_SIZE);
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

net_err_t raw_in(PktBuffer::ptr buf) {
    ipv4_hdr_t* iphdr = (ipv4_hdr_t*)buf->get_data();
    ipaddr_t src = iphdr->src_ip;
    ipaddr_t dest = iphdr->dest_ip;
    Sock* sock = Sock::find_sock(src, dest, iphdr->protocol);
    if (!sock) {
        TINYTCP_LOG_ERROR(g_logger) << "no raw fro this packet";
        return net_err_t::NET_ERR_UNREACH;
    }
    if (sock->push_buf(buf)) {
        sock->wakeup(SOCK_WAIT_READ, net_err_t::NET_ERR_OK);
    }
    else {
        TINYTCP_LOG_ERROR(g_logger) << "buf full";
        return net_err_t::NET_ERR_MEM;
    }
    return net_err_t::NET_ERR_OK;
}


} // namespace tinytcp

