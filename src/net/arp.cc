#include "arp.h"
#include "log.h"
#include "endiantool.h"
#include "protocol.h"
#include "netif.h"
#include "ip.h"
#include "icmp.h"
#include "src/magic_enum.h"
#include "src/config.h"


namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");
static ConfigVar<uint32_t>::ptr g_arp_cache_size =
    Config::look_up("arp.arp_cache_size", (uint32_t)100, "arp cache_size");
static ConfigVar<uint32_t>::ptr g_arp_cache_max_pktbuf_size =
    Config::look_up("arp.arp_cache_max_pktbuf_size", (uint32_t)100, "arp cache中的等待的pktbuf最大值");
static ConfigVar<uint32_t>::ptr g_arp_cache_timeout = 
    Config::look_up("arp.arp_cache_timeout", (uint32_t)1000, "给arp缓存表的扫描时间, ms");
static ConfigVar<uint32_t>::ptr g_arp_cache_stable_timeout = 
    Config::look_up("arp.arp_cache_stable_timeout", (uint32_t)20000, "一个RESOLVE状态的arp缓存保持的时间, ms");
static ConfigVar<uint32_t>::ptr g_arp_cache_pending_timeout = 
    Config::look_up("arp.arp_cache_pending_timeout", (uint32_t)1000, "arp缓存重新发出请求后的等待时间, ms");
static ConfigVar<uint32_t>::ptr g_arp_cache_retry_cnt = 
    Config::look_up("arp.arp_cache_retry_cnt", (uint32_t)5, "arp缓存查询重试次数");

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
    m_buf_list.clear();
}

void ARPEntry::clear_buf() {
    m_buf_list.clear();
}

ARPProcessor::ARPProcessor() {

}

ARPProcessor::~ARPProcessor() {

}

PktBuffer::ptr ARPProcessor::make_request(EtherNet* netif, const ipaddr_t& dest) {

    // test
    // uint8_t* ip = (uint8_t*)dest.a_addr;
    // ip[0] = 0x1;
    // cache_insert(netif, *(uint32_t*)ip, netif->get_hwaddr().addr);
    // ip[0] = 0x2;
    // cache_insert(netif, *(uint32_t*)ip, netif->get_hwaddr().addr);
    // ip[0] = 0x3;
    // cache_insert(netif, *(uint32_t*)ip, netif->get_hwaddr().addr);
    // cache_insert(netif, *(uint32_t*)ip, netif->get_hwaddr().addr);


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

    TINYTCP_LOG_DEBUG(g_logger) << *arp_packet;
    return buf;
}


PktBuffer::ptr ARPProcessor::make_gratuitous(EtherNet* netif) {
    TINYTCP_LOG_INFO(g_logger) << "make a gratuitous arp...";
    return make_request(netif, netif->get_ipaddr());
}


PktBuffer::ptr ARPProcessor::make_response(EtherNet* netif, PktBuffer::ptr buf) {
    arp_pkt_t* arp_packet = (arp_pkt_t*)buf->get_data();
    arp_packet->opcode = host_to_net((uint16_t)ARP_REPLAY);
    memcpy(arp_packet->target_hwaddr, arp_packet->sender_hwaddr, ETHER_HWA_SIZE);
    memcpy(arp_packet->target_ipaddr, arp_packet->sender_ipaddr, IPV4_ADDR_SIZE);
    memcpy(arp_packet->sender_hwaddr, netif->get_hwaddr().addr, ETHER_HWA_SIZE);
    memcpy(arp_packet->sender_ipaddr, netif->get_ipaddr().a_addr, ETHER_HWA_SIZE);
    TINYTCP_LOG_DEBUG(g_logger) << *arp_packet;
    return buf;
}

net_err_t ARPEntry::cache_send_all() {

    for (auto& it : m_buf_list) {
        net_err_t err = m_netif->ether_raw_out(NET_PROTOCOL_IPv4, m_hwaddr, it);
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "cache_send_all error";
        }
    }
    m_buf_list.clear();

    return net_err_t::NET_ERR_OK;
}

uint32_t ARPProcessor::get_cache_timeout() const noexcept {
    return g_arp_cache_timeout->value();
}

net_err_t ARPProcessor::cache_timer() {
    uint32_t cache_timeout = g_arp_cache_timeout->value();
    // TINYTCP_LOG_DEBUG(g_logger) << "cache_timer trigger";
    for (auto it = m_cache_list.begin(); it != m_cache_list.end();) {
        auto cache = *it;
        cache->m_timeout -= cache_timeout;
        if (cache->m_timeout > 0) {
            continue;
        }
        bool has_erase = false;
        switch ((int32_t)cache->m_state) {
            case ARPEntry::NET_ARP_RESOLVED: {
                TINYTCP_LOG_DEBUG(g_logger) << "state to pending";
                cache->m_state = ARPEntry::NET_ARP_WAITING;
                cache->m_timeout = g_arp_cache_pending_timeout->value();
                cache->m_retry_cnt = g_arp_cache_retry_cnt->value();
                cache->m_netif->make_arp_request(*(uint32_t*)cache->m_ipaddr);
                break;
            }
            case ARPEntry::NET_ARP_WAITING: {
                if (--cache->m_retry_cnt == 0) {
                    TINYTCP_LOG_INFO(g_logger) << "pending timeout, free it";
                    it = m_cache_list.erase(it);
                    has_erase = true;
                    debug_print();
                }
                else {
                    TINYTCP_LOG_DEBUG(g_logger) << "pending timeout, send request";
                    cache->m_timeout = g_arp_cache_pending_timeout->value();
                    cache->m_netif->make_arp_request(*(uint32_t*)cache->m_ipaddr);
                    break;
                }
            }

            default: {
                TINYTCP_LOG_ERROR(g_logger) << "unknown arp state";
                break;
            }
        }

        if (!has_erase) {
            ++it;
        }
    }
    return net_err_t::NET_ERR_OK;
}

void ARPProcessor::update_from_ipbuf(EtherNet* netif, PktBuffer::ptr buf) {
    net_err_t err = buf->set_cont_header(sizeof(ipv4_hdr_t) + sizeof(ether_hdr_t));
    if (int8_t(err) < 0) {
        TINYTCP_LOG_ERROR(g_logger) << "update from ipbuf, set cont header error";
        return;
    }

    ether_hdr_t* eth_hdr = (ether_hdr_t*)buf->get_data();
    ipv4_hdr_t* ipv4_hdr = (ipv4_hdr_t*)((uint8_t*)eth_hdr + sizeof(ether_hdr_t));
    if (ipv4_hdr->version != NET_VERSION_IPV4) {
        TINYTCP_LOG_WARN(g_logger) << "not ipv4";
        return;
    }

    cache_insert(netif, ipaddr_t(ipv4_hdr->src_ip), eth_hdr->src);
}

void ARPProcessor::debug_print() {
    std::stringstream ss;
    ss << "\n";
    for (auto& it : m_cache_list) {
        if (it->m_state == ARPEntry::NET_ARP_FREE) {
            continue;
        }
        ss << *(it.get()) << "\n\n";
    }
    ss << "-----------------------";
    TINYTCP_LOG_DEBUG(g_logger) << ss.str();
}

ARPEntry::ptr ARPProcessor::cache_alloc() {
    ARPEntry::ptr arp_entry = std::make_shared<ARPEntry>();
    arp_entry->m_state = ARPEntry::NET_ARP_FREE;
    arp_entry->m_retry_cnt = g_arp_cache_retry_cnt->value();
    if (m_cache_list.size() >= g_arp_cache_size->value()) {
        auto back = m_cache_list.back();
        back->clear_buf();
        m_cache_list.pop_back();
    }
    m_cache_list.push_front(arp_entry);
    return arp_entry;
}


void ARPProcessor::release_cache(ARPEntry* arp_entry) {
    for (auto it = m_cache_list.begin(); it != m_cache_list.end(); ++it) {
        if ((*it).get() == arp_entry) {
            (*it)->clear_buf();
            m_cache_list.erase(it);
        }
    }
}

std::optional<ARPProcessor::CacheIterator> ARPProcessor::cache_find(const ipaddr_t& ipaddr) {
    for (auto it = m_cache_list.begin(); it != m_cache_list.end(); ++it) {
        // TINYTCP_LOG_DEBUG(g_logger) << ipaddr_t(*((uint32_t*)(*it)->m_ipaddr)) << "==" << ipaddr;
        if (*((uint32_t*)(*it)->m_ipaddr) == ipaddr.q_addr) {
            return it;
        }
    }
    return std::nullopt;
}


const uint8_t* ARPProcessor::arp_find(EtherNet* netif, const ipaddr_t& ipaddr) {
    // 判断当前地址是不是广播地址
    if (netif->is_local_broadcast(ipaddr) || netif->is_direct_broadcast(ipaddr)) {
        return ether_broadcast_addr();
    }

    auto entry = cache_find(ipaddr);
    if (entry != std::nullopt) {
        auto arp_entry = *entry.value();
        if (arp_entry->m_state == ARPEntry::NET_ARP_RESOLVED) {
            return arp_entry->m_hwaddr;
        }
    }

    return nullptr;
}

net_err_t ARPProcessor::cache_insert(EtherNet* netif, const ipaddr_t& ipaddr, uint8_t* hwaddr) {
    auto arp_it = cache_find(ipaddr);
    if (arp_it == std::nullopt) {
        auto arp_entry = cache_alloc();
        arp_entry->m_netif = netif;
        memcpy(arp_entry->m_ipaddr, ipaddr.a_addr, IPV4_ADDR_SIZE);
        memcpy(arp_entry->m_hwaddr, hwaddr, ETHER_HWA_SIZE);
        arp_entry->m_state = ARPEntry::NET_ARP_RESOLVED;
        arp_entry->m_timeout = g_arp_cache_stable_timeout->value();
    }
    else {
        TINYTCP_LOG_DEBUG(g_logger) << "update arp cache";
        auto arp_entry = *arp_it.value();
        arp_entry->m_netif = netif;
        memcpy(arp_entry->m_ipaddr, ipaddr.a_addr, IPV4_ADDR_SIZE);
        memcpy(arp_entry->m_hwaddr, hwaddr, ETHER_HWA_SIZE);
        arp_entry->m_state = ARPEntry::NET_ARP_RESOLVED;
        arp_entry->m_timeout = g_arp_cache_stable_timeout->value();
        if (arp_it.value() != m_cache_list.begin()) {
            m_cache_list.erase(arp_it.value());
            m_cache_list.push_front(arp_entry);
        }
        // 如果有缓存的数据包，这时有mac地址来了，就直接全部发送
        net_err_t err = arp_entry->cache_send_all();
        if ((int8_t)err < 0) {
            TINYTCP_LOG_ERROR(g_logger) << "send packet error";
            return err;
        }
    }

    debug_print();
    return net_err_t::NET_ERR_OK;
}

net_err_t ARPProcessor::arp_resolve(EtherNet* netif, const ipaddr_t& ipaddr, PktBuffer::ptr buf) {
    auto cache_opt = cache_find(ipaddr);
    if (cache_opt != std::nullopt) {
        auto cache = *cache_opt.value();
        // 如果在缓存中找到有地址的arp缓存，就直接响应
        if (cache->m_state == ARPEntry::NET_ARP_RESOLVED) {
            return netif->ether_raw_out(NET_PROTOCOL_IPv4, cache->m_hwaddr, buf);
        }
        else {
            if (cache->m_buf_list.size() < g_arp_cache_max_pktbuf_size->value()) {
                cache->m_buf_list.push_back(buf);
                return net_err_t::NET_ERR_OK;
            }
            else {
                TINYTCP_LOG_WARN(g_logger) << "too many pktbuf waiting";
                return net_err_t::NET_ERR_FULL;
            }
        }
    }
    else {
        TINYTCP_LOG_INFO(g_logger) << "make arp request";
        auto entry = cache_alloc();
        entry->m_netif = netif;
        memcpy(entry->m_ipaddr, ipaddr.a_addr, IPV4_ADDR_SIZE);
        entry->m_state = ARPEntry::NET_ARP_WAITING;
        entry->m_timeout = g_arp_cache_pending_timeout->value();
        entry->m_buf_list.push_back(buf);

        debug_print();
        return netif->make_arp_request(ipaddr);
    }

    return net_err_t::NET_ERR_OK;
}

void ARPProcessor::clear(EtherNet* netif) {
    for (auto it = m_cache_list.begin(); it != m_cache_list.end(); ) {
        if ((*it)->m_netif == netif) {
            it = m_cache_list.erase(it);
        }
        else {
            ++it;
        }
    }
}

std::ostream& operator<<(std::ostream& os, const arp_pkt_t& arp_pkt) {
    os  << "htype=        "   << std::hex << (uint32_t)net_to_host(arp_pkt.htype) << std::dec
        << "\niptype=       " << (uint32_t)net_to_host(arp_pkt.iptype)
        << "\nhwlen=        " << (uint32_t)(arp_pkt.hwlen)
        << "\niplen=        " << (uint32_t)(arp_pkt.iplen)
        << "\nopcode=       " << (uint32_t)net_to_host(arp_pkt.opcode)
        << "\nsender_hwaddr=" << std::make_pair(std::string((char*)arp_pkt.sender_hwaddr, ETHER_HWA_SIZE), true)
        << "\nsender_ipaddr=" << ipaddr_t(*((uint32_t*)arp_pkt.sender_ipaddr))
        << "\ntarget_hwaddr=" << std::make_pair(std::string((char*)arp_pkt.target_hwaddr, ETHER_HWA_SIZE), true)
        << "\ntarget_ipaddr=" << ipaddr_t(*((uint32_t*)arp_pkt.target_ipaddr));

    return os;
}

std::ostream& operator<<(std::ostream& os, const ARPEntry& arp_entry) {
    os  << "ip=" << ipaddr_t(*(uint32_t*)arp_entry.m_ipaddr)
        << "\nmac=" << std::make_pair(std::string((char*)arp_entry.m_hwaddr, ETHER_HWA_SIZE), true)
        << "\nstate=" << magic_enum::enum_name(arp_entry.m_state)
        << "\ntimeout=" << arp_entry.m_timeout
        << "\nretry_cnt=" << arp_entry.m_retry_cnt
        << "\nbuf_size=" << arp_entry.m_buf_list.size();

    return os;
}

} // namespace tinytcp


