#include "arp.h"
#include "log.h"
#include "endiantool.h"
#include "protocol.h"
#include "netif.h"


namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

bool arp_pkt_t::is_pkt_ok(uint16_t size) {
    if (size < sizeof(arp_pkt_t)) {
        TINYTCP_LOG_WARN(g_logger) << "size < sizeof(arp_pkt_t)";
        return false;
    }

    if (net_to_host(htype) != ARP_HW_ETHER
        || hwlen != ETHER_HWA_SIZE
        || net_to_host(iptype) != NET_PROTOCOL_IPv4
        || iplen != IPV4_ADDR_SIZE) {
        TINYTCP_LOG_WARN(g_logger) << "protocol error";
        return false;
    }

    uint16_t op = net_to_host(opcode);
    if (op != ARP_REPLAY && op != ARP_REQUEST) {
        TINYTCP_LOG_WARN(g_logger) << "opcode error, opcode=" << op;
        return false;
    }

    return true;
}

ARPEntry::ARPEntry() {

}

ARPEntry::~ARPEntry() {

}

ARPProcessor::ARPProcessor() {

}

ARPProcessor::~ARPProcessor() {

}

PktBuffer::ptr ARPProcessor::make_request(INetIF* netif, const ipaddr_t& dest) {
    auto pktmgr = PktMgr::get_instance();
    PktBuffer::ptr buf = pktmgr->get_pktbuffer();
    if (buf == nullptr) {
        TINYTCP_LOG_WARN(g_logger) << "alloc buf error";
        return nullptr;
    }

    bool ok = buf->alloc(sizeof(arp_pkt_t));
    if (!ok) {
        TINYTCP_LOG_WARN(g_logger) << "alloc block error";
        // buf->free();
        return nullptr;
    }

    buf->set_cont_header(sizeof(arp_pkt_t));
    arp_pkt_t* arp_packet = (arp_pkt_t*)buf->get_data();
    arp_packet->htype    = host_to_net((uint16_t)ARP_HW_ETHER);
    arp_packet->iptype   = host_to_net((uint16_t)NET_PROTOCOL_IPv4);
    arp_packet->hwlen    = ETHER_HWA_SIZE;
    arp_packet->iplen    = IPV4_ADDR_SIZE;
    arp_packet->opcode   = host_to_net((uint16_t)ARP_REQUEST);
    memcpy(arp_packet->sender_hwaddr, netif->get_hwaddr().addr, ETHER_HWA_SIZE);
    memcpy(arp_packet->sender_ipaddr, netif->get_ipaddr().a_addr, IPV4_ADDR_SIZE);
    memcpy(arp_packet->target_ipaddr, dest.a_addr, IPV4_ADDR_SIZE);
    memset(arp_packet->target_hwaddr, 0, ETHER_HWA_SIZE);

    return buf;
}


PktBuffer::ptr ARPProcessor::make_gratuitous(INetIF* netif) {
    TINYTCP_LOG_INFO(g_logger) << "make a gratuitous arp...";
    return make_request(netif, netif->get_ipaddr());
}


PktBuffer::ptr ARPProcessor::make_response(INetIF* netif, PktBuffer::ptr buf) {
    arp_pkt_t* arp_packet = (arp_pkt_t*)buf->get_data();
    arp_packet->opcode = host_to_net((uint16_t)ARP_REPLAY);
    memcpy(arp_packet->target_hwaddr, arp_packet->sender_hwaddr, ETHER_HWA_SIZE);
    memcpy(arp_packet->target_ipaddr, arp_packet->sender_ipaddr, IPV4_ADDR_SIZE);
    memcpy(arp_packet->sender_hwaddr, netif->get_hwaddr().addr, ETHER_HWA_SIZE);
    memcpy(arp_packet->sender_ipaddr, netif->get_ipaddr().a_addr, ETHER_HWA_SIZE);
    return buf;
}

} // namespace tinytcp


