#pragma once

#include "ipaddr.h"
#include "link_layer.h"
#include "pktbuf.h"
#include <optional>


namespace tinytcp {

#define ARP_HW_ETHER     1
#define ARP_REQUEST      1
#define ARP_REPLAY       2


#pragma pack(1)
struct arp_pkt_t {
    uint16_t htype;
    uint16_t iptype;
    uint8_t hwlen;
    uint8_t iplen;
    uint16_t opcode;
    uint8_t sender_hwaddr[ETHER_HWA_SIZE];
    uint8_t sender_ipaddr[IPV4_ADDR_SIZE];
    uint8_t target_hwaddr[ETHER_HWA_SIZE];
    uint8_t target_ipaddr[IPV4_ADDR_SIZE];

    bool is_pkt_ok(uint16_t size);
};
#pragma pack()

class EtherNet;
class ARPProcessor;

class ARPEntry {
friend ARPProcessor;
friend std::ostream& operator<<(std::ostream& os, const ARPEntry& arp_entry);
public:
    using ptr = std::shared_ptr<ARPEntry>;

    enum arp_state {
        NET_ARP_FREE,
        NET_ARP_WAITING,
        NET_ARP_RESOLVED,
    };
public:
    ARPEntry();
    ~ARPEntry();

    void clear_buf();
    net_err_t cache_send_all();

private:
    uint8_t m_ipaddr[IPV4_ADDR_SIZE] = {0};
    uint8_t m_hwaddr[ETHER_HWA_SIZE] = {0};
    arp_state m_state;

    int m_timeout = 0;
    int m_retry_cnt = 0;

    std::list<PktBuffer::ptr> m_buf_list;
    EtherNet* m_netif;
};

class ARPProcessor {
public:
    using CacheIterator = std::list<ARPEntry::ptr>::iterator;

    ARPProcessor();
    ~ARPProcessor();

    PktBuffer::ptr make_request(EtherNet* netif, const ipaddr_t& dest);
    PktBuffer::ptr make_gratuitous(EtherNet* netif);
    PktBuffer::ptr make_response(EtherNet* netif, PktBuffer::ptr buf);

    void debug_print();

    ARPEntry::ptr cache_alloc();
    void release_cache(ARPEntry* arp_entry);
    std::optional<CacheIterator> cache_find(const ipaddr_t& ipaddr);
    const uint8_t* arp_find(EtherNet* netif, const ipaddr_t& ipaddr); // 查找ip地址对应的mac地址，可以用于解析广播地址
    net_err_t cache_insert(EtherNet* netif, const ipaddr_t& ipaddr, uint8_t* hwaddr);
    net_err_t arp_resolve(EtherNet* netif, const ipaddr_t& ipaddr, PktBuffer::ptr buf);

    uint32_t get_cache_timeout() const noexcept;
    void cache_timer();

    // 网卡关闭时关闭相关的缓存
    void clear(EtherNet* netif);
private:
    std::list<ARPEntry::ptr> m_cache_list;
};

std::ostream& operator<<(std::ostream& os, const arp_pkt_t& arp_pkt);
std::ostream& operator<<(std::ostream& os, const ARPEntry& arp_entry);

} // namespace tinytcp



