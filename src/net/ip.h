#pragma once
#include <inttypes.h>
#include <memory>
#include "ipaddr.h"
#include "net_err.h"
#include "pktbuf.h"
#include "memblock.h"

namespace tinytcp {

class INetIF;


#pragma pack(1)

struct ipv4_hdr_t {
    union {
        struct {
#if BYTE_ORDER == LITTLE_ENDIAN
            uint16_t shdr    : 4;
            uint16_t version : 4;
            uint16_t tos     : 8;
#else
            uint16_t version : 4;
            uint16_t shdr    : 4;
            uint16_t tos     : 8;
#endif
        };
        uint16_t shdr_all;
    };
    uint16_t total_len;
    uint16_t id;
    union frag_t {
        struct {
#if BYTE_ORDER == LITTLE_ENDIAN
            uint16_t offset    : 13;
            uint16_t more      : 1;
            uint16_t disable   : 1;
            uint16_t reversed  : 1;
#else
            uint16_t reversed  : 1;
            uint16_t offset    : 13;
            uint16_t more      : 1;
            uint16_t disable   : 1;
#endif
        };
        uint16_t frag_all;
    } frag;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum;
    uint32_t src_ip;
    uint32_t dest_ip;

    void hdr_net_to_host() noexcept;
    void hdr_host_to_net() noexcept;
    uint32_t get_header_size() noexcept;
    uint32_t get_data_size() noexcept;
    uint16_t get_frag_start() noexcept;
    uint16_t get_frag_end() noexcept;
};

struct ipv4_pkt_t {
    ipv4_hdr_t hdr;
    uint8_t data[1];

    bool is_pkt_ok(uint16_t size, INetIF* netif);
};

#pragma pack()

struct ip_frag_t {
    using ptr = std::shared_ptr<ip_frag_t>;

    ipaddr_t ip;      // id分片从哪个地址发过来的
    uint16_t id;      // 包id
    uint64_t timeout; // 超时时间
    std::list<PktBuffer::ptr> buf_list;

    void reset() noexcept;
    bool frag_is_all_arrived() noexcept;
    PktBuffer::ptr frag_join() noexcept;
};


class IPProtocol {
public:
    using uptr = std::unique_ptr<IPProtocol>;

    IPProtocol();
    ~IPProtocol();
    void init();

    net_err_t ipv4_in(INetIF* netif, PktBuffer::ptr buf);
    net_err_t ipv4_out(uint8_t protocol, const ipaddr_t& dest, const ipaddr_t& src, PktBuffer::ptr buf);
    // 不重组的情况
    net_err_t ip_normal_in(INetIF* netif, PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip);
    // 重组的情况
    net_err_t ip_frag_in(INetIF* netif, PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip);
    net_err_t ip_frag_out(uint8_t protocol, INetIF* netif, const ipaddr_t& dest, const ipaddr_t& src, PktBuffer::ptr buf);
    void frag_add(ip_frag_t::ptr frag, const ipaddr_t& ip, uint16_t id);
    void remove_frag(const ip_frag_t& frag);

public:
    static LockFreeRingQueue<ip_frag_t*>& get_ip_frag_queue() noexcept;

public:
    ip_frag_t::ptr get_ip_frag();
    void release_ip_frag(ip_frag_t*);
    std::list<ip_frag_t::ptr>::iterator find_frag(const ipaddr_t& ipaddr, uint16_t id);
    net_err_t frag_insert(ip_frag_t::ptr frag, PktBuffer::ptr buf, ipv4_pkt_t* pkt);
    net_err_t frag_timer();
    void frag_debug_print();

private:
    std::list<ip_frag_t::ptr> m_frag_list;
};


std::ostream& operator<<(std::ostream& os, const ipv4_hdr_t& ipv4_hdr);
std::ostream& operator<<(std::ostream& os, const ip_frag_t& frag);

} // namespace tinytcp

