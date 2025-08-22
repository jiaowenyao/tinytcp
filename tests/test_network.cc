
#include <thread>
#include "src/net/net.h"
#include "src/net/pktbuf.h"
#include "src/config.h"
#include <list>
#include <string.h>

template class std::list<tinytcp::PktBlock*>;
static tinytcp::Logger::ptr g_logger = TINYTCP_LOG_ROOT();


int main() {

    YAML::Node root = YAML::LoadFile("/home/jwy/workspace/tinytcp/config/log.yaml");
    tinytcp::Config::load_from_yaml(root);

    auto pktmgr = tinytcp::PktMgr::get_instance();


    tinytcp::ProtocolStack p;

    // p.init();
    //
    // p.start();

    auto network = p.get_network();
    auto netif = network->netif_open("loop");


    uint8_t hwaddr[] = {0x00, 0x15, 0x5d, 0x4b, 0x97, 0x15};
    tinytcp::pcap_data_t pcap_data {
        .ip = "172.27.241.212",
        .hwaddr = hwaddr
    };
    auto eth0 = network->netif_open("eth0", &pcap_data);

    network->debug_print();

    tinytcp::PktBuffer* buf = pktmgr->get_pktbuffer();
    buf->alloc(100);
    netif->netif_out(tinytcp::ipaddr_t(), buf);

    while (1) {
        std::this_thread::yield();
        // sleep(10);
    }

    return 0;
}

