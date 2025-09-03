#pragma once
#include <inttypes.h>
#include <memory>
#include "ipaddr.h"
#include "net_err.h"
#include "pktbuf.h"

namespace tinytcp {

class INetIF;


#pragma pack(1)

struct ipv4_hdr_t {
    union {
#if BYTE_ORDER == LITTLE_ENDIAN
        struct {
            uint16_t shdr    : 4;
            uint16_t version : 4;
            uint16_t tos     : 8;
        };
#else
        struct {
            uint16_t version : 4;
            uint16_t shdr    : 4;
            uint16_t tos     : 8;
        };
#endif
        uint16_t shdr_all;
    };
    uint16_t total_len;
    uint16_t id;
    uint16_t frag_all;
    uint8_t ttl;
    uint8_t protocol;
    uint16_t hdr_checksum;
    uint32_t src_ip;
    uint32_t dest_ip;

    void hdr_net_to_host();
    void hdr_host_to_net();
    uint32_t get_header_size();
};

struct ipv4_pkt_t {
    ipv4_hdr_t hdr;
    uint8_t data[1];

    bool is_pkt_ok(uint16_t size, INetIF* netif);
};

#pragma pack()


class IPProtocol {
public:
    using uptr = std::unique_ptr<IPProtocol>;

    IPProtocol();
    ~IPProtocol();

    net_err_t ipv4_in(INetIF* netif, PktBuffer::ptr buf);
    net_err_t ipv4_out(uint8_t protocol, const ipaddr_t& dest, const ipaddr_t& src, PktBuffer::ptr buf);
    // 不重组的情况
    net_err_t ip_normal_in(INetIF* netif, PktBuffer::ptr buf, const ipaddr_t& src_ip, const ipaddr_t& dest_ip);
private:
};


std::ostream& operator<<(std::ostream& os, const ipv4_hdr_t& ipv4_hdr);

} // namespace tinytcp

