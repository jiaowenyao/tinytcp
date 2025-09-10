#include "ip.h"
#include "netif.h"
#include "endiantool.h"
#include "protocol.h"
#include "net.h"
#include "icmp.h"
#include "network.h"
#include "raw.h"
#include "udp.h"
#include "src/log.h"
#include "src/magic_enum.h"
#include "src/util.h"
#include "src/config.h"

namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");
static uint16_t packet_id = 0;
static ConfigVar<uint32_t>::ptr g_ip_frag_max_nr
    = Config::look_up<uint32_t>("ip.ip_frag_max_nr", 8, "最多支持几组ip分片");
static ConfigVar<uint32_t>::ptr g_ip_frag_scanning_cycle
    = Config::look_up<uint32_t>("ip.ip_frag_scanning_cycle", 1000, "ip分片的计时器(ms)，检查现在的分片是否有效");
static ConfigVar<uint32_t>::ptr g_ip_frag_timeout
    = Config::look_up<uint32_t>("ip.ip_frag_timeout", 10000, "ip分片的存活时间(ms)");


LockFreeRingQueue<ip_frag_t*>& IPProtocol::get_ip_frag_queue() noexcept {
    static LockFreeRingQueue<ip_frag_t*> s_ip_frag_queue = LockFreeRingQueue<ip_frag_t*>(g_ip_frag_max_nr->value());
    return s_ip_frag_queue;
}

namespace {
    struct ip_frag_init {
        ip_frag_init() {
            LockFreeRingQueue<ip_frag_t*>& ip_frag_queue = IPProtocol::get_ip_frag_queue();
            for (uint32_t i = 0; i < ip_frag_queue.capacity(); ++i) {
                ip_frag_queue.push(new ip_frag_t);
            }
        }
    };
}

void ip_frag_t::reset() noexcept {

}

bool ip_frag_t::frag_is_all_arrived() noexcept {
    uint32_t offset = 0;
    ipv4_pkt_t* pkt = nullptr;
    for (auto& buf : buf_list) {
        pkt = (ipv4_pkt_t*)buf->get_data();
        uint16_t curr_offset = pkt->hdr.get_frag_start();
        if (curr_offset != offset) {
            return false;
        }
        offset += pkt->hdr.get_data_size();
    }
    if (pkt != nullptr) {
        ipv4_hdr_t::frag_t _frag;
        _frag.frag_all = net_to_host(pkt->hdr.frag.frag_all);
        return !_frag.more;
    }
    return false;
}

PktBuffer::ptr ip_frag_t::frag_join() noexcept {
    PktBuffer::ptr target = nullptr;
    while (!buf_list.empty()) {
        PktBuffer::ptr buf = buf_list.front();
        buf_list.pop_front();
        if (target == nullptr) {
            target = buf;
            continue;
        }
        ipv4_pkt_t* pkt = (ipv4_pkt_t*)buf->get_data();
        net_err_t err = buf->remove_header(pkt->hdr.get_header_size());
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "remove hdr failed!";
            goto free_and_return;
        }
        err = target->merge_buf(buf);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "merge buf failed!";
            goto free_and_return;
        }
    }
    return target;
free_and_return:
    buf_list.clear();
    return nullptr;
}

ip_frag_t::ptr IPProtocol::get_ip_frag() {
    LockFreeRingQueue<ip_frag_t*>& ip_frag_queue = get_ip_frag_queue();
    ip_frag_t* frag;
    if (!ip_frag_queue.pop(&frag)) {
        m_frag_list.pop_back();
        if (!ip_frag_queue.pop(&frag)) {
            return nullptr;
        }
    }
    std::shared_ptr<ip_frag_t> ptr(frag, [&ip_frag_queue](ip_frag_t* _frag) {
        _frag->buf_list.clear();
        ip_frag_queue.push(_frag);
    });
    ptr->reset();
    return ptr;
}

uint32_t ipv4_hdr_t::get_data_size() noexcept{
    return net_to_host(total_len) - get_header_size();
}

uint16_t ipv4_hdr_t::get_frag_start() noexcept {
    ipv4_hdr_t::frag_t _frag;
    _frag.frag_all = net_to_host(frag.frag_all);
    return _frag.offset * 8;
}

uint16_t ipv4_hdr_t::get_frag_end() noexcept {
    return get_frag_start() + get_data_size();
}

uint32_t ipv4_hdr_t::get_header_size() noexcept {
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
        uint16_t c = checksum16(0, this, hdr_size, 0, 1);
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


void ipv4_hdr_t::hdr_net_to_host() noexcept {
    total_len = net_to_host(total_len);
    id = net_to_host(id);
    frag.frag_all = net_to_host(frag.frag_all);
}

void ipv4_hdr_t::hdr_host_to_net() noexcept {
    total_len = host_to_net(total_len);
    id = host_to_net(id);
    frag.frag_all = host_to_net(frag.frag_all);
}

IPProtocol::IPProtocol() {
    ip_frag_init __ip_frag_init;
}

uint32_t get_ip_frag_scanning_cycle() {
    return g_ip_frag_scanning_cycle->value();
}

// void IPProtocol::init() {
//     auto p = tinytcp::ProtocolStackMgr::get_instance();
//     p->add_timer(g_ip_frag_scanning_cycle->value(), std::bind(&IPProtocol::frag_timer, this), true);
// }

IPProtocol::~IPProtocol() {

}

net_err_t IPProtocol::ip_frag_out(uint8_t protocol, INetIF* netif, const ipaddr_t& dest, const ipaddr_t& src, PktBuffer::ptr buf, const ipaddr_t& next_hop) {
    TINYTCP_LOG_INFO(g_logger) << "ip_frag_out";
    auto pktmgr = PktMgr::get_instance();
    buf->reset_access();
    uint16_t offset = 0;
    uint32_t total = buf->get_capacity();
    while (total) {
        uint32_t curr_size = total;
        if (curr_size + sizeof(ipv4_hdr_t) > netif->get_mtu()) {
            curr_size = netif->get_mtu() - sizeof(ipv4_hdr_t);
        }
        PktBuffer::ptr dest_buf = pktmgr->get_pktbuffer();
        if (dest_buf != nullptr) {
            bool ok = dest_buf->alloc(curr_size + sizeof(ipv4_hdr_t));
            if (!ok) {
                TINYTCP_LOG_ERROR(g_logger) << "alloc buffer for frag send failed";
                return net_err_t::NET_ERR_NONE;
            }
        }
        else {
            TINYTCP_LOG_ERROR(g_logger) << "alloc buffer for frag send failed";
            return net_err_t::NET_ERR_NONE;
        }

        ipv4_pkt_t* pkt = (ipv4_pkt_t*)dest_buf->get_data();
        pkt->hdr.shdr_all = 0;
        pkt->hdr.version = NET_VERSION_IPV4;
        pkt->hdr.shdr = sizeof(ipv4_hdr_t) / 4;
        pkt->hdr.tos = 0;
        pkt->hdr.total_len = (uint16_t)dest_buf->get_capacity();
        pkt->hdr.id = packet_id;
        pkt->hdr.ttl = NET_IP_DEFAULT_TTL;
        pkt->hdr.protocol = protocol;
        pkt->hdr.hdr_checksum = 0;
        if (!(src == ipaddr_t(0U))) {
            pkt->hdr.src_ip = src.q_addr;
        }
        else {
            pkt->hdr.src_ip = netif->get_ipaddr().q_addr;
        }
        pkt->hdr.dest_ip = dest.q_addr;

        pkt->hdr.frag.offset = offset >> 3;
        pkt->hdr.frag.more = total > curr_size;

        dest_buf->seek(sizeof(ipv4_hdr_t));
        net_err_t err = dest_buf->copy(buf, curr_size);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "frag copy failed";
            return err;
        }
        buf->remove_header(curr_size);
        buf->reset_access();

        pkt->hdr.hdr_host_to_net();
        dest_buf->reset_access();
        pkt->hdr.hdr_checksum = dest_buf->buf_checksum16(pkt->hdr.get_header_size(), 0, 1);

        TINYTCP_LOG_DEBUG(g_logger) << "frag out:\n" << pkt->hdr;

        err = netif->netif_out(next_hop, dest_buf);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "send ip packet error";
            return err;
        }

        total -= curr_size;
        offset += curr_size;
    }
    packet_id++;
    return net_err_t::NET_ERR_OK;
}

net_err_t IPProtocol::ipv4_out(uint8_t protocol, const ipaddr_t& dest, const ipaddr_t& src, PktBuffer::ptr buf) {
    TINYTCP_LOG_DEBUG(g_logger) << "ipv4_out";
    const static ipaddr_t any(0U);

    auto route = route_find(dest);
    if (!route) {
        TINYTCP_LOG_ERROR(g_logger) << "send failed, no route";
        return net_err_t::NET_ERR_UNREACH;
    }
    ipaddr_t next_hop;
    if (route->next_hop == any) {
        next_hop = dest;
    }
    else {
        next_hop = route->next_hop;
    }

    auto netif = route->netif;
    if (netif->get_mtu() && (buf->get_capacity() + sizeof(ipv4_hdr_t)) > netif->get_mtu()) {
        net_err_t err = ip_frag_out(protocol, netif, dest, src, buf, next_hop);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "send ip frag failed";
            return err;
        }
        return net_err_t::NET_ERR_OK;
    }

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
    if (!(src == ipaddr_t(0U))) {
        pkt->hdr.src_ip = src.q_addr;
    }
    else {
        pkt->hdr.src_ip = netif->get_ipaddr().q_addr;
    }
    pkt->hdr.dest_ip = dest.q_addr;

    pkt->hdr.hdr_host_to_net();
    buf->reset_access();
    pkt->hdr.hdr_checksum = buf->buf_checksum16(pkt->hdr.get_header_size(), 0, 1);

    TINYTCP_LOG_DEBUG(g_logger) << pkt->hdr << "\n" << std::make_pair(std::string((char*)buf->get_data(), sizeof(ipv4_hdr_t)), true);

    err = netif->netif_out(next_hop, buf);
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

    ipv4_hdr_t::frag_t frag;
    frag.frag_all = net_to_host(ipv4_pkt->hdr.frag.frag_all);
    if (frag.offset != 0 || frag.more != 0) {
        err = ip_frag_in(netif, buf, ipaddr_t(ipv4_pkt->hdr.src_ip), ipaddr_t(ipv4_pkt->hdr.dest_ip));
    }
    else {
        err = ip_normal_in(netif, buf, ipaddr_t(ipv4_pkt->hdr.src_ip), ipaddr_t(ipv4_pkt->hdr.dest_ip));
    }

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
            net_err_t err = udp_in(buf, src_ip, dest_ip);
            if ((int8_t)err < 0) {
                TINYTCP_LOG_WARN(g_logger) << "udp in failed";
                if (err == net_err_t::NET_ERR_UNREACH) {
                    net_err_t err = ProtocolStackMgr::get_instance()->get_icmpprotocol()->icmpv4_out_unreach(src_ip, netif->get_ipaddr(), ICMPv4_UNREACH_PORT, buf);
                }
                return err;
            }
            return net_err_t::NET_ERR_OK;
        }
        case NET_PROTOCOL_TCP: {
            break;
        }
        default: {
            net_err_t err = raw_in(buf);
            if ((int8_t)err < 0) {
                TINYTCP_LOG_WARN(g_logger) << "raw in error";
                return err;
            }
            break;
        }
    }

    return net_err_t::NET_ERR_OK;
}

std::list<ip_frag_t::ptr>::iterator IPProtocol::find_frag(const ipaddr_t& ipaddr, uint16_t id) {
    for (auto it = m_frag_list.begin(); it != m_frag_list.end(); ++it) {
        if ((*it)->ip == ipaddr && (*it)->id == id) {
            m_frag_list.splice(m_frag_list.begin(), m_frag_list, it);
            return it;
        }
    }
    return m_frag_list.end();
}

net_err_t IPProtocol::frag_timer() {
    for (auto it = m_frag_list.begin(); it != m_frag_list.end();) {
        auto frag = *it;
        if (--(frag->timeout) <= 0) {
            it = m_frag_list.erase(it);
        }
        else {
            ++it;
        }
    }
    return net_err_t::NET_ERR_OK;
}

void IPProtocol::frag_add(ip_frag_t::ptr frag, const ipaddr_t& ip, uint16_t id) {
    frag->ip = ip;
    frag->id = id;
    // timeout:每次扫描就减1
    frag->timeout = g_ip_frag_timeout->value() / g_ip_frag_scanning_cycle->value();
    m_frag_list.push_front(frag);
}

void IPProtocol::remove_frag(const ip_frag_t& frag) {
    for (auto it = m_frag_list.begin(); it != m_frag_list.end(); ++it) {
        if ((*it)->id == frag.id) {
            m_frag_list.erase(it);
            return ;
        }
    }
}

net_err_t IPProtocol::frag_insert(ip_frag_t::ptr frag, PktBuffer::ptr buf, ipv4_pkt_t* pkt) {
    if (frag->buf_list.size() == 0) {
        frag->buf_list.push_back(buf);
        return net_err_t::NET_ERR_OK;
    }
    for (auto it = frag->buf_list.begin(); it != frag->buf_list.end(); ++it) {
        ipv4_pkt_t* curr_pkt = (ipv4_pkt_t*)(*it)->get_data();
        uint16_t curr_frag_start = curr_pkt->hdr.get_frag_start();
        if (pkt->hdr.get_frag_start() == curr_frag_start) {
            return net_err_t::NET_ERR_EXIST;
        }
        else if (pkt->hdr.get_frag_end() <= curr_frag_start) {
            frag->buf_list.insert(it, buf);
            return net_err_t::NET_ERR_OK;
        }
    }
    frag->buf_list.push_back(buf);
    return net_err_t::NET_ERR_OK;
}

net_err_t IPProtocol::ip_frag_in(INetIF* netif, PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip) {
    ipv4_pkt_t* pkt = (ipv4_pkt_t*)buf->get_data();

    auto frag_it = find_frag(src_ip, pkt->hdr.id);
    ip_frag_t::ptr frag = nullptr;
    if (frag_it == m_frag_list.end()) {
        frag = get_ip_frag();
        if (frag == nullptr) {
            TINYTCP_LOG_ERROR(g_logger) << "get_ip_frag error!!!!!!!!!!!!!!!!!";
            return net_err_t::NET_ERR_EMPTY;
        }
        frag_add(frag, src_ip, pkt->hdr.id);
    }
    else {
        frag = *frag_it;
    }

    net_err_t err = frag_insert(frag, buf, pkt);
    if ((int8_t)err < 0) {
        TINYTCP_LOG_WARN(g_logger) << "frag insert error";
        return err;
    }

    if (frag->frag_is_all_arrived()) {
        PktBuffer::ptr full_buf = frag->frag_join();
        if (full_buf == nullptr) {
            TINYTCP_LOG_ERROR(g_logger) << "join ip bufs failed";
            frag_debug_print();
            return net_err_t::NET_ERR_OK;
        }
        // 全部到达之后，从列表中删除
        remove_frag(*(frag.get()));

        err = ip_normal_in(netif, full_buf, src_ip, dest_ip);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_WARN(g_logger) << "ip frag in failed";
            return err;
        }
    }

    frag_debug_print();
    return net_err_t::NET_ERR_OK;
}

void IPProtocol::frag_debug_print() {
    std::stringstream ss;
    int frag_index = 0;
    for (auto& frag : m_frag_list) {
        ss << "frag[" << frag_index++ << "]:\n" << *(frag.get()) << "\n\n";
    }
    TINYTCP_LOG_DEBUG(g_logger) << "\n" << ss.str();
}

void IPProtocol::route_add(const ipaddr_t& net, const ipaddr_t& mask, const ipaddr_t& next_hop, INetIF* netif) {
    m_route_list.push_front(route_entry_t(net, mask, next_hop, netif));
}

void IPProtocol::route_remove(const ipaddr_t& net, const ipaddr_t& mask) {
    for (auto it = m_route_list.begin(); it != m_route_list.end();) {
        auto& route = *it;
        if (route.net == net && route.mask == mask) {
            it = m_route_list.erase(it);
        }
        else {
            ++it;
        }
    }
}

void IPProtocol::debug_print_route_list() {
    std::stringstream ss;
    ss << "\n";
    for (auto& route : m_route_list) {
        ss << route << "\n\n";
    }
    TINYTCP_LOG_DEBUG(g_logger) << ss.str();
}

route_entry_t* IPProtocol::route_find(const ipaddr_t& ip) {
    route_entry_t* res = nullptr;
    for (auto& route : m_route_list) {
        ipaddr_t net_ip(ip.q_addr & route.mask.q_addr);
        if (!(net_ip == route.net)) {
            continue;
        }
        if (res == nullptr || res->mask_1_cnt < route.mask_1_cnt) {
            res = &route;
        }
    }
    return res;
}

std::ostream& operator<<(std::ostream& os, const ipv4_hdr_t& hdr) {
    os  << "\n"
        << "\nshdr=" << hdr.shdr << " version=" << hdr.version << " tos=" << hdr.tos
        << "\ntotal_len=" << hdr.total_len
        << "\nid=" << hdr.id
        << "\nfrag_offset=" << hdr.frag.offset << " frag_more=" << hdr.frag.more
        << " frag_disable=" << hdr.frag.disable
        << "\nttl=" << (uint32_t)hdr.ttl << " protocol=" <<  (uint32_t)hdr.protocol
        << "\nhdr_checksum=" << hdr.hdr_checksum
        << "\nsrc_ip=" << ipaddr_t(hdr.src_ip)
        << "\ndest_ip=" << ipaddr_t(hdr.dest_ip);

    return os;
}

std::ostream& operator<<(std::ostream& os, const ip_frag_t& frag) {
    os  << "ip=" << frag.ip
        << "\nid=" << frag.id
        << "\ntimeout=" << frag.timeout
        << "\npktbuf_size=" << frag.buf_list.size();
    int buf_index = 0;
    for (auto& buf : frag.buf_list) {
        ipv4_pkt_t* pkt = (ipv4_pkt_t*)buf->get_data();
        os  << "\n"
            << "buf[" << buf_index++ << "]:"
            << " frag_start=" << pkt->hdr.get_frag_start()
            << " frag_end=" << pkt->hdr.get_frag_end() - 1;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const route_entry_t& route) {
    os  << "net=" << route.net
        << "\nmask=" << route.mask
        << "\nnext_hop=" << route.next_hop
        << "\nnetif_name=" << route.netif->get_name();
    return os;
}

} // namespace tinytcp

