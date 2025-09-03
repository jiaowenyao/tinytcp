#pragma once
#include "net_err.h"
#include "ipaddr.h"
#include "pktbuf.h"

namespace tinytcp {

#define ICMPv4_ECHO             0

#define ICMPv4_ECHO_REQUEST     8
#define ICMPv4_ECHO_REPLY       0

#pragma pack(1)
struct icmpv4_hdr_t {
    uint8_t type;
    uint8_t code;
    uint16_t checksum;
};

struct icmpv4_pkt_t {
    icmpv4_hdr_t hdr;
    union {
        uint32_t reverse;
    };
    uint8_t data[1];

    bool is_pkt_ok(uint32_t size, PktBuffer::ptr buf);
};


#pragma pack()


class ICMPProtocol {
public:
    ICMPProtocol();
    ~ICMPProtocol();

    net_err_t icmpv4_in(const ipaddr_t& src_ip, const ipaddr_t& netif_ip, PktBuffer::ptr buf);
    net_err_t icmpv4_out(const ipaddr_t& dest_ip, const ipaddr_t& src_ip, PktBuffer::ptr buf);
    net_err_t icmpv4_echo_reply(const ipaddr_t& dest_ip, const ipaddr_t& src_ip, PktBuffer::ptr buf);
    // net_err_t icmpv4_out_unreach()
};


std::ostream& operator<<(std::ostream& os, const icmpv4_hdr_t& hdr);

} // namespace tinytcp


