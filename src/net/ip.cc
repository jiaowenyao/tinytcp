#include "ip.h"
#include "netif.h"
#include "endiantool.h"
#include "protocol.h"
#include "net.h"
#include "icmp.h"
#include "network.h"
#include "src/log.h"
#include "src/magic_enum.h"
#include "src/util.h"

namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");
static uint16_t packet_id = 0;

uint32_t ipv4_hdr_t::get_header_size() {
    return shdr * 4;
}

bool ipv4_pkt_t::is_pkt_ok(uint16_t size, INetIF* netif) {
    if (hdr.version != NET_VERSION_IPV4) {
        TINYTCP_LOG_WARN(g_logger) << "invalid ip version";
        return false;
    }

    int hdr_size = hdr.get_header_size();
    if (hdr_size < sizeof(ipv4_hdr_t)) {
        TINYTCP_LOG_WARN(g_logger) << "ipv4 header size error";
        return false;
    }

    uint16_t total_size = net_to_host(hdr.total_len);
    if ((total_size < sizeof(ipv4_hdr_t)) || (size < total_size)) {
        TINYTCP_LOG_WARN(g_logger) << "ipv4 total size error";
        return false;
    }

    // 校验和检查
    if (hdr.hdr_checksum) {
        uint16_t c = checksum16(this, hdr_size, 0, 1);
        if (c != 0) {
            TINYTCP_LOG_WARN(g_logger) << "bad checksum16";
            return false;
        }
    }

    // 地址检查
    ipaddr_t dest_ip(hdr.dest_ip);
    ipaddr_t src_ip(hdr.src_ip);
    if (!ipaddr_is_match(dest_ip, netif->get_ipaddr(), netif->get_netmask())) {
        TINYTCP_LOG_WARN(g_logger) << "ipaddr not match, dest_ip=" << dest_ip;
        return false;
    }


    return true;
}


void ipv4_hdr_t::hdr_net_to_host() {
    total_len = net_to_host(total_len);
    id = net_to_host(id);
    frag_all = net_to_host(frag_all);
}

void ipv4_hdr_t::hdr_host_to_net() {
    total_len = host_to_net(total_len);
    id = host_to_net(id);
    frag_all = host_to_net(frag_all);
}

IPProtocol::IPProtocol() {

}

IPProtocol::~IPProtocol() {

}

net_err_t IPProtocol::ipv4_out(uint8_t protocol, const ipaddr_t& dest, const ipaddr_t& src, PktBuffer::ptr buf) {
    TINYTCP_LOG_DEBUG(g_logger) << "ipv4_out";

    net_err_t err = buf->alloc_header(sizeof(ipv4_hdr_t));
    buf->reset_access();
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "alloc_header error";
        return net_err_t::NET_ERR_SIZE;
    }

    ipv4_pkt_t* pkt = (ipv4_pkt_t*)buf->get_data();
    pkt->hdr.shdr_all = 0;
    pkt->hdr.version = NET_VERSION_IPV4;
    pkt->hdr.shdr = sizeof(ipv4_hdr_t) / 4;
    pkt->hdr.tos = 0;
    // pkt->hdr.total_len = host_to_net((uint16_t)buf->get_capacity());
    pkt->hdr.total_len = (uint16_t)buf->get_capacity();
    pkt->hdr.id = packet_id++;
    pkt->hdr.ttl = NET_IP_DEFAULT_TTL;
    pkt->hdr.protocol = protocol;
    pkt->hdr.hdr_checksum = 0;
    pkt->hdr.src_ip = src.q_addr;
    pkt->hdr.dest_ip = dest.q_addr;

    pkt->hdr.hdr_host_to_net();
    buf->reset_access();
    pkt->hdr.hdr_checksum = buf->buf_checksum16(pkt->hdr.get_header_size(), 0, 1);

    TINYTCP_LOG_DEBUG(g_logger) << pkt->hdr << "\n" << std::make_pair(std::string((char*)buf->get_data(), sizeof(ipv4_hdr_t)), true);

    auto default_netif = ProtocolStackMgr::get_instance()->get_network()->get_default();
    err = default_netif->netif_out(dest, buf);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "send ip packet error";
        return err;
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t IPProtocol::ipv4_in(INetIF* netif, PktBuffer::ptr buf) {
    TINYTCP_LOG_INFO(g_logger) << "ipv4 in";

    net_err_t err = buf->set_cont_header(sizeof(ipv4_hdr_t));
    if ((int8_t)err < 0) {
        return err;
    }

    ipv4_pkt_t* ipv4_pkt = (ipv4_pkt_t*)buf->get_data();
    if (!ipv4_pkt->is_pkt_ok(buf->get_capacity(), netif)) {
        TINYTCP_LOG_WARN(g_logger) << "ipv4_pkt check error";
        return net_err_t::NET_ERR_NONE;
    }

    if (!ipaddr_is_match(ipaddr_t(ipv4_pkt->hdr.dest_ip), netif->get_ipaddr(), netif->get_netmask())) {
        return net_err_t::NET_ERR_UNREACH;
    }

    err = ip_normal_in(netif, buf, ipaddr_t(ipv4_pkt->hdr.src_ip), ipaddr_t(ipv4_pkt->hdr.dest_ip));

    // ipv4_pkt->hdr.hdr_net_to_host();

    // TINYTCP_LOG_DEBUG(g_logger) << "recv ipv4 pkt:" << ipv4_pkt->hdr;

    // err = buf->resize(net_to_host(ipv4_pkt->hdr.total_len));
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "ipv4 in: buf resize error: " << magic_enum::enum_name(err);
        return err;
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t IPProtocol::ip_normal_in(INetIF* netif, PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip) {
    ipv4_pkt_t* pkt = (ipv4_pkt_t*)buf->get_data();

    switch (pkt->hdr.protocol) {
        case NET_PROTOCOL_ICMPv4: {
            net_err_t err = ProtocolStackMgr::get_instance()->get_icmpprotocol()->icmpv4_in(src_ip, netif->get_ipaddr(), buf);
            if ((int8_t)err < 0) {
                TINYTCP_LOG_WARN(g_logger) << "icmp in failed";
                return err;
            }
            break;
        }
        case NET_PROTOCOL_UDP: {
            break;
        }
        case NET_PROTOCOL_TCP: {
            break;
        }
        default: {
            TINYTCP_LOG_WARN(g_logger) << "unknown protocol";
            break;
        }
    }

    return net_err_t::NET_ERR_OK;
}


std::ostream& operator<<(std::ostream& os, const ipv4_hdr_t& hdr) {
    os  << "\n"
        << "\nshdr=" << hdr.shdr << " version=" << hdr.version << " tos=" << hdr.tos
        << "\ntotal_len=" << hdr.total_len
        << "\nid=" << hdr.id << " frag_all=" << hdr.frag_all
        << "\nttl=" << (uint32_t)hdr.ttl << " protocol=" <<  (uint32_t)hdr.protocol
        << "\nhdr_checksum=" << hdr.hdr_checksum
        << "\nsrc_ip=" << ipaddr_t(hdr.src_ip)
        << "\ndest_ip=" << ipaddr_t(hdr.dest_ip);

    return os;
}

} // namespace tinytcp

