#pragma once

#include <inttypes.h>
#include <iostream>
#include "log.h"


namespace tinytcp {

#define ETHER_MTU         1500
#define ETHER_HWA_SIZE    6
#define ETHER_DATA_MIN    46

// 以太网协议

#pragma pack(1)
struct ether_hdr_t {
    uint8_t dest[ETHER_HWA_SIZE];
    uint8_t src[ETHER_HWA_SIZE];
    uint16_t protocol;
};

struct ether_pkt_t {
    ether_hdr_t hdr;
    uint8_t data[ETHER_MTU];
};
#pragma pack()


std::string hwaddr_to_string(uint8_t* src);
std::ostream& operator<<(std::ostream& os, const ether_hdr_t& hdr);
std::string ether_pkt_data_to_string(const ether_pkt_t& pkt, uint32_t size);
void debug_print_ether_pkt(const ether_pkt_t& pkt, uint32_t size);
const uint8_t* ether_broadcast_addr();



} // namespace tinytcp



