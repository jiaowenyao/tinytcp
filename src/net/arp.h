#pragma once

#include "ipaddr.h"
#include "link_layer.h"
#include "pktbuf.h"


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

class INetIF;
class ARPProcessor;

class ARPEntry {
friend ARPProcessor;
public:
    enum arp_state {
        NET_ARP_FREE,
        NET_ARP_WAITING,
        NET_ARP_RESOLVED,
    };
public:
    ARPEntry();
    ~ARPEntry();

private:
    uint8_t ipaddr[IPV4_ADDR_SIZE];
    uint8_t hwaddr[ETHER_HWA_SIZE];
    arp_state state;

    std::list<PktBuffer::ptr> buf_list;
    INetIF* netif;
};

class ARPProcessor {
public:
    ARPProcessor();
    ~ARPProcessor();

    PktBuffer::ptr make_request(INetIF* netif, const ipaddr_t& dest);
    PktBuffer::ptr make_gratuitous(INetIF* netif);
    PktBuffer::ptr make_response(INetIF* netif, PktBuffer::ptr buf);

// private:
//     NetIF* netif;

};

} // namespace tinytcp



