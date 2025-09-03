#include "icmp.h"
#include "log.h"
#include "ip.h"
#include "protocol.h"
#include "net.h"


namespace tinytcp {


static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

ICMPProtocol::ICMPProtocol() {

}

ICMPProtocol::~ICMPProtocol() {

}

bool icmpv4_pkt_t::is_pkt_ok(uint32_t size, PktBuffer::ptr buf) {
    if (size <= sizeof(ipv4_hdr_t)) {
        TINYTCP_LOG_WARN(g_logger) << "size error";
        return false;
    }

    // 校验和
    uint16_t checksum = buf->buf_checksum16(size, 0, 1);
    if (checksum != 0) {
        TINYTCP_LOG_WARN(g_logger) << "bad checksum";
        return false;
    }

    return true;
}

net_err_t ICMPProtocol::icmpv4_out(const ipaddr_t& dest_ip, const ipaddr_t& src_ip, PktBuffer::ptr buf) {
    icmpv4_pkt_t* pkt = (icmpv4_pkt_t*)buf->get_data();
    buf->reset_access();
    pkt->hdr.checksum = buf->buf_checksum16(buf->get_capacity(), 0, 1);

    // TINYTCP_LOG_DEBUG(g_logger) << "icmpv4_out:" << pkt->hdr;

    return ProtocolStackMgr::get_instance()->get_ipprotocol()->ipv4_out(NET_PROTOCOL_ICMPv4, dest_ip, src_ip, buf);
}

net_err_t ICMPProtocol::icmpv4_in(const ipaddr_t& src_ip, const ipaddr_t& netif_ip, PktBuffer::ptr buf) {
    TINYTCP_LOG_INFO(g_logger) << "icmpv4_in";

    ipv4_pkt_t* ip_pkt = (ipv4_pkt_t*)buf->get_data();
    uint32_t iphdr_size = ip_pkt->hdr.get_header_size();

    net_err_t err = buf->set_cont_header(iphdr_size + sizeof(icmpv4_hdr_t));
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "icmpv4 set cont header error";
        return err;
    }

    ip_pkt = (ipv4_pkt_t*)buf->get_data();
    err = buf->remove_header(iphdr_size);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "remove ip header error";
        return err;
    }
    buf->reset_access();
    icmpv4_pkt_t* icmp_pkt = (icmpv4_pkt_t*)buf->get_data();
    if (!icmp_pkt->is_pkt_ok(buf->get_capacity(), buf)) {
        TINYTCP_LOG_WARN(g_logger) << "icmp pkt hdr error";
        return net_err_t::NET_ERR_STATE;
    }


    switch (icmp_pkt->hdr.type) {
        case ICMPv4_ECHO_REQUEST: {
            return icmpv4_echo_reply(src_ip, netif_ip, buf);
        }
        default: {
            return net_err_t::NET_ERR_OK;
        }
    }

    return net_err_t::NET_ERR_OK;
}

net_err_t ICMPProtocol::icmpv4_echo_reply(const ipaddr_t& dest_ip, const ipaddr_t& src_ip, PktBuffer::ptr buf) {
    // buf->reset_access();
    icmpv4_pkt_t* pkt = (icmpv4_pkt_t*)buf->get_data();

    pkt->hdr.type = ICMPv4_ECHO_REPLY;
    pkt->hdr.checksum = 0;

    return icmpv4_out(dest_ip, src_ip, buf);
}

std::ostream& operator<<(std::ostream& os, const icmpv4_hdr_t& hdr) {
    os  << "\n"
        << "\ntype=" << (uint32_t)hdr.type
        << "\ncode=" << (uint32_t)hdr.code
        << "\nchecksum=" << hdr.checksum;

    return os;
}


} // namespace tinytcp



