#include "link_layer.h"
#include "util.h"
#include "endiantool.h"
#include <string.h>
#include <iomanip>



namespace tinytcp {

static Logger::ptr g_logger = TINYTCP_LOG_NAME("system");

std::string hwaddr_to_string(uint8_t* src) {
    std::stringstream ss;
    ss << std::hex << std::setfill('0');

    for (int i = 0; i < ETHER_HWA_SIZE; ++i) {
        if (i == 0) {
            ss << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(src[i]));
        }
        else {
            ss << "-" << std::setw(2) << static_cast<unsigned int>(static_cast<unsigned char>(src[i]));
        }
    }
    return ss.str();
}

std::ostream& operator<<(std::ostream& os, const ether_hdr_t& hdr) {
    os  << "dest=" << hwaddr_to_string((uint8_t*)hdr.dest)
        << "; src=" << hwaddr_to_string((uint8_t*)hdr.src)
        << "; protocol=" << std::hex << std::setfill('0') << std::setw(4) << net_to_host(hdr.protocol);
    return os;
}

std::string ether_pkt_data_to_string(const ether_pkt_t& pkt, uint32_t size) {
    return std::string((char*)pkt.data, size);
}

void debug_print_ether_pkt(const ether_pkt_t& pkt, uint32_t size) {
    TINYTCP_LOG_DEBUG(g_logger)
        << pkt.hdr
        << "\ndata=" << std::make_pair(ether_pkt_data_to_string(pkt, size), true)
        << "\n";
}

const uint8_t* ether_broadcast_addr() {
    static const uint8_t broadcast[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
    return broadcast;
}

} // namespace tinytcp



